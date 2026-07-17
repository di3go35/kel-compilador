# Plan 4 — Etapa 5: optimización local (`optimize.c`)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Un optimizador que mejora el TAC sin cambiar el resultado observable del programa. Cierra el criterio 9 de la rúbrica (1.0 pt) y, con la prueba de equivalencia, demuestra que las Etapas 4-6 forman un compilador correcto.

**Architecture:** `kel_optimize(IRProgram*)` reescribe cada `IRFunction` in-place, preservando el formato del IR (misma struct de entrada y salida), de modo que el optimizador es enteramente desconectable: `--emit-c` sin `--opt` produce C válido, solo peor. Los pases trabajan **por bloque básico** (optimización local del dragon book) y se repiten hasta punto fijo. La verificación central es la **prueba de equivalencia**: cada programa de `tests/run/` se compila y ejecuta con y sin `--opt`, y ambas salidas deben ser idénticas byte a byte.

**Tech Stack:** C11, gcc (TDM-GCC), `mingw32-make`, bash.

**Spec:** [design doc](../specs/2026-07-16-unidad-ii-etapas-4-6-design.md) §4 (los cinco pases, alcance por bloques, reglas de seguridad), §6.2 (equivalencia), §6.3 (`tests/opt`).

**Plan anterior:** [Plan 3](2026-07-17-plan-3-etapa-6-emision-c.md) dejó `--emit-c` y `-o` completos, suite 33/33. La Etapa 6 es lo que hace posible este plan: sin ejecutar programas no hay prueba de equivalencia.

---

## Notas para quien ejecute esto

- **`make` no existe en esta máquina.** Solo `mingw32-make` (TDM-GCC en `/c/TDM-GCC-64/bin`). `export PATH="/c/TDM-GCC-64/bin:$PATH"` primero.
- **ASan no funciona** (no hay libasan). La suite es la única red contra fugas.
- **Estado de partida:** `mingw32-make clean && mingw32-make && bash tests/run_tests.sh` → "33 pasaron, 0 fallaron", cero warnings con `-Wall -Wextra -Wpedantic`.
- **La prueba de equivalencia es la red del optimizador.** Un bug de optimizador es silencioso: no da error de compilación ni crash, cambia el resultado o (el caso del `while`) cuelga. La equivalencia lo caza en el acto. Escríbela ANTES que cualquier pase (Task 1), aunque con el optimizador no-op pase trivialmente.
- **CRLF, como siempre:** `kelc.exe` y los ejecutables generados emiten CRLF. El runner normaliza con `tr -d '\r'` los dos lados. Nunca CRLF en un `.present`/`.absent`.
- **Commits: español, imperativo.** Trailer `Co-Authored-By` según la configuración del entorno.
- **Lee `src/ir.h` y `src/ir.c` ENTEROS antes de empezar.** El optimizador manipula las mismas structs que genera `ir.c`; los constructores de `Addr`, `emit`, y el formato de `print_instr` son el contrato.

## Hechos verificados que el optimizador debe respetar

1. **Alcance por bloque básico, no por función** (§4.2). La propagación sobre la función entera rompe `var x = 0; while x < 10 { x = x + 1 }`: propagar `x = 0` al cuerpo produce un bucle infinito silencioso. El entorno de propagación se **vacía en cada frontera de bloque** (una etiqueta `IR_LABEL`, y tras un salto o `IR_RETURN`). La condición del `while` vive en su propio bloque (tras `L_cond`), así que `x` nunca se conoce ahí. Esto es lo que hace la propagación segura.
2. **Los temporales cruzan bloques.** El temporal del cortocircuito (`gen_shortcircuit`, `ir.c:164`) se escribe en un bloque y se lee tras la etiqueta `L`. Por tanto la **eliminación de código muerto NO puede asumir localidad de bloque**: cuenta las lecturas de cada temporal en TODA la función y borra solo los que tienen cero lecturas.
3. **Las variables se asumen vivas al salir del bloque** (conservador). DCE **nunca** borra una escritura a variable (`ADDR_VAR`), solo a temporales. Kel no tiene análisis de vida global; esto es correcto y simple.
4. **Nunca eliminar instrucciones con efectos:** `IR_CALL`, `IR_PRINTLN`, `IR_READ`, `IR_INDEX_STORE`, `IR_RETURN`, saltos y etiquetas referenciadas. DCE solo toca `IR_COPY`/`IR_BINOP`/`IR_UNOP`/`IR_INDEX_LOAD`/`IR_ARRAY_NEW` que escriben un temporal muerto.
5. **Una llamada no invalida variables escalares** (§4.3). Kel no tiene globales ni punteros a escalares, así que `k_f()` no puede tocar las variables del llamador. El entorno de propagación las conserva a través de un `IR_CALL`.
6. **No se rastrean los resultados de `IR_INDEX_LOAD` en el entorno.** Los arreglos se pasan por referencia y una llamada puede mutarlos (§3.4); no registrar los loads en el entorno sortea el problema entero sin análisis extra. Solo se registran las `IR_COPY` (constantes y copias) y los plegados que se convierten en `IR_COPY`.
7. **`IR_INDEX_STORE` lee `dst`, no lo escribe** (`op1[op2] = dst`, ver `ir.h`). Al contar lecturas para DCE, `dst` de un store cuenta como **lectura** de ese temporal. Al invalidar el entorno, un store no escribe `dst`.
8. **No plegar división ni módulo por cero.** `5 / 0` se deja sin plegar: hacerlo o rompería el compilador o produciría un valor falso, y la equivalencia exige que el opt haga lo mismo que el sin-opt (que delega en el runtime). Los flotantes `/0.0` también se dejan sin plegar por uniformidad.
9. **Simplificación algebraica solo sobre enteros.** En flotante `x*0` no es `0` (`NaN*0 = NaN`, `inf*0 = NaN`) y `x+0` puede cambiar el signo de un cero. Las identidades (`x*1`, `x+0`, `x-0`, `x/1`, `x*0`) se aplican solo cuando el tipo del resultado del `IR_BINOP` es `KT_INT`. Es correcto y buen material de defensa.
10. **El temporal de la comparación del `for` tiene `type == NULL`** (`ir.c:457`, hueco conocido de `ir.h`). Es un bool por contexto. El plegado maneja `dst.type` NULL sin caerse (nunca se pliega de todos modos: sus operandos no son constantes en el bloque del bucle).
11. **`--opt` preserva el formato del IR.** La salida de `kel_optimize` es un `IRProgram` que `kel_ir_print` y `kel_emit_c` consumen igual que el no optimizado. Ningún opcode nuevo, ninguna struct nueva. Esto es lo que hace posible la equivalencia.

