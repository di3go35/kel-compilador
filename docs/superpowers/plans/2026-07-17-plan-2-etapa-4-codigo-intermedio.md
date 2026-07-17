# Plan 2 — Etapa 4: generación de código intermedio (TAC)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Implementar `src/ir.c`: traducir el AST anotado a Three Address Code, visible con `--ir`. Cierra el criterio 8 de la rúbrica (1.5 pts).

**Architecture:** `kel_gen(Node*)` recorre el AST por funciones y produce un `IRProgram` (array de `IRFunction`, ya definido en `ir.h`). Cada expresión se traduce a una `Addr` (constante, variable o temporal), emitiendo instrucciones por el camino; cada sentencia emite instrucciones sin devolver nada. Los temporales y las etiquetas se numeran por función.

**Tech Stack:** C11, gcc (TDM-GCC), `mingw32-make`, bash para la suite.

**Spec:** [`2026-07-16-unidad-ii-etapas-4-6-design.md`](../specs/2026-07-16-unidad-ii-etapas-4-6-design.md) — §3.

**Plan anterior:** [Plan 1](2026-07-16-plan-1-fundamentos.md) dejó `ir.h` con los tipos, `--symbols` y los built-ins `read_*`.

---

## Notas para quien ejecute esto

**`make` no existe en esta máquina.** Solo `mingw32-make` (TDM-GCC). En Linux/WSL sería `make`.

**ASan no funciona aquí** (TDM-GCC no trae libasan; `mingw32-make asan` falla con `cannot find -lasan`). La suite no ejercita memoria. Este plan reserva y libera bastante, así que **lee con cuidado**: la suite no te va a salvar de una fuga.

**Estado de partida.** `mingw32-make clean && mingw32-make && bash tests/run_tests.sh` debe dar "16 pasaron, 0 fallaron" y compilar sin warnings con `-Wall -Wextra -Wpedantic`.

**Los golden tests llevan CRLF.** `kelc.exe` emite `\r\n` porque MinGW abre stdout en modo texto. El runner ya normaliza con `tr -d '\r'` los dos lados. No pongas CRLF en los `.expected`: `.gitattributes` declara `eol=lf` y git lo revertiría.

**Convención de commits:** español, imperativo, como los existentes.

---

## Formas del AST — verificadas leyendo `parser.c`

**No asumas nada de esto: ya está comprobado, pero si algo no cuadra, para y reporta.**

| Nodo | Campos que usa |
|------|----------------|
| `N_INT_LIT` / `N_FLOAT_LIT` / `N_BOOL_LIT` / `N_STR_LIT` | `int_val` / `float_val` / `bool_val` / `str_val` |
| `N_IDENT` | `str_val` = nombre |
| `N_BINOP` | `op` (lexema), `lhs`, `rhs` |
| `N_UNOP` | `op` (lexema), **`lhs`** = operando (no `rhs`) |
| `N_CALL` | **`lhs`** = callee (`N_IDENT`), `items`/`item_count` = argumentos |
| `N_INDEX` | `lhs` = arreglo, `rhs` = índice |
| `N_ARRAY_LIT` | `items`, `item_count` |
| `N_VAR_DECL` | `decl_name`, `decl_type`, `decl_init`, `is_mutable` |
| `N_ASSIGN` | **`decl_name`** = destino, **`decl_init`** = valor. **NO usa `lhs`/`rhs`** |
| `N_INDEX_ASSIGN` | `lhs` = nodo `N_INDEX`, `rhs` = valor |
| `N_IF` | `cond`, `then_branch`, `else_branch` (puede ser NULL) |
| `N_WHILE` | `cond`, `body` |
| `N_FOR` | `loop_var`, `range_start`, `range_end`, `body` |
| `N_RETURN` | `lhs` = valor, **NULL si es `return` sin valor** |
| `N_EXPR_STMT` | `lhs` |
| `N_BLOCK` | `items`, `item_count` |
| `N_FUNCTION` | `fn_name`, `params`, `param_count`, `ret_type`, `body` (un `N_BLOCK`) |
| `N_PROGRAM` | `items` = funciones |

**`N_ASSIGN` es la trampa**: `parser.c:346` construye `n->decl_name = strdup(lhs->str_val); n->decl_init = rhs;` y libera el `N_IDENT`. Un `gen_stmt` que leyera `n->lhs` obtendría NULL y generaría IR vacío en silencio.

**`println` no tiene nodo propio.** `TOKEN_PRINTLN` se parsea como `N_IDENT` (`parser.c:152`), de modo que `println(x)` es un `N_CALL` con callee `N_IDENT("println")` — igual que los `read_*`. `N_PRINTLN` existe en el enum pero **está muerto** (ver Task 1).

**`Node.inferred_type`** está anotado por el semántico en todo nodo de expresión. Es de donde sale el tipo de cada `Addr`.

---

## Estructura de archivos

| Archivo | Responsabilidad | Acción |
|---------|-----------------|--------|
| `src/ir.h` | Tipos del TAC + API | Modificar (`ADDR_NONE`, API, convenciones) |
| `src/ir.c` | AST anotado → `IRProgram`; impresión; liberación | Crear |
| `src/semantic.h` / `src/semantic.c` | Exponer `kel_is_builtin` | Modificar |
| `src/parser.h` / `src/parser.c` | Quitar `N_PRINTLN` muerto | Modificar |
| `src/main.c` | Flag `--ir`; mover `kel_free_ast` | Modificar |
| `Makefile` | Añadir `src/ir.c` | Modificar |
| `tests/ir/*.kel` + `.expected` | Golden tests del TAC | Crear |
| `tests/run_tests.sh` | Sección de golden tests de `--ir` | Modificar |

---

## Task 1: Preparación — código muerto y `kel_is_builtin`

Dos obstáculos que hay que quitar antes de escribir `ir.c`.

**Files:** `src/parser.h`, `src/parser.c`, `src/semantic.h`, `src/semantic.c`

- [ ] **Step 1: Confirmar que `N_PRINTLN` está muerto**

Run: `grep -rn "N_PRINTLN" src/`

Expected: exactamente dos apariciones — la declaración en `parser.h` y una rama en el `switch` de `print_node` (`parser.c`). **Ningún sitio lo construye.** Si encuentras un `new_node(N_PRINTLN, ...)`, para y reporta: este task se basa en que no existe.

- [ ] **Step 2: Eliminar `N_PRINTLN`**

En `src/parser.h`, borrar la línea `    N_PRINTLN,` del enum `NodeKind`.

En `src/parser.c`, borrar del `switch` de `print_node`:

```c
        case N_PRINTLN:   printf("Println\n"); break;
```

Se quita porque, si se queda, quien implemente `ir.c` escribirá un `case N_PRINTLN:` que no se ejecuta nunca y perderá el rato buscando por qué su `println` no genera IR. `println` llega como `N_CALL`.

- [ ] **Step 3: Exponer `kel_is_builtin`**

