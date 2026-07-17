# Diseño — Unidad II: Etapas 4-6 del compilador de Kel

**Fecha:** 2026-07-16
**Estado:** aprobado, pendiente de plan de implementación
**Alcance:** completar el pipeline del compilador (código intermedio, optimización,
generación de código final) y cerrar los huecos de la rúbrica del proyecto final.

---

## 1. Contexto y motivación

La Unidad I está completa y verificada: el proyecto compila sin warnings y los
9/9 tests de regresión pasan (verificado el 2026-07-16 con `mingw32-make` +
`bash tests/run_tests.sh`). El último commit es del 17 de abril; las etapas 4-6
nunca se empezaron y `src/codegen.h` es solo un esqueleto de tipos sin
implementación.

### Diagnóstico contra la rúbrica

La rúbrica del proyecto final (`ESQUEMA DE PRESENTACIÓN DEL PROYECTO FINAL.pdf`)
reparte 20 puntos en 12 criterios. Estado actual:

| # | Criterio | Pts | Estado |
|---|----------|-----|--------|
| 1 | Definición del lenguaje | 1.0 | Cubierto por SPEC.md |
| 2 | Especificación léxica | 1.5 | Cubierto (tabla de 47 tokens) |
| 3 | Expresiones regulares y autómatas | 1.5 | **Falta** |
| 4 | Implementación del léxico | 2.0 | Cubierto (`lexer.c`) |
| 5 | Gramática BNF/EBNF + ambigüedad | 2.0 | **Falta** |
| 6 | Implementación del sintáctico | 2.0 | Cubierto (`parser.c`) |
| 7 | Semántico + tabla de símbolos | 2.0 | **Cubierto** (`--symbols`, Plan 1) |
| 8 | Código intermedio | 1.5 | **Falta** |
| 9 | Optimización | 1.0 | **Falta** |
| 10 | Código final | 2.0 | **Falta** |
| 11 | Funcionamiento integral | 1.5 | Cubierto (suite de tests) |
| 12 | Documentación, informe y manuales | 2.0 | **Falta** (responsabilidad del grupo) |

**~10 de 20 puntos sin asegurar.** De esos, 3.5 (criterios 3 y 5) no requieren
programar: son documentación de comportamiento que el código ya tiene.

### Reparto de trabajo

Acordado con el usuario: **él escribe el código, el grupo escribe el informe.**
Este diseño cubre el código y los dos documentos técnicos (`GRAMMAR.md`,
`AUTOMATA.md`) que deben derivarse del código y que el grupo no puede escribir
sin leerlo. El informe, los manuales y el PPT quedan fuera de alcance.

No hay presión de fecha (indicación explícita del usuario), así que el diseño
prioriza corrección sobre atajos.

---

## 2. Arquitectura

```
programa.kel
   ↓  lexer.c        Etapa 1  ──→ --tokens
   ↓  parser.c       Etapa 2  ──→ --ast
   ↓  semantic.c     Etapa 3  ──→ --sem, --symbols
   ↓  ir.c           Etapa 4  ──→ --ir        [nuevo]
   ↓  optimize.c     Etapa 5  ──→ --opt       [nuevo]
   ↓  emit_c.c       Etapa 6  ──→ --emit-c    [nuevo]
   ↓  gcc                      ──→ -o programa.exe
programa.exe
```

Coincide con el diagrama de arquitectura de la página 5 del PDF
(Código Fuente → Léxico → Sintáctico → Semántico → Intermedio → Optimización →
Generación → Ejecutable).

### Módulos nuevos

| Módulo | Responsabilidad | Interfaz | Depende de |
|--------|-----------------|----------|------------|
| `ir.c` | AST anotado → TAC | `kel_gen(Node*) -> IRProgram` | `ir.h`, `parser.h` |
| `optimize.c` | optimiza un `IRProgram` in-place | `kel_optimize(IRProgram*) -> OptStats` | `ir.h` |
| `emit_c.c` | `IRProgram` → texto C | `kel_emit_c(const IRProgram*, FILE*)` | `ir.h` |
| `symtab.c` | log de declaraciones + impresión | `kel_symlog_add()`, `kel_symlog_print()` | `parser.h` |

