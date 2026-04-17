\# CLAUDE.md — Compilador de Kel



Contexto completo del proyecto para no perder estado entre sesiones.

Este archivo es la fuente de verdad. Actualízalo cuando haya decisiones nuevas.



\---



\## Contexto académico



\- \*\*Curso:\*\* Compiladores y Teoría de Lenguajes — UNJBG 2026-I

\- \*\*Carrera:\*\* Ingeniería en Informática y Sistemas — 5to semestre

\- \*\*Grupo:\*\* 4 personas

\- \*\*Docente:\*\* MSc. Ing. Manuel Yuri Apaza Valencia

\- \*\*Entregas:\*\*

&#x20; - Unidad I (Etapas 1-3): semana 08, exposición del avance. Examen semana 09 (25–29 mayo 2026)

&#x20; - Unidad II (Etapas 4-6): semana 17, exposición final. Examen semana 18 (20–24 julio 2026)



\### Criterio de notas (según el profesor)

\- Completar las 3 fases de Unidad I bien = nota muy buena

\- Completar las 6 fases = nota de 20 (excelente, poco común)

\- El objetivo de este grupo es completar las 6 fases



\---



\## El lenguaje: Kel



Kel es un lenguaje de programación imperativo diseñado por el grupo para ser compilado.

\- \*\*Extensión de archivos:\*\* `.kel`

\- \*\*Inspiración estética:\*\* Kotlin + Rust (sintaxis limpia, moderna)

\- \*\*Paradigma:\*\* imperativo, tipado estático con inferencia de tipos



\### Programa de ejemplo completo



```kel

// Comentario de línea

/\* Comentario

&#x20;  multilínea \*/



fn suma(a: int, b: int) -> int {

&#x20; return a + b

}



fn main() {

&#x20; // Variables

&#x20; val nombre = "Diego"          // inmutable, inferencia de tipo

&#x20; var puntaje: int = 0          // mutable, tipo explícito postfijo

&#x20; val pi: float = 3.1415



&#x20; // Arrays

&#x20; val nums: \[int] = \[1, 2, 3]   // con tipo explícito

&#x20; val tags = \["a", "b", "c"]    // con inferencia



&#x20; // For con rango

&#x20; for i in 0..3 {

&#x20;   puntaje = suma(puntaje, nums\[i])

&#x20; }



&#x20; // While

&#x20; var x = 0

&#x20; while x < 10 {

&#x20;   x = x + 1

&#x20; }



&#x20; // Condicional (sin paréntesis)

&#x20; if puntaje > 5 {

&#x20;   println("Hola, " + nombre)

&#x20; } else {

&#x20;   println("Puntaje bajo")

&#x20; }

}

```



\---



\## Especificación del lenguaje



\### Variables



```kel

val nombre = "Diego"         // inmutable, inferencia

var puntaje = 10             // mutable, inferencia

val pi: float = 3.1415       // inmutable, tipo explícito

var meta: string = "Kel"     // mutable, tipo explícito

```



\- `val` = inmutable (no reasignable después de la declaración)

\- `var` = mutable

\- El tipo es \*\*postfijo\*\* y \*\*opcional\*\* (inferencia automática)



\### Tipos de datos



| Tipo | Descripción | Ejemplo |

|---|---|---|

| `int` | Entero | `42`, `-7` |

| `float` | Flotante | `3.14`, `-1.5` |

| `bool` | Booleano | `true`, `false` |

| `string` | Cadena | `"hola"` |

| `\[int]` | Array de enteros | `\[1, 2, 3]` |

| `\[float]` | Array de flotantes | `\[1.0, 2.5]` |

| `\[bool]` | Array de booleanos | `\[true, false]` |

| `\[string]` | Array de cadenas | `\["a", "b"]` |



\### Funciones



```kel

fn nombre(param1: tipo1, param2: tipo2) -> tipoRetorno {

&#x20; return expresion

}



fn main() {

&#x20; // punto de entrada del programa

}

```



\- Palabra clave: `fn`