## Estructura de archivos

| Archivo | Responsabilidad | Acción |
|---------|-----------------|--------|
| `src/optimize.h` | API: `OptStats kel_optimize(IRProgram*);` + struct `OptStats` | Crear |
| `src/optimize.c` | Los cinco pases + plegado de saltos + limpieza, hasta punto fijo | Crear |
| `src/main.c` | flag `--opt`; optimizar el IR antes de imprimir/emitir | Modificar |
| `Makefile` | añadir `src/optimize.c` | Modificar |
| `tests/opt/*.kel` + `.present`/`.absent` | un caso por pase: afirma que el pase disparó | Crear |
| `tests/run_tests.sh` | sección de equivalencia + sección `tests/opt` | Modificar |
| `SPEC.md`, `README.md` | `--opt`, quitar `(pendiente)` de `optimize.c` | Modificar |

---

## Task 1: Infraestructura — `--opt` no-op y la prueba de equivalencia

TDD: la equivalencia existe y pasa con un optimizador que no toca nada (identidad), antes de que ningún pase exista. Es la red que atrapará los bugs de los tasks siguientes.

**Files:** Create `src/optimize.h`, `src/optimize.c`. Modify `src/main.c`, `Makefile`, `tests/run_tests.sh`.

- [ ] **Step 1: `src/optimize.h`**

```c
#ifndef KEL_OPTIMIZE_H
#define KEL_OPTIMIZE_H

#include "ir.h"

/* Etapa 5: optimización local. kel_optimize reescribe el IRProgram in-place
 * preservando su formato (misma struct de entrada y salida), de modo que el
 * optimizador es desconectable: --emit-c sin --opt produce C válido, solo peor.
 * OptStats alimenta --stats (Plan 5); aquí solo se cuenta el antes/después. */
typedef struct {
    size_t instr_before;
    size_t instr_after;
} OptStats;

OptStats kel_optimize(IRProgram* p);

#endif
```

- [ ] **Step 2: `src/optimize.c` — esqueleto no-op**

```c
#include "optimize.h"

#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Etapa 5 — Optimización local (por bloque básico).
 *
 * Los pases (propagación de constantes y copias, plegado, simplificación
 * algebraica, código muerto, plegado de saltos, código inalcanzable, limpieza
 * de etiquetas) llegan en los tasks siguientes. Hoy kel_optimize es la
 * identidad: la prueba de equivalencia debe pasar igual, porque un optimizador
 * que no toca nada no puede cambiar la salida.
 * ============================================================ */

static size_t count_instrs(const IRProgram* p) {
    size_t n = 0;
    for (size_t i = 0; i < p->count; i++) n += p->fns[i].count;
    return n;
}

OptStats kel_optimize(IRProgram* p) {
    OptStats st;
    st.instr_before = count_instrs(p);
    /* pases: tasks 2-6; iteración a punto fijo: task 7 */
    st.instr_after = count_instrs(p);
    return st;
}
```

- [ ] **Step 3: `main.c` — flag `--opt`**

`#include "optimize.h"` junto a los otros includes. En `usage()`, tras la línea de `--ir`:
```c
        "  --opt       Muestra el TAC optimizado (Etapa 5)\n"
```
Variable junto a los otros flags: `int opt_flag = 0;`. Parseo, tras el de `--ir`:
```c
        else if (strcmp(argv[i], "--opt") == 0)     opt_flag = 1;
```
Extender la supresión del resumen con `&& !opt_flag`:
```c
            } else if (!show_tokens && !show_ast && !show_symbols && !show_ir && !emit_c_flag && !out_exe && !opt_flag) {
```
Y el bloque del IR pasa a optimizar antes de imprimir/emitir. Reemplazar:
```c
            if (!sr.had_error && (show_ir || emit_c_flag || out_exe)) {
                IRProgram ir = kel_gen(pr.root);
                if (show_ir)     kel_ir_print(&ir);
                if (emit_c_flag) kel_emit_c(&ir, stdout);
                if (out_exe)     rc = compile_to_exe(&ir, out_exe);
                kel_ir_free(&ir);
            }
```
por:
```c
            if (!sr.had_error && (show_ir || opt_flag || emit_c_flag || out_exe)) {
                IRProgram ir = kel_gen(pr.root);
                if (opt_flag) kel_optimize(&ir);
                /* --opt imprime el TAC optimizado, salvo que se pida C o exe */
                if (show_ir || (opt_flag && !emit_c_flag && !out_exe)) kel_ir_print(&ir);
                if (emit_c_flag) kel_emit_c(&ir, stdout);
                if (out_exe)     rc = compile_to_exe(&ir, out_exe);
                kel_ir_free(&ir);
            }
```
**Verifica que `kel_free_ast` sigue después del bloque.**

- [ ] **Step 4: `Makefile`** — añadir ` src/optimize.c` a la lista de fuentes (`SRC`), junto a `src/emit_c.c`.

- [ ] **Step 5: La sección de equivalencia en el runner**

En `tests/run_tests.sh`, tras la sección de `tests/read` (el `rm -rf "$READ_DIR"`) y antes del bloque "Resultado:", añadir:

```bash
echo
echo "== equivalencia del optimizador (--opt no altera la salida) =="
EQ_DIR="$(mktemp -d)"
for f in tests/run/*.kel; do
    [ -e "$f" ] || continue
    base="$(basename "${f%.kel}")"
    c0="$EQ_DIR/$base.0.c"; e0="$EQ_DIR/$base.0.exe"
    c1="$EQ_DIR/$base.1.c"; e1="$EQ_DIR/$base.1.exe"
    if ! "$KELC" --emit-c "$f" > "$c0" 2>/dev/null \
      || ! "$KELC" --opt --emit-c "$f" > "$c1" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (emisión falló)")
        printf "  FAIL %s (emisión falló)\n" "$f"; continue
    fi
    # El C optimizado también debe compilar limpio con -Werror: si el
    # optimizador deja una etiqueta sin salto, aquí revienta con -Wunused-label.
    if ! gcc -Wall -Wextra -Werror -o "$e0" "$c0" 2>/dev/null \
      || ! gcc -Wall -Wextra -Werror -o "$e1" "$c1" 2> "$EQ_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (el C optimizado no compila)")
        printf "  FAIL %s (gcc rechazó el C optimizado):\n" "$f"
        sed 's/^/    /' "$EQ_DIR/$base.gcc.log"; continue
    fi
    o0=$("$e0" < /dev/null | tr -d '\r'); o1=$("$e1" < /dev/null | tr -d '\r')
    if [ "$o0" = "$o1" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (--opt cambió la salida)")
        printf "  FAIL %s (--opt cambió la salida)\n    --- sin --opt ---\n%s\n    --- con --opt ---\n%s\n" \
               "$f" "$o0" "$o1"
    fi
done
rm -rf "$EQ_DIR"
```

