# Especificación del lenguaje Kel

Kel es un lenguaje imperativo, tipado estático con inferencia, diseñado para
ser compilado. La estética está inspirada en Kotlin y Rust: sintaxis limpia
con `val`/`var`, `fn`, tipos postfijos y `->` para retornos.

- **Extensión de archivos:** `.kel`
- **Entry point:** `fn main()`

## Programa de ejemplo

```kel
// Comentario de línea
/* Comentario
   multilínea */

fn suma(a: int, b: int) -> int {
  return a + b
}

fn main() {
  // Variables
  val nombre = "Diego"          // inmutable, inferencia de tipo
  var puntaje: int = 0          // mutable, tipo explícito postfijo
  val pi: float = 3.1415

  // Arrays
  val nums: [int] = [1, 2, 3]   // con tipo explícito
  val tags = ["a", "b", "c"]    // con inferencia

  // For con rango
  for i in 0..3 {
    puntaje = suma(puntaje, nums[i])
  }

  // While
  var x = 0
  while x < 10 {
    x = x + 1
  }

  // Condicional (sin paréntesis)
  if puntaje > 5 {
    println("Hola, " + nombre)
  } else {
    println("Puntaje bajo")
  }
}
```

---

## Sintaxis

### Variables

```kel
val nombre = "Diego"         // inmutable, inferencia
var puntaje = 10             // mutable, inferencia
val pi: float = 3.1415       // inmutable, tipo explícito
var meta: string = "Kel"     // mutable, tipo explícito
```

- `val` = inmutable (no reasignable)
- `var` = mutable
- El tipo es **postfijo** y **opcional** (inferencia automática)

### Tipos de datos

| Tipo       | Descripción        | Ejemplo              |
|------------|--------------------|----------------------|
| `int`      | Entero             | `42`, `-7`           |
| `float`    | Flotante           | `3.14`, `-1.5`       |
| `bool`     | Booleano           | `true`, `false`      |
| `string`   | Cadena             | `"hola"`             |
| `[int]`    | Array de enteros   | `[1, 2, 3]`          |
| `[float]`  | Array de flotantes | `[1.0, 2.5]`         |
| `[bool]`   | Array de booleanos | `[true, false]`      |
| `[string]` | Array de cadenas   | `["a", "b"]`         |

### Funciones

```kel
fn nombre(param1: tipo1, param2: tipo2) -> tipoRetorno {
  return expresion
}

fn main() {
  // punto de entrada del programa
}
```

- Palabra clave: `fn`
- Parámetros con tipo postfijo obligatorio
- Tipo de retorno con `->` (obligatorio si retorna algo)
- Funciones sin retorno no llevan `->`
- `fn main()` es el punto de entrada

### Condicionales

```kel
if condicion {
  ...
} else {
  ...
}
```

- **Sin paréntesis** alrededor de la condición
- `else if` encadenado es válido
- `else` es opcional

### Bucles

```kel
// Bucle condicional
while condicion {
  ...
}

// Bucle de rango (0 inclusive, 10 exclusive)
for i in 0..10 {
  ...
}
```

- `for` itera sobre un rango `inicio..fin` (fin exclusivo)
- **El `fin` se evalúa una sola vez**, al entrar al bucle — no en cada vuelta.
  La variable del bucle es inmutable dentro del cuerpo.
- No hay `break` ni `continue` en v1 (simplificación deliberada)

#### El `for` de Kel no es el `for` de C

En C, `for (i = 0; i < n; i++)` reevalúa `n` en **cada** iteración. El `for` de
Kel no: el fin se calcula una vez y se congela. Es la semántica de Rust y de
Kotlin, de donde viene la sintaxis `0..10`.

La diferencia se ve cuando el fin no es constante:

```kel
var n = 3
for i in 0..n {
  n = n + 1      // no afecta al bucle: itera 3 veces y termina
}

for i in 0..limite() {
  ...            // limite() se llama UNA vez, no en cada vuelta
}
```