\- Parámetros con tipo postfijo obligatorio

\- Tipo de retorno con `->` (obligatorio si retorna algo)

\- Funciones sin retorno no llevan `->`

\- `fn main()` es el punto de entrada



\### Condicionales



```kel

if condicion {

&#x20; ...

} else {

&#x20; ...

}

```



\- \*\*Sin paréntesis\*\* alrededor de la condición

\- `else if` encadenado es válido

\- `else` es opcional



\### Bucles



```kel

// Bucle condicional

while condicion {

&#x20; ...

}



// Bucle de rango (0 inclusive, 10 exclusive)

for i in 0..10 {

&#x20; ...

}

```



\- `for` itera sobre un rango `inicio..fin` (fin exclusivo)

\- No hay `break` ni `continue` en v1 (simplificación deliberada)



\### Arrays



```kel

val nums: \[int] = \[1, 2, 3]   // forma 1: tipo explícito

val nums = \[1, 2, 3]           // forma 2: inferencia

nums\[0]                        // acceso por índice (base 0)

```



\- Tamaño fijo en v1 (no hay push/pop)

\- Acceso por índice con `\[]`



\### Salida estándar



```kel

println("Hola mundo")

println("Valor: " + nombre)   // concatenación con +

```



\- Solo `println` (incluye salto de línea automático)

\- No hay `print` sin salto de línea en v1



\### Operadores



| Categoría | Operadores |

|---|---|

| Aritméticos | `+` `-` `\*` `/` `%` |

| Comparación | `==` `!=` `<` `>` `<=` `>=` |

| Lógicos | `\&\&` `\\|\\|` `!` |

| Asignación | `=` |

| Concatenación | `+` (strings) |

| Rango | `..` (en for) |



\### Comentarios



```kel

// comentario de línea

/\* comentario

&#x20;  multilínea \*/

```



\- Los comentarios son descartados por el lexer, nunca llegan al parser



\---



\## Lista completa de tokens



\### Palabras clave



| Lexema | Token | Uso |

|---|---|---|

| `val` | `TOKEN\_VAL` | Variable inmutable |

| `var` | `TOKEN\_VAR` | Variable mutable |

| `fn` | `TOKEN\_FN` | Definición de función |

| `return` | `TOKEN\_RETURN` | Retorno |

| `if` | `TOKEN\_IF` | Condicional |

| `else` | `TOKEN\_ELSE` | Rama alternativa |

| `while` | `TOKEN\_WHILE` | Bucle condicional |

| `for` | `TOKEN\_FOR` | Bucle de rango |

| `in` | `TOKEN\_IN` | Separador de rango |

| `true` | `TOKEN\_TRUE` | Literal booleano |

| `false` | `TOKEN\_FALSE` | Literal booleano |

| `println` | `TOKEN\_PRINTLN` | Salida estándar |

| `int` | `TOKEN\_TYPE\_INT` | Tipo entero |

| `float` | `TOKEN\_TYPE\_FLOAT` | Tipo flotante |

| `bool` | `TOKEN\_TYPE\_BOOL` | Tipo booleano |

| `string` | `TOKEN\_TYPE\_STRING` | Tipo cadena |



\### Operadores



| Lexema | Token |

|---|---|

| `+` | `TOKEN\_PLUS` |

| `-` | `TOKEN\_MINUS` |

| `\*` | `TOKEN\_STAR` |

| `/` | `TOKEN\_SLASH` |

| `%` | `TOKEN\_PERCENT` |

| `=` | `TOKEN\_ASSIGN` |

| `==` | `TOKEN\_EQ` |

| `!=` | `TOKEN\_NEQ` |

| `<` | `TOKEN\_LT` |

| `>` | `TOKEN\_GT` |

| `<=` | `TOKEN\_LTE` |

| `>=` | `TOKEN\_GTE` |

| `\&\&` | `TOKEN\_AND` |

| `\\|\\|` | `TOKEN\_OR` |

| `!` | `TOKEN\_NOT` |

