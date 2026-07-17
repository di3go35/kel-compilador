# Plan 1 — Fundamentos: IR, tabla de símbolos y built-ins de entrada

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Dejar listos los cimientos de la Unidad II: el header del IR reestructurado, la tabla de símbolos visible con `--symbols`, y los built-ins `read_int`/`read_float`/`read_line` type-chequeando.

**Architecture:** Tres bloques independientes. (1) `src/codegen.h` se renombra a `src/ir.h` y se reestructura para que el IR esté organizado por función, no como lista plana. (2) Un log append-only en `src/symtab.c` graba cada declaración cuando ocurre, porque `scope_pop` libera los símbolos al cerrar el scope y al final de la compilación no queda nada que imprimir. (3) Los `read_*` se registran como firmas built-in en la tabla de funciones del semántico, sin tocar lexer ni parser.

**Tech Stack:** C11, gcc (TDM-GCC en `/c/TDM-GCC-64/bin`), `mingw32-make`, bash para la suite de regresión.

**Spec:** [`docs/superpowers/specs/2026-07-16-unidad-ii-etapas-4-6-design.md`](../specs/2026-07-16-unidad-ii-etapas-4-6-design.md) — §3.1, §3.2, §5.1, §5.2.

---

## Notas para quien ejecute esto

**`make` no existe en esta máquina.** Solo `mingw32-make` (TDM-GCC). Todos los comandos de este plan usan `mingw32-make`. En Linux/WSL sería `make`.

**Cómo se prueba este proyecto.** No hay framework de tests unitarios de C. La suite es `tests/run_tests.sh`: corre `./kelc` sobre cada `.kel` y compara el código de salida. Este plan añade un modo nuevo: comparar la **salida estándar** contra un archivo `.expected` (golden test). Es la forma de probar `--symbols`.

**Estado de partida.** `mingw32-make && bash tests/run_tests.sh` debe dar "9 pasaron, 0 fallaron" antes de empezar.

**Convención de commits.** Mensajes en español, en imperativo, como los ya existentes en el repo.

---

## Estructura de archivos

| Archivo | Responsabilidad | Acción |
|---------|-----------------|--------|
| `src/ir.h` | Tipos del código intermedio (TAC) | Renombrar desde `codegen.h` + reestructurar |
| `src/symtab.h` | Interfaz del log de declaraciones | Crear |
| `src/symtab.c` | Log append-only + impresión de la tabla | Crear |
| `src/semantic.c` | Etiquetas de ámbito, desplazamientos, hook del log, built-ins `read_*` | Modificar |
| `src/main.c` | Flag `--symbols` | Modificar |
| `Makefile` | Añadir `symtab.c` a `SRC` | Modificar |
| `tests/run_tests.sh` | Sección de golden tests | Modificar |
| `tests/symbols/*.kel` + `.expected` | Golden tests de `--symbols` | Crear |
| `tests/ok/read.kel` | `read_*` type-chequea | Crear |
| `tests/bad/read_*.kel` | Usos inválidos de `read_*` | Crear |
| `SPEC.md`, `README.md` | Referencias a `ir.h`, arreglos por referencia | Modificar |

---

## Task 1: Renombrar `codegen.h` → `ir.h`

Cambio mecánico. `codegen.h` contiene tipos del código *intermedio* (Etapa 4), pero "codegen" significa generación de código *final* (Etapa 6). El nombre invita a confusión durante la defensa.

**Files:**
- Rename: `src/codegen.h` → `src/ir.h`
- Modify: `SPEC.md:282`
- Modify: `README.md:83`

- [ ] **Step 1: Verificar que nadie incluye el header todavía**

Run: `grep -rn "codegen" src/ Makefile`

Expected: solo aparece `src/codegen.h` a sí mismo (su propio `#ifndef KEL_CODEGEN_H`). Ningún `.c` lo incluye — no está en el `SRC` del Makefile. Si algún `.c` lo incluyera, añadir ese archivo a los edits de abajo.

- [ ] **Step 2: Renombrar con git para conservar el historial**

```bash
git mv src/codegen.h src/ir.h
```

- [ ] **Step 3: Actualizar el include guard**

En `src/ir.h`, cambiar las dos primeras líneas y la última:

```c
#ifndef KEL_IR_H
#define KEL_IR_H
```

```c
#endif
```

- [ ] **Step 4: Actualizar la cabecera del comentario**

En `src/ir.h`, la línea 5 dice `* Etapa 4 — Código intermedio (TAC: Three Address Code)`. Dejarla igual — sigue siendo correcta. Cambiar solo la línea 10, que dice:

```c
 *                             →  C de salida (Etapa 6)
```

por:

```c
 *                             →  C de salida (Etapa 6, emit_c.c)
```

- [ ] **Step 5: Actualizar SPEC.md**

En `SPEC.md`, dentro del bloque "Estructura del proyecto", reemplazar:

```
  codegen.h                — esqueleto para Etapa 4 (TAC)
```

por:

```
  ir.h                     — Etapa 4: tipos del código intermedio (TAC)
  ir.c                     — Etapa 4: generación de TAC             (pendiente)
  optimize.h/ optimize.c   — Etapa 5: optimización local            (pendiente)
  emit_c.h  / emit_c.c     — Etapa 6: generación de C               (pendiente)
  symtab.h  / symtab.c     — log de la tabla de símbolos            (pendiente)
```

**Los marcadores `(pendiente)` son obligatorios.** El bloque describe la
estructura del repo, y listar archivos que no existen manda al lector (el
profesor, durante la defensa) a buscar un `optimize.c` que no está. El texto
original decía "esqueleto" justo por eso; esa señal no se puede perder.
`ir.h` e `ir.c` van en líneas separadas porque solo existe el header.