Con la regla de C, el primero sería un **bucle infinito** (`n - i` se mantiene
invariante) y el segundo llamaría a `limite()` una vez por iteración. Se ve en
el código intermedio: `./kelc --ir` saca el cálculo del fin fuera de la etiqueta
de condición, y si el fin es una variable la copia a un temporal, porque el
cuerpo podría reasignarla.

### Arrays

```kel
val nums: [int] = [1, 2, 3]   // tipo explícito
val nums = [1, 2, 3]          // con inferencia
nums[0]                       // acceso por índice (base 0)
```

- Tamaño fijo en v1 (no hay push/pop)
- **Se pasan por referencia** a las funciones: si una función muta un
  elemento del arreglo que recibe, el cambio es visible para quien la llamó.
  Es coherente con C, que es el target de la Etapa 6.
- Literal vacío (`[]`) válido solo cuando hay tipo esperado del contexto:
  `val v: [int] = []`

### Salida estándar

```kel
println("Hola mundo")
println("Valor: " + nombre)   // concatenación con +
```

- Solo `println` (incluye salto de línea automático)
- No hay `print` sin salto de línea en v1

### Entrada estándar

```kel
val a = read_int()       // lee una línea y la convierte a int
val f = read_float()     // lee una línea y la convierte a float
val s = read_line()      // lee una línea completa, incluidos espacios
```

- Los tres leen una **línea completa** de la entrada estándar.
- `read_line` devuelve la línea sin el salto de línea final.
- **No son palabras reservadas.** Son funciones built-in registradas en la
  tabla de símbolos, no tokens: para el lexer `read_int` es un identificador
  cualquiera, y `read_int()` parsea como cualquier otra llamada. Por eso
  añadir un built-in no toca el autómata léxico ni la gramática.
- Redefinirlas es un error: `fn read_int() -> int { ... }` no compila.

#### ¿Por qué `println` sí es palabra reservada y `read_*` no?

La diferencia está en el **léxico, no en la gramática**. `println` tiene token
propio (`TOKEN_PRINTLN`), pero **no tiene producción propia**: aparece en la
regla de `primary` como una alternativa más junto a `IDENT`
(`primary ::= IDENT | PRINTLN | ...`), el parser lo acepta en la misma rama que
a un identificador cualquiera y construye un `N_IDENT`. De ahí en adelante el
camino es idéntico al de cualquier llamada: `println(x)` llega al semántico como
un `N_CALL` normal, igual que `read_int()`.

Que `println(x)` no sirva dentro de una expresión no lo impone la gramática,
sino el **sistema de tipos**: `check_call` le da tipo `void`. Por eso
`val a: int = println(1)` falla con *"inicializador de 'a': esperado int,
encontrado void"*, palabra por palabra el mismo error que da una función void
del usuario:

```kel
fn f() {}
val a: int = f()      // inicializador de 'a': esperado int, encontrado void
```

Lo único que compra `TOKEN_PRINTLN` es **reservar la palabra**: el lexer nunca
la entrega como identificador, de modo que `var println: int = 5` no compila
(*"se esperaba nombre de variable (token 'println')"*).

Ese es el intercambio. Reservar cuesta un token y a cambio protege el nombre.
No reservar deja el lexer intacto, pero el nombre queda libre: `var read_int:
int = 5` **compila**. No llega a romper nada — las variables y las funciones
viven en tablas distintas, así que `read_int()` sigue llamando al built-in
mientras `read_int` a secas es la variable —, pero es código legal y confuso
que el compilador no ataja.

La consecuencia práctica: añadir un built-in de entrada no toca el lexer ni el
parser, solo la tabla de funciones del análisis semántico.

### Operadores

| Categoría     | Operadores                    |
|---------------|-------------------------------|
| Aritméticos   | `+` `-` `*` `/` `%`           |
| Comparación   | `==` `!=` `<` `>` `<=` `>=`   |
| Lógicos       | `&&` `||` `!`                 |
| Asignación    | `=`                           |
| Concatenación | `+` (strings)                 |
| Rango         | `..` (en for)                 |

### Comentarios