**Decisión: `src/codegen.h` se renombra a `src/ir.h`.** El contenido son tipos
del código *intermedio* (Etapa 4), pero "codegen" significa universalmente
generación de código *final* (Etapa 6). El nombre actual invita a la confusión
durante la defensa. `emit_c.c` pasa a ser el generador. Requiere actualizar la
referencia en SPEC.md (una línea) y en README.md.

**Invariante central: el optimizador preserva el formato del IR.** Recibe un
`IRProgram` válido y lo deja siendo un `IRProgram` válido (lo modifica in-place
y devuelve estadísticas). `emit_c.c` no sabe ni le importa si el IR pasó por el
optimizador, de modo que este es enteramente desconectable: `--emit-c` sin
`--opt` produce C válido, solo peor. Esto es lo que hace posible la prueba de
equivalencia (§6.2).

**Módulos que no se tocan:** `lexer.c`, `parser.c`, `diag.c`. Funcionan y están
probados. `semantic.c` recibe solo dos añadidos acotados: el registro de
símbolos en el log y las firmas de los built-ins `read_*`.

---

## 3. Etapa 4 — Código intermedio (`ir.c`)

Traducción del AST anotado a TAC. La mayoría es directa (`N_BINOP` → `IR_BINOP`,
`N_CALL` → `IR_PARAM`×n + `IR_CALL`, `N_IF`/`N_WHILE` → etiquetas y saltos).
`for i in a..b` se desazucara a init + condición + incremento.

### Cambios necesarios sobre el esqueleto de `codegen.h`

**3.1 — `IRProgram` pasa a ser por función.**

El header de abril define `IRProgram` como una lista plana de `Instr`, pero el
propio header documenta que los temporales y etiquetas van "numerados por
función". Además, emitir `int suma(int a, int b)` en la Etapa 6 requiere la
firma, que una lista de instrucciones no lleva.

```c
typedef struct {
    char*    name;          /* owned */
    Param*   params;        /* no-owned; apunta al AST */
    size_t   param_count;
    KelType* ret_type;      /* no-owned; apunta al AST */
    Instr*   body;          /* owned */
    size_t   count, capacity;
    size_t   n_temps;       /* temporales usados: t1..t_temps */
    size_t   n_labels;      /* etiquetas usadas: L1..L_labels */
} IRFunction;

typedef struct {
    IRFunction* fns;        /* owned */
    size_t      count, capacity;
} IRProgram;
```

(Tal como quedó implementado en `src/ir.h`: `size_t` en los contadores, por
coherencia con el resto del codebase.)

El optimizador trabaja por función de forma natural con esta estructura.

**Tiempo de vida: el AST debe sobrevivir al `IRProgram`.** `params` y `ret_type`
apuntan al AST sin poseerlo. Hoy `main.c` llama a `kel_free_ast()` justo tras el
análisis semántico; al conectar `kel_gen` esa llamada debe moverse detrás de la
generación de IR y de la emisión de C, o `emit_c.c` leerá memoria liberada. Está
documentado en el propio `ir.h` para que quien implemente la Etapa 4 lo vea sin
tener que leer este spec.

**3.2 — Dos opcodes nuevos.**

| Opcode | Forma | Motivo |
|--------|-------|--------|
| `IR_ARRAY_NEW` | `dst = alloc <tipo>[n]` | No había forma de representar `val nums = [1,2,3]` |
| `IR_READ` | `dst = read <tipo>` | Built-ins de entrada (§5.1) |

`IR_ARRAY_NEW` se imprime como `alloc`, no como `array`: `IR_INDEX_LOAD` imprime
`t4 = arr[i]`, y `array` no es palabra reservada en Kel, así que `t6 = array[3]`
se leería como una indexación sobre una variable con ese nombre. El `--ir` se
demuestra en vivo, de modo que la ambigüedad tiene coste real.