Y en la sección "Roadmap", donde dice:

```
TAC (Three Address Code). Ver `src/codegen.h` para el formato decidido.
```

reemplazar por:

```
TAC (Three Address Code). Ver `src/ir.h` para el formato decidido.
```

- [ ] **Step 6: Actualizar README.md**

En `README.md`, bloque "Estructura del proyecto", reemplazar:

```
  codegen.h                — esqueleto para Etapa 4 (TAC)
```

por:

```
  ir.h                     — Etapa 4: tipos del código intermedio (TAC)
  ir.c                     — Etapa 4: generación de TAC             (pendiente)
  symtab.h  / symtab.c     — log de la tabla de símbolos            (pendiente)
```

El marcador de `symtab` se quita en el Task 3, que es donde se crea de verdad.

- [ ] **Step 7: Verificar que sigue compilando**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 9 pasaron, 0 fallaron."

- [ ] **Step 8: Commit**

```bash
git add src/ir.h SPEC.md README.md
git commit -m "Renombrar codegen.h a ir.h

El header contiene los tipos del código intermedio (Etapa 4), pero
'codegen' significa universalmente generación de código final (Etapa 6).
El generador será emit_c.c."
```

---

## Task 2: Reestructurar `ir.h` — IR por función y opcodes nuevos

Tres cambios del spec (§3.1, §3.2): `IRProgram` pasa a estar organizado por función, y se añaden `IR_ARRAY_NEW` e `IR_READ`.

Este task es solo el header — no hay implementación todavía, así que no hay test que pueda fallar. La verificación es que compile.

**Files:**
- Modify: `src/ir.h`

- [ ] **Step 1: Añadir los dos opcodes nuevos**

En `src/ir.h`, en el `enum IROp`, añadir tras `IR_UNOP`:

```c
    IR_ARRAY_NEW,         /* dst = array[op1]  (op1 = nº de elementos, const int) */
```

y tras `IR_PRINTLN`:

```c
    IR_READ               /* dst = read <tipo de dst>  (entrada estándar) */
```

Cuidado con la coma: `IR_PRINTLN` pasa a llevar coma al final.

- [ ] **Step 2: Documentar los opcodes nuevos en el comentario de cabecera**

En el bloque de comentario de `ir.h`, en la lista de formatos de impresión, añadir tras la línea de `IR_INDEX_STORE`:

```c
 *     t6 = array[3]           ; IR_ARRAY_NEW  (reserva, no inicializa)
```

y tras la línea de `IR_PRINTLN`:

```c
 *     t7 = read int           ; IR_READ
```

- [ ] **Step 3: Reemplazar `IRProgram` por la estructura por función**

En `src/ir.h`, reemplazar el bloque:

```c
typedef struct {
    Instr* data;
    size_t count, capacity;
} IRProgram;
```

por:

```c
/* Una función compilada. Los temporales (t1..t_temps) y las etiquetas
 * (L1..L_labels) se numeran por función, no globalmente. */
typedef struct {
    char*    name;          /* owned */
    Param*   params;        /* no-owned; apunta al AST */
    size_t   param_count;
    KelType* ret_type;      /* no-owned; apunta al AST */
    Instr*   body;          /* owned */
    size_t   count, capacity;
    int      n_temps;       /* temporales usados: t1..t_temps */
    int      n_labels;      /* etiquetas usadas: L1..L_labels */
} IRFunction;

typedef struct {
    IRFunction* fns;        /* owned */
    size_t      count, capacity;
} IRProgram;
```

`Param` y `KelType` ya vienen de `#include "parser.h"`, que `ir.h` ya hace.

- [ ] **Step 4: Actualizar el comentario de la API prevista**

Reemplazar el bloque final de comentario:

```c
/* API a implementar en la Etapa 4.
 *
 *   IRProgram kel_gen(Node* program);   // genera TAC desde el AST
 *   void      kel_ir_print(const IRProgram*);
 *   void      kel_ir_free(IRProgram*);
 */
```

por:

```c
/* API de la Etapa 4 (ir.c) — se implementa en el Plan 2.
 *
 *   IRProgram kel_gen(Node* program);        // genera TAC desde el AST anotado
 *   void      kel_ir_print(const IRProgram*);
 *   void      kel_ir_free(IRProgram*);
 *
 * API de la Etapa 5 (optimize.c) — Plan 4.
 * API de la Etapa 6 (emit_c.c)   — Plan 3.
 */
```

- [ ] **Step 5: Verificar que el header compila por sí solo**

Run: `gcc -fsyntax-only -Wall -Wextra -std=c11 -x c src/ir.h`
Expected: sin salida (éxito). Si se queja de `Param` o `KelType`, es que falta el `#include "parser.h"` — ya debería estar.

- [ ] **Step 6: Verificar que el proyecto sigue compilando**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 9 pasaron, 0 fallaron."

- [ ] **Step 7: Commit**

```bash
git add src/ir.h
git commit -m "Reestructurar ir.h: IR por función y dos opcodes nuevos

- IRProgram pasa a ser un array de IRFunction. El header ya documentaba
  que temporales y etiquetas se numeran por función, pero la struct era
  una lista plana; además emit_c necesita la firma para generar
  'int suma(int a, int b)', que una lista de instrucciones no lleva.
- IR_ARRAY_NEW: no había forma de representar val nums = [1,2,3].
- IR_READ: para los built-ins de entrada."
```

---

## Task 3: `symtab.c` — el log de declaraciones