- [ ] **Step 6: Correr y ver que PASA** — `mingw32-make clean && mingw32-make && bash tests/run_tests.sh`. La sección de equivalencia aparece; con el optimizador no-op, `o0 == o1` para los ~7 programas de `tests/run`. Total esperado: **40 pasaron, 0 fallaron** (33 previos + 7 de equivalencia). Cero warnings.

- [ ] **Step 7: Commit**

```bash
git add src/optimize.h src/optimize.c src/main.c Makefile tests/run_tests.sh
git commit -m "Etapa 5: esqueleto de optimize.c, flag --opt y prueba de equivalencia

La equivalencia compila y ejecuta cada programa de tests/run con y sin
--opt y exige salidas idénticas byte a byte: es la propiedad que DEFINE
un optimizador correcto y la única red capaz de cazar sus bugs, que son
silenciosos. Con el optimizador no-op pasa trivialmente; los pases llegan
en los tasks siguientes y la red ya está puesta."
```

---

## Task 2: Bloques básicos + propagación de constantes + plegado

El primer pase real: un recorrido hacia adelante por cada función que mantiene un **entorno** (por ubicación escrita, la constante a la que es igual ahora mismo), vaciándolo en cada frontera de bloque. Reescribe operandos con el entorno (propagación) y pliega los `IR_BINOP` de operandos constantes.

**Files:** `src/optimize.c`, `tests/opt/plegado.kel`, `tests/opt/plegado.present`, `tests/opt/plegado.absent`. Modify `tests/run_tests.sh`.

- [ ] **Step 1: Los tests**

`tests/opt/plegado.kel`:
```kel
fn main() {
  val a = 10 * 10 + 5
  println(a)
}
```
`tests/opt/plegado.present` (una línea; el `100 * ... + 5` debe quedar plegado a `105`):
```
105
```
`tests/opt/plegado.absent` (ninguna de estas subcadenas debe aparecer en `--opt`):
```
10 * 10
```

Razonamiento: sin optimizar, el TAC es `t1 = 10 * 10`, `t2 = t1 + 5`, `a = t2`, `println a`. El plegado convierte `t1 = 100`; la propagación reescribe `t2 = 100 + 5`; el plegado lo cierra en `t2 = 105`; la propagación reescribe `a = 105` y `println 105`. Que `105` aparezca prueba el plegado; que `10 * 10` desaparezca, que no quedó sin plegar. (Los temporales `t1`/`t2` pueden seguir ahí: los borra el DCE del Task 5.)

- [ ] **Step 2: La sección `tests/opt` en el runner**

En `tests/run_tests.sh`, tras la sección de equivalencia (el `rm -rf "$EQ_DIR"`) y antes del "Resultado:", añadir:

```bash
echo
echo "== tests/opt (cada pase dispara sobre --opt) =="
for f in tests/opt/*.kel; do
    [ -e "$f" ] || continue
    base="${f%.kel}"
    got=$("$KELC" --opt "$f" 2>/dev/null | tr -d '\r')
    okc=1; why=""
    if [ -e "$base.present" ]; then
        while IFS= read -r pat; do
            [ -z "$pat" ] && continue
            case "$got" in *"$pat"*) : ;; *) okc=0; why="falta: $pat" ;; esac
        done < "$base.present"
    fi
    if [ -e "$base.absent" ]; then
        while IFS= read -r pat; do
            [ -z "$pat" ] && continue
            case "$got" in *"$pat"*) okc=0; why="sobra: $pat" ;; esac
        done < "$base.absent"
    fi
    if [ "$okc" = 1 ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f ($why)")
        printf "  FAIL %s (%s)\n    --- --opt ---\n%s\n" "$f" "$why" "$got"
    fi
done
```

- [ ] **Step 3: Correr y ver que falla** — con el no-op, `--opt` imprime el TAC sin tocar: `105` no aparece (sigue `10 * 10`) y `plegado.absent` lo caza. Pégalo.

- [ ] **Step 4: Implementar el entorno y el pase local**

En `src/optimize.c`, antes de `kel_optimize`, añadir:

```c
/* ---------- Entorno de propagación local ----------
 * Por cada ubicación escrita (variable o temporal), la Addr a la que es igual
 * ahora mismo. En este task solo constantes; el Task 3 añade copias. */

static int is_const(const Addr* a) {
    return a->kind == ADDR_CONST_INT || a->kind == ADDR_CONST_FLOAT
        || a->kind == ADDR_CONST_BOOL || a->kind == ADDR_CONST_STR;
}

/* Igualdad de UBICACIONES (var por nombre, temporal por id). Las constantes
 * no son ubicaciones: devuelven 0. */
static int addr_same_loc(const Addr* a, const Addr* b) {
    if (a->kind != b->kind) return 0;
    if (a->kind == ADDR_TEMP) return a->i == b->i;
    if (a->kind == ADDR_VAR)  return strcmp(a->s, b->s) == 0;
    return 0;
}

typedef struct { Addr key; Addr val; } EnvEntry;
typedef struct { EnvEntry* v; size_t n, cap; } Env;

static void env_init(Env* e)  { e->v = NULL; e->n = e->cap = 0; }
static void env_free(Env* e)  { free(e->v); e->v = NULL; e->n = e->cap = 0; }
static void env_clear(Env* e) { e->n = 0; }

/* Devuelve el valor asociado a la ubicación `k`, o NULL. */
static const Addr* env_lookup(const Env* e, const Addr* k) {
    if (k->kind != ADDR_VAR && k->kind != ADDR_TEMP) return NULL;
    for (size_t i = 0; i < e->n; i++)
        if (addr_same_loc(&e->v[i].key, k)) return &e->v[i].val;
    return NULL;
}

/* Borra toda entrada cuya CLAVE o cuyo VALOR sea la ubicación `w`: cuando w se
 * reescribe, ni «w vale X» ni «Y vale w» siguen siendo ciertos. */
static void env_invalidate(Env* e, const Addr* w) {
    size_t k = 0;
    for (size_t i = 0; i < e->n; i++) {
        if (addr_same_loc(&e->v[i].key, w) || addr_same_loc(&e->v[i].val, w))
            continue;
        e->v[k++] = e->v[i];
    }
    e->n = k;
}

static void env_set(Env* e, Addr key, Addr val) {
    if (e->n == e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8;
        e->v = (EnvEntry*)realloc(e->v, e->cap * sizeof(EnvEntry));
    }
    e->v[e->n].key = key;
    e->v[e->n].val = val;
    e->n++;
}

/* ---------- Plegado de constantes ----------
 * dst y su tipo se conservan; solo cambia la operación a IR_COPY de la
 * constante. La constante toma el tipo del dst para que la Etapa 6 lo emita
 * bien. */
static void make_copy(Instr* in, Addr c) {
    in->op = IR_COPY;
    in->op1 = c;
    memset(&in->op2, 0, sizeof(in->op2));
    in->sym = NULL;
}
static Addr const_int(long long v, KelType* t)  { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_INT;  a.type=t; a.i=v; return a; }
static Addr const_float(double v, KelType* t)   { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_FLOAT;a.type=t; a.f=v; return a; }
static Addr const_bool(int v, KelType* t)       { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_BOOL; a.type=t; a.b=v; return a; }

static int try_fold(Instr* in) {
    if (in->op != IR_BINOP) return 0;
    const Addr* a = &in->op1; const Addr* b = &in->op2; const char* o = in->sym;
    if (a->kind == ADDR_CONST_INT && b->kind == ADDR_CONST_INT) {
        long long x = a->i, y = b->i;
        if      (strcmp(o,"+")==0) { make_copy(in, const_int(x+y, in->dst.type)); return 1; }
        else if (strcmp(o,"-")==0) { make_copy(in, const_int(x-y, in->dst.type)); return 1; }
        else if (strcmp(o,"*")==0) { make_copy(in, const_int(x*y, in->dst.type)); return 1; }
        else if (strcmp(o,"/")==0) { if (y==0) return 0; make_copy(in, const_int(x/y, in->dst.type)); return 1; }
        else if (strcmp(o,"%")==0) { if (y==0) return 0; make_copy(in, const_int(x%y, in->dst.type)); return 1; }
        else {
            int c;
            if      (strcmp(o,"<")==0)  c = x <  y;
            else if (strcmp(o,">")==0)  c = x >  y;
            else if (strcmp(o,"<=")==0) c = x <= y;
            else if (strcmp(o,">=")==0) c = x >= y;
            else if (strcmp(o,"==")==0) c = x == y;
            else if (strcmp(o,"!=")==0) c = x != y;
            else return 0;
            make_copy(in, const_bool(c, in->dst.type)); return 1;
        }
    }
    if (a->kind == ADDR_CONST_FLOAT && b->kind == ADDR_CONST_FLOAT) {
        double x = a->f, y = b->f;
        if      (strcmp(o,"+")==0) { make_copy(in, const_float(x+y, in->dst.type)); return 1; }
        else if (strcmp(o,"-")==0) { make_copy(in, const_float(x-y, in->dst.type)); return 1; }
        else if (strcmp(o,"*")==0) { make_copy(in, const_float(x*y, in->dst.type)); return 1; }
        else if (strcmp(o,"/")==0) { if (y==0.0) return 0; make_copy(in, const_float(x/y, in->dst.type)); return 1; }
        else {
            int c;
            if      (strcmp(o,"<")==0)  c = x <  y;
            else if (strcmp(o,">")==0)  c = x >  y;
            else if (strcmp(o,"<=")==0) c = x <= y;
            else if (strcmp(o,">=")==0) c = x >= y;
            else if (strcmp(o,"==")==0) c = x == y;
            else if (strcmp(o,"!=")==0) c = x != y;
            else return 0;
            make_copy(in, const_bool(c, in->dst.type)); return 1;
        }
    }
    return 0;
}

/* ¿Escribe esta instrucción una ubicación (dst como DESTINO)? Devuelve la Addr
 * escrita o NULL. OJO: IR_INDEX_STORE NO escribe dst (dst es su fuente, ver
 * ir.h), y IR_CALL void tampoco. */
static const Addr* written_loc(const Instr* in) {
    switch (in->op) {
        case IR_COPY: case IR_BINOP: case IR_UNOP:
        case IR_ARRAY_NEW: case IR_INDEX_LOAD: case IR_READ:
            return &in->dst;
        case IR_CALL:
            return in->dst.kind == ADDR_NONE ? NULL : &in->dst;
        default:
            return NULL;
    }
}

/* Reescribe un operando fuente con el entorno. Devuelve 1 si cambió. */
static int rewrite_operand(const Env* e, Addr* op) {
    const Addr* v = env_lookup(e, op);
    if (!v) return 0;
    KelType* keep = op->type;      /* conserva el tipo del operando original */
    *op = *v;
    if (!op->type) op->type = keep;
    return 1;
}

/* Actualiza el entorno según el efecto de la instrucción ya reescrita. */
static void update_env(Env* e, const Instr* in) {
    const Addr* w = written_loc(in);
    if (w) env_invalidate(e, w);
    /* Solo se registran copias de CONSTANTE en este task (Task 3 añade copias
     * de ubicación). Un plegado ya dejó la instrucción como IR_COPY de const. */
    if (in->op == IR_COPY && is_const(&in->op1))
        env_set(e, in->dst, in->op1);
}

static int is_block_end(IROp op) {
    return op == IR_GOTO || op == IR_IF_GOTO
        || op == IR_IF_FALSE_GOTO || op == IR_RETURN;
}

/* Un recorrido hacia adelante: reescribe operandos, pliega, actualiza entorno,
 * y vacía el entorno en cada frontera de bloque. Devuelve 1 si algo cambió. */
static int local_pass(IRFunction* f) {
    int changed = 0;
    Env e; env_init(&e);
    for (size_t j = 0; j < f->count; j++) {
        Instr* in = &f->body[j];
        if (in->op == IR_LABEL) { env_clear(&e); continue; }   /* nueva frontera */
        changed |= rewrite_operand(&e, &in->op1);
        changed |= rewrite_operand(&e, &in->op2);
        changed |= try_fold(in);
        update_env(&e, in);
        if (is_block_end(in->op)) env_clear(&e);
    }
    env_free(&e);
    return changed;
}
```