`ir.c` necesita distinguir un `N_CALL` a `println`/`read_*` de una llamada de usuario. `kel_builtins[]` es `static` en `semantic.c`; sin exponer una consulta, `ir.c` re-hardcodearía los nombres.

En `src/semantic.h`, tras `SemResult kel_analyze(Node* program);`:

```c
/* ¿`name` es un built-in del lenguaje (println, read_int, read_float,
 * read_line)? La Etapa 4 lo necesita para emitir IR_PRINTLN / IR_READ en
 * vez de IR_CALL, sin duplicar los nombres. */
int kel_is_builtin(const char* name);

/* ¿`name` es uno de los read_*? Los tres generan IR_READ; println no. */
int kel_is_read_builtin(const char* name);
```

En `src/semantic.c`, tras `register_builtins`:

```c
int kel_is_read_builtin(const char* name) {
    for (size_t i = 0; i < KEL_N_BUILTINS; i++)
        if (strcmp(name, kel_builtins[i].name) == 0) return 1;
    return 0;
}

int kel_is_builtin(const char* name) {
    return strcmp(name, "println") == 0 || kel_is_read_builtin(name);
}
```

Nota que `println` **no** está en `kel_builtins[]` (esa tabla es solo de los `read_*`, que se registran como firmas; `println` se maneja aparte en `check_call`). Por eso `kel_is_builtin` lo comprueba explícitamente. Si te chirría la asimetría, es real y viene de que `println` es una sentencia y los `read_*` son expresiones — no la "arregles" aquí.

- [ ] **Step 4: Verificar**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 16 pasaron, 0 fallaron."

- [ ] **Step 5: Commit**

```bash
git add src/parser.h src/parser.c src/semantic.h src/semantic.c
git commit -m "Preparar la Etapa 4: quitar N_PRINTLN muerto y exponer kel_is_builtin

- N_PRINTLN estaba en el enum pero el parser nunca lo construía: println
  se parsea como N_IDENT y llega como N_CALL. Dejarlo habría hecho que
  ir.c escribiera un case que no se ejecuta nunca.
- ir.c necesita distinguir llamadas a built-ins de las de usuario, y
  kel_builtins[] era static en semantic.c."
```

---

## Task 2: Arreglar el contrato de `ir.h`

Tres problemas del header que hay que cerrar antes de implementar contra él.

**Files:** `src/ir.h`

- [ ] **Step 1: Añadir `ADDR_NONE`**

El header dice que `IR_RETURN` distingue el caso void con `op1.kind == 0`. Pero `ADDR_CONST_INT` **es** 0, así que `return 0` sería indistinguible de un `return` sin valor. Es un bug del contrato, no de la implementación.

En `enum AddrKind`, añadir como **primer** miembro:

```c
typedef enum {
    ADDR_NONE = 0,   /* operando ausente. Un Addr en cero es ADDR_NONE. */
    ADDR_CONST_INT,
```

Ahora "kind == 0 significa ausente" es cierto, y un `Addr` puesto a cero con `memset` es válido por construcción.

- [ ] **Step 2: Corregir el comentario de `IR_RETURN`**

```c
    IR_RETURN,            /* return op1; op1.kind == ADDR_NONE si es void */
```

- [ ] **Step 3: Fijar el contrato de `IR_CALL`**

El comentario actual dice `dst = call <op1.s>, n_params (usar 'i' en op2)`, pero `sym` ya está documentado como "o nombre de fn". Dos sitios para lo mismo. Nos quedamos con `sym`:

```c
    IR_CALL,              /* dst = call <sym>, op2.i args  (dst ADDR_NONE si void) */
```

- [ ] **Step 4: Actualizar la convención de la etiqueta de función**

El bloque de comentarios dice "Cada función empieza con un IR_LABEL con el nombre de la función". Eso era cierto cuando `IRProgram` era una lista plana. Ahora `IRFunction` lleva `name`, `params` y `ret_type`, así que la etiqueta sería redundante y `--ir` imprime la cabecera desde la struct.

Reemplazar esa línea de "Convenciones" por:

```
 *   - IRFunction lleva nombre, params y ret_type: no hace falta un IR_LABEL
 *     con el nombre de la función. --ir imprime la cabecera desde la struct.
 *   - IR_LABEL solo lleva etiquetas numéricas L1, L2..., en dst (ADDR_LABEL).
```

- [ ] **Step 5: Declarar la API de verdad**

Reemplazar el bloque de comentario `/* API de la Etapa 4 (ir.c) ... */` por declaraciones reales, conservando el aviso de tiempo de vida:

```c
/* ---------- API de la Etapa 4 (ir.c) ----------
 *
 * CUIDADO — tiempo de vida: IRFunction.params, ret_type y los Addr.type
 * apuntan al AST sin poseerlo, y los ADDR_VAR / ADDR_CONST_STR apuntan a
 * cadenas del AST. El AST debe seguir vivo mientras se use el IRProgram:
 * llama a kel_free_ast() DESPUÉS de kel_ir_free(), nunca antes.
 *
 * API de la Etapa 5 (optimize.c) — Plan 4.
 * API de la Etapa 6 (emit_c.c)   — Plan 3.
 */

IRProgram kel_gen(Node* program);          /* genera TAC desde el AST anotado */
void      kel_ir_print(const IRProgram*);  /* imprime el TAC (--ir) */
void      kel_ir_free(IRProgram*);
```

- [ ] **Step 6: Verificar**

Run: `gcc -fsyntax-only -Wall -Wextra -Wpedantic -std=c11 -x c src/ir.h`
Expected: sin salida.

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; 16/16. (`ir.h` aún no está en el build.)

- [ ] **Step 7: Commit**

```bash
git add src/ir.h
git commit -m "Cerrar el contrato de ir.h antes de implementar la Etapa 4

- ADDR_NONE = 0. El header decía que IR_RETURN marca void con
  op1.kind == 0, pero ADDR_CONST_INT era 0: 'return 0' y 'return' sin
  valor eran indistinguibles.
- IR_CALL guarda el nombre en sym, no duplicado en op1.s.
- Sin IR_LABEL con el nombre de función: IRFunction ya lleva nombre,
  params y ret_type desde el Plan 1.
- La API pasa de comentario a declaraciones reales."
```

---

## Task 3: Esqueleto de `ir.c` + `--ir` — golden test primero

TDD: el test se escribe y se ejecuta antes de que exista el flag.

Este task genera el IR de un programa **sin sentencias** — solo la cabecera de la función. Las expresiones y sentencias llegan en los tasks siguientes.

**Files:** Create `src/ir.c`, `tests/ir/vacio.kel`, `tests/ir/vacio.expected`. Modify `Makefile`, `tests/run_tests.sh`, `src/main.c`.

- [ ] **Step 1: Escribir el programa de prueba**

Crear `tests/ir/vacio.kel`:

```kel
fn nada() {
}

fn main() {
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/vacio.expected`:

```
fn nada() {
}

fn main() {
}
```

El formato de la cabecera imita a Kel a propósito: hace trivial comparar el `--ir` con el fuente en la exposición. El cuerpo va indentado 2 espacios (aquí no hay).

- [ ] **Step 3: Añadir la sección de golden tests de `--ir` al runner**

En `tests/run_tests.sh`, tras la sección de `tests/symbols` y antes del bloque final de "Resultado:", añadir:

```bash
echo
echo "== tests/ir (salida de --ir) =="
for f in tests/ir/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    if [ ! -e "$exp" ]; then
        fail=$((fail+1))
        fails+=("$f (falta $exp)")
        printf "  FAIL %s (falta %s)\n" "$f" "$exp"
        continue
    fi
    got=$("$KELC" --ir "$f" 2>/dev/null | tr -d '\r')
    if [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1))
        printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1))
        fails+=("$f (--ir no coincide con $exp)")
        printf "  FAIL %s (--ir no coincide)\n" "$f"
        printf "    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done
```

El `tr -d '\r'` en los dos lados es obligatorio: `kelc.exe` emite CRLF.

- [ ] **Step 4: Correr el test y verificar que FALLA**

Run: `bash tests/run_tests.sh`
Expected: **FAIL** en `tests/ir/vacio.kel`. `--ir` no existe, así que `main.c` dice "flag desconocido" y sale 1.

**No sigas hasta ver este fallo. Pégalo en tu reporte.**

- [ ] **Step 5: Crear `src/ir.c` con el esqueleto**

**Este esqueleto es deliberadamente mínimo: solo lo que se usa.** El proyecto
compila con cero warnings, y `-Wall` dispara `unused-function` sobre cualquier
`static` que no se llame. Las constructoras de `Addr` y la maquinaria de emisión
llegan en el Task 4, que es cuando hay algo que emitir.

```c
#include "ir.h"
#include "semantic.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Etapa 4 — Generación de código intermedio (TAC).
 *
 * gen_expr() traduce una expresión y devuelve la Addr donde quedó su
 * valor, emitiendo instrucciones por el camino. gen_stmt() traduce una
 * sentencia y no devuelve nada.
 *
 * Los Addr.type y los nombres (ADDR_VAR, ADDR_CONST_STR) apuntan al AST
 * sin poseerlo: el AST debe sobrevivir al IRProgram. Ver ir.h.
 * ============================================================ */

/* ---------- Generación por función ---------- */

static void gen_function(IRProgram* p, Node* fn) {
    if (p->count == p->capacity) {
        p->capacity = p->capacity ? p->capacity * 2 : 8;
        p->fns = (IRFunction*)realloc(p->fns, p->capacity * sizeof(IRFunction));
    }
    IRFunction* f = &p->fns[p->count++];
    memset(f, 0, sizeof(*f));
    f->name        = strdup(fn->fn_name);
    f->params      = fn->params;      /* no-owned: apunta al AST */
    f->param_count = fn->param_count;
    f->ret_type    = fn->ret_type;    /* no-owned */

    (void)f;  /* el cuerpo se genera en los tasks siguientes */
}

IRProgram kel_gen(Node* program) {
    IRProgram p;
    memset(&p, 0, sizeof(p));
    for (size_t i = 0; i < program->item_count; i++)
        gen_function(&p, program->items[i]);
    return p;
}

void kel_ir_free(IRProgram* p) {
    for (size_t i = 0; i < p->count; i++) {
        free(p->fns[i].name);
        free(p->fns[i].body);
    }
    free(p->fns);
    p->fns = NULL;
    p->count = p->capacity = 0;
}

/* ---------- Impresión ---------- */

static void print_instr(const Instr* in) {
    /* Cada task siguiente añade sus opcodes aquí. Hoy ninguna función
     * tiene cuerpo, así que este switch no se ejecuta nunca. */
    printf("  <opcode %d sin imprimir>\n", (int)in->op);
}

/* Cabecera con forma de Kel: hace inmediato comparar --ir con el fuente. */
static void print_fn_header(const IRFunction* f) {
    printf("fn %s(", f->name);
    for (size_t i = 0; i < f->param_count; i++) {
        if (i) printf(", ");
        printf("%s: %s", f->params[i].name, kel_type_name(f->params[i].type));
    }
    printf(")");
    if (f->ret_type && f->ret_type->kind != KT_VOID)
        printf(" -> %s", kel_type_name(f->ret_type));
    printf(" {\n");
}

void kel_ir_print(const IRProgram* p) {
    for (size_t i = 0; i < p->count; i++) {
        if (i) printf("\n");
        const IRFunction* f = &p->fns[i];
        print_fn_header(f);
        for (size_t j = 0; j < f->count; j++)
            print_instr(&f->body[j]);
        printf("}\n");
    }
}
```

Nada aquí queda sin usar: por eso el esqueleto no trae todavía ni las
constructoras de `Addr` ni `emit`. **Si aun así gcc avisa de algo, párate y
repórtalo — no lo silencies con un cast a void.**

- [ ] **Step 6: Añadir `ir.c` al Makefile**

En `Makefile`, la línea `SRC`, añadir ` src/ir.c` al final.

- [ ] **Step 7: Añadir `--ir` a `main.c`**

Añadir el include tras los otros:

```c
#include "ir.h"
```

En `usage()`, tras la línea de `--symbols`:

```c
        "  --ir        Muestra el código intermedio TAC (Etapa 4)\n"
```

Declarar la variable junto a las otras:

```c
    int show_tokens = 0, show_ast = 0, show_sem = 0, show_symbols = 0, show_ir = 0;
```

Y el parseo:

```c
        else if (strcmp(argv[i], "--ir") == 0) show_ir = 1;
```

- [ ] **Step 8: Generar y liberar el IR — y mover `kel_free_ast`**

**Este es el paso delicado.** `main.c` llama hoy a `kel_free_ast(pr.root)` justo tras el semántico, pero el `IRProgram` apunta al AST sin poseerlo. Hay que generar el IR **antes** de liberar el AST.

En el bloque del análisis semántico, tras el manejo de `sr.had_error` y antes de `kel_free_ast(pr.root);`, añadir:

```c
            if (!sr.had_error && show_ir) {
                IRProgram ir = kel_gen(pr.root);
                kel_ir_print(&ir);
                kel_ir_free(&ir);
            }
```

Y extender la condición del resumen para que `--ir` no ensucie el golden:

```c
            } else if (!show_tokens && !show_ast && !show_symbols && !show_ir) {
```

**Verifica que `kel_free_ast(pr.root)` queda DESPUÉS de ese bloque.** El IR muere al final del bloque (`kel_ir_free`), así que el AST sobrevive al IR, que es el orden correcto. Cuando el Plan 3 añada `--emit-c`, el IR tendrá que vivir más y este orden importará todavía más.

- [ ] **Step 9: Correr el test y verificar que PASA**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 17 pasaron, 0 fallaron."