La tabla de símbolos real vive en `semantic.c` y `scope_pop` (`semantic.c:87`) libera los símbolos al cerrar cada scope. Al terminar la compilación no queda nada que imprimir. Este módulo graba cada declaración en el momento en que ocurre, **sin cambiar el ciclo de vida de los scopes**.

Usa estado global de archivo, igual que `diag.c` (`src/diag.c:7`), para ser coherente con el patrón del codebase.

**Files:**
- Create: `src/symtab.h`
- Create: `src/symtab.c`
- Modify: `Makefile:4`

- [ ] **Step 1: Crear `src/symtab.h`**

```c
#ifndef KEL_SYMTAB_H
#define KEL_SYMTAB_H

#include "parser.h"
#include <stddef.h>

/* Log append-only de declaraciones (--symbols).
 *
 * La tabla de símbolos real vive en semantic.c y sus scopes se liberan en
 * scope_pop al cerrarse, de modo que al terminar la compilación no queda
 * nada que imprimir. Este log graba cada declaración cuando ocurre, sin
 * tocar el ciclo de vida de los scopes.
 *
 * Modelo del desplazamiento (campo `offset`): posición relativa al marco de
 * la función, alineada al tamaño del tipo. No es una dirección absoluta —
 * esas las decide gcc. Los arreglos cuentan como una referencia de 8 bytes
 * porque KelType no guarda el número de elementos. */

typedef struct {
    char*    scope;       /* owned: "main", "main.for", "suma" */
    char*    name;        /* owned */
    KelType* type;        /* owned */
    int      is_mutable;  /* 0 = val, 1 = var */
    int      offset;      /* desplazamiento en el marco */
    int      line;
    char*    value;       /* owned; NULL si no es constante conocida */
} SymEntry;

/* Copia todo lo que recibe: el llamante conserva la propiedad de sus datos.
 * `value` puede ser NULL. */
void   kel_symlog_add(const char* scope, const char* name, const KelType* type,
                      int is_mutable, int offset, int line, const char* value);
void   kel_symlog_print(void);
void   kel_symlog_free(void);
size_t kel_symlog_count(void);

/* Tamaño y alineación del modelo de marco. Expuestos para poder probarlos. */
int kel_type_size(const KelType* t);
int kel_type_align(const KelType* t);

#endif
```

- [ ] **Step 2: Crear `src/symtab.c`**

```c
#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static SymEntry* g_log = NULL;
static size_t    g_count = 0;
static size_t    g_cap = 0;

/* Modelo de tamaños del marco. Ver el comentario de symtab.h.
 * Los arreglos son referencias de 8 bytes: KelType no guarda cuántos
 * elementos tiene, así que n×elemento no es computable desde el tipo. */
int kel_type_size(const KelType* t) {
    if (!t) return 0;
    switch (t->kind) {
        case KT_INT:    return 4;
        case KT_FLOAT:  return 8;
        case KT_BOOL:   return 1;
        case KT_STRING: return 8;   /* char* */
        case KT_ARRAY:  return 8;   /* referencia */
        default:        return 0;   /* KT_VOID, KT_UNKNOWN */
    }
}

int kel_type_align(const KelType* t) {
    int s = kel_type_size(t);
    return s > 0 ? s : 1;
}

static KelType* type_copy(const KelType* t) {
    if (!t) return NULL;
    KelType* n = (KelType*)calloc(1, sizeof(KelType));
    n->kind = t->kind;
    n->elem = type_copy(t->elem);
    return n;
}

static void type_release(KelType* t) {
    if (!t) return;
    type_release(t->elem);
    free(t);
}

void kel_symlog_add(const char* scope, const char* name, const KelType* type,
                    int is_mutable, int offset, int line, const char* value) {
    if (g_count == g_cap) {
        g_cap = g_cap ? g_cap * 2 : 16;
        g_log = (SymEntry*)realloc(g_log, g_cap * sizeof(SymEntry));
    }
    SymEntry* e = &g_log[g_count++];
    e->scope      = strdup(scope ? scope : "");
    e->name       = strdup(name ? name : "");
    e->type       = type_copy(type);
    e->is_mutable = is_mutable;
    e->offset     = offset;
    e->line       = line;
    e->value      = value ? strdup(value) : NULL;
}

size_t kel_symlog_count(void) { return g_count; }

/* La cabecera va escrita a mano, no con %-12s.
 *
 * printf rellena contando BYTES, no caracteres. "Ámbito" ocupa 7 bytes en
 * UTF-8 pero 6 columnas visuales (Á son 2 bytes), y "Línea" lo mismo. Con
 * %-12s la cabecera saldría desplazada una columna respecto a los datos.
 * Las filas de datos sí usan %-Ns sin problema: los identificadores, los
 * tipos y los ámbitos son todos ASCII. El "—" de la columna Valor es UTF-8
 * pero es la última columna y no se rellena. */
static const char* SYM_HEADER =
    "Ámbito       Identificador  Tipo      Mut   Despl  Línea  Valor";

void kel_symlog_print(void) {
    puts(SYM_HEADER);
    for (size_t i = 0; i < g_count; i++) {
        SymEntry* e = &g_log[i];
        printf("%-12s %-14s %-9s %-4s %6d %6d  %s\n",
               e->scope, e->name, kel_type_name(e->type),
               e->is_mutable ? "var" : "val",
               e->offset, e->line,
               e->value ? e->value : "—");
    }
}

void kel_symlog_free(void) {
    for (size_t i = 0; i < g_count; i++) {
        free(g_log[i].scope);
        free(g_log[i].name);
        type_release(g_log[i].type);
        free(g_log[i].value);
    }
    free(g_log);
    g_log = NULL;
    g_count = g_cap = 0;
}
```

