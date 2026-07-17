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
- No hay `break` ni `continue` en v1 (simplificación deliberada)

### Arrays

```kel
val nums: [int] = [1, 2, 3]   // tipo explícito
val nums = [1, 2, 3]          // con inferencia
nums[0]                       // acceso por índice (base 0)
```

- Tamaño fijo en v1 (no hay push/pop)
- Literal vacío (`[]`) válido solo cuando hay tipo esperado del contexto:
  `val v: [int] = []`

### Salida estándar

```kel
println("Hola mundo")
println("Valor: " + nombre)   // concatenación con +
```

- Solo `println` (incluye salto de línea automático)
- No hay `print` sin salto de línea en v1

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
| Comentarios     | *(descartado)*    | No generan token            |
| Whitespace      | *(descartado)*    | No generan token            |

**Total: 47 tokens**

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
  symtab.h  / symtab.c     — log de la tabla de símbolos            (pendiente)
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
./kelc --help                # ayuda
```

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