- [ ] **Step 10: Commit**

```bash
git add src/ir.c src/main.c Makefile tests/ir tests/run_tests.sh
git commit -m "Etapa 4: esqueleto de ir.c y flag --ir

Genera el IRProgram con la cabecera de cada función; los cuerpos llegan
en los tasks siguientes.

main.c genera el IR antes de liberar el AST: IRFunction.params, ret_type
y los Addr.type apuntan al AST sin poseerlo."
```

---

## Task 4: Expresiones simples y `var`/`assign`

Literales, identificadores, binops, unops, declaraciones y asignaciones.

**Files:** `src/ir.c`, `tests/ir/expr.kel`, `tests/ir/expr.expected`

- [ ] **Step 1: Escribir el test**

Crear `tests/ir/expr.kel`:

```kel
fn main() {
  val a = 2 + 3 * 4
  var b = a - 1
  b = -b
  val s = "x" + "y"
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/expr.expected`:

```
fn main() {
  t1 = 3 * 4
  t2 = 2 + t1
  a = t2
  t3 = a - 1
  b = t3
  t4 = -b
  b = t4
  t5 = "x" + "y"
  s = t5
}
```

Razonamiento — entiéndelo antes de correrlo:
- `2 + 3 * 4` respeta la precedencia del parser: el `*` es el hijo derecho del `+`, así que se genera primero.
- Cada `var`/`val` emite un `IR_COPY` del temporal a la variable. Es redundante y el optimizador del Plan 4 lo limpiará con propagación de copias — **eso es deliberado**: enseñar el antes/después es el argumento de la Etapa 5.
- Los temporales se numeran en orden de creación, por función.
- Las constantes no ocupan temporal: son `Addr` directas.

- [ ] **Step 3: Correr y ver que falla**

Run: `bash tests/run_tests.sh`
Expected: **FAIL** en `tests/ir/expr.kel` — el cuerpo sale vacío.

- [ ] **Step 4: Implementar `gen_expr` y `gen_stmt`**

En `src/ir.c`, añadir **antes** de `gen_function` toda la maquinaria de `Addr` y
de emisión (el Task 3 no la trajo porque nada la usaba y habría dado warnings):

```c
/* ---------- Constructores de Addr ---------- */

/* Todo Addr nace en cero: kind == ADDR_NONE por defecto. */
static Addr addr_none(void) {
    Addr a;
    memset(&a, 0, sizeof(a));
    return a;
}

static Addr addr_var(const char* name, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_VAR;
    a.type = t;
    a.s = name;      /* no-owned: apunta al AST */
    return a;
}

static Addr addr_const_int(long long v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_INT;
    a.type = t;
    a.i = v;
    return a;
}

static Addr addr_const_float(double v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_FLOAT;
    a.type = t;
    a.f = v;
    return a;
}

static Addr addr_const_bool(int v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_BOOL;
    a.type = t;
    a.b = v;
    return a;
}

static Addr addr_const_str(const char* s, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_STR;
    a.type = t;
    a.s = s;         /* no-owned: apunta al AST */
    return a;
}

/* ---------- Emisión ---------- */

static void emit(IRFunction* f, Instr in) {
    if (f->count == f->capacity) {
        f->capacity = f->capacity ? f->capacity * 2 : 16;
        f->body = (Instr*)realloc(f->body, f->capacity * sizeof(Instr));
    }
    f->body[f->count++] = in;
}

static Instr instr(IROp op) {
    Instr in;
    memset(&in, 0, sizeof(in));
    in.op = op;
    return in;
}

/* Los temporales se numeran por función (ver ir.h). */
static Addr new_temp(IRFunction* f, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_TEMP;
    a.type = t;
    a.i = (long long)(++f->n_temps);
    return a;
}

/* ---------- Generación ---------- */

static Addr gen_expr(IRFunction* f, Node* n);
static void gen_stmt(IRFunction* f, Node* n);

static void emit_copy(IRFunction* f, Addr dst, Addr src) {
    Instr in = instr(IR_COPY);
    in.dst = dst;
    in.op1 = src;
    emit(f, in);
}

static Addr gen_binop(IRFunction* f, Node* n) {
    Addr a = gen_expr(f, n->lhs);
    Addr b = gen_expr(f, n->rhs);
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_BINOP);
    in.dst = d;
    in.op1 = a;
    in.op2 = b;
    in.sym = n->op;   /* no-owned: apunta al AST */
    emit(f, in);
    return d;
}

static Addr gen_unop(IRFunction* f, Node* n) {
    Addr a = gen_expr(f, n->lhs);   /* el operando va en lhs, no en rhs */
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_UNOP);
    in.dst = d;
    in.op1 = a;
    in.sym = n->op;
    emit(f, in);
    return d;
}

static Addr gen_expr(IRFunction* f, Node* n) {
    switch (n->kind) {
        case N_INT_LIT:   return addr_const_int(n->int_val, n->inferred_type);
        case N_FLOAT_LIT: return addr_const_float(n->float_val, n->inferred_type);
        case N_BOOL_LIT:  return addr_const_bool(n->bool_val, n->inferred_type);
        case N_STR_LIT:   return addr_const_str(n->str_val, n->inferred_type);
        case N_IDENT:     return addr_var(n->str_val, n->inferred_type);
        case N_BINOP:     return gen_binop(f, n);
        case N_UNOP:      return gen_unop(f, n);
        default:
            fprintf(stderr, "ir: nodo de expresión no soportado (kind %d)\n", (int)n->kind);
            return addr_none();
    }
}

static void gen_stmt(IRFunction* f, Node* n) {
    switch (n->kind) {
        case N_VAR_DECL: {
            Addr v = gen_expr(f, n->decl_init);
            emit_copy(f, addr_var(n->decl_name, n->decl_init->inferred_type), v);
            break;
        }
        case N_ASSIGN: {
            /* OJO: N_ASSIGN usa decl_name/decl_init, no lhs/rhs. */
            Addr v = gen_expr(f, n->decl_init);
            emit_copy(f, addr_var(n->decl_name, n->decl_init->inferred_type), v);
            break;
        }
        case N_BLOCK:
            for (size_t i = 0; i < n->item_count; i++)
                gen_stmt(f, n->items[i]);
            break;
        case N_EXPR_STMT:
            gen_expr(f, n->lhs);
            break;
        default:
            fprintf(stderr, "ir: sentencia no soportada (kind %d)\n", (int)n->kind);
            break;
    }
}
```

El `default` con `fprintf` es un andamio: cada task siguiente va quitando casos de ahí. **Al terminar el Task 8 no debe quedar ningún nodo cayendo al default en los tests.**

En `gen_function`, reemplazar el `(void)f;` por:

```c
    /* body es un N_BLOCK; gen_stmt lo recorre. */
    gen_stmt(f, fn->body);
```

- [ ] **Step 5: Implementar la impresión**

Reemplazar entero el `print_instr` mínimo del Task 3 por un `switch`, y añadir
`print_addr` justo encima (ahora sí se usa):