- [ ] **Step 3: Añadir `symtab.c` al Makefile**

En `Makefile`, línea 4, reemplazar:

```make
SRC = src/main.c src/lexer.c src/parser.c src/semantic.c src/diag.c
```

por:

```make
SRC = src/main.c src/lexer.c src/parser.c src/semantic.c src/diag.c src/symtab.c
```

- [ ] **Step 4: Verificar que compila sin warnings**

Run: `mingw32-make clean && mingw32-make`
Expected: compila sin warnings. `symtab.c` se compila aunque nadie lo llame todavía.

Nota: si gcc avisa de `defined but not used`, es un falso problema — son funciones no-static de un header público. No debería avisar.

- [ ] **Step 5: Verificar que la suite sigue verde**

Run: `bash tests/run_tests.sh`
Expected: "Resultado: 9 pasaron, 0 fallaron."

- [ ] **Step 6: Quitar el marcador `(pendiente)` de symtab en la documentación**

`symtab.h` y `symtab.c` ya existen, así que el marcador que puso el Task 1 ahora miente al revés.

En `SPEC.md`, en el bloque "Estructura del proyecto", reemplazar:

```
  symtab.h  / symtab.c     — log de la tabla de símbolos            (pendiente)
```

por:

```
  symtab.h  / symtab.c     — log de la tabla de símbolos (--symbols)
```

En `README.md`, en el mismo bloque, reemplazar:

```
  symtab.h  / symtab.c     — log de la tabla de símbolos            (pendiente)
```

por:

```
  symtab.h  / symtab.c     — log de la tabla de símbolos
```

Verificar la regla que fija el Task 1: todo archivo listado sin `(pendiente)` debe existir. Run: `ls src/`

- [ ] **Step 7: Commit**

```bash
git add src/symtab.h src/symtab.c Makefile SPEC.md README.md
git commit -m "Añadir symtab.c: log append-only de declaraciones

scope_pop libera los símbolos al cerrar cada scope, así que al terminar
la compilación no queda tabla que imprimir. El log graba cada declaración
cuando ocurre, sin tocar el ciclo de vida de los scopes.

Estado global de archivo, igual que diag.c."
```

---

## Task 4: `--symbols` — golden test primero

Aquí empieza el TDD de verdad: el test se escribe antes que el código y debe fallar.

**Files:**
- Create: `tests/symbols/basico.kel`
- Create: `tests/symbols/basico.expected`
- Modify: `tests/run_tests.sh`
- Modify: `src/semantic.c`
- Modify: `src/main.c`

- [ ] **Step 1: Escribir el programa de prueba**

Crear `tests/symbols/basico.kel`:

```kel
fn suma(a: int, b: int) -> int {
  return a + b
}

fn main() {
  val nombre = "Diego"
  var puntaje: int = 0
  val pi: float = 3.1415
  puntaje = suma(puntaje, 1)
  println(nombre)
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/symbols/basico.expected`:

```
Ámbito       Identificador  Tipo      Mut   Despl  Línea  Valor
suma         a              int       var       0      1  —
suma         b              int       var       4      1  —
main         nombre         string    val       0      6  "Diego"
main         puntaje        int       var       8      7  0
main         pi             float     val      16      8  3.1415
```

Notas sobre por qué es exactamente así:

- **El orden es de declaración**, y las funciones se recorren en el orden del archivo (`kel_analyze` pass 2, `semantic.c:546`). `suma` va antes que `main`.
- **Los parámetros salen como `var`**: `check_function` (`semantic.c:506`) los declara con `mutable = 1`. Es el comportamiento actual y este plan no lo cambia.
- **Los desplazamientos se reinician por función**: `suma` empieza en 0 otra vez.
- `nombre` es `string` (8 bytes) → `puntaje` va alineado a 4 pero el offset ya es 8 → 8. `pi` es `float` (align 8): tras `puntaje` el cursor está en 12, se alinea a 16.
- **`Valor` solo se llena con literales directos.** `puntaje = suma(...)` es una asignación, no una declaración, así que no aparece.
- **La cabecera está alineada a mano** y debe coincidir columna a columna con el `printf` de las filas (`"%-12s %-14s %-9s %-4s %6d %6d  %s\n"`). Si tocas el formato de las filas, la cabecera y todos los `.expected` se mueven. Ver el comentario de `SYM_HEADER` en `symtab.c` para el porqué.

- [ ] **Step 3: Añadir la sección de golden tests al runner**

En `tests/run_tests.sh`, insertar antes de la línea `echo` que precede a `echo "Resultado: ..."` (es decir, tras el bucle de `tests/bad`):

```bash
echo
echo "== tests/symbols (salida de --symbols) =="
for f in tests/symbols/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    if [ ! -e "$exp" ]; then
        fail=$((fail+1))
        fails+=("$f (falta $exp)")
        printf "  FAIL %s (falta %s)\n" "$f" "$exp"
        continue
    fi
    got=$("$KELC" --symbols "$f" 2>/dev/null)
    if [ "$got" = "$(cat "$exp")" ]; then
        pass=$((pass+1))
        printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1))
        fails+=("$f (--symbols no coincide con $exp)")
        printf "  FAIL %s (--symbols no coincide)\n" "$f"
        printf "    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" "$(cat "$exp")" "$got"
    fi
done
```

- [ ] **Step 4: Correr el test y verificar que falla**

Run: `bash tests/run_tests.sh`
Expected: **FAIL** en `tests/symbols/basico.kel`. El flag `--symbols` no existe todavía, así que `main.c` responde "flag desconocido" y sale con código 1; `got` queda vacío y no coincide con el `.expected`.

