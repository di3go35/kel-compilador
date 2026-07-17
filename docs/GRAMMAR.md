# Gramática de Kel (BNF/EBNF)

> **Criterio 5 de la rúbrica** — Gramática BNF/EBNF + análisis de ambigüedad.
>
> Esta gramática **no es una idealización**: está derivada directamente de
> `src/parser.c`, función por función, y describe exactamente el lenguaje que
> el parser acepta. Cada no terminal indica la función de `parser.c` que lo
> implementa, para que el docente pueda contrastar gramática y código. Un BNF
> inventado que no cuadre con el parser sería peor que no tenerlo.

El parser de Kel es **descendente recursivo predictivo**, con **un token de
anticipación** (`peek`, `parser.c:18`). Salvo la única excepción documentada
más abajo (sentencia de asignación), cada decisión se toma mirando el token
actual, lo que hace la gramática **LL(1)** y, por construcción, **no ambigua**.

Notación EBNF empleada:

| Símbolo | Significado |
|---------|-------------|
| `x*`      | cero o más repeticiones |
| `x?`      | opcional (cero o una) |
| `x \| y`  | alternativa |
| `( … )`   | agrupación |
| `"fn"`    | terminal literal (palabra clave u operador) |
| `MAYÚS`   | clase de token del lexer (ver [AUTOMATA.md](AUTOMATA.md)) |

Terminales que son **clases de token** (su forma la define el analizador
léxico): `IDENT`, `INT_LIT`, `FLOAT_LIT`, `STR_LIT`. El resto de terminales son
palabras clave y operadores literales.

---

## 1. Estructura del programa

```ebnf
program   ::= function* EOF
```
`kel_parse` (`parser.c:424`): un programa es una secuencia de funciones hasta
el fin de archivo. No hay variables globales ni sentencias sueltas a nivel de
programa: **todo vive dentro de una función**.

```ebnf
function  ::= "fn" IDENT "(" params? ")" ( "->" type )? block
params    ::= param ( "," param )*
param     ::= IDENT ":" type
```
`parse_function` (`parser.c:394`). El tipo de retorno es opcional; si se omite
la flecha `->`, la función es `void` (`parser.c:417`). Los parámetros se separan
con comas y **no** admiten coma final (el bucle `do … while(match COMMA)` exige
un parámetro tras cada coma, `parser.c:402-411`).

---

## 2. Tipos

```ebnf
type      ::= "int" | "float" | "bool" | "string" | "[" type "]"
```
`parse_type` (`parser.c:85`). Los arreglos son recursivos: `[[int]]` es un
arreglo de arreglos de `int`. No hay más constructores de tipo (ni funciones
como valores, ni tuplas, ni tipos nombrados por el usuario).

---

## 3. Bloques y sentencias

```ebnf
block     ::= "{" statement* "}"
```
`parse_block` (`parser.c:380`). **Las llaves son obligatorias** en todo bloque
(cuerpo de función, ramas de `if`/`else`, cuerpos de `while`/`for`). Esta
decisión elimina el problema del *dangling else* (ver §6).

```ebnf
statement ::= var_decl
            | if_stmt
            | while_stmt
            | for_stmt
            | return_stmt
            | expr_or_assign
```
`parse_stmt` (`parser.c:367`) despacha según el token inicial. **No hay
separador de sentencias**: no se usa `;`. Dos sentencias se distinguen porque
cada una empieza por su palabra clave o por una expresión.

```ebnf
var_decl      ::= ( "val" | "var" ) IDENT ( ":" type )? "=" expr
```
`parse_var_decl` (`parser.c:278`). `val` es inmutable, `var` mutable. La
anotación de tipo es opcional (se infiere del inicializador); el inicializador
es **obligatorio** (no hay declaración sin valor).

```ebnf
if_stmt       ::= "if" expr block ( "else" ( if_stmt | block ) )?
```
`parse_if` (`parser.c:294`). La condición **no lleva paréntesis** (estética
Go/Rust). El `else` puede encadenar otro `if` (`else if …`) o abrir un bloque.

```ebnf
while_stmt    ::= "while" expr block
```
`parse_while` (`parser.c:308`).