Y `kel_optimize` pasa a llamar al pase (una vez por ahora; el punto fijo llega en el Task 7):
```c
OptStats kel_optimize(IRProgram* p) {
    OptStats st;
    st.instr_before = count_instrs(p);
    for (size_t i = 0; i < p->count; i++)
        local_pass(&p->fns[i]);
    st.instr_after = count_instrs(p);
    return st;
}
```

- [ ] **Step 5: Verificar** — `plegado` pasa (present `105`, absent `10 * 10`). La equivalencia sigue verde: propagar y plegar constantes no cambia resultados. Cero warnings. Total: **42** (40 + 1 equivalencia ya contada + 1 opt... en realidad 40 previos + `plegado` = 41). Cuenta la que reporte el runner; lo que importa es 0 fallidos.

- [ ] **Step 6: Commit**

```bash
git add src/optimize.c tests/opt/plegado.kel tests/opt/plegado.present tests/opt/plegado.absent tests/run_tests.sh
git commit -m "Etapa 5: propagación de constantes y plegado, por bloque básico

El entorno se vacía en cada etiqueta y tras cada salto: por eso propagar
x=0 no entra al cuerpo de un while (su condición vive en otro bloque) y
no cuelga el programa. Plegar 10*10+5 a 105 sin tocar la salida lo
confirma la prueba de equivalencia."
```

---

## Task 3: Propagación de copias

Extensión de una línea del entorno: registrar también copias de ubicación (`t1 = a`), no solo de constante. Con eso `t1 = a; t2 = t1 + 1` se reescribe a `t2 = a + 1`.

**Files:** `src/optimize.c`, `tests/opt/copias.kel`, `tests/opt/copias.present`, `tests/opt/copias.absent`

- [ ] **Step 1: Los tests**

`tests/opt/copias.kel`:
```kel
fn ident(x: int) -> int {
  val y = x
  return y
}

fn main() {
  println(ident(7))
}
```
`tests/opt/copias.present`:
```
return x
```
`tests/opt/copias.absent`:
```
return y
```

Razonamiento: en `ident`, `y = x` es una copia de ubicación (`x` no es constante). La propagación de copias reescribe `return y` a `return x`. Que aparezca `return x` y no `return y` prueba el pase. (`y = x` puede seguir: `y` es variable y no se elimina.)

- [ ] **Step 2: Correr y ver que falla** — sin la extensión, `env_set` solo guarda constantes, así que `y` no se propaga y `--opt` imprime `return y`. `copias.absent` lo caza. Pégalo.

- [ ] **Step 3: Implementar** — en `update_env`, ampliar la condición para registrar copias de cualquier ubicación (constante O variable/temporal):

```c
static void update_env(Env* e, const Instr* in) {
    const Addr* w = written_loc(in);
    if (w) env_invalidate(e, w);
    /* Registra cualquier copia: de constante (propagación de constantes) o de
     * otra ubicación (propagación de copias). Un plegado ya dejó la instrucción
     * como IR_COPY. No se registran IR_INDEX_LOAD/READ/CALL: valor desconocido
     * (y los loads de arreglo, mutables por una llamada, no deben propagarse). */
    if (in->op == IR_COPY && (is_const(&in->op1)
            || in->op1.kind == ADDR_VAR || in->op1.kind == ADDR_TEMP))
        env_set(e, in->dst, in->op1);
}
```

- [ ] **Step 4: Verificar** — `copias` pasa (present `return x`, absent `return y`). Equivalencia verde. Cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/optimize.c tests/opt/copias.kel tests/opt/copias.present tests/opt/copias.absent
git commit -m "Etapa 5: propagación de copias

Registrar en el entorno cualquier copia, no solo la de constante: t1=a
hace que t2=t1+1 se reescriba a t2=a+1. Es la misma maquinaria que la
propagación de constantes; solo cambia qué se guarda. return y -> return x."
```

---

## Task 4: Simplificación algebraica

`x*1`→`x`, `x+0`→`x`, `x*0`→`0`, `x/1`→`x`, `x-0`→`x`. **Solo enteros** (§ hecho verificado 9: en flotante `x*0` no es `0`).

**Files:** `src/optimize.c`, `tests/opt/algebra.kel`, `tests/opt/algebra.present`, `tests/opt/algebra.absent`

- [ ] **Step 1: Los tests**

`tests/opt/algebra.kel`:
```kel
fn escala(x: int) -> int {
  return x * 1 + 0
}

fn main() {
  println(escala(9))
}
```
`tests/opt/algebra.present`:
```
return x
```
`tests/opt/algebra.absent`:
```
* 1
```

Razonamiento: `t1 = x * 1` → `t1 = x`; propagación → `t2 = x + 0`; `x + 0` → `t2 = x`; propagación → `return x`. Que `* 1` desaparezca prueba la simplificación.

- [ ] **Step 2: Correr y ver que falla** — sin el pase, `x * 1` sobrevive y `algebra.absent` lo caza. Pégalo.

- [ ] **Step 3: Implementar** — añadir `try_algebraic` antes de `local_pass`, y llamarlo en el recorrido cuando el plegado no aplicó:

```c
/* Copia dst = src, conservando dst. src puede ser una ubicación o constante. */
static void make_copy_src(Instr* in, Addr src) { make_copy(in, src); }

/* Identidades algebraicas, SOLO sobre enteros: en flotante x*0 != 0 (NaN*0) y
 * x+0 puede cambiar el signo del cero. El tipo del resultado del BINOP manda. */
static int try_algebraic(Instr* in) {
    if (in->op != IR_BINOP) return 0;
    if (!(in->dst.type && in->dst.type->kind == KT_INT)) return 0;
    const char* o = in->sym;
    Addr a = in->op1, b = in->op2;
    int a0 = (a.kind == ADDR_CONST_INT && a.i == 0);
    int a1 = (a.kind == ADDR_CONST_INT && a.i == 1);
    int b0 = (b.kind == ADDR_CONST_INT && b.i == 0);
    int b1 = (b.kind == ADDR_CONST_INT && b.i == 1);
    if (strcmp(o, "+") == 0) {
        if (b0) { make_copy_src(in, a); return 1; }
        if (a0) { make_copy_src(in, b); return 1; }
    } else if (strcmp(o, "-") == 0) {
        if (b0) { make_copy_src(in, a); return 1; }
    } else if (strcmp(o, "*") == 0) {
        if (b1) { make_copy_src(in, a); return 1; }
        if (a1) { make_copy_src(in, b); return 1; }
        if (b0 || a0) { make_copy(in, const_int(0, in->dst.type)); return 1; }
    } else if (strcmp(o, "/") == 0) {
        if (b1) { make_copy_src(in, a); return 1; }
    }
    return 0;
}
```

En `local_pass`, sustituir `changed |= try_fold(in);` por:
```c
        if (try_fold(in)) changed = 1;
        else if (try_algebraic(in)) changed = 1;