- [ ] **Step 5: Añadir la etiqueta de ámbito al `Scope`**

En `src/semantic.c`, reemplazar la struct `Scope` (líneas 18-21):

```c
typedef struct Scope {
    Symbol* head;
    struct Scope* parent;
} Scope;
```

por:

```c
typedef struct Scope {
    Symbol* head;
    struct Scope* parent;
    const char* label;   /* "main", "for", ... o NULL en bloques anónimos */
} Scope;
```

- [ ] **Step 6: Añadir el cursor de marco y el nombre de función a `Sem`**

En `src/semantic.c`, reemplazar la struct `Sem` (líneas 31-36):

```c
typedef struct {
    Scope* scope;
    FnSig* fns;
    KelType* current_ret;  /* ret_type de la función que se está analizando */
    int errors;
} Sem;
```

por:

```c
typedef struct {
    Scope* scope;
    FnSig* fns;
    KelType* current_ret;  /* ret_type de la función que se está analizando */
    int errors;
    int frame_offset;      /* cursor del marco; se reinicia por función */
} Sem;
```

- [ ] **Step 7: Dar etiqueta a `scope_push`**

En `src/semantic.c`, reemplazar `scope_push` (líneas 81-85):

```c
static void scope_push(Sem* S) {
    Scope* s = (Scope*)calloc(1, sizeof(Scope));
    s->parent = S->scope;
    S->scope = s;
}
```

por:

```c
static void scope_push(Sem* S, const char* label) {
    Scope* s = (Scope*)calloc(1, sizeof(Scope));
    s->parent = S->scope;
    s->label = label;
    S->scope = s;
}

/* Construye el ámbito punteado ("main.for") caminando hasta la raíz y
 * saltando los bloques anónimos. Escribe en `out` y lo devuelve. */
static const char* scope_path(Sem* S, char* out, size_t cap) {
    const char* parts[16];
    int n = 0;
    for (Scope* s = S->scope; s && n < 16; s = s->parent)
        if (s->label) parts[n++] = s->label;
    out[0] = '\0';
    size_t len = 0;
    for (int i = n - 1; i >= 0; i--) {
        size_t need = strlen(parts[i]) + (len ? 1 : 0);
        if (len + need + 1 > cap) break;
        if (len) out[len++] = '.';
        strcpy(out + len, parts[i]);
        len += strlen(parts[i]);
    }
    return out;
}
```

- [ ] **Step 8: Actualizar los tres sitios de llamada de `scope_push`**

Hay exactamente tres. En `check_block` (`semantic.c:355`):

```c
    scope_push(S, NULL);
```

En el check del `for` (`semantic.c:436`):

```c
    scope_push(S, "for");
```

En `check_function` (`semantic.c:504`):

```c
    scope_push(S, fn->fn_name);
```

- [ ] **Step 9: Reiniciar el cursor de marco por función**

En `src/semantic.c`, en `check_function`, reemplazar:

```c
static void check_function(Sem* S, Node* fn) {
    S->current_ret = fn->ret_type;
    scope_push(S, fn->fn_name);
```

por:

```c
static void check_function(Sem* S, Node* fn) {
    S->current_ret = fn->ret_type;
    S->frame_offset = 0;
    scope_push(S, fn->fn_name);
```

- [ ] **Step 10: Enganchar el log en `declare()`**

En `src/semantic.c`, añadir el include arriba, tras `#include "diag.h"`:

```c
#include "symtab.h"
```

Reemplazar `declare` (líneas 115-127) por:

```c
/* Devuelve el literal del inicializador como texto, o NULL si no es una
 * constante conocida en compilación. El llamante libera. */
static char* const_text(const Node* init) {
    if (!init) return NULL;
    char buf[128];
    switch (init->kind) {
        case N_INT_LIT:   snprintf(buf, sizeof(buf), "%lld", init->int_val); break;
        case N_FLOAT_LIT: snprintf(buf, sizeof(buf), "%g", init->float_val); break;
        case N_BOOL_LIT:  snprintf(buf, sizeof(buf), "%s", init->bool_val ? "true" : "false"); break;
        case N_STR_LIT:   snprintf(buf, sizeof(buf), "\"%s\"", init->str_val); break;
        default: return NULL;
    }
    return strdup(buf);
}

static int declare(Sem* S, const char* name, KelType* type, int mutable, const Node* where) {
    if (scope_find_local(S->scope, name)) {
        err(S, where, "'%s' ya está declarado en este ámbito", name);
        return 0;
    }
    Symbol* sy = (Symbol*)calloc(1, sizeof(Symbol));
    sy->name = strdup(name);
    sy->type = type_clone(type);
    sy->is_mutable = mutable;
    sy->next = S->scope->head;
    S->scope->head = sy;

    /* Grabar en el log para --symbols. Alineamos el cursor del marco al
     * tamaño del tipo antes de asignar el desplazamiento. */
    int align = kel_type_align(type);
    S->frame_offset = (S->frame_offset + align - 1) / align * align;
    int off = S->frame_offset;
    S->frame_offset += kel_type_size(type);

    char path[128];
    scope_path(S, path, sizeof(path));
    char* val = (where && where->kind == N_VAR_DECL) ? const_text(where->decl_init) : NULL;
    kel_symlog_add(path, name, type, mutable, off, where ? where->line : 0, val);
    free(val);

    return 1;
}
```

- [ ] **Step 11: Añadir el flag `--symbols` a `main.c`**

En `src/main.c`, añadir el include tras `#include "diag.h"`:

```c
#include "symtab.h"
```