`IR_INDEX_LOAD` e `IR_INDEX_STORE` ya existen y se reutilizan: un literal de
array se traduce a un `IR_ARRAY_NEW` seguido de un `IR_INDEX_STORE` por
elemento.

**3.3 — `&&` y `||` no son `IR_BINOP`.**

Traducirlos como binop normal evalúa ambos lados siempre, de modo que
`f() && g()` ejecutaría `g()` aunque `f()` sea falso. La traducción correcta
usa cortocircuito con etiquetas:

```
t1 = a
ifFalse t1 goto L1
t1 = b
L1:
```

Para `||`, simétrico con `IR_IF_GOTO`. Es coherente con C/Kotlin/Rust y
demuestra generación de control de flujo a partir de un operador aparentemente
aritmético — buen material de exposición en el `--ir`.

**3.4 — Arreglos como parámetros: paso por referencia.**

Kel permite `fn f(a: [int])` estructuralmente (`Param` lleva un `KelType*`).
Al emitir C, un arreglo decae a puntero, de modo que una mutación dentro de la
función es visible para quien llama. **Decisión: los arreglos se pasan por
referencia**, coherente con C y con la mayoría de lenguajes con arreglos
mutables. Consecuencias:

- Debe documentarse explícitamente en SPEC.md (hoy no lo dice).
- El optimizador nunca propaga a través de `IR_INDEX_LOAD` cruzando un
  `IR_CALL` (§4.3).

Este punto era un hueco semántico sin resolver de la Unidad I, no una decisión
nueva de la Unidad II.

### Verificación

El `--ir` imprime el TAC en el formato ya documentado en `codegen.h`, que se
conserva sin cambios salvo los dos opcodes nuevos.

---

## 4. Etapa 5 — Optimización (`optimize.c`)

### 4.1 — Los cinco pases

El criterio 15 del PDF pide cuatro optimizaciones concretas; CLAUDE.md quería
una quinta. Las cinco:

| Pase | Ejemplo |
|------|---------|
| Plegado de constantes | `t1 = 5 + 8` → `t1 = 13` |
| Propagación de constantes | `t1 = 5; t2 = t1 + 3` → `t2 = 5 + 3` → `t2 = 8` |
| Propagación de copias | `t1 = a; t2 = t1 + 1` → `t2 = a + 1` |
| Simplificación algebraica | `x*1`→`x`, `x+0`→`x`, `x*0`→`0`, `x/1`→`x`, `x-0`→`x` |
| Eliminación de código muerto | temporal asignado y nunca leído → se borra |

Los pases se repiten **hasta punto fijo** (un plegado habilita una propagación
que habilita otro plegado), con un tope de iteraciones para garantizar
terminación.

### 4.2 — Alcance: bloques básicos, no la función entera

**Esta es la decisión de corrección más importante del diseño.** La propagación
ingenua sobre la función completa rompe este programa:

```kel
var x = 0
while x < 10 { x = x + 1 }
```

Propagar `x = 0` dentro del cuerpo del bucle produce un bucle infinito: el
programa no falla al compilar, se cuelga en ejecución.

El IR se parte en **bloques básicos** (un bloque termina en una etiqueta o en un
salto) y la propagación opera solo dentro de cada bloque. Es la *optimización
local* del dragon book. Justificación de por qué no se optimiza entre bloques:
requiere análisis de flujo de datos global, que excede el alcance del curso.

### 4.3 — Reglas de seguridad

- Los temporales son locales a su bloque por construcción; las **variables se
  asumen vivas al salir del bloque** (conservador).
- Nunca se eliminan instrucciones con efectos: `IR_CALL`, `IR_PRINTLN`,
  `IR_READ`, `IR_INDEX_STORE`, `IR_RETURN`, saltos y etiquetas.
- No se propaga a través de `IR_INDEX_LOAD` cruzando un `IR_CALL` (§3.4:
  los arreglos se pasan por referencia y la llamada puede mutarlos).