```ebnf
for_stmt      ::= "for" IDENT "in" expr ".." expr block
```
`parse_for` (`parser.c:316`). Rango semiabierto `inicio..fin`. La variable del
bucle se declara implícitamente.

```ebnf
return_stmt   ::= "return" expr?
```
`parse_return` (`parser.c:329`). La expresión se omite si el siguiente token es
`}` o `EOF` (retorno void, `parser.c:332`).

```ebnf
expr_or_assign ::= expr ( "=" expr )?
```
`parse_expr_or_assign_stmt` (`parser.c:337`). **Única producción con matiz** —
ver §6.2: el parser lee una expresión completa y, si le sigue `=`, la trata como
asignación; el lado izquierdo queda restringido semánticamente a `IDENT` (→
asignación a variable) o a un indexado `IDENT[expr]` (→ asignación a elemento).
Cualquier otro lado izquierdo es un error de sintaxis.

---

## 4. Expresiones — precedencia por estratos

La precedencia se codifica con **un no terminal por nivel**, de menor a mayor
prioridad. Todos los operadores binarios son **asociativos por la izquierda**
(el bucle `while` de cada función produce un plegado a la izquierda); el unario
es asociativo por la derecha (recursión).

```ebnf
expr           ::= logic_or

logic_or       ::= logic_and ( "||" logic_and )*
logic_and      ::= equality  ( "&&" equality )*
equality       ::= comparison ( ( "==" | "!=" ) comparison )*
comparison     ::= additive  ( ( "<" | ">" | "<=" | ">=" ) additive )*
additive       ::= multiplicative ( ( "+" | "-" ) multiplicative )*
multiplicative ::= unary ( ( "*" | "/" | "%" ) unary )*

unary          ::= ( "!" | "-" ) unary
                 | postfix

postfix        ::= primary ( call_suffix | index_suffix )*
call_suffix    ::= "(" args? ")"
index_suffix   ::= "[" expr "]"
args           ::= expr ( "," expr )*

primary        ::= INT_LIT
                 | FLOAT_LIT
                 | "true" | "false"
                 | STR_LIT
                 | "(" expr ")"
                 | array_lit
                 | IDENT
                 | "println"
array_lit      ::= "[" ( expr ( "," expr )* )? "]"
```

Correspondencia con `parser.c`:

| No terminal      | Función            | Línea |
|------------------|--------------------|-------|
| `logic_or`       | `parse_or`         | 264   |
| `logic_and`      | `parse_and`        | 254   |
| `equality`       | `parse_eq`         | 244   |
| `comparison`     | `parse_cmp`        | 233   |
| `additive`       | `parse_add`        | 223   |
| `multiplicative` | `parse_mul`        | 213   |
| `unary`          | `parse_unary`      | 193   |
| `postfix`        | `parse_postfix`    | 166   |
| `primary`        | `parse_primary`    | 108   |

Tabla de precedencia resultante (de menor a mayor):

| Nivel | Operadores            | Asociatividad |
|-------|-----------------------|---------------|
| 1     | `\|\|`                | izquierda     |
| 2     | `&&`                  | izquierda     |
| 3     | `==`  `!=`            | izquierda     |
| 4     | `<`  `>`  `<=`  `>=`  | izquierda     |
| 5     | `+`  `-`              | izquierda     |
| 6     | `*`  `/`  `%`         | izquierda     |
| 7     | `!`  `-` (unarios)    | derecha       |
| 8     | `()` llamada, `[]` indexado | izquierda |

---

## 5. Terminales del lenguaje

Palabras clave (16): `val` `var` `fn` `return` `if` `else` `while` `for` `in`
`true` `false` `println` `int` `float` `bool` `string`.

Operadores y signos: `+` `-` `*` `/` `%` `=` `==` `!=` `<` `>` `<=` `>=` `&&`
`||` `!` `->` `..` `(` `)` `{` `}` `[` `]` `:` `,`.

Clases de token: `IDENT`, `INT_LIT`, `FLOAT_LIT`, `STR_LIT`, más `EOF`. La forma
léxica exacta de cada una está en [AUTOMATA.md](AUTOMATA.md).

---

## 6. Análisis de ambigüedad