En `usage()`, tras la línea de `--ast`, añadir:

```c
        "  --symbols   Muestra la tabla de símbolos (Etapa 3)\n"
```

En `main()`, añadir la variable junto a las otras:

```c
    int show_tokens = 0, show_ast = 0, show_sem = 0, show_symbols = 0;
```

Y el parseo del flag, tras la línea de `--sem`:

```c
        else if (strcmp(argv[i], "--symbols") == 0) show_symbols = 1;
```

- [ ] **Step 12: Imprimir la tabla tras el análisis**

En `src/main.c`, reemplazar el bloque del análisis semántico:

```c
            SemResult sr = kel_analyze(pr.root);
            if (show_ast) kel_print_ast(pr.root);  /* con tipos inferidos */
            if (sr.had_error) {
                fprintf(stderr, "Semántico: %d error(es)\n", sr.errors);
                rc = 1;
            } else if (show_sem) {
                printf("Semántico OK\n");
            } else if (!show_tokens && !show_ast) {
                printf("Lexer OK — %zu tokens\nParser OK\nSemántico OK\n", tokens.count);
            }
```

por:

```c
            SemResult sr = kel_analyze(pr.root);
            if (show_ast) kel_print_ast(pr.root);  /* con tipos inferidos */
            if (show_symbols) kel_symlog_print();
            if (sr.had_error) {
                fprintf(stderr, "Semántico: %d error(es)\n", sr.errors);
                rc = 1;
            } else if (show_sem) {
                printf("Semántico OK\n");
            } else if (!show_tokens && !show_ast && !show_symbols) {
                printf("Lexer OK — %zu tokens\nParser OK\nSemántico OK\n", tokens.count);
            }
```

- [ ] **Step 13: Liberar el log al salir**

En `src/main.c`, antes de `kel_free_tokens(&tokens);`, añadir:

```c
    kel_symlog_free();
```

- [ ] **Step 14: Correr el test y verificar que pasa**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 10 pasaron, 0 fallaron."

Si el golden no coincide, el runner imprime esperado vs obtenido. Los desalineamientos de espacios son reales: ajustar el `.expected`, no el `printf` — salvo que el `printf` esté mal de verdad.

- [ ] **Step 15: Commit**

```bash
git add src/semantic.c src/main.c tests/symbols tests/run_tests.sh
git commit -m "Añadir --symbols: tabla de símbolos visible

Cierra el criterio 7 de la rúbrica, que pedía una tabla de símbolos
demostrable y no solo el análisis.

- Scope lleva una etiqueta; el ámbito punteado (main.for) se construye
  caminando hasta la raíz. Solo había 3 sitios de llamada de scope_push.
- El desplazamiento se calcula alineando al tamaño del tipo, reiniciando
  el cursor por función. Es un modelo del marco, no una dirección real:
  esas las decide gcc, e inventarlas sería un dato falso.
- La columna Valor solo se llena con literales directos.
- El runner gana una sección de golden tests que compara stdout."
```

---

## Task 5: Ámbitos anidados — el `for`

El `basico.kel` no tiene bucles, así que la construcción del ámbito punteado (`main.for`) no está probada. Este task la prueba.

**Files:**
- Create: `tests/symbols/anidado.kel`
- Create: `tests/symbols/anidado.expected`

- [ ] **Step 1: Escribir el programa de prueba**

Crear `tests/symbols/anidado.kel`:

```kel
fn main() {
  var total = 0
  for i in 0..3 {
    val doble = i * 2
    total = total + doble
  }
  println(total)
}
```

- [ ] **Step 2: Escribir la salida esperada**

Crear `tests/symbols/anidado.expected`:

```
Ámbito       Identificador  Tipo      Mut   Despl  Línea  Valor
main         total          int       var       0      2  0
main.for     i              int       val       4      3  —
main.for     doble          int       val       8      4  —
```

Por qué:

- `i` se declara en el scope del `for` (`semantic.c:436-438`), que lleva la etiqueta `"for"` → ámbito `main.for`.
- `doble` está en el `N_BLOCK` del cuerpo, que abre un scope anónimo (`label = NULL`). Los anónimos se saltan al construir el path, así que también sale `main.for`. **Esto es correcto y deliberado**: el ámbito punteado nombra construcciones, no niveles de anidamiento.
- `i` es `val`: el `for` lo declara inmutable. Confirma la decisión de diseño de que la variable del bucle no es reasignable.
- El cursor del marco **no** se reinicia al cerrar un scope, solo por función: `total`=0, `i`=4, `doble`=8.
- `doble = i * 2` no es literal → `Valor` es `—`.

- [ ] **Step 3: Correr el test**

Run: `bash tests/run_tests.sh`
Expected: "Resultado: 11 pasaron, 0 fallaron."

Este test debería pasar a la primera: el código ya se escribió en el Task 4. Si falla, el runner imprime esperado vs obtenido. **No ajustes el `.expected` para que cuadre sin entender por qué difiere** — si el ámbito sale `main` en vez de `main.for`, la etiqueta del `for` no se está aplicando y eso es un bug del Task 4, no del test.

- [ ] **Step 4: Commit**

```bash
git add tests/symbols/anidado.kel tests/symbols/anidado.expected
git commit -m "Probar ámbitos anidados en --symbols

Cubre el ámbito punteado (main.for) y confirma que la variable del
bucle es val y que el cursor del marco no se reinicia por scope, solo
por función."
```

---

## Task 6: Built-ins `read_*` — tests primero

`read_int()`, `read_float()`, `read_line()` como firmas built-in en la tabla de funciones. **Sin tocar lexer ni parser**: `read_int` es un `TOKEN_IDENT` normal y `read_int()` ya parsea hoy como `N_CALL`.