- Kel no tiene variables globales ni punteros, de modo que una llamada **no**
  puede modificar las variables escalares de quien llama. Esas no se invalidan.
- Código inalcanzable (instrucciones tras `IR_GOTO`/`IR_RETURN` hasta la
  siguiente etiqueta) se elimina.
- Tras eliminar código, las **etiquetas que quedan sin ningún salto que las
  apunte se eliminan** también. No es una optimización real (no ahorra trabajo
  en ejecución): existe para que el C generado no dispare `-Wunused-label`
  (§5.4.2).

### 4.4 — Salida

`--opt` imprime el TAC optimizado. `OptStats` reporta instrucciones antes,
después y por pase, alimentando `--stats` (§5.3).

---

## 5. Etapa 6 y añadidos

### 5.1 — Built-ins de entrada

Tres funciones: `read_int() -> int`, `read_float() -> float`,
`read_line() -> string`. Snake_case por coherencia con el sabor Rust del
lenguaje (`println`, `read_line`).

**No son palabras reservadas.** `println` es una sentencia (`TOKEN_PRINTLN` →
`N_PRINTLN`), pero `read_int()` devuelve un valor, así que es una expresión — y
`read_int` seguido de `(` ya parsea hoy como `N_CALL`, porque es un
`TOKEN_IDENT` normal. Por tanto:

- **Lexer y parser: cero cambios.** No hay tokens nuevos.
- `semantic.c`: registrar las tres firmas en la tabla de funciones e impedir su
  redefinición por el usuario.
- `ir.c` / `emit_c.c`: `N_CALL` a un `read_*` → `IR_READ` → `scanf` del runtime.

Cierra el hueco de "Entrada y salida" del criterio 1 y permite reproducir el
ejemplo `LEER A` del profesor: `val a = read_int()`.

**Riesgo para el Plan 2:** `kel_builtins[]` (`semantic.c`, cerca de la línea
268) es una tabla `static` local a `semantic.c` — nada fuera de ese archivo
puede preguntar "¿este nombre es un builtin?". `ir.c` necesita exactamente
esa pregunta para decidir si un `N_CALL` a `read_int`/`read_float`/
`read_line` se traduce a `IR_READ` en vez de a `IR_CALL` normal. Si `ir.c`
simplemente repite las tres cadenas literales (`"read_int"`, `"read_float"`,
`"read_line"`) para reconocerlas, es la misma duplicación de nombres que
este proyecto ya tuvo que limpiar una vez — dos listas que se pueden
desincronizar si algún día se agrega un cuarto builtin. La implementación
del Plan 2 debe exponer una consulta desde `semantic.h` (por ejemplo,
`int kel_is_builtin(const char* name)`) que `ir.c` pueda llamar, en vez de
hardcodear los tres nombres de nuevo. No se agrega esa API todavía —nada la
consume hasta que `ir.c` exista— pero debe entrar como parte del trabajo de
la Etapa 4, no como deuda a mitad de esa implementación.

### 5.2 — Tabla de símbolos visible (`--symbols`)

**Problema:** `scope_pop` (`semantic.c:87`) libera los símbolos al cerrar el
scope. Al terminar la compilación no queda nada que imprimir, así que el
criterio 7 no se puede demostrar aunque el análisis funcione.

**Solución:** `symtab.c` mantiene un log append-only. Cada vez que `declare()`
registra un símbolo, se copia al log. **El ciclo de vida de los scopes no
cambia** — solo se graba lo que ocurre.

Columnas:

```
Ámbito      Identificador  Tipo      Mut  Despl  Línea  Valor
main        nombre         string    val      0     23  "Diego"
main        puntaje        int       var      8     24  0
main        pi             float     val     16     25  3.1415
main.for    i              int       val     24     32  —
suma        a              int       val      0      1  —
```