```

- [ ] **Step 4: Verificar** — `algebra` pasa. Equivalencia verde (las identidades son exactas en entero). Cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/optimize.c tests/opt/algebra.kel tests/opt/algebra.present tests/opt/algebra.absent
git commit -m "Etapa 5: simplificación algebraica (solo enteros)

x*1, x+0, x-0, x/1 -> x ; x*0 -> 0. Restringido a enteros a propósito:
en flotante x*0 es NaN cuando x es NaN/inf, y x+0 cambia el signo de un
cero. La restricción es correcta y es buen material de defensa."
```

---

## Task 5: Eliminación de código muerto

Un temporal escrito y nunca leído en TODA la función (los temporales cruzan bloques, § hecho 2) y sin efectos secundarios (§ hecho 4) se borra. Las variables no se tocan (§ hecho 3).

**Files:** `src/optimize.c`, `tests/opt/muerto.kel`, `tests/opt/muerto.present`, `tests/opt/muerto.absent`

- [ ] **Step 1: Los tests**

`tests/opt/muerto.kel`:
```kel
fn main() {
  val a = 5 + 8
  println(a)
}
```
`tests/opt/muerto.present`:
```
println 13
```
`tests/opt/muerto.absent`:
```
t1
```

Razonamiento: `t1 = 5 + 8` → `t1 = 13`; propagación → `a = 13`, `println 13`. Tras la propagación **nadie lee `t1`** → el DCE lo borra. Que ninguna `t1` sobreviva prueba el pase. (`a = 13` se queda: `a` es variable.)

- [ ] **Step 2: Correr y ver que falla** — sin DCE, `t1 = 13` sigue en la salida y `muerto.absent` lo caza. Pégalo.

- [ ] **Step 3: Implementar** — añadir `dce_pass` antes de `kel_optimize` y llamarlo tras `local_pass`:

```c
/* Cuenta lecturas de cada temporal (id 1..n_temps). op1 y op2 siempre son
 * fuentes; en IR_INDEX_STORE, dst TAMBIÉN es fuente (op1[op2] = dst, ver ir.h). */
static void count_temp_reads(const Instr* in, unsigned* reads, size_t nt) {
    const Addr* srcs[3]; int k = 0;
    srcs[k++] = &in->op1;
    srcs[k++] = &in->op2;
    if (in->op == IR_INDEX_STORE) srcs[k++] = &in->dst;
    for (int s = 0; s < k; s++)
        if (srcs[s]->kind == ADDR_TEMP && srcs[s]->i >= 1
                && (size_t)srcs[s]->i <= nt)
            reads[srcs[s]->i]++;
}

/* ¿Escribe un temporal muerto y sin efectos? Solo estas cinco lo son: CALL y
 * READ escriben pero tienen efectos, así que se conservan aunque su dst no se
 * lea. */
static int is_dead_temp_write(const Instr* in, const unsigned* reads, size_t nt) {
    if (in->dst.kind != ADDR_TEMP) return 0;
    if (in->dst.i < 1 || (size_t)in->dst.i > nt) return 0;
    if (reads[in->dst.i] != 0) return 0;
    switch (in->op) {
        case IR_COPY: case IR_BINOP: case IR_UNOP:
        case IR_INDEX_LOAD: case IR_ARRAY_NEW: return 1;
        default: return 0;
    }
}

static int dce_pass(IRFunction* f) {
    size_t nt = f->n_temps;
    unsigned* reads = (unsigned*)calloc(nt + 1, sizeof(unsigned));
    for (size_t j = 0; j < f->count; j++)
        count_temp_reads(&f->body[j], reads, nt);
    size_t w = 0; int changed = 0;
    for (size_t j = 0; j < f->count; j++) {
        if (is_dead_temp_write(&f->body[j], reads, nt)) { changed = 1; continue; }
        f->body[w++] = f->body[j];
    }
    f->count = w;
    free(reads);
    return changed;
}
```

En `kel_optimize`, tras `local_pass`:
```c
    for (size_t i = 0; i < p->count; i++) {
        local_pass(&p->fns[i]);
        dce_pass(&p->fns[i]);
    }
```

- [ ] **Step 4: Verificar** — `muerto` pasa (no queda `t1`). `plegado` sigue verde (el DCE no rompe lo anterior). Equivalencia verde: borrar un temporal muerto no cambia la salida. Cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/optimize.c tests/opt/muerto.kel tests/opt/muerto.present tests/opt/muerto.absent
git commit -m "Etapa 5: eliminación de código muerto

Un temporal sin lecturas en toda la función y sin efectos se borra. Se
cuenta por función, no por bloque, porque el temporal del cortocircuito
cruza su etiqueta. Las variables no se tocan (se asumen vivas al salir
del bloque); CALL y READ se conservan aunque su dst no se lea."
```

---

## Task 6: Plegado de saltos, código inalcanzable y limpieza de etiquetas

Cuando la condición de un salto es una constante (tras plegar), el salto se resuelve: `ifFalse false goto L` → `goto L`; `ifFalse true goto L` → se elimina. Eso deja código inalcanzable tras los saltos incondicionales y etiquetas sin ningún salto que las apunte, que se limpian — esto último es lo que impide que `-Wunused-label` tumbe el C optimizado bajo `-Werror` (§5.4.2).

**Files:** `src/optimize.c`, `tests/opt/rama.kel`, `tests/opt/rama.present`, `tests/opt/rama.absent`

- [ ] **Step 1: Los tests**

`tests/opt/rama.kel`:
```kel
fn main() {
  if 2 > 1 {
    println("si")
  } else {
    println("no")
  }
}
```
`tests/opt/rama.present`:
```
si
```
`tests/opt/rama.absent`:
```
no
```
(Segunda línea de `rama.absent`, en el mismo archivo:)
```
ifFalse
```

Razonamiento: `t1 = 2 > 1` → `t1 = true`; `ifFalse t1 goto L_else` → `ifFalse true goto L_else` → se elimina (nunca salta). Queda `println "si"; goto L_end; L_else:; println "no"; L_end:`. `L_else` ya no lo apunta nadie → se borra; entonces `println "no"` queda tras `goto L_end` sin etiqueta delante → inalcanzable → se borra. Resultado: solo se imprime `si`. Que no quede ningún `ifFalse` ni el `println "no"` prueba los tres pases; la equivalencia confirma que el programa sigue imprimiendo `si`.

- [ ] **Step 2: Correr y ver que falla** — sin estos pases, `ifFalse` y `println "no"` siguen en `--opt`. Pégalo.

- [ ] **Step 3: Implementar** — añadir los tres pases antes de `kel_optimize`:

```c
/* Plegado de saltos: si la condición ya es una constante booleana, el salto se
 * resuelve. Reconstruye el cuerpo porque un salto que nunca se toma se elimina.
 * Seguro con la localidad de bloque: una condición solo es constante si su
 * cálculo cayó en el mismo bloque (un while con variable nunca llega aquí). */