En este plan solo type-chequean. La generación de código llega en los Planes 2 y 3.

**Files:**
- Create: `tests/ok/read.kel`
- Create: `tests/bad/read_arity.kel`
- Create: `tests/bad/read_redefine.kel`
- Create: `tests/bad/read_type.kel`
- Modify: `src/semantic.c`

- [ ] **Step 1: Escribir el test que debe compilar**

Crear `tests/ok/read.kel`:

```kel
fn main() {
  val a = read_int()
  val b = read_float()
  val s = read_line()
  val suma: int = a + 1
  val prod: float = b * 2.0
  println(s)
  println(suma)
  println(prod)
}
```

Prueba a la vez que los tipos de retorno se infieren bien: si `read_int()` no devolviera `int`, `a + 1` fallaría.

- [ ] **Step 2: Escribir los tests que deben fallar**

Crear `tests/bad/read_arity.kel`:

```kel
fn main() {
  val a = read_int(5)
  println(a)
}
```

Crear `tests/bad/read_redefine.kel`:

```kel
fn read_int() -> int {
  return 0
}

fn main() {
  println(read_int())
}
```

Crear `tests/bad/read_type.kel`:

```kel
fn main() {
  val s: string = read_int()
  println(s)
}
```

- [ ] **Step 3: Correr los tests y verificar que fallan como no se espera**

Run: `bash tests/run_tests.sh`
Expected: **FAIL en `tests/ok/read.kel`** (dice "función 'read_int' no definida" y sale 1, pero debería compilar) y **FAIL en `tests/bad/read_redefine.kel`** (compila bien porque `read_int` es un nombre libre hoy, pero debería fallar).

`read_arity.kel` y `read_type.kel` "pasan" ya, pero por el motivo equivocado: fallan con "función no definida" en vez de por aridad o por tipo. Quedarán correctos tras el Step 5.

- [ ] **Step 4: Registrar las firmas built-in**

En `src/semantic.c`, añadir tras `fn_register` (tras la línea 149):

```c
/* Built-ins de entrada. No son palabras reservadas: read_int es un
 * TOKEN_IDENT normal y read_int() ya parsea como N_CALL, así que el lexer y
 * el parser no se enteran. Se registran como firmas sin parámetros en la
 * tabla de funciones, antes que las del usuario, de modo que fn_register
 * rechaza cualquier intento de redefinirlas. */
static const struct { const char* name; KelTypeKind ret; } kel_builtins[] = {
    { "read_int",   KT_INT    },
    { "read_float", KT_FLOAT  },
    { "read_line",  KT_STRING },
};
#define KEL_N_BUILTINS (sizeof(kel_builtins) / sizeof(kel_builtins[0]))

static void register_builtins(Sem* S) {
    for (size_t i = 0; i < KEL_N_BUILTINS; i++) {
        FnSig* sig = (FnSig*)calloc(1, sizeof(FnSig));
        sig->name = (char*)kel_builtins[i].name;   /* literal estático, no se libera */
        sig->params = NULL;
        sig->param_count = 0;
        sig->ret_type = mk(kel_builtins[i].ret);
        sig->next = S->fns;
        S->fns = sig;
    }
}
```

- [ ] **Step 5: Llamarlo antes del pass 1**

En `src/semantic.c`, en `kel_analyze`, reemplazar:

```c
    /* Pass 1: registrar funciones */
    int has_main = 0;
```

por:

```c
    /* Los built-ins van primero: así fn_register rechaza redefinirlos. */
    register_builtins(&S);

    /* Pass 1: registrar funciones */
    int has_main = 0;
```

- [ ] **Step 6: Liberar los `ret_type` de los built-ins**

`register_builtins` hace `mk(...)`, que reserva memoria. El bucle de liberación actual (`semantic.c:550`) solo libera el contenedor `FnSig`, porque los `ret_type` de las funciones del usuario son referencias al AST. Los de los built-ins no.

En `src/semantic.c`, reemplazar:

```c
    /* Libera fn signatures (solo contenedor) */
    while (S.fns) { FnSig* nx = S.fns->next; free(S.fns); S.fns = nx; }
```

por:

```c
    /* Libera fn signatures. Los ret_type de las funciones del usuario son
     * referencias al AST y no se liberan aquí; los de los built-ins sí,
     * porque register_builtins los reservó con mk(). */
    while (S.fns) {
        FnSig* nx = S.fns->next;
        for (size_t i = 0; i < KEL_N_BUILTINS; i++) {
            if (strcmp(S.fns->name, kel_builtins[i].name) == 0) {
                type_free(S.fns->ret_type);
                break;
            }
        }
        free(S.fns);
        S.fns = nx;
    }
```

- [ ] **Step 7: Correr los tests y verificar que pasan**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 15 pasaron, 0 fallaron." (11 previos + `ok/read.kel` + los 3 de `bad/read_*`.)

- [ ] **Step 8: Verificar que los mensajes de error son los correctos, no solo el código de salida**

La suite solo mira el código de salida, así que hay que confirmar a mano que fallan por el motivo correcto:

Run: `./kelc tests/bad/read_arity.kel`
Expected: `función 'read_int' espera 0 argumento(s), recibió 1` — **no** "función no definida".

Run: `./kelc tests/bad/read_redefine.kel`
Expected: `función 'read_int' ya definida`

Run: `./kelc tests/bad/read_type.kel`
Expected: un error de incompatibilidad entre `string` e `int` — **no** "función no definida".

Si alguno dice "función 'read_int' no definida", `register_builtins` no se está llamando antes del pass 1.