| `->` | `TOKEN\_ARROW` |

| `..` | `TOKEN\_RANGE` |



\### Delimitadores



| Lexema | Token |

|---|---|

| `(` | `TOKEN\_LPAREN` |

| `)` | `TOKEN\_RPAREN` |

| `{` | `TOKEN\_LBRACE` |

| `}` | `TOKEN\_RBRACE` |

| `\[` | `TOKEN\_LBRACKET` |

| `]` | `TOKEN\_RBRACKET` |

| `:` | `TOKEN\_COLON` |

| `,` | `TOKEN\_COMMA` |



\### Literales y especiales



| Tipo | Token | Notas |

|---|---|---|

| Entero `42` | `TOKEN\_INT\_LIT` | Dígitos, posible `-` |

| Flotante `3.14` | `TOKEN\_FLOAT\_LIT` | Dígitos + `.` + dígitos |

| Cadena `"hola"` | `TOKEN\_STR\_LIT` | Entre comillas dobles |

| Identificador | `TOKEN\_IDENT` | `\[a-zA-Z\_]\[a-zA-Z0-9\_]\*` |

| Fin de archivo | `TOKEN\_EOF` | — |

| Comentarios | \*(descartado)\* | No genera token |

| Whitespace | \*(descartado)\* | No genera token |



\*\*Total: 47 tokens\*\*



\### Notas de implementación críticas



1\. \*\*Keywords vs identificadores:\*\* el lexer lee cualquier secuencia `\[a-zA-Z\_]\[a-zA-Z0-9\_]\*` como `TOKEN\_IDENT` y luego hace lookup en una tabla de keywords para reclasificar. No detectar keywords directamente con condiciones separadas.



2\. \*\*Lookahead de 1 carácter necesario en:\*\*

&#x20;  - `.` → si el siguiente es `.` = `TOKEN\_RANGE`; si el siguiente es dígito = parte de `TOKEN\_FLOAT\_LIT`

&#x20;  - `=` → si el siguiente es `=` = `TOKEN\_EQ`; si no = `TOKEN\_ASSIGN`

&#x20;  - `!` → si el siguiente es `=` = `TOKEN\_NEQ`; si no = `TOKEN\_NOT`

&#x20;  - `<` → si el siguiente es `=` = `TOKEN\_LTE`; si no = `TOKEN\_LT`

&#x20;  - `>` → si el siguiente es `=` = `TOKEN\_GTE`; si no = `TOKEN\_GT`

&#x20;  - `\&` → si el siguiente es `\&` = `TOKEN\_AND`

&#x20;  - `|` → si el siguiente es `|` = `TOKEN\_OR`

&#x20;  - `-` → si el siguiente es `>` = `TOKEN\_ARROW`; si no = `TOKEN\_MINUS`



3\. \*\*Números:\*\* leer dígitos, si aparece `.` seguido de dígito → float. Si `.` no va seguido de dígito → error.



4\. \*\*Strings:\*\* leer hasta la siguiente `"` sin escape. En v1 no hay secuencias de escape (simplificación).



\---



\## Estructura del proyecto



```

kel/

&#x20; src/

&#x20;   lexer.h       — definición de TokenType, struct Token, firma de funciones

&#x20;   lexer.c       — implementación del scanner

&#x20;   parser.h      — definición de nodos del AST

&#x20;   parser.c      — implementación del parser recursivo descendente

&#x20;   semantic.h    — tabla de símbolos, tipos

&#x20;   semantic.c    — análisis semántico

&#x20;   codegen.h     — generación de código intermedio / final

&#x20;   codegen.c     — implementación

&#x20;   main.c        — entry point: lee archivo .kel, ejecuta fases

&#x20; tests/

&#x20;   test\_lexer.kel

&#x20;   test\_parser.kel

&#x20;   test\_semantic.kel

&#x20;   test\_full.kel

&#x20; Makefile

&#x20; CLAUDE.md       — este archivo

```



\### Makefile básico esperado