static int branch_fold_pass(IRFunction* f) {
    int changed = 0; size_t w = 0;
    for (size_t j = 0; j < f->count; j++) {
        Instr* in = &f->body[j];
        if ((in->op == IR_IF_GOTO || in->op == IR_IF_FALSE_GOTO)
                && in->op1.kind == ADDR_CONST_BOOL) {
            int taken = (in->op == IR_IF_GOTO) ? in->op1.b : !in->op1.b;
            changed = 1;
            if (taken) {                 /* siempre salta -> goto incondicional */
                Instr g; memset(&g, 0, sizeof g);
                g.op = IR_GOTO; g.op1 = in->op2;
                f->body[w++] = g;
            }                            /* nunca salta -> se descarta */
            continue;
        }
        f->body[w++] = *in;
    }
    f->count = w;
    return changed;
}

/* Código inalcanzable: tras un goto o return incondicional, todo hasta la
 * siguiente etiqueta es inalcanzable. Una etiqueta reinicia la alcanzabilidad
 * (puede ser destino de otro salto). */
static int unreachable_pass(IRFunction* f) {
    int changed = 0; size_t w = 0; int dead = 0;
    for (size_t j = 0; j < f->count; j++) {
        Instr* in = &f->body[j];
        if (in->op == IR_LABEL) dead = 0;
        if (dead) { changed = 1; continue; }
        f->body[w++] = *in;
        if (in->op == IR_GOTO || in->op == IR_RETURN) dead = 1;
    }
    f->count = w;
    return changed;
}

/* Etiquetas sin ningún salto que las apunte: se borran. No ahorra trabajo en
 * ejecución; existe para que el C generado no dispare -Wunused-label con
 * -Werror (§5.4.2). */
static int label_cleanup_pass(IRFunction* f) {
    unsigned char* ref = (unsigned char*)calloc(f->n_labels + 1, 1);
    for (size_t j = 0; j < f->count; j++) {
        const Instr* in = &f->body[j];
        if (in->op == IR_GOTO && in->op1.kind == ADDR_LABEL
                && in->op1.i >= 1 && (size_t)in->op1.i <= f->n_labels)
            ref[in->op1.i] = 1;
        if ((in->op == IR_IF_GOTO || in->op == IR_IF_FALSE_GOTO)
                && in->op2.kind == ADDR_LABEL
                && in->op2.i >= 1 && (size_t)in->op2.i <= f->n_labels)
            ref[in->op2.i] = 1;
    }
    int changed = 0; size_t w = 0;
    for (size_t j = 0; j < f->count; j++) {
        const Instr* in = &f->body[j];
        if (in->op == IR_LABEL && in->dst.i >= 1
                && (size_t)in->dst.i <= f->n_labels && !ref[in->dst.i]) {
            changed = 1; continue;
        }
        f->body[w++] = *in;
    }
    f->count = w;
    free(ref);
    return changed;
}
```

En `kel_optimize`, añadir los tres tras `local_pass` y antes de `dce_pass`.
**El orden importa: `label_cleanup_pass` va ANTES que `unreachable_pass`.** Al
plegar `if 2 > 1`, `branch_fold` borra el `ifFalse`; entonces `L_else` queda sin
referencia y `label_cleanup` lo borra; solo entonces `println "no"` queda tras
`goto L_end` sin etiqueta delante y `unreachable` puede eliminarlo. Con el orden
inverso, `println "no"` sobreviviría a esta única pasada (y `rama` fallaría hasta
que el punto fijo del Task 7 lo alcanzara):
```c
    for (size_t i = 0; i < p->count; i++) {
        IRFunction* f = &p->fns[i];
        local_pass(f);
        branch_fold_pass(f);
        label_cleanup_pass(f);   /* antes que unreachable: ver arriba */
        unreachable_pass(f);
        dce_pass(f);
    }
```

- [ ] **Step 4: Verificar** — `rama` pasa. La equivalencia sigue verde y, sobre todo, **el C optimizado compila con `-Werror`**: si `label_cleanup_pass` fallara, una etiqueta huérfana dispararía `-Wunused-label` aquí. Cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/optimize.c tests/opt/rama.kel tests/opt/rama.present tests/opt/rama.absent
git commit -m "Etapa 5: plegado de saltos, código inalcanzable y etiquetas huérfanas

Una condición constante (if 2>1) resuelve el salto; eso deja código
inalcanzable y etiquetas sin referencia, que se limpian. La limpieza de
etiquetas no ahorra ejecución: existe para que el C optimizado no dispare
-Wunused-label bajo -Werror, cosa que la prueba de equivalencia verifica."
```

---

## Task 7: Punto fijo, `OptStats` y cierre

Los pases se habilitan entre sí: un plegado abre una propagación que abre otro plegado. Se repiten hasta que ninguno cambia nada, con un tope de iteraciones.

**Files:** `src/optimize.c`, `SPEC.md`, `README.md`

- [ ] **Step 1: El test — cascada a punto fijo**

`tests/opt/cascada.kel`:
```kel
fn main() {
  val a = 2 + 3
  val b = a * 10
  val c = b - 50
  println(c)
}
```
`tests/opt/cascada.present`:
```
println 0
```
`tests/opt/cascada.absent`:
```
t1
```