```c
static void print_addr(const Addr* a) {
    switch (a->kind) {
        case ADDR_NONE:        break;
        case ADDR_CONST_INT:   printf("%lld", a->i); break;
        case ADDR_CONST_FLOAT: printf("%.15g", a->f); break;
        case ADDR_CONST_BOOL:  printf("%s", a->b ? "true" : "false"); break;
        case ADDR_CONST_STR:   printf("\"%s\"", a->s); break;
        case ADDR_VAR:         printf("%s", a->s); break;
        case ADDR_TEMP:        printf("t%lld", a->i); break;
        case ADDR_LABEL:       printf("L%lld", a->i); break;
    }
}

static void print_instr(const Instr* in) {
    switch (in->op) {
        case IR_LABEL:
            /* Sin indentar, en la columna 0, igual que en C. */
            printf("L%lld:\n", in->dst.i);
            break;
        case IR_COPY:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_BINOP:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf(" %s ", in->sym);
            print_addr(&in->op2);
            printf("\n");
            break;
        case IR_UNOP:
            printf("  ");
            print_addr(&in->dst);
            printf(" = %s", in->sym);
            print_addr(&in->op1);
            printf("\n");
            break;
        default:
            printf("  <opcode %d sin imprimir>\n", (int)in->op);
            break;
    }
}
```

El `default` es un andamio visible: si un opcode se genera pero no se imprime,
sale `<opcode N sin imprimir>` en el golden y el test falla. Cada task siguiente
añade sus casos. **Al terminar el Task 9 no debe aparecer nunca.**

- [ ] **Step 6: Correr y verificar**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 18 pasaron, 0 fallaron."

Si tu `.expected` y la salida difieren, **entiende por qué antes de tocar nada**. Una numeración de temporales distinta a la esperada puede significar que estás generando los operandos en otro orden — y el orden importa cuando hay efectos secundarios.

- [ ] **Step 7: Commit**

```bash
git add src/ir.c tests/ir/expr.kel tests/ir/expr.expected
git commit -m "Etapa 4: expresiones simples, declaraciones y asignaciones

Los IR_COPY de cada declaración son redundantes a propósito: la
propagación de copias del Plan 4 los limpia, y enseñar ese antes/después
es el argumento de la Etapa 5.

N_ASSIGN usa decl_name/decl_init, no lhs/rhs (parser.c:346)."
```

---

## Task 5: Cortocircuito de `&&` y `||`

**Files:** `src/ir.c`, `tests/ir/corto.kel`, `tests/ir/corto.expected`

- [ ] **Step 1: Escribir el test**

Crear `tests/ir/corto.kel`:

```kel
fn main() {
  val p = true
  val q = false
  val a = p && q
  val b = p || q
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/corto.expected`:

```
fn main() {
  p = true
  q = false
  t1 = p
  ifFalse t1 goto L1
  t1 = q
L1:
  a = t1
  t2 = p
  if t2 goto L2
  t2 = q
L2:
  b = t2
}
```

Razonamiento:
- `t1` se reserva **antes** de generar el lado izquierdo, porque es el temporal del resultado y ambas ramas escriben en él.
- `&&` usa `ifFalse ... goto`: si el izquierdo es falso, `t1` ya vale falso y se salta el derecho.
- `||` usa `if ... goto`: si el izquierdo es cierto, se salta.
- `IR_LABEL` se imprime **sin indentar** (columna 0), igual que en C.

Lo que este test demuestra es la *forma*: el salto que se brinca el lado derecho. La prueba de que el cortocircuito importa de verdad (que `g()` no se ejecuta en `f() && g()`) la da el golden de `full.kel` en el Task 9, que sí tiene llamadas.

- [ ] **Step 3: Correr y ver que falla**

Run: `bash tests/run_tests.sh`
Expected: **FAIL** en `tests/ir/corto.kel` — sin cortocircuito, `&&` sale como un `IR_BINOP` normal (`t1 = p && q`).

- [ ] **Step 4: Implementar**

En `src/ir.c`, añadir la maquinaria de etiquetas junto a `new_temp` (no estaba antes porque nada la usaba):

```c
static Addr addr_label(long long id) {
    Addr a = addr_none();
    a.kind = ADDR_LABEL;
    a.i = id;
    return a;
}

/* Las etiquetas se numeran por función, igual que los temporales. */
static long long new_label(IRFunction* f) {
    return (long long)(++f->n_labels);
}

static void emit_label(IRFunction* f, long long id) {
    Instr in = instr(IR_LABEL);
    in.dst = addr_label(id);
    emit(f, in);
}
```

Y antes de `gen_binop`:

```c
/* `&&` y `||` no son IR_BINOP: eso evaluaría los dos lados siempre, y
 * `f() && g()` ejecutaría g() aunque f() fuese falso. La traducción usa
 * cortocircuito con etiquetas:
 *
 *     t = a                 t = a
 *     ifFalse t goto L      if t goto L
 *     t = b                 t = b
 *   L:                    L:
 *      (&&)                  (||)
 *
 * El temporal se reserva antes del lado izquierdo porque ambas ramas
 * escriben en él. */
static Addr gen_shortcircuit(IRFunction* f, Node* n) {
    int is_and = (strcmp(n->op, "&&") == 0);
    Addr t = new_temp(f, n->inferred_type);

    Addr a = gen_expr(f, n->lhs);
    emit_copy(f, t, a);

    long long L = new_label(f);
    Instr jmp = instr(is_and ? IR_IF_FALSE_GOTO : IR_IF_GOTO);
    jmp.op1 = t;
    jmp.op2 = addr_label(L);
    emit(f, jmp);

    Addr b = gen_expr(f, n->rhs);
    emit_copy(f, t, b);

    emit_label(f, L);
    return t;
}
```

Y al principio de `gen_binop`:

```c
static Addr gen_binop(IRFunction* f, Node* n) {
    if (strcmp(n->op, "&&") == 0 || strcmp(n->op, "||") == 0)
        return gen_shortcircuit(f, n);
    /* ...resto igual... */
```

- [ ] **Step 5: Imprimir los saltos**

En `print_instr`, añadir antes del `default`:

```c
        case IR_GOTO:
            printf("  goto ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_IF_GOTO:
            printf("  if ");
            print_addr(&in->op1);
            printf(" goto ");
            print_addr(&in->op2);
            printf("\n");
            break;
        case IR_IF_FALSE_GOTO:
            printf("  ifFalse ");
            print_addr(&in->op1);
            printf(" goto ");
            print_addr(&in->op2);
            printf("\n");
            break;
```

`IR_GOTO` no lo usa nadie todavía (llega en el Task 8), pero es un `case` dentro
de un `switch`, no una función suelta, así que no dispara `unused-function`.

- [ ] **Step 6: Verificar**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 19 pasaron, 0 fallaron."

- [ ] **Step 7: Commit**