```makefile

CC = gcc

CFLAGS = -Wall -Wextra -std=c11 -g

SRC = src/main.c src/lexer.c src/parser.c src/semantic.c src/codegen.c

OUT = kelc



all: $(OUT)



$(OUT): $(SRC)

&#x09;$(CC) $(CFLAGS) -o $(OUT) $(SRC)



clean:

&#x09;rm -f $(OUT)

```



\### Uso esperado del compilador



```bash

./kelc programa.kel          # compila y muestra resultado

./kelc --tokens programa.kel # muestra solo el stream de tokens (debug)

./kelc --ast programa.kel    # muestra el AST (debug)

./kelc --sem programa.kel    # muestra resultado semántico (debug)

```



Los flags de debug son muy útiles para la exposición.



\---



\## Roadmap de implementación



\### Unidad I — en C (entrega semana 08)



\#### Etapa 1 — Lexer (`lexer.h` / `lexer.c`)

\*\*Objetivo:\*\* dado un string de código Kel, producir un array de tokens.



Struct esperado:

```c

typedef enum {

&#x20; TOKEN\_VAL, TOKEN\_VAR, TOKEN\_FN, TOKEN\_RETURN,

&#x20; TOKEN\_IF, TOKEN\_ELSE, TOKEN\_WHILE, TOKEN\_FOR, TOKEN\_IN,

&#x20; TOKEN\_TRUE, TOKEN\_FALSE, TOKEN\_PRINTLN,

&#x20; TOKEN\_TYPE\_INT, TOKEN\_TYPE\_FLOAT, TOKEN\_TYPE\_BOOL, TOKEN\_TYPE\_STRING,

&#x20; TOKEN\_PLUS, TOKEN\_MINUS, TOKEN\_STAR, TOKEN\_SLASH, TOKEN\_PERCENT,

&#x20; TOKEN\_ASSIGN, TOKEN\_EQ, TOKEN\_NEQ,

&#x20; TOKEN\_LT, TOKEN\_GT, TOKEN\_LTE, TOKEN\_GTE,

&#x20; TOKEN\_AND, TOKEN\_OR, TOKEN\_NOT,

&#x20; TOKEN\_ARROW, TOKEN\_RANGE,

&#x20; TOKEN\_LPAREN, TOKEN\_RPAREN,

&#x20; TOKEN\_LBRACE, TOKEN\_RBRACE,

&#x20; TOKEN\_LBRACKET, TOKEN\_RBRACKET,

&#x20; TOKEN\_COLON, TOKEN\_COMMA,

&#x20; TOKEN\_INT\_LIT, TOKEN\_FLOAT\_LIT, TOKEN\_STR\_LIT,

&#x20; TOKEN\_IDENT,

&#x20; TOKEN\_EOF

} TokenType;



typedef struct {

&#x20; TokenType type;

&#x20; char\* value;    // lexema original

&#x20; int line;       // línea en el fuente (para errores)

&#x20; int col;        // columna

} Token;

```



\#### Etapa 2 — Parser (`parser.h` / `parser.c`)

\*\*Objetivo:\*\* dado el stream de tokens, producir un AST.



\- Técnica: \*\*recursive descent parser\*\* (descendente recursivo)

\- Un nodo del AST por construcción del lenguaje

\- El parser reporta errores de sintaxis con línea y columna



\#### Etapa 3 — Análisis semántico (`semantic.h` / `semantic.c`)

\*\*Objetivo:\*\* dado el AST, verificar que el programa tiene sentido.



\- Tabla de símbolos con scopes anidados (para funciones y bloques)

\- Verificar que variables estén declaradas antes de uso

\- Verificar que `val` no se reasigne

\- Verificar tipos en expresiones (`int + int` = ok, `int + string` = error)

\- Verificar que funciones llamadas existan y con aridad correcta

\- Verificar que `return` coincida con el tipo de retorno de la función



\### Unidad II — lenguaje libre (entrega semana 17)



\#### Etapa 4 — Código intermedio