Una gramática es ambigua si alguna cadena admite dos árboles de derivación
distintos. La de Kel **no lo es**, y a continuación se justifican los cuatro
puntos donde una gramática ingenua sí lo sería.

### 6.1 Precedencia y asociatividad de operadores

La forma ingenua `expr ::= expr OP expr | primary` es ambigua por partida doble:
`2 + 3 * 4` deriva tanto `(2+3)*4` como `2+(3*4)`, y `1 - 2 - 3` deriva
`(1-2)-3` y `1-(2-3)`.

Kel lo resuelve **estratificando** la gramática (§4): un no terminal por nivel
de precedencia fija qué operador liga más fuerte, y la forma iterativa
`nivel ::= superior ( OP superior )*` fija la asociatividad **por la izquierda**
(el código la implementa como un bucle que va plegando `l = binop(l, r)`,
`parser.c:216-218`). Resultado:

- `2 + 3 * 4` → `2 + (3 * 4)` — `*` está en un nivel más profundo que `+`.
- `1 - 2 - 3` → `(1 - 2) - 3` — plegado a la izquierda.

Cada cadena tiene **un solo** árbol. No hacen falta reglas de desambiguación
externas (tipo declaraciones `%left` de yacc): la propia estructura de la
gramática las incorpora.

### 6.2 Sentencia de asignación (la única excepción a LL(1) puro)

`a = b` podría, en principio, exigir decidir por adelantado si se está ante una
asignación o una expresión-sentencia. Kel lo resuelve **sin** anticipación
extra: `parse_expr_or_assign_stmt` (`parser.c:337`) analiza primero una
expresión completa y **luego** mira si sigue un `=`. Es una decisión
determinista tomada *después* de reducir el lado izquierdo, no una ambigüedad:
para una misma entrada el resultado es único.

El lado izquierdo se restringe **después** de analizarlo: solo un `IDENT`
(→ `N_ASSIGN`) o un indexado `IDENT[expr]` (→ `N_INDEX_ASSIGN`) son destinos
válidos (`parser.c:345-357`); `a + b = c` analiza `a + b`, ve el `=` y produce
el error «lado izquierdo inválido en asignación». Esta restricción es
**contextual**, no gramatical, y por eso no introduce ambigüedad.

### 6.3 *Dangling else* — ausente por diseño

La ambigüedad clásica `if C1 if C2 S1 else S2` (¿el `else` es del `if` interno o
del externo?) **no puede darse en Kel**: la rama de un `if` es siempre un
`block` entre llaves (`parser.c:298`), no una sentencia suelta. Como el `then`
está delimitado por `{ … }`, el `else` solo puede pertenecer al `if` que lo
precede inmediatamente. El encadenamiento `else if` se maneja por recursión
(`parse_if` se llama a sí mismo, `parser.c:300-301`) y también es inequívoco.

### 6.4 Menos unario frente a menos binario

`-` es siempre un único token `MINUS` (el lexer nunca lo pega al número,
decisión de SPEC.md). La gramática lo desambigua **por posición**: la regla
`unary ::= "-" unary` solo dispara en posición prefija, mientras que `additive`
maneja el `-` infijo. Así:

- `-5`      → menos unario aplicado a `5`.
- `a - 5`   → resta binaria.
- `a - -5`  → resta binaria de `a` y el unario `-5`.

Ningún caso admite dos lecturas.

### 6.5 Encadenado de llamada e indexado

`f(x)[i](y)` es inequívoco: el bucle de `parse_postfix` (`parser.c:168-189`)
consume sufijos de izquierda a derecha, produciendo `(((f(x))[i])(y))`. La
asociatividad por la izquierda del postfijo lo fija.

---

## 7. Nota sobre `println`

`println` es palabra reservada (tiene su propio token) pero **no** tiene una
producción propia de sentencia: aparece en `primary` como alternativa de
`IDENT` (`parser.c:152`) y construye el mismo nodo `N_IDENT`. Por tanto
`println(x)` se analiza como una llamada corriente. Que solo valga como
sentencia (y no dentro de una expresión con valor) **lo impone el sistema de
tipos**, que le asigna `void`, no la gramática. El detalle completo, y por qué
`println` reserva la palabra pero `read_int`/`read_float`/`read_line` no, está
en SPEC.md.