```bash
git add src/ir.c tests/ir/corto.kel tests/ir/corto.expected
git commit -m "Etapa 4: cortocircuito de && y ||

Como IR_BINOP evaluarían los dos lados siempre, y f() && g() ejecutaría
g() aunque f() fuese falso. Se traducen con etiquetas y saltos
condicionales, que además es de lo mejor que enseña el --ir: control de
flujo generado a partir de un operador que parecía aritmético."
```

---

## Task 6: Llamadas — `println`, `read_*` y funciones de usuario

**Files:** `src/ir.c`, `tests/ir/llamadas.kel`, `tests/ir/llamadas.expected`

- [ ] **Step 1: Escribir el test**

Crear `tests/ir/llamadas.kel`:

```kel
fn suma(a: int, b: int) -> int {
  return a + b
}

fn saluda() {
  println("hola")
}

fn main() {
  val x = suma(1, 2)
  val y = read_int()
  saluda()
  println(x)
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/llamadas.expected`:

```
fn suma(a: int, b: int) -> int {
  t1 = a + b
  return t1
}

fn saluda() {
  println "hola"
}

fn main() {
  param 1
  param 2
  t1 = call suma, 2
  x = t1
  t2 = read int
  y = t2
  call saluda, 0
  println x
}
```

Razonamiento:
- Los argumentos se emiten como `IR_PARAM` en orden, y luego el `IR_CALL` con el número.
- `call saluda, 0` no tiene `dst`: `saluda` es void, así que `dst` es `ADDR_NONE` y no se imprime el `=`.
- `println` no es una llamada normal: emite `IR_PRINTLN`.
- `read_int()` emite `IR_READ`, con el tipo tomado del `dst`.
- Los temporales se reinician por función: `suma` y `main` empiezan ambos en `t1`.

- [ ] **Step 3: Correr y ver que falla**

Run: `bash tests/run_tests.sh`
Expected: **FAIL** — `ir: nodo de expresión no soportado` por stderr y cuerpo incompleto.

- [ ] **Step 4: Implementar**

En `src/ir.c`, antes de `gen_expr`:

```c
/* println y los read_* llegan como N_CALL: println se parsea como N_IDENT
 * (parser.c:152) y los read_* son identificadores normales. Se distinguen
 * por nombre con kel_is_builtin/kel_is_read_builtin, expuestos por
 * semantic.h, para no duplicar aquí la lista de nombres. */
static Addr gen_call(IRFunction* f, Node* n) {
    const char* name = n->lhs->str_val;

    if (strcmp(name, "println") == 0) {
        Addr v = gen_expr(f, n->items[0]);
        Instr in = instr(IR_PRINTLN);
        in.op1 = v;
        emit(f, in);
        return addr_none();          /* println es void */
    }

    if (kel_is_read_builtin(name)) {
        Addr d = new_temp(f, n->inferred_type);
        Instr in = instr(IR_READ);
        in.dst = d;
        emit(f, in);                 /* el tipo va en dst.type */
        return d;
    }

    for (size_t i = 0; i < n->item_count; i++) {
        Addr a = gen_expr(f, n->items[i]);
        Instr pin = instr(IR_PARAM);
        pin.op1 = a;
        emit(f, pin);
    }

    Instr in = instr(IR_CALL);
    in.sym = name;                   /* no-owned: apunta al AST */
    in.op2 = addr_const_int((long long)n->item_count, NULL);
    int is_void = !n->inferred_type || n->inferred_type->kind == KT_VOID;
    Addr d = addr_none();
    if (!is_void) {
        d = new_temp(f, n->inferred_type);
        in.dst = d;
    }
    emit(f, in);
    return d;
}
```

Añadir a `gen_expr`, antes del `default`:

```c
        case N_CALL:      return gen_call(f, n);
```

Y a `gen_stmt`, antes del `default`:

```c
        case N_RETURN: {
            Instr in = instr(IR_RETURN);
            if (n->lhs) in.op1 = gen_expr(f, n->lhs);   /* NULL si es void */
            emit(f, in);
            break;
        }
```

- [ ] **Step 5: Imprimir**

En `print_instr`, antes del `default`:

```c
        case IR_PARAM:
            printf("  param ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_CALL:
            printf("  ");
            if (in->dst.kind != ADDR_NONE) {
                print_addr(&in->dst);
                printf(" = ");
            }
            printf("call %s, %lld\n", in->sym, in->op2.i);
            break;
        case IR_RETURN:
            if (in->op1.kind == ADDR_NONE) {
                printf("  return\n");
            } else {
                printf("  return ");
                print_addr(&in->op1);
                printf("\n");
            }
            break;
        case IR_PRINTLN:
            printf("  println ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_READ:
            printf("  ");
            print_addr(&in->dst);
            printf(" = read %s\n", kel_type_name(in->dst.type));
            break;
```

`ADDR_NONE` es lo que hace posible distinguir `return` de `return 0` — ver el Task 2.

- [ ] **Step 6: Verificar**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 20 pasaron, 0 fallaron."

- [ ] **Step 7: Commit**

```bash
git add src/ir.c tests/ir/llamadas.kel tests/ir/llamadas.expected
git commit -m "Etapa 4: llamadas, println, read_* y return

println y los read_* llegan como N_CALL y se distinguen por nombre vía
kel_is_builtin, expuesto en el Task 1, en vez de duplicar los nombres.

El return sin valor se distingue de 'return 0' gracias a ADDR_NONE."
```

---

## Task 7: Arreglos

**Files:** `src/ir.c`, `tests/ir/arreglos.kel`, `tests/ir/arreglos.expected`

- [ ] **Step 1: Escribir el test**

Crear `tests/ir/arreglos.kel`:

```kel
fn main() {
  var nums: [int] = [10, 20, 30]
  nums[0] = 99
  val x = nums[1]
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/arreglos.expected`:

```
fn main() {
  t1 = alloc int[3]
  t1[0] = 10
  t1[1] = 20
  t1[2] = 30
  nums = t1
  nums[0] = 99
  t2 = nums[1]
  x = t2
}
```

Razonamiento:
- El literal reserva un temporal con `IR_ARRAY_NEW` y luego un `IR_INDEX_STORE` por elemento. `IR_INDEX_LOAD`/`IR_INDEX_STORE` ya existían en el header desde abril; solo `IR_ARRAY_NEW` es nuevo.
- Se imprime `alloc int[3]`, no `array[3]`: `array` no es palabra reservada en Kel, así que `t1 = array[3]` se leería como indexar una variable con ese nombre.
- El `IR_INDEX_STORE` imprime `dst` como fuente: `op1[op2] = dst`. Es la convención del header — rara, pero documentada.

- [ ] **Step 3: Correr y ver que falla**

- [ ] **Step 4: Implementar**

En `src/ir.c`, antes de `gen_expr`:

```c
/* Un literal de array reserva y luego rellena: IR_ARRAY_NEW + un
 * IR_INDEX_STORE por elemento. El tamaño sale del AST (item_count), no
 * del KelType, que no lo guarda. */
static Addr gen_array_lit(IRFunction* f, Node* n) {
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_ARRAY_NEW);
    in.dst = d;
    in.op1 = addr_const_int((long long)n->item_count, NULL);
    emit(f, in);

    for (size_t i = 0; i < n->item_count; i++) {
        Addr v = gen_expr(f, n->items[i]);
        Instr st = instr(IR_INDEX_STORE);
        st.op1 = d;                                   /* arreglo */
        st.op2 = addr_const_int((long long)i, NULL);  /* índice */
        st.dst = v;                                   /* fuente (ver ir.h) */
        emit(f, st);
    }
    return d;
}

static Addr gen_index(IRFunction* f, Node* n) {
    Addr arr = gen_expr(f, n->lhs);
    Addr idx = gen_expr(f, n->rhs);
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_INDEX_LOAD);
    in.dst = d;
    in.op1 = arr;
    in.op2 = idx;
    emit(f, in);
    return d;
}
```

Añadir a `gen_expr`, antes del `default`:

```c
        case N_ARRAY_LIT: return gen_array_lit(f, n);
        case N_INDEX:     return gen_index(f, n);
```

Y a `gen_stmt`, antes del `default`:

```c
        case N_INDEX_ASSIGN: {
            /* lhs es un N_INDEX; rhs es el valor. */
            Addr arr = gen_expr(f, n->lhs->lhs);
            Addr idx = gen_expr(f, n->lhs->rhs);
            Addr v   = gen_expr(f, n->rhs);
            Instr in = instr(IR_INDEX_STORE);
            in.op1 = arr;
            in.op2 = idx;
            in.dst = v;
            emit(f, in);
            break;
        }
```

- [ ] **Step 5: Imprimir**

En `print_instr`, antes del `default`:

```c
        case IR_ARRAY_NEW:
            printf("  ");
            print_addr(&in->dst);
            printf(" = alloc %s[%lld]\n",
                   in->dst.type && in->dst.type->elem
                       ? kel_type_name(in->dst.type->elem) : "?",
                   in->op1.i);
            break;
        case IR_INDEX_LOAD:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf("[");
            print_addr(&in->op2);
            printf("]\n");
            break;
        case IR_INDEX_STORE:
            printf("  ");
            print_addr(&in->op1);
            printf("[");
            print_addr(&in->op2);
            printf("] = ");
            print_addr(&in->dst);
            printf("\n");
            break;
```

- [ ] **Step 6: Verificar y commit**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 21 pasaron, 0 fallaron."

```bash
git add src/ir.c tests/ir/arreglos.kel tests/ir/arreglos.expected
git commit -m "Etapa 4: literales de array, indexado y asignación indexada

El literal reserva con IR_ARRAY_NEW y rellena con un IR_INDEX_STORE por
elemento; ambos opcodes de índice ya existían desde abril. El tamaño sale
del AST, no del KelType, que no lo guarda."
```

---

## Task 8: Control de flujo — `if`, `while`, `for`

**Files:** `src/ir.c`, `tests/ir/flujo.kel`, `tests/ir/flujo.expected`

- [ ] **Step 1: Escribir el test**

Crear `tests/ir/flujo.kel`:

```kel
fn main() {
  var t = 0
  if t > 0 {
    t = 1
  } else {
    t = 2
  }
  while t < 5 {
    t = t + 1
  }
  for i in 0..3 {
    t = t + i
  }
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/ir/flujo.expected`:

```
fn main() {
  t = 0
  t1 = t > 0
  ifFalse t1 goto L1
  t = 1
  goto L2
L1:
  t = 2
L2:
L3:
  t2 = t < 5
  ifFalse t2 goto L4
  t3 = t + 1
  t = t3
  goto L3
L4:
  i = 0
L5:
  t4 = i < 3
  ifFalse t4 goto L6
  t5 = t + i
  t = t5
  t6 = i + 1
  i = t6
  goto L5
L6:
}
```

Razonamiento:
- `if` con `else`: `ifFalse cond goto Lelse` / then / `goto Lfin` / `Lelse:` / else / `Lfin:`.
- `while`: la etiqueta de inicio va **antes** de evaluar la condición, porque hay que re-evaluarla en cada vuelta.
- `for i in 0..3` se desazucara a `i = 0` / `Lcond:` / `ifFalse i < 3 goto Lfin` / cuerpo / `i = i + 1` / `goto Lcond` / `Lfin:`. Aquí se ve que el rango es de fin exclusivo.
- Un `if` sin `else` no emite el `goto Lfin` ni la segunda etiqueta.

**Ojo con `t`**: en este programa la variable de usuario se llama `t` y los temporales `t1`, `t2`… No colisionan porque los temporales llevan número, pero es incómodo de leer. Se deja así a propósito: si un día colisionaran de verdad sería un bug del Plan 3 al emitir C, y este test lo dejaría a la vista.

- [ ] **Step 3: Correr y ver que falla**

- [ ] **Step 4: Implementar**

En `src/ir.c`, añadir antes de `gen_stmt`:

```c
static void emit_goto(IRFunction* f, long long L) {
    Instr in = instr(IR_GOTO);
    in.op1 = addr_label(L);
    emit(f, in);
}

static void emit_if_false_goto(IRFunction* f, Addr cond, long long L) {
    Instr in = instr(IR_IF_FALSE_GOTO);
    in.op1 = cond;
    in.op2 = addr_label(L);
    emit(f, in);
}
```

Y en `gen_stmt`, antes del `default`:

```c
        case N_IF: {
            Addr c = gen_expr(f, n->cond);
            long long L_else = new_label(f);
            emit_if_false_goto(f, c, L_else);
            gen_stmt(f, n->then_branch);
            if (n->else_branch) {
                long long L_end = new_label(f);
                emit_goto(f, L_end);
                emit_label(f, L_else);
                gen_stmt(f, n->else_branch);
                emit_label(f, L_end);
            } else {
                emit_label(f, L_else);
            }
            break;
        }
        case N_WHILE: {
            /* La etiqueta va antes de la condición: hay que reevaluarla
             * en cada vuelta. */
            long long L_cond = new_label(f);
            long long L_end  = new_label(f);
            emit_label(f, L_cond);
            Addr c = gen_expr(f, n->cond);
            emit_if_false_goto(f, c, L_end);
            gen_stmt(f, n->body);
            emit_goto(f, L_cond);
            emit_label(f, L_end);
            break;
        }
        case N_FOR: {
            /* for i in a..b  ==>  i = a; Lc: ifFalse i < b goto Lf;
             *                     cuerpo; i = i + 1; goto Lc; Lf:
             * El fin es exclusivo (ver SPEC.md). */
            Addr start = gen_expr(f, n->range_start);
            KelType* it = n->range_start->inferred_type;
            Addr iv = addr_var(n->loop_var, it);
            emit_copy(f, iv, start);

            long long L_cond = new_label(f);
            long long L_end  = new_label(f);
            emit_label(f, L_cond);

            Addr end = gen_expr(f, n->range_end);
            Addr c = new_temp(f, NULL);   /* bool; el AST no tiene nodo para esta comparación */
            Instr cmp = instr(IR_BINOP);
            cmp.dst = c;
            cmp.op1 = iv;
            cmp.op2 = end;
            cmp.sym = "<";
            emit(f, cmp);
            emit_if_false_goto(f, c, L_end);

            gen_stmt(f, n->body);

            Addr inc = new_temp(f, it);
            Instr add = instr(IR_BINOP);
            add.dst = inc;
            add.op1 = iv;
            add.op2 = addr_const_int(1, it);
            add.sym = "+";
            emit(f, add);
            emit_copy(f, iv, inc);

            emit_goto(f, L_cond);
            emit_label(f, L_end);
            break;
        }
```