- **`Despl`** es el desplazamiento relativo al marco de la función, **alineado al
  tamaño del tipo**, con este modelo de tamaños:

  | Tipo | Tamaño | Alineación |
  |------|--------|-----------|
  | `int` | 4 | 4 |
  | `float` | 8 | 8 |
  | `bool` | 1 | 1 |
  | `string` | 8 (puntero) | 8 |
  | `[T]` | 8 (referencia) | 8 |

  No es una dirección absoluta: en Kel las direcciones reales las decide gcc.
  El PDF muestra direcciones tipo 100/104, pero inventarlas sería un dato falso;
  el desplazamiento tiene la misma forma y lo calcula el compilador de verdad.

  **Los arreglos cuentan como una referencia de 8 bytes, no como n×elemento.**
  Motivo: `KelType` no guarda el tamaño del arreglo — `[int]` no dice cuántos
  elementos — así que n no está disponible desde el tipo. Es exactamente cierto
  para parámetros (§3.4: los arreglos se pasan por referencia) y es una
  simplificación declarada para locales, donde el C generado los coloca inline.
  El desplazamiento es un modelo del marco, no un compromiso con el layout que
  gcc acabe eligiendo, así que la simplificación es honesta mientras se declare.
- **`Valor`** se llena solo cuando el inicializador es una constante conocida en
  compilación; si no, `—`. La tabla de símbolos no conoce valores de runtime.
- **`Ámbito`** y **`Mut`** no están en el ejemplo del PDF, pero son información
  real que el compilador tiene y demuestran los scopes anidados y el `val`/`var`
  — dos cosas que el semántico hace bien y que hoy no se pueden ver.

### 5.3 — Estadísticas (`--stats`)

El criterio 21 pide indicadores concretos y hoy no hay de dónde sacarlos. Con
todas las fases instrumentadas:

```
Tokens reconocidos:     247
Reglas sintácticas:      31
Nodos del AST:          156
Símbolos declarados:     18
Instrucciones TAC:       84
Tras optimizar:          61   (-27%)
Líneas de C generadas:   93
Errores detectados:       0
Tiempo de compilación:  3.2 ms  (léx 0.4 · sint 0.9 · sem 0.7 · ir 0.6 · opt 0.3 · emit 0.3)
```

El porcentaje de reducción es la evidencia de que la Etapa 5 hace algo, y le da
al grupo números reales para la sección de Resultados del informe.

### 5.4 — Generación de C (`emit_c.c`)

Cada `IRFunction` → una función de C. Temporales declarados como locales al
inicio, etiquetas → etiquetas de C, `goto` → `goto`. El resultado es C con
saltos, que es exactamente el aspecto de la salida de un compilador real antes
del register allocator, y permite poner `--ir` y el `.c` lado a lado con
correspondencia línea por línea.

**Archivo único autocontenido.** El runtime se emite inline en la cabecera del
`.c` generado, de modo que `kelc programa.kel --emit-c > programa.c && gcc
programa.c` funciona sin rutas de include ni librerías.

Runtime (`kel_rt.h`, emitido inline):

| Función | Uso |
|---------|-----|
| `kel_concat(a, b)` | `IR_BINOP` `+` sobre strings |
| `kel_read_raw_line()` | interna: única puerta de entrada de stdin (§5.4.1) |
| `kel_read_int()`, `kel_read_float()`, `kel_read_line()` | `IR_READ` |
| helpers de `printf` por tipo | `IR_PRINTLN` |

Mapeo de tipos: `int`→`long long`, `float`→`double`, `bool`→`int`,
`string`→`char*`, `[T]`→arreglo nativo de C (tamaño fijo conocido en
compilación). `fn main()` → `int main(void)`.

**Limitación conocida y aceptada:** `kel_concat` hace `malloc` y nunca libera.
Los strings tienen fugas; la memoria se recupera al terminar el proceso. Kel v1
no tiene GC ni conteo de referencias. Va documentado en "Alcances y
limitaciones" del informe.

#### 5.4.1 — Lectura de entrada: sin `scanf`, todo por líneas

**`scanf` queda prohibido en el runtime.** Tiene dos fallos independientes:

1. `scanf("%s", buf)` no acota la escritura → desbordamiento de búfer.
2. `%s` corta en el primer espacio → `read_line()` devolvería una palabra, no
   una línea. Estaría mal aunque el búfer fuera infinito.

Y hay un tercero, peor, al mezclarlo: `scanf("%lld")` **deja el `\n` en el
búfer**, de modo que un `read_line()` posterior devuelve cadena vacía sin
esperar entrada. Es el bug clásico de C y el ejemplo `LEER A; LEER B` del
profesor es exactamente el patrón que lo dispara.

**Diseño:** una sola función interna `kel_read_raw_line()` es la única que toca
stdin. Bucle con `fgetc` y crecimiento geométrico del búfer: no acota mal, no
trunca, y es portable — `getline()` es POSIX y TDM-GCC va contra msvcrt, donde
no existe. Los tres built-ins se construyen encima:

- `kel_read_line()` → `kel_read_raw_line()` directo.
- `kel_read_int()` → `kel_read_raw_line()` + `strtoll`.
- `kel_read_float()` → `kel_read_raw_line()` + `strtod`.

Al no haber `scanf` no hay estado residual en el búfer, y `strtoll`/`strtod`
permiten detectar entrada inválida y emitir un error legible en vez de dejar la
variable sin tocar.

El puntero devuelto se hereda de `malloc` y no se libera, coherente con la
limitación de strings ya aceptada.

#### 5.4.2 — Etiquetas: siempre `L1: ;`

En C anterior a C23 una etiqueta debe ir seguida de una sentencia, así que
`L1: }` viola el estándar (gcc: *label at end of compound statement*). **En este
codegen está garantizado que ocurre**: todo `while` termina con su etiqueta de
salida, de modo que cualquier `while` en última posición del cuerpo de una
función lo produce (`tests/ok/full.kel` ya tiene uno).

No se trata como contingencia: **se emite `L1: ;` incondicionalmente**. El `;`
sobrante es gratis y evita tener que decidir si una etiqueta es la última —
lógica que podría fallar.

Además, el optimizador puede dejar etiquetas sin ningún salto que las apunte, lo
que dispara `-Wunused-label`. Un pase que elimine etiquetas sin referencias lo
evita.

**Requisito: el C generado debe compilar limpio con `gcc -Wall`.** El profesor lo
va a compilar durante la defensa.

#### 5.4.3 — Temporales: inicializados a cero

Todos los temporales se declaran inicializados (`long long t1 = 0;`).

Con la forma de IR del cortocircuito (§3.3) los temporales quedan siempre
asignados antes de leerse, de modo que **no hay un bug real de uso sin
inicializar**. Pero el `-Wmaybe-uninitialized` de gcc da falsos positivos
conocidos alrededor de `goto`, porque su análisis de flujo se rinde ante saltos
arbitrarios. La inicialización es un dead store que gcc elimina al optimizar:
silencia el falso positivo, cumple el requisito de compilar limpio con `-Wall`,
y actúa de red de seguridad si un bug futuro de codegen dejara un camino sin
asignar. Coste real: cero.

### 5.5 — CLI final

```
kelc [flags] archivo.kel
  --tokens      Etapa 1: stream de tokens
  --ast         Etapa 2+3: AST con tipos inferidos
  --symbols     Etapa 3: tabla de símbolos
  --sem         Etapa 3: solo resultado semántico
  --ir          Etapa 4: TAC sin optimizar
  --opt         Etapa 5: TAC optimizado
  --emit-c      Etapa 6: imprime el C generado
  -o <exe>      compila el C con gcc → ejecutable
  --stats       indicadores (criterio 21)
  -h, --help    ayuda
```

Los flags cubren de paso el criterio 20 ("Interfaz del Programa"): dan al grupo
capturas de cada fase sin construir una GUI.

---

## 6. Pruebas

La suite actual comprueba **solo el código de salida**: compiló o no. Basta para
las etapas 1-3; no vale nada para las 4-6, porque un compilador puede terminar
con éxito y generar código que hace algo equivocado.