```kel
// comentario de línea
/* comentario
   multilínea */
```

Los comentarios son descartados por el lexer, nunca llegan al parser.

---

## Lista completa de tokens

### Palabras clave

| Lexema    | Token               | Uso                 |
|-----------|---------------------|---------------------|
| `val`     | `TOKEN_VAL`         | Variable inmutable  |
| `var`     | `TOKEN_VAR`         | Variable mutable    |
| `fn`      | `TOKEN_FN`          | Función             |
| `return`  | `TOKEN_RETURN`      | Retorno             |
| `if`      | `TOKEN_IF`          | Condicional         |
| `else`    | `TOKEN_ELSE`        | Rama alternativa    |
| `while`   | `TOKEN_WHILE`       | Bucle condicional   |
| `for`     | `TOKEN_FOR`         | Bucle de rango      |
| `in`      | `TOKEN_IN`          | Separador de rango  |
| `true`    | `TOKEN_TRUE`        | Literal booleano    |
| `false`   | `TOKEN_FALSE`       | Literal booleano    |
| `println` | `TOKEN_PRINTLN`     | Salida estándar     |
| `int`     | `TOKEN_TYPE_INT`    | Tipo entero         |
| `float`   | `TOKEN_TYPE_FLOAT`  | Tipo flotante       |
| `bool`    | `TOKEN_TYPE_BOOL`   | Tipo booleano       |
| `string`  | `TOKEN_TYPE_STRING` | Tipo cadena         |

### Operadores

| Lexema | Token           |
|--------|-----------------|
| `+`    | `TOKEN_PLUS`    |
| `-`    | `TOKEN_MINUS`   |
| `*`    | `TOKEN_STAR`    |
| `/`    | `TOKEN_SLASH`   |
| `%`    | `TOKEN_PERCENT` |
| `=`    | `TOKEN_ASSIGN`  |
| `==`   | `TOKEN_EQ`      |
| `!=`   | `TOKEN_NEQ`     |
| `<`    | `TOKEN_LT`      |
| `>`    | `TOKEN_GT`      |
| `<=`   | `TOKEN_LTE`     |
| `>=`   | `TOKEN_GTE`     |
| `&&`   | `TOKEN_AND`     |
| `||`   | `TOKEN_OR`      |
| `!`    | `TOKEN_NOT`     |
| `->`   | `TOKEN_ARROW`   |
| `..`   | `TOKEN_RANGE`   |

### Delimitadores

| Lexema | Token             |
|--------|-------------------|
| `(`    | `TOKEN_LPAREN`    |
| `)`    | `TOKEN_RPAREN`    |
| `{`    | `TOKEN_LBRACE`    |
| `}`    | `TOKEN_RBRACE`    |
| `[`    | `TOKEN_LBRACKET`  |
| `]`    | `TOKEN_RBRACKET`  |
| `:`    | `TOKEN_COLON`     |
| `,`    | `TOKEN_COMMA`     |

### Literales y especiales

| Tipo            | Token             | Notas                       |
|-----------------|-------------------|-----------------------------|
| Entero `42`     | `TOKEN_INT_LIT`   | Dígitos                     |
| Flotante `3.14` | `TOKEN_FLOAT_LIT` | Dígitos + `.` + dígitos     |
| Cadena `"hola"` | `TOKEN_STR_LIT`   | Entre comillas dobles       |
| Identificador   | `TOKEN_IDENT`     | `[a-zA-Z_][a-zA-Z0-9_]*`    |
| Fin de archivo  | `TOKEN_EOF`       | —                           |
| Error léxico    | `TOKEN_ERROR`     | Carácter no reconocido      |
| Comentarios     | *(descartado)*    | No generan token            |
| Whitespace      | *(descartado)*    | No generan token            |

**Total: 47 tokens** — 16 palabras clave + 17 operadores + 8 delimitadores +
6 literales y especiales. Coincide con el enum `TokenType` de `src/lexer.h`.

### Notas de implementación críticas