- [ ] **Step 9: Commit**

```bash
git add src/semantic.c tests/ok/read.kel tests/bad/read_arity.kel tests/bad/read_redefine.kel tests/bad/read_type.kel
git commit -m "Añadir built-ins read_int, read_float y read_line

Cierra el hueco de 'Entrada y salida' del criterio 1: el lenguaje solo
tenía println.

No son palabras reservadas. println es una sentencia, pero read_int()
devuelve un valor, así que es una expresión — y read_int seguido de '('
ya parsea como N_CALL porque es un TOKEN_IDENT normal. Lexer y parser
sin cambios; solo firmas pre-registradas en la tabla de funciones.

Registradas antes del pass 1 para que fn_register rechace redefinirlas.

Aquí solo type-chequean; la generación de código va en los Planes 2 y 3."
```

---

## Task 7: Documentar los arreglos por referencia en SPEC.md

§3.4 del spec: los arreglos se pasan por referencia. Era un hueco semántico sin resolver de la Unidad I y hoy SPEC.md no dice nada. Debe quedar escrito antes de que el Plan 3 lo dé por hecho al emitir C.

**Files:**
- Modify: `SPEC.md`

- [ ] **Step 1: Documentarlo en la sección de Arrays**

En `SPEC.md`, en la sección "### Arrays", tras la línea:

```
- Tamaño fijo en v1 (no hay push/pop)
```

añadir:

```
- **Se pasan por referencia** a las funciones: si una función muta un
  elemento del arreglo que recibe, el cambio es visible para quien la llamó.
  Es coherente con C, que es el target de la Etapa 6.
```

- [ ] **Step 2: Añadirlo a la tabla de decisiones de diseño**

En `SPEC.md`, en la tabla "## Decisiones de diseño", añadir una fila tras la de "Tipos incluidos":

```
| Paso de arreglos      | Por referencia                    | Coherente con el target C   |
```

- [ ] **Step 3: Verificar que la suite sigue verde**

Run: `bash tests/run_tests.sh`
Expected: "Resultado: 15 pasaron, 0 fallaron." (Solo se tocó documentación.)

- [ ] **Step 4: Commit**

```bash
git add SPEC.md
git commit -m "Documentar que los arreglos se pasan por referencia

Hueco semántico que la Unidad I dejó sin resolver: el lenguaje permite
fn f(a: [int]) y mutar a[0] dentro, pero no decía qué ve quien llama.
Se fija en 'por referencia', coherente con C, que es el target de la
Etapa 6. El Plan 3 depende de esta decisión al emitir código."
```

---

## Task 8: Cierre del plan

- [ ] **Step 1: Verificar el estado final completo**

Run: `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`
Expected: compila sin warnings; "Resultado: 15 pasaron, 0 fallaron."

- [ ] **Step 2: Verificar a mano las dos salidas nuevas**

Run: `./kelc --symbols tests/ok/full.kel`
Expected: la tabla con los símbolos de `mul`, `div`, `neg`, `cadena` y `main`, incluidos los arreglos (`ai`, `af`, `ab`, `as`) con `Despl` de 8 en 8, por contar como referencias.

Run: `./kelc --help`
Expected: la ayuda incluye `--symbols`.

- [ ] **Step 3: Verificar que no hay fugas nuevas (opcional, requiere Linux/WSL)**

Run: `mingw32-make asan` (o `make asan` en WSL)
Expected: la suite pasa sin reportes de ASan.

Si ASan no está disponible en TDM-GCC, saltar este paso — el Makefile ya avisa de ello en su comentario. No es bloqueante.

- [ ] **Step 4: Actualizar el estado en README.md**

En `README.md`, en la sección "## Estado", reemplazar:

```
**Unidad I completa** (lexer, parser, análisis semántico). Unidad II (código
intermedio, optimización, generación final) pendiente.
```

por:

```
**Unidad I completa** (lexer, parser, análisis semántico) con tabla de
símbolos visible (`--symbols`) y built-ins de entrada. Unidad II (código
intermedio, optimización, generación final) en curso.
```

En la sección "## Uso", tras la línea de `--ast`, añadir:

```
./kelc --symbols programa.kel   # muestra la tabla de símbolos
```

En "## Requisitos", reemplazar:

```
- `make`
```

por:

```
- `make` (en Windows con TDM-GCC: `mingw32-make`)
```

- [ ] **Step 5: Commit**

```bash
git add README.md
git commit -m "Actualizar README: --symbols, built-ins y mingw32-make

En Windows con TDM-GCC el binario es mingw32-make, no make; el README
decía 'make' a secas y no funciona tal cual."
```

---

## Criterio de terminado

- [ ] `mingw32-make clean && mingw32-make` compila sin warnings
- [ ] `bash tests/run_tests.sh` da "15 pasaron, 0 fallaron"
- [ ] `./kelc --symbols tests/ok/full.kel` imprime la tabla con ámbitos, mutabilidad, desplazamientos y valores constantes
- [ ] `./kelc tests/ok/read.kel` compila sin errores
- [ ] Los tres `tests/bad/read_*.kel` fallan **por el motivo correcto** (aridad, redefinición, tipo), no por "función no definida"
- [ ] `src/ir.h` existe, tiene `IRFunction`, `IR_ARRAY_NEW` e `IR_READ`; `src/codegen.h` ya no existe
- [ ] SPEC.md documenta el paso de arreglos por referencia

## Qué queda fuera de este plan

`ir.h` solo define tipos: no hay `ir.c` todavía. Los `read_*` type-chequean pero no generan código. Ambas cosas son deliberadas y llegan en los Planes 2 y 3.