### 6.1 — Tests end-to-end (`tests/run/`)

Cada caso: `programa.kel` + `programa.expected`. Cadena completa: `kelc` →
`.c` → `gcc` → ejecutar → comparar stdout contra `.expected`. Es lo único que
demuestra que las etapas 4-6 funcionan de verdad.

**El `gcc` de estos tests corre con `-Wall -Wextra` y trata los warnings como
error.** El requisito de §5.4.2 (el C generado compila limpio) solo vale algo si
un test lo hace fallar; si no, es una intención. Esto cubre a la vez las
etiquetas colgantes, las etiquetas sin usar y los falsos positivos de
`-Wmaybe-uninitialized`.

### 6.2 — Prueba de equivalencia del optimizador

**La prueba más valiosa del proyecto.** Cada programa de `tests/run/` se compila
dos veces, **con y sin `--opt`, y ambas salidas deben ser idénticas byte a
byte.**

Un optimizador correcto es exactamente aquel que no altera el resultado
observable, de modo que esta prueba verifica la propiedad que *define* la Etapa
5. Los bugs de optimizador son silenciosos: el caso del `while x < 10` de §4.2
no da error de compilación ni crash — se cuelga. Esta prueba lo caza en el
momento, y cuesta un bucle de dos líneas en el script.

### 6.3 — Tests de optimizaciones concretas (`tests/opt/`)

La equivalencia demuestra que el optimizador no rompe nada, pero no que *haga*
algo: un optimizador que no toca nada la pasa perfecto. Un programa mínimo por
pase, afirmando contra `--stats` / `--opt` que el pase disparó: que `5 + 8`
desaparece y aparece `13`, que el código muerto se fue, que `x * 1` se
simplificó.

### 6.4 — Tests de entrada (`tests/read/`)

`programa.kel` + `programa.stdin` + `programa.expected`, para alimentar los
`read_*`.

Casos obligatorios, cada uno cubriendo un fallo concreto de §5.4.1:

| Caso | Qué caza |
|------|----------|
| `read_int()` seguido de `read_line()` | El `\n` residual: con `scanf` el `read_line` devolvería vacío |
| `read_line()` con espacios ("hola mundo") | El `%s` que corta en el primer espacio |
| `read_line()` con una línea muy larga (>4 KB) | Desbordamiento y truncado silencioso del búfer |
| `read_int()` con entrada no numérica | Que `strtoll` detecte el error en vez de dejar basura |

El primero replica el `LEER A; LEER B` del ejemplo del profesor, que es el
patrón exacto que dispara el bug del búfer.

`tests/ok/` y `tests/bad/` se conservan sin cambios.

### 6.5 — Realidades de Windows

- **En la máquina del usuario no existe `make`, solo `mingw32-make`**
  (TDM-GCC en `/c/TDM-GCC-64/bin`). El `make` del README no funciona tal cual.
- Los ejecutables generados llevan `.exe`; el script debe manejarlo.
- Ambas cosas deben quedar documentadas en el README, o el grupo pierde tiempo.

---

## 7. Documentos técnicos

Los criterios 3 y 5 son **3.5 puntos de documentación pura que el grupo no puede
escribir**: hay que derivarlos leyendo `lexer.c` y `parser.c`. Un BNF inventado
que no coincida con el parser real es peor que no tenerlo, porque el profesor
puede contrastarlos.

- **`docs/GRAMMAR.md`** — BNF/EBNF completa de Kel, más la eliminación de
  ambigüedad: jerarquía de precedencia del parser, ausencia de recursividad
  izquierda (el descenso recursivo la prohíbe), factorización, y la prueba de
  que es LL(1) con un lookahead. Todo esto ya es cierto de `parser.c`; solo
  está sin escribir.