1. **Keywords vs identificadores:** el lexer lee cualquier secuencia
   `[a-zA-Z_][a-zA-Z0-9_]*` como `TOKEN_IDENT` y luego hace lookup en una
   tabla de keywords para reclasificar.

2. **Lookahead de 1 carácter necesario en:**
   - `.` → si el siguiente es `.` = `TOKEN_RANGE`; si es dígito = parte de `TOKEN_FLOAT_LIT`
   - `=` → `==` = `TOKEN_EQ`; si no = `TOKEN_ASSIGN`
   - `!` → `!=` = `TOKEN_NEQ`; si no = `TOKEN_NOT`
   - `<` → `<=` = `TOKEN_LTE`; si no = `TOKEN_LT`
   - `>` → `>=` = `TOKEN_GTE`; si no = `TOKEN_GT`
   - `&` → `&&` = `TOKEN_AND`
   - `|` → `||` = `TOKEN_OR`
   - `-` → `->` = `TOKEN_ARROW`; si no = `TOKEN_MINUS`

3. **Números:** dígitos y opcional `.` seguido de más dígitos para float.
   Un `.` sin dígitos atrás es error.

4. **Strings:** hasta la siguiente `"` sin escapes (v1).

5. **Literales negativos:** `-5` se tokeniza como `TOKEN_MINUS` +
   `TOKEN_INT_LIT(5)`. El parser lo aplica como unario. Evita ambigüedad
   en `a-5` vs `a -5`.

---

## Estructura del proyecto

```
src/
  lexer.h   / lexer.c      — Etapa 1: tokenizador
  parser.h  / parser.c     — Etapa 2: parser descendente recursivo → AST
  semantic.h/ semantic.c   — Etapa 3: tabla de símbolos + chequeo de tipos
  diag.h    / diag.c       — reporte de errores con línea fuente y carat
  ir.h                     — Etapa 4: tipos del código intermedio (TAC)
  ir.c                     — Etapa 4: generación de TAC             (pendiente)
  optimize.h/ optimize.c   — Etapa 5: optimización local            (pendiente)
  emit_c.h  / emit_c.c     — Etapa 6: generación de C               (pendiente)
  symtab.h  / symtab.c     — log de la tabla de símbolos (--symbols)
  main.c                   — CLI
tests/
  ok/                      — programas válidos
  bad/                     — programas inválidos (se espera que fallen)
  run_tests.sh             — suite de regresión
Makefile
```

### Uso del compilador

```bash
./kelc programa.kel          # compila y reporta fases
./kelc --tokens programa.kel # debug léxico
./kelc --ast programa.kel    # debug sintáctico/semántico
./kelc --sem programa.kel    # solo fase semántica
./kelc --symbols programa.kel # tabla de símbolos
./kelc --help                # ayuda
```

#### Salida de `--symbols`

`--symbols` vuelca el log de declaraciones registrado durante la fase
semántica: una fila por cada `val`/`var` declarado, en el orden en que
el análisis las va viendo. Ejemplo, sobre `tests/symbols/basico.kel`:

```
Ámbito           Identificador  Tipo      Mut   Despl  Línea  Valor
suma             a              int       var       0      1  —
suma             b              int       var       4      1  —
main             nombre         string    val       0      6  "Diego"
main             puntaje        int       var       8      7  0
main             pi             float     val      16      8  3.1415
```

Columnas:

- **Ámbito**: ruta de scopes separada por `.`, p. ej. `main.for`. No son
  niveles de anidamiento numerados — son los nombres de las construcciones
  que abren scope (`main`, `for`, etc.), concatenados según se entra en
  ellas.
- **Identificador**: el nombre declarado.
- **Tipo**: `int` / `float` / `bool` / `string` / `[T]`.
- **Mut**: `val` o `var`.
- **Despl**: desplazamiento dentro del marco de la función. **Es un
  modelo del marco de pila, no una dirección real.** Se calcula sumando
  el tamaño del tipo de cada variable (`int`=4, `float`=8, `bool`=1,
  `string`=8 como puntero, arreglos=8 como referencia), alineando cada
  desplazamiento al tamaño de su propio tipo, y reiniciando el contador
  en cada función. La dirección real de cada variable la decide gcc al
  compilar el C generado en la Etapa 6 — este número es solo la
  contabilidad que el compilador de Kel lleva internamente, útil para
  razonar sobre el layout pero no para leer memoria.