\- Generar código de tres direcciones (TAC) desde el AST anotado

\- Formato: `t1 = a + b`, `if t1 goto L1`, etc.



\#### Etapa 5 — Optimización básica

\- Constant folding: `2 + 3` → `5` en tiempo de compilación

\- Eliminación de código muerto: bloques inalcanzables

\- Propagación de copias: `x = 5; y = x` → `y = 5`



\#### Etapa 6 — Generación de código final

\- \*\*Target recomendado: C como output\*\*

\- El compilador de Kel genera un `.c` que luego compila con gcc

\- Es académicamente válido (así funciona Cython, entre otros)

\- Alternativa: bytecode de una VM simple definida en el proyecto



\---



\## Decisiones de diseño tomadas (no reabrir)



| Decisión | Elección | Razón |

|---|---|---|

| Variables | `val`/`var` con tipo postfijo opcional | Estética Kotlin/Rust, coherente |

| Funciones | `fn nombre(p: tipo) -> tipo` | Estética Rust |

| Tipos incluidos | `int`, `float`, `bool`, `string`, arrays | Completo pero manejable |

| Condicionales | Sin paréntesis | Estética Go/Rust |

| Bucles | `while` + `for i in 0..n` | Cubre los casos comunes |

| Salida | `println(...)` | Simple, estilo Kotlin |

| Comentarios | `//` y `/\* \*/` | Estándar C-like |

| Entry point | `fn main()` | Consistente con el resto del lenguaje |

| Arrays | Dos formas: con tipo explícito e inferencia | Flexibilidad |

| Nombre | Kel | Elegido por el grupo |

| Target Unidad II | C como output (o VM simple) | Manejable para 4 personas |

| Break/continue | No en v1 | Simplificación deliberada |

| Escape en strings | No en v1 | Simplificación deliberada |



\---



\## Simplificaciones deliberadas de v1



Estas decisiones reducen complejidad sin afectar la nota:



\- \*\*No hay `break`/`continue`\*\* en bucles

\- \*\*No hay strings con escape\*\* (`\\n`, `\\"`, etc.)

\- \*\*Arrays de tamaño fijo\*\* (no dinámicos)

\- \*\*No hay recursión\*\* verificada explícitamente (funciona pero no se garantiza)

\- \*\*Un solo archivo fuente\*\* por programa (no hay imports)

\- \*\*No hay structs\*\* ni tipos compuestos definidos por el usuario

\- \*\*No hay null/nil\*\*

\- \*\*Funciones no son first-class\*\* (no se pasan como parámetros)



Estas pueden agregarse en iteraciones futuras si el tiempo lo permite.



\---



\## Manejo de errores esperado



El compilador debe reportar errores con este formato:



```

\[Kel Error] línea 5, col 12: variable 'x' no declarada

\[Kel Error] línea 8, col 3: tipo incompatible: esperado int, encontrado string

\[Kel Error] línea 12, col 1: función 'foo' no definida

```



Cada fase reporta sus propios errores y puede continuar o abortar según la gravedad.



\---



\## Notas para la exposición



\- Tener listos ejemplos de programas `.kel` que demuestren cada fase

\- El flag `--tokens` es esencial para demostrar el léxico visualmente

\- El flag `--ast` imprime el árbol bonito con indentación

\- Preparar un ejemplo que falle semánticamente (variable no declarada, tipo incorrecto) para mostrar que el compilador detecta errores

\- El lenguaje tiene un estilo estético deliberado (val/var, fn, ->) que es fácil de defender en la exposición como decisión de diseño



\---



\## Estado de la implementación (abril 2026)



Unidad I \*\*completa\*\*. Todo compila con `-Wall -Wextra -Wpedantic` sin warnings.



\### Estructura actual