**Un detalle que hay que entender, no copiar a ciegas:** la comparación `i < b` del `for` **no existe en el AST** — el parser no crea un nodo para ella, la genera el desazucarado. Por eso su temporal se crea con `NULL` como tipo. Eso deja un `Addr.type == NULL` que el Plan 3 tendrá que tolerar al emitir C (un bool). **Repórtalo como concern al terminar**: si el Plan 3 necesita el tipo, la solución limpia es que `ir.c` construya un `KelType` propio para estos casos, y entonces `IRFunction` tendría que poseerlo y liberarlo — un cambio de contrato que no toca hacer aquí.

- [ ] **Step 5: Verificar y commit**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio; "Resultado: 22 pasaron, 0 fallaron."

```bash
git add src/ir.c tests/ir/flujo.kel tests/ir/flujo.expected
git commit -m "Etapa 4: if, while y for

El for se desazucara a init + condición + incremento, que enseña bien el
fin exclusivo del rango. En el while la etiqueta va antes de la condición
porque hay que reevaluarla en cada vuelta."
```

---

## Task 9: Integración — `full.kel` y memoria

**Files:** `tests/ir/full.expected`, `tests/run_tests.sh`

- [ ] **Step 1: Verificar que no queda ningún nodo sin soportar**

Run: `./kelc --ir tests/ok/full.kel 2>&1 >/dev/null`
Expected: **sin salida**. Cualquier `ir: nodo ... no soportado` en stderr significa que falta un caso.

Run lo mismo sobre `tests/ok/edge.kel`, `tests/ok/empty_arrays.kel`, `tests/ok/lexer.kel`, `tests/ok/read.kel`.
Expected: sin salida en todos.

**Si alguno reporta un nodo no soportado, arréglalo antes de seguir** y añade un test que lo cubra.

- [ ] **Step 2: Golden test de `full.kel`**

`tests/ok/full.kel` cubre todos los tokens y construcciones. Genera su IR y revísalo **a mano, línea por línea**, antes de congelarlo:

```bash
cp tests/ok/full.kel tests/ir/full.kel
./kelc --ir tests/ir/full.kel | tr -d '\r' > tests/ir/full.expected
```

**Generar un golden desde la salida es circular y normalmente no vale.** Aquí se acepta solo si lo lees entero y verificas: que cada función tenga su cabecera correcta, que los temporales se reinicien por función, que los `&&`/`||` tengan sus saltos, que los arrays reserven y rellenen, que el `for` incremente. **Si algo no cuadra, es un bug del generador, no del test.** Pega en tu reporte lo que verificaste.

- [ ] **Step 3: Comprobar fugas a mano**

ASan no funciona aquí, así que revisa el código:

- `kel_ir_free` libera `name` (strdup'd) y `body` (realloc'd) de cada función, y luego `fns`. ¿Falta algo?
- Los `Addr.s` de `ADDR_VAR` y `ADDR_CONST_STR` apuntan al AST: **no se liberan**. Confirma que `kel_ir_free` no los toca.
- `Instr.sym` apunta al AST (`n->op`, `name`) o a un literal estático (`"<"` del `for`): **no se libera**. Confirma.
- `main.c` llama a `kel_ir_free` antes de `kel_free_ast`. Confirma el orden.

Documenta lo que encuentres en el reporte. Si algo se libera dos veces o se filtra, arréglalo.

- [ ] **Step 4: Verificar el estado final**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: limpio con `-Wall -Wextra -Wpedantic`; "Resultado: 23 pasaron, 0 fallaron."

- [ ] **Step 5: Actualizar la documentación**

En `SPEC.md`, en el bloque "Estructura del proyecto", quitar el `(pendiente)` de `ir.c`:

```
  ir.c                     — Etapa 4: generación de TAC
```

En `SPEC.md`, sección "### Uso del compilador", añadir:

```
./kelc --ir programa.kel     # código intermedio (Etapa 4)
```

En `README.md`: mismo cambio en su bloque de estructura y en "## Uso". Y en "## Estado", reflejar que la Etapa 4 está hecha.

Verifica la invariante del Plan 1: **todo archivo listado sin `(pendiente)` debe existir.** Run: `ls src/`

- [ ] **Step 6: Commit**

```bash
git add tests/ir SPEC.md README.md
git commit -m "Cerrar el Plan 2: golden de full.kel y documentación

Etapa 4 completa: --ir genera TAC para todas las construcciones de Kel.
Cierra el criterio 8 de la rúbrica."
```

---

## Criterio de terminado

- [ ] `mingw32-make clean && mingw32-make` sin warnings con `-Wall -Wextra -Wpedantic`
- [ ] `bash tests/run_tests.sh` → "23 pasaron, 0 fallaron"
- [ ] `./kelc --ir` sobre los 5 programas de `tests/ok/` no imprime nada en stderr
- [ ] `./kelc --ir tests/ok/full.kel` produce TAC correcto, verificado a mano
- [ ] `kel_free_ast` se llama DESPUÉS de `kel_ir_free` en `main.c`
- [ ] `N_PRINTLN` ya no existe; `kel_is_builtin` está expuesto en `semantic.h`
- [ ] `ADDR_NONE` existe y `return` se distingue de `return 0`
- [ ] SPEC.md y README.md no marcan `ir.c` como pendiente

## Qué queda fuera

No hay optimización (Plan 4) ni emisión de C (Plan 3). El IR generado es
deliberadamente ingenuo: cada declaración emite un `IR_COPY` redundante y no
se pliega ninguna constante. **Eso es el material del Plan 4**: el antes/después
del optimizador es lo que demuestra la Etapa 5.

## Concerns conocidos que hay que reportar al terminar

- El temporal de la comparación del `for` lleva `type == NULL` porque esa
  comparación no existe en el AST. El Plan 3 tendrá que tolerarlo o habrá que
  darle un tipo propio (y entonces `IRFunction` pasaría a poseer `KelType`s).
- `realloc` sin comprobar en `emit`/`gen_function`, igual que en todo el
  codebase. Coherente, no un descuido.