Razonamiento: requiere varias vueltas. `a = 5`, propagar → `b = 5 * 10` → `b = 50`, propagar → `c = 50 - 50` → `c = 0`, propagar → `println 0`, y DCE borra los temporales. Con una sola pasada de `local_pass` no todo se cierra (la propagación de `b` necesita que `a` ya se haya plegado en la misma pasada — cae bien por el orden, pero la cadena `a→b→c` a través de tres declaraciones se cierra con seguridad solo iterando). El punto fijo lo garantiza.

- [ ] **Step 2: Correr y ver el estado** — con una sola pasada puede que `cascada` ya pase o no según el orden; da igual, el objetivo del task es el bucle. Pégalo tal cual salga.

- [ ] **Step 3: Implementar el punto fijo** — reescribir `kel_optimize`:

```c
#define OPT_MAX_ITERS 50

OptStats kel_optimize(IRProgram* p) {
    OptStats st;
    st.instr_before = count_instrs(p);
    for (size_t i = 0; i < p->count; i++) {
        IRFunction* f = &p->fns[i];
        int changed = 1, iters = 0;
        /* Hasta punto fijo: un pase habilita a otro. El tope garantiza
         * terminación aunque un pase futuro oscile. */
        while (changed && iters++ < OPT_MAX_ITERS) {
            changed = 0;
            changed |= local_pass(f);
            changed |= branch_fold_pass(f);
            changed |= label_cleanup_pass(f);   /* antes que unreachable (Task 6) */
            changed |= unreachable_pass(f);
            changed |= dce_pass(f);
        }
    }
    st.instr_after = count_instrs(p);
    return st;
}
```

- [ ] **Step 4: Verificar** — `cascada` pasa (present `println 0`). Toda la suite verde, cero warnings. Corre además el barrido de equivalencia mental: `st.instr_after <= st.instr_before` siempre.

- [ ] **Step 5: Documentación**

- `SPEC.md`: quitar `(pendiente)` de la línea de `optimize.h/ optimize.c`. En el bloque de uso, tras `--emit-c`:
  ```
  ./kelc --opt programa.kel     # TAC optimizado (Etapa 5)
  ```
- `README.md` "## Estado": la Etapa 5 pasa a hecha; ya solo queda el Plan 5 (`--stats`, `GRAMMAR.md`, `AUTOMATA.md`). En el bloque de uso, añadir la línea de `--opt`.
- Verificar la invariante: `grep -c pendiente SPEC.md` debe dar **0** (era 1, la de `optimize.*`).

- [ ] **Step 6: Verificación final del plan**

`mingw32-make clean && mingw32-make && bash tests/run_tests.sh` → cero warnings; **todos los tests pasan**, incluidas las secciones de equivalencia (7) y `tests/opt` (6: plegado, copias, algebra, muerto, rama, cascada). Y el argumento de la exposición: `./kelc --ir tests/run/full.kel` vs `./kelc --opt tests/run/full.kel` lado a lado muestra la reducción.

- [ ] **Step 7: Commit**

```bash
git add src/optimize.c tests/opt/cascada.kel tests/opt/cascada.present tests/opt/cascada.absent SPEC.md README.md
git commit -m "Cerrar el Plan 4: optimización a punto fijo

Los pases se repiten hasta que ninguno cambia nada: un plegado abre una
propagación que abre otro plegado (2+3 -> 5, 5*10 -> 50, 50-50 -> 0). El
tope de iteraciones garantiza terminación. Cierra el criterio 9 de la
rúbrica; la prueba de equivalencia demuestra que las Etapas 4-6 forman
un compilador correcto."
```

---

## Criterio de terminado

- [ ] `mingw32-make clean && mingw32-make` sin warnings (`-Wall -Wextra -Wpedantic`)
- [ ] `bash tests/run_tests.sh` → todos pasan, incluidas las secciones de equivalencia y `tests/opt`
- [ ] **Equivalencia**: cada programa de `tests/run/` da salida idéntica con y sin `--opt`, y el C optimizado compila con `gcc -Wall -Wextra -Werror` (lo impone el runner)
- [ ] Los seis `tests/opt/` afirman cada pase: plegado, copias, algebra, muerto, rama, cascada
- [ ] La propagación es local al bloque: `var x = 0; while x < 10 { x = x + 1 }` NO se cuelga con `--opt` (añádelo a `tests/run/` si quieres una prueba explícita del caso del §4.2)
- [ ] DCE cuenta lecturas por función (no por bloque) y nunca borra escrituras a variable, `IR_CALL` ni `IR_READ`
- [ ] No se pliega división/módulo por cero; la simplificación algebraica es solo entera
- [ ] SPEC/README: `optimize.c` sin `(pendiente)`; `grep -c pendiente SPEC.md` = 0

## Qué queda fuera

El Plan 5: `--stats` (que consume `OptStats` y el resto de contadores), `docs/GRAMMAR.md` y `docs/AUTOMATA.md` (con el MCP de draw.io). La optimización aquí es **local** (por bloque básico) a propósito: la global exige análisis de flujo de datos, fuera del alcance del curso (§4.2). No hay asignación de registros ni reordenamiento de instrucciones — el C emitido sigue delegando eso en gcc.

## Concerns conocidos que hay que reportar al terminar

- **La prueba de equivalencia solo cubre `tests/run/` (sin stdin).** Los programas con `read_*` (`tests/read/`) no entran en la equivalencia porque necesitarían fijar su stdin; si se quisiera, se replica la sección alimentando `< "$sin"`. Reportar si se hizo o no.
- **El tope `OPT_MAX_ITERS = 50`** es un cinturón de seguridad, no una necesidad: los pases actuales solo quitan trabajo, así que convergen. Si un pase futuro oscilara, el tope corta y el resultado sigue siendo correcto (solo menos optimizado).
- **`--opt` combinado con `--emit-c`/`-o`** optimiza antes de emitir; `--opt` a secas imprime el TAC optimizado. Verificar que `--ir --opt` no imprime dos veces (la condición de impresión lo evita, pero conviene comprobarlo a mano).
- **La simplificación algebraica y el plegado dejan `IR_COPY` a variables que el DCE no borra** (variables asumidas vivas). Es el antes/después esperado; el C optimizado tendrá algún `k_a = 13;` sin uso posterior, ya marcado `__attribute__((unused))` desde el Plan 3, así que no rompe `-Werror`.
```