```

src/

&#x20; lexer.h / lexer.c      — Etapa 1 ✅

&#x20; parser.h / parser.c    — Etapa 2 ✅ (incluye Node.inferred\_type)

&#x20; semantic.h / semantic.c — Etapa 3 ✅

&#x20; diag.h / diag.c        — reporte de errores con carat ✅

&#x20; codegen.h              — esqueleto TAC para Etapa 4

&#x20; main.c                 — CLI con --tokens / --ast / --sem / --help

tests/

&#x20; ok/                    — programas válidos (edge, empty\_arrays, full, lexer)

&#x20; bad/                   — programas inválidos (empty\_array\_no\_hint, lex\_error,

&#x20;                          no\_return, parse\_error, sem\_errors)

&#x20; run\_tests.sh           — suite de regresión (exit 0 esperado en ok/, !=0 en bad/)

Makefile                  — targets: all, test, asan, valgrind, clean

```



\### Features añadidas sobre el spec base



\- \*\*Tipos inferidos anotados en el AST\*\* (`Node.inferred\_type`): el flag `--ast`

&#x20; muestra el tipo de cada expresión (`BinOp '+' : int`, `Call : string`, etc.).

\- \*\*Scope correcto en `for`\*\*: la variable del loop está en un scope exterior

&#x20; y el body abre su propio scope anidado (via `check\_block`).

\- \*\*Análisis de rutas de retorno\*\*: toda función no-void debe retornar en

&#x20; todas las rutas. Implementado con `stmt\_always\_returns` en `semantic.c`.

\- \*\*Errores con carat\*\* estilo rustc/gcc (módulo `diag.c`):

&#x20; ```

&#x20; \[Kel Error] línea 15, col 13: operador '+' no aplicable a int y string

&#x20;      15 |   val a = 1 + "texto"

&#x20;         |             ^

&#x20; ```

\- \*\*Arrays vacíos con hint de tipo\*\* (`val v: [int] = []`): `check\_expr\_h`

&#x20; propaga el tipo esperado desde var\_decl, assign, return y args de función.

\- \*\*`println` como built-in\*\*: acepta 1 argumento de tipo printable

&#x20; (int/float/bool/string), no un array.

\- \*\*Suite de regresión\*\*: `make test` corre 9 casos (4 ok + 5 bad).

\- \*\*Flag `--help` / `-h`\*\*: ayuda con ejemplos y nota sobre literales negativos.



\### Decisiones de implementación relevantes



\- \*\*Literales negativos\*\*: `-5` se tokeniza como `MINUS` + `INT\_LIT(5)` y se

&#x20; parsea como `UnOp '-'` sobre `Int 5`. Evita ambigüedad en `a-5` vs `a -5`.

\- \*\*Forward reference de funciones\*\*: análisis semántico en 2 pasos —

&#x20; primero registra firmas de todas las funciones, luego chequea cuerpos.

&#x20; Permite llamadas forward y recursión sin requerir orden en el archivo.

\- \*\*Sin coerción int↔float\*\*: `1 + 1.0` es error. Coherente, simple.

\- \*\*`val arr[i] = x`\*\*: rechazado por mutabilidad del array base.

\- \*\*`main`\*\*: obligatoria, sin parámetros, retorno void.



\### Pendiente para Unidad II



\- \*\*Etapa 4 (código intermedio)\*\*: formato TAC decidido y documentado en

&#x20; `src/codegen.h`. Opcodes: `IR\_LABEL`, `IR\_COPY`, `IR\_BINOP`, `IR\_UNOP`,

&#x20; `IR\_INDEX\_LOAD/STORE`, `IR\_PARAM`, `IR\_CALL`, `IR\_GOTO`, `IR\_IF\_GOTO`,

&#x20; `IR\_IF\_FALSE\_GOTO`, `IR\_RETURN`, `IR\_PRINTLN`. Temporales `t1..tN`,

&#x20; etiquetas `L1..LN`, por función. Aprovechará `Node.inferred\_type`.

\- \*\*Etapa 5 (optimización)\*\*: constant folding, dead code, copy propagation.

\- \*\*Etapa 6 (codegen final)\*\*: target = C como output.



\---



\*Última actualización: abril 2026 — Diego, UNJBG\*