- **Un arreglo cuenta como una referencia de 8 bytes, no como
  n × tamaño del elemento**: `KelType` no guarda cuántos elementos tiene
  un arreglo, así que no hay forma de saber ese tamaño en tiempo de
  compilación. Para los parámetros esto es exacto (los arreglos se pasan
  por referencia); para variables locales es una simplificación
  declarada, no un intento de modelar el tamaño real del buffer.
- **Línea**: línea fuente de la declaración.
- **Valor**: el texto del literal, si el inicializador es una constante
  reconocible en tiempo de compilación (`5`, `-5`, `3.5`, `true`,
  `"texto"`); `—` en cualquier otro caso (inicializador con una
  expresión, llamada a función, etc.). La tabla de símbolos no ejecuta
  nada, así que no puede conocer valores que solo existen en tiempo de
  ejecución.

---

## Roadmap

### Unidad I — en C (semana 08)

#### Etapa 1 — Lexer

Dado un string de código Kel, producir un array de tokens.

#### Etapa 2 — Parser

Técnica: parser descendente recursivo. Produce AST. Reporta errores de
sintaxis con línea y columna.

#### Etapa 3 — Análisis semántico

- Tabla de símbolos con scopes anidados
- Variables declaradas antes de uso; `val` no reasignable
- Tipos en expresiones; funciones llamadas existen con aridad y tipos correctos
- `return` coincide con el tipo declarado; todas las rutas retornan en funciones no-void

### Unidad II — lenguaje libre (semana 17)

#### Etapa 4 — Código intermedio

TAC (Three Address Code). Ver `src/ir.h` para el formato decidido.

#### Etapa 5 — Optimización básica

- Constant folding: `2 + 3` → `5` en tiempo de compilación
- Eliminación de código muerto
- Propagación de copias

#### Etapa 6 — Generación de código final

Target recomendado: **C como output**. El compilador de Kel genera un `.c`
que luego compila con gcc. Alternativa: bytecode de una VM simple.

---

## Decisiones de diseño

| Decisión              | Elección                          | Razón                       |
|-----------------------|-----------------------------------|-----------------------------|
| Variables             | `val`/`var`, tipo postfijo opcional | Estética Kotlin/Rust      |
| Funciones             | `fn nombre(p: tipo) -> tipo`      | Estética Rust               |
| Tipos incluidos       | int, float, bool, string, arrays  | Completo pero manejable     |
| Paso de arreglos      | Por referencia                    | Coherente con el target C   |
| Condicionales         | Sin paréntesis                    | Estética Go/Rust            |
| Bucles                | `while` + `for i in 0..n`         | Cubre casos comunes         |
| Salida                | `println(...)`                    | Simple, estilo Kotlin       |
| Comentarios           | `//` y `/* */`                    | Estándar C-like             |
| Entry point           | `fn main()`                       | Consistente                 |
| Target Unidad II      | C como output                     | Manejable para 4 personas   |

## Simplificaciones deliberadas de v1

- No hay `break`/`continue` en bucles
- No hay strings con escape (`\n`, `\"`, etc.)
- Arrays de tamaño fijo (sin push/pop)
- Un solo archivo fuente por programa (sin imports)
- No hay structs ni tipos compuestos definidos por el usuario
- No hay null/nil
- Funciones no son first-class (no se pasan como parámetros)
- Sin coerción implícita int↔float: `1 + 1.0` es error

---

## Manejo de errores

El compilador reporta errores con este formato:

```
[Kel Error] línea 15, col 13: operador '+' no aplicable a int y string
     15 |   val a = 1 + "texto"
        |             ^
```

Cada fase reporta sus propios errores y puede continuar o abortar según la
gravedad. El lexer y parser usan modo pánico con resync para reportar
varios errores por compilación.