- **`docs/AUTOMATA.md`** — expresiones regulares por clase de token y diagrama
  del AFD del lexer (estados, transiciones, inicial, finales). El lexer ya *es*
  ese autómata: los lookaheads de un carácter documentados en SPEC.md (`=` vs
  `==`, `-` vs `->`, `.` vs `..`) son sus transiciones.
  - El diagrama se genera con el **MCP de draw.io** (primera herramienta que
    sugiere el profesor), como `.drawio` editable, no como imagen, para que el
    grupo lo retoque y lo exporte al informe y al PPT.

**Fuera de alcance:** informe, manuales y PPT (los hace el grupo). El `--help`
del compilador ya funciona como esbozo de manual de usuario.

### Correcciones de documentación existente

- `README.md:89` dice que `CLAUDE.md` es "la especificación completa del
  lenguaje"; eso se movió a SPEC.md en el commit de abril. Corregir.
- README: `mingw32-make` y el mapa de módulos nuevo.
- SPEC.md: referencia `codegen.h` → `ir.h`; documentar el paso de arreglos por
  referencia (§3.4); marcar las etapas 4-6 como implementadas.

---

## 8. Orden de implementación

**Las etapas se implementan en orden 4 → 6 → 5, no 4 → 5 → 6.**

La razón es de verificación. La prueba de equivalencia (§6.2) — la más valiosa
del proyecto — necesita *ejecutar* los programas con y sin `--opt` y comparar
salidas, lo que exige emitir C y compilarlo. Es decir: **exige la Etapa 6**.
Construir el optimizador antes que el generador significaría escribirlo entero
sin la única prueba capaz de detectar sus bugs, y descubrirlos después todos
juntos sin saber qué pase los causó.

Con el orden 4 → 6 → 5, al terminar la Etapa 6 existe un compilador completo y
funcionando, y cada pase del optimizador se valida contra toda la suite en el
momento de añadirlo. El coste es que el orden de construcción no coincide con
el orden de exposición, lo cual es irrelevante: el código final es el mismo y
la narrativa del informe la decide el grupo.

Cada paso deja el repo en verde (compila y pasa tests). Cada plan produce
software funcionando por sí solo.

| Plan | Pasos | Al terminar |
|------|-------|-------------|
| 1 | Renombrar `codegen.h` → `ir.h`; `IRFunction`/`IRProgram`; opcodes `IR_ARRAY_NEW` e `IR_READ`. `symtab.c` + `--symbols`. Built-ins `read_*` en `semantic.c` (type-checkean; aún no generan código). | Tabla de símbolos visible; `read_*` type-checkea |
| 2 | **Etapa 4:** `ir.c` + `--ir`, incluyendo cortocircuito y arreglos. | El TAC visible |
| 3 | **Etapa 6:** `emit_c.c` + runtime + `--emit-c` + `-o`. Suite `tests/run/` y `tests/read/`. | **Compilador completo funcionando** |
| 4 | **Etapa 5:** `optimize.c` + `--opt` sobre bloques básicos. Prueba de equivalencia + `tests/opt/`. | Optimizador con red de seguridad |
| 5 | `--stats`. `docs/GRAMMAR.md`, `docs/AUTOMATA.md` + `.drawio`. Correcciones de README.md y SPEC.md. | Los 3.5 pts de documentación |

---

## 9. Decisiones abiertas

Ninguna. Todas las decisiones de diseño quedaron cerradas con el usuario el
2026-07-16.

## 10. Riesgos

| Riesgo | Mitigación |
|--------|------------|
| Un pase del optimizador altera la semántica | Prueba de equivalencia (§6.2) sobre toda la suite |
| El C generado no compila, o compila con warnings delante del profesor | Tests end-to-end (§6.1) invocan gcc de verdad con `-Wall -Wextra` y warnings como error |
| Desbordamiento o búfer residual al leer entrada | `scanf` prohibido; toda lectura por líneas (§5.4.1), con 4 tests dedicados (§6.4) |
| Fugas de memoria en strings | Limitación aceptada y documentada (§5.4) |
| El BNF documentado no coincide con `parser.c` | Se deriva leyendo el parser, no de memoria |
