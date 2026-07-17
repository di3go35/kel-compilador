# Plan 3 — Etapa 6: emisión de C y ejecutables (`emit_c.c`)

> **For agentic workers:** REQUIRED SUB-SKILL: Use superpowers:subagent-driven-development (recommended) or superpowers:executing-plans to implement this plan task-by-task. Steps use checkbox (`- [ ]`) syntax for tracking.

**Goal:** Del TAC a un `.c` autocontenido que compila limpio con `gcc -Wall -Wextra -Werror`, y de ahí a un ejecutable con `-o`. Cierra el criterio 10 de la rúbrica (2.0 pts) y habilita la demo estrella: escribir Kel → sale un `.exe` que corre.

**Architecture:** `kel_emit_c(IRProgram*, FILE*)` recorre cada `IRFunction` y emite una función de C: temporales declarados al inicio, etiquetas → etiquetas de C, `goto` → `goto`. El runtime (concat, lectura por líneas sin `scanf`) va inline en la cabecera del `.c` — un solo archivo, sin includes propios. La verificación es **end-to-end**: cada test compila el `.c` con gcc, ejecuta el binario y compara stdout.

**Tech Stack:** C11, gcc (TDM-GCC), `mingw32-make`, bash.

**Spec:** [design doc](../specs/2026-07-16-unidad-ii-etapas-4-6-design.md) §5.4 (emisión, runtime, etiquetas, temporales), §5.5 (CLI), §6.1/6.4/6.5 (tests e2e, de entrada, y Windows).

**Plan anterior:** [Plan 2](2026-07-17-plan-2-etapa-4-codigo-intermedio.md) dejó `--ir` completo, 23/23.

---

## Notas para quien ejecute esto

- **`make` no existe en esta máquina.** Solo `mingw32-make` (TDM-GCC en `/c/TDM-GCC-64/bin`). `export PATH="/c/TDM-GCC-64/bin:$PATH"` primero.
- **ASan no funciona** (no hay libasan). La suite es la única red.
- **Estado de partida:** `mingw32-make clean && mingw32-make && bash tests/run_tests.sh` → "23 pasaron, 0 fallaron", cero warnings con `-Wall -Wextra -Wpedantic`.
- **CRLF, ahora por partida doble:** `kelc.exe` emite CRLF **y los ejecutables generados también** (msvcrt abre stdout en modo texto). El runner normaliza con `tr -d '\r'` los dos lados, siempre. Nunca CRLF en un `.expected` (`.gitattributes` lo revertiría).
- **Commits: español, imperativo.**
- **Lee `src/ir.h` ENTERO antes de empezar.** Es el contrato: casos NULL de `Addr.type`, `IR_INDEX_STORE` con `dst` como fuente, ids como identidad y no orden, el HUECO del `for`, y el aviso de colisión `t1`.

## Hechos verificados que el emisor debe respetar

1. **`==`/`!=` aceptan strings** (`semantic.c:343`: cualquier par de tipos iguales). En C, `==` sobre `char*` compara punteros → **hay que emitir `strcmp(a,b) == 0`** cuando `op1.type` es string. Para arreglos, comparar punteros **es** lo correcto: son referencias (SPEC).
2. **`+` sobre strings** → `kel_concat` (malloc que nunca se libera — limitación aceptada, SPEC).
3. **Colisión de nombres**: `t1`, `register`, `switch` son identificadores legales en Kel y reservados/conflictivos en C. Solución: **prefijo `k_` a toda variable y función de usuario** (`main` se queda `main`). Con eso los temporales `t1..` y el runtime `kel_*` no pueden chocar. Resuelve el AVISO de `ir.h`.
4. **Constantes float en C fuente**: `%.17g` de `1.0` da `1` → `t = 1 / 2` sería división entera **en el C generado**. Igual que `print_float` de `ir.c`: si el texto no tiene `. e E n N`, añadir `.0`.
5. **`Addr.type` PUEDE SER NULL** (6 casos en `ir.h`). El temporal de la comparación del `for` se declara `long long` (una comparación de C cabe ahí) y su BINOP va por la rama por defecto.
6. **`IR_ARRAY_NEW` de tamaño 0** (`val v: [int] = []`): `T t[0]` es una extensión de gcc; emitir `T t[1]` cuando n==0 (documentado con comentario en el emisor).
7. **Etiquetas: SIEMPRE `L1: ;`** (§5.4.2). `L1: }` viola C < C23 y `tests/ok/full.kel` lo dispara garantizado (while en última posición). Sin condicionales: el `;` es gratis. Sin optimizador todavía, toda etiqueta emitida tiene su salto, así que no hay `-Wunused-label` — eso llega con el Plan 4.
8. **Temporales inicializados a cero** (§5.4.3): `-Wmaybe-uninitialized` da falsos positivos alrededor de `goto`. El init es un dead store que gcc elimina al optimizar.
9. **Strings del AST pueden traer bytes de escape**: al emitirlas como literal de C, escapar `\\`, `"`, `\n`, `\r`, `\t` byte a byte.
10. **Convenio de llamadas** (comentario en `gen_call`): `call f, n` consume los n `param` más recientes. En el emisor, una **pila de `Addr`**: cada `IR_PARAM` apila, cada `IR_CALL` desapila n y los imprime en orden. Las llamadas anidadas funcionan solas (la interna vacía su parte de la pila antes de que la externa apile).
11. **`main` de Kel es void → `int main(void)` en C**: todo `return` sin valor dentro de `main` se emite `return 0;`, y al final del cuerpo de `main` se añade `return 0;`.
12. **El IR debe sobrevivir a la emisión**: en `main.c`, gen → (print | emit | compilar) → `kel_ir_free` → `kel_free_ast`, en ese orden. Ver el bloque actual de `--ir`.

## Estructura de archivos

| Archivo | Responsabilidad | Acción |
|---------|-----------------|--------|
| `src/emit_c.h` | API: `void kel_emit_c(const IRProgram*, FILE*);` | Crear |
| `src/emit_c.c` | IR → texto C (runtime inline + funciones) | Crear |
| `src/main.c` | `--emit-c`, `-o <exe>`; reestructurar el bloque del IR | Modificar |
| `Makefile` | añadir `src/emit_c.c` | Modificar |
| `tests/run/*.kel` + `.expected` | e2e: kelc → gcc → ejecutar → stdout | Crear |
| `tests/read/*.kel` + `.stdin` + `.expected` | e2e con entrada estándar | Crear |
| `tests/run_tests.sh` | secciones e2e (run y read) | Modificar |
| `SPEC.md`, `README.md` | `--emit-c`, `-o`, quitar `(pendiente)` | Modificar |

---

## Task 1: Infraestructura e2e + esqueleto de `emit_c.c` — test primero

TDD: el runner y el primer e2e existen y FALLAN antes que el emisor.

**Files:** Create `src/emit_c.h`, `src/emit_c.c`, `tests/run/vacio.kel`, `tests/run/vacio.expected`. Modify `tests/run_tests.sh`, `src/main.c`, `Makefile`.

- [ ] **Step 1: El test**

`tests/run/vacio.kel`:
```kel
fn main() {
}
```
`tests/run/vacio.expected`: archivo **vacío** (0 bytes: `: > tests/run/vacio.expected`).

- [ ] **Step 2: La sección e2e del runner**

En `tests/run_tests.sh`, tras la sección de `tests/ir` y antes del bloque "Resultado:", añadir:

```bash
echo
echo "== tests/run (end-to-end: kelc -> gcc -> ejecutar) =="
E2E_DIR="$(mktemp -d)"
for f in tests/run/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"
    base="$(basename "${f%.kel}")"
    cfile="$E2E_DIR/$base.c"
    exe="$E2E_DIR/$base.exe"
    if ! "$KELC" --emit-c "$f" > "$cfile" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (kelc --emit-c falló)")
        printf "  FAIL %s (kelc --emit-c falló)\n" "$f"; continue
    fi
    # El requisito de §5.4.2 solo vale si un test lo hace fallar:
    if ! gcc -Wall -Wextra -Werror -o "$exe" "$cfile" 2> "$E2E_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (el C generado no compila limpio)")
        printf "  FAIL %s (gcc rechazó el C generado):\n" "$f"
        sed 's/^/    /' "$E2E_DIR/$base.gcc.log"; continue
    fi
    got=$("$exe" < /dev/null | tr -d '\r')
    if [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (salida distinta)")
        printf "  FAIL %s\n    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" "$f" "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done
rm -rf "$E2E_DIR"
```

- [ ] **Step 3: Correr y ver que FALLA** — `--emit-c` no existe; pega el fallo en tu reporte.

- [ ] **Step 4: `src/emit_c.h`**

```c
#ifndef KEL_EMIT_C_H
#define KEL_EMIT_C_H

#include "ir.h"
#include <stdio.h>

/* Etapa 6: escribe en `out` un programa C completo y autocontenido
 * (runtime inline) equivalente al IRProgram. El IR y el AST deben seguir
 * vivos durante la llamada (ver el aviso de tiempo de vida en ir.h). */
void kel_emit_c(const IRProgram* p, FILE* out);

#endif
```

- [ ] **Step 5: `src/emit_c.c` — esqueleto**

Runtime completo + cabeceras de función + prototipos. Los cuerpos llegan en los tasks siguientes; hoy un cuerpo vacío ya es un programa válido.

```c
#include "emit_c.h"
#include <string.h>

/* ============================================================
 * Etapa 6 — Emisión de C desde el TAC.
 *
 * Un solo .c autocontenido: el runtime va inline para que
 *   kelc prog.kel --emit-c > prog.c && gcc prog.c
 * funcione sin includes ni librerías. Debe compilar limpio con
 * -Wall -Wextra -Werror: el profesor lo va a compilar en la defensa.
 * ============================================================ */

/* Las funciones del runtime NO son static: si lo fueran, las no usadas
 * dispararían -Wunused-function y -Werror tumbaría la compilación. */
static const char* RUNTIME =
"/* --- runtime de Kel, generado por kelc --- */\n"
"#include <stdio.h>\n"
"#include <stdlib.h>\n"
"#include <string.h>\n"
"\n"
"char* kel_concat(const char* a, const char* b) {\n"
"    size_t la = strlen(a), lb = strlen(b);\n"
"    char* r = (char*)malloc(la + lb + 1);\n"
"    memcpy(r, a, la);\n"
"    memcpy(r + la, b, lb + 1);\n"
"    return r;   /* nunca se libera: Kel v1 no tiene GC */\n"
"}\n"
"\n"
"/* Unica puerta de stdin. Sin scanf: no acota mal, no corta en espacios\n"
" * y no deja \\n residual (el bug del LEER A; LEER B). fgetc + crecimiento\n"
" * geometrico; getline() es POSIX y msvcrt no lo tiene. */\n"
"char* kel_read_raw_line(void) {\n"
"    size_t cap = 64, len = 0;\n"
"    char* buf = (char*)malloc(cap);\n"
"    int c;\n"
"    while ((c = fgetc(stdin)) != EOF && c != '\\n') {\n"
"        if (len + 1 == cap) { cap *= 2; buf = (char*)realloc(buf, cap); }\n"
"        buf[len++] = (char)c;\n"
"    }\n"
"    if (len && buf[len-1] == '\\r') len--;   /* CRLF de Windows */\n"
"    buf[len] = 0;\n"
"    return buf;\n"
"}\n"
"\n"
"char* kel_read_line(void) { return kel_read_raw_line(); }\n"
"\n"
"long long kel_read_int(void) {\n"
"    char* s = kel_read_raw_line();\n"
"    char* end;\n"
"    long long v = strtoll(s, &end, 10);\n"
"    while (*end == ' ' || *end == '\\t') end++;\n"
"    if (end == s || *end) {\n"
"        fprintf(stderr, \"kel: entrada invalida, se esperaba un entero: \\\"%s\\\"\\n\", s);\n"
"        exit(1);\n"
"    }\n"
"    return v;\n"
"}\n"
"\n"
"double kel_read_float(void) {\n"
"    char* s = kel_read_raw_line();\n"
"    char* end;\n"
"    double v = strtod(s, &end);\n"
"    while (*end == ' ' || *end == '\\t') end++;\n"
"    if (end == s || *end) {\n"
"        fprintf(stderr, \"kel: entrada invalida, se esperaba un numero: \\\"%s\\\"\\n\", s);\n"
"        exit(1);\n"
"    }\n"
"    return v;\n"
"}\n"
"\n"
"/* Mismo formato que print_float en ir.c: %g suelta el punto y 1.0\n"
" * imprimiria 1. */\n"
"void kel_print_float(double v) {\n"
"    char buf[64];\n"
"    snprintf(buf, sizeof buf, \"%.15g\", v);\n"
"    printf(strpbrk(buf, \".eEnN\") ? \"%s\\n\" : \"%s.0\\n\", buf);\n"
"}\n"
"/* --- fin del runtime --- */\n";

/* Todo identificador de usuario lleva prefijo k_: en Kel `t1`, `switch` o
 * `printf` son nombres legales, y sin prefijo chocarian con los temporales,
 * las palabras reservadas de C o el propio runtime. `main` se queda main. */
static void c_fn_name(FILE* out, const char* name) {
    if (strcmp(name, "main") == 0) fprintf(out, "main");
    else fprintf(out, "k_%s", name);
}

/* int->long long, float->double, bool->int, string->char*, [T]->T'* */
static void c_type(FILE* out, const KelType* t) {
    if (!t) { fprintf(out, "long long"); return; }   /* ver ir.h: NULL = bool del for */
    switch (t->kind) {
        case KT_INT:    fprintf(out, "long long"); break;
        case KT_FLOAT:  fprintf(out, "double"); break;
        case KT_BOOL:   fprintf(out, "int"); break;
        case KT_STRING: fprintf(out, "char*"); break;
        case KT_ARRAY:  c_type(out, t->elem); fprintf(out, "*"); break;
        default:        fprintf(out, "void"); break;
    }
}

static void emit_fn_signature(FILE* out, const IRFunction* f) {
    if (strcmp(f->name, "main") == 0) { fprintf(out, "int main(void)"); return; }
    if (f->ret_type && f->ret_type->kind != KT_VOID) c_type(out, f->ret_type);
    else fprintf(out, "void");
    fprintf(out, " ");
    c_fn_name(out, f->name);
    fprintf(out, "(");
    for (size_t i = 0; i < f->param_count; i++) {
        if (i) fprintf(out, ", ");
        c_type(out, f->params[i].type);
        fprintf(out, " k_%s", f->params[i].name);
    }
    fprintf(out, f->param_count ? ")" : "void)");
}

static void emit_function(FILE* out, const IRFunction* f) {
    emit_fn_signature(out, f);
    fprintf(out, " {\n");
    /* Declaraciones e instrucciones: tasks siguientes. */
    if (strcmp(f->name, "main") == 0) fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
}

void kel_emit_c(const IRProgram* p, FILE* out) {
    fprintf(out, "%s\n", RUNTIME);
    /* Prototipos primero: Kel permite llamar a una funcion definida despues. */
    for (size_t i = 0; i < p->count; i++) {
        if (strcmp(p->fns[i].name, "main") == 0) continue;
        emit_fn_signature(out, &p->fns[i]);
        fprintf(out, ";\n");
    }
    for (size_t i = 0; i < p->count; i++) {
        fprintf(out, "\n");
        emit_function(out, &p->fns[i]);
    }
}
```

- [ ] **Step 6: `main.c` — `--emit-c` y reestructurar el bloque del IR**

Include `#include "emit_c.h"`. En `usage()`, tras `--ir`:
```c
        "  --emit-c    Imprime el C generado (Etapa 6)\n"
```
Variable `int emit_c_flag = 0;` y parseo `else if (strcmp(argv[i], "--emit-c") == 0) emit_c_flag = 1;`.

Reemplazar el bloque actual `if (!sr.had_error && show_ir) { ... }` por:

```c
            if (!sr.had_error && (show_ir || emit_c_flag)) {
                IRProgram ir = kel_gen(pr.root);
                if (show_ir)     kel_ir_print(&ir);
                if (emit_c_flag) kel_emit_c(&ir, stdout);
                kel_ir_free(&ir);
            }
```

Y extender la condición del resumen: `!show_ir && !emit_c_flag`. **`kel_free_ast` sigue después del bloque — verifícalo.**

- [ ] **Step 7: Makefile** — añadir ` src/emit_c.c` a `SRC`.

- [ ] **Step 8: Correr y ver que PASA** — `mingw32-make clean && mingw32-make && bash tests/run_tests.sh` → limpio; "24 pasaron, 0 fallaron". El e2e compila el `.c` con `-Werror`: si el esqueleto emite algo sucio, aquí revienta.

- [ ] **Step 9: Commit**

```bash
git add src/emit_c.h src/emit_c.c src/main.c Makefile tests/run tests/run_tests.sh
git commit -m "Etapa 6: esqueleto de emit_c.c, flag --emit-c y runner e2e

El e2e compila el C generado con -Wall -Wextra -Werror y ejecuta el
binario: es la única prueba que demuestra que las etapas 4-6 funcionan
de verdad. El runtime va inline (un solo .c, sin includes) y sin scanf."
```

---

## Task 2: Expresiones, copias y println

**Files:** `src/emit_c.c`, `tests/run/expr.kel`, `tests/run/expr.expected`

- [ ] **Step 1: El test**

`tests/run/expr.kel`:
```kel
fn main() {
  val a = 2 + 3 * 4
  println(a)
  var b = a - 1
  b = -b
  println(b)
  val s = "ho" + "la"
  println(s)
  val f = 1.0 / 2.0
  println(f)
  println(3.0)
  val t = s == "hola"
  println(t)
}
```
`tests/run/expr.expected`:
```
14
-13
hola
0.5
3.0
true
```

Razonamiento (verifícalo antes de correr): `1.0 / 2.0` → `0.5` demuestra que las constantes float llevan punto en el C (si no, división entera → `0`). `3.0` imprime `3.0` (kel_print_float añade el punto). `s == "hola"` → `strcmp`, no comparación de punteros — con punteros daría `false` y el test lo caza.

- [ ] **Step 2: Correr y ver que falla** (cuerpo vacío → sin salida). Pégalo.

- [ ] **Step 3: Implementar declaraciones + emisión de instrucciones**

En `src/emit_c.c`, antes de `emit_function`:

```c
/* Constante float como texto C: %.17g de 1.0 da "1", y `t = 1 / 2` seria
 * division entera EN EL C GENERADO. Mismo remedio que print_float de ir.c. */
static void c_float(FILE* out, double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.17g", v);
    fprintf(out, strpbrk(buf, ".eEnN") ? "%s" : "%s.0", buf);
}

/* Los str_val del AST pueden traer bytes de escape reales. */
static void c_string_lit(FILE* out, const char* s) {
    fputc('"', out);
    for (; *s; s++) {
        switch (*s) {
            case '"':  fprintf(out, "\\\""); break;
            case '\\': fprintf(out, "\\\\"); break;
            case '\n': fprintf(out, "\\n"); break;
            case '\r': fprintf(out, "\\r"); break;
            case '\t': fprintf(out, "\\t"); break;
            default:   fputc(*s, out); break;
        }
    }
    fputc('"', out);
}

static void c_addr(FILE* out, const Addr* a) {
    switch (a->kind) {
        case ADDR_NONE:        break;
        case ADDR_CONST_INT:   fprintf(out, "%lld", a->i); break;
        case ADDR_CONST_FLOAT: c_float(out, a->f); break;
        case ADDR_CONST_BOOL:  fprintf(out, "%d", a->b); break;
        case ADDR_CONST_STR:   c_string_lit(out, a->s); break;
        case ADDR_VAR:         fprintf(out, "k_%s", a->s); break;
        case ADDR_TEMP:        fprintf(out, "t%lld", a->i); break;
        case ADDR_LABEL:       fprintf(out, "L%lld", a->i); break;
    }
}
```

Declaraciones al inicio de cada función (pre-scan del cuerpo). Añadir antes de `emit_function`:

```c
/* El IR no distingue declaración de asignación (`a = t2` es ambas), así
 * que las variables se descubren escaneando los dst de COPY/READ. Los
 * parámetros no se redeclaran. Todo inicializado a 0: -Wmaybe-uninitialized
 * da falsos positivos alrededor de goto (§5.4.3 del diseño); gcc elimina
 * el dead store al optimizar. */
static int is_param(const IRFunction* f, const char* name) {
    for (size_t i = 0; i < f->param_count; i++)
        if (strcmp(f->params[i].name, name) == 0) return 1;
    return 0;
}

static void emit_declarations(FILE* out, const IRFunction* f) {
    /* temporales: el primer dst ADDR_TEMP fija tipo; IR_ARRAY_NEW declara
     * arreglo nativo (tamaño del op1; 0 -> 1: T t[0] es extensión de gcc) */
    unsigned char* seen = (unsigned char*)calloc(f->n_temps + 1, 1);
    for (size_t j = 0; j < f->count; j++) {
        const Instr* in = &f->body[j];
        if (in->dst.kind != ADDR_TEMP) continue;
        long long id = in->dst.i;
        if (seen[id]) continue;
        seen[id] = 1;
        if (in->op == IR_ARRAY_NEW) {
            fprintf(out, "    ");
            c_type(out, in->dst.type ? in->dst.type->elem : NULL);
            fprintf(out, " t%lld[%lld];\n", id, in->op1.i > 0 ? in->op1.i : 1);
        } else {
            fprintf(out, "    ");
            c_type(out, in->dst.type);   /* NULL -> long long (bool del for) */
            fprintf(out, " t%lld = 0;\n", id);
        }
    }
    free(seen);
    /* variables de usuario (dst ADDR_VAR de COPY/READ), sin duplicar */
    const char* done[256]; size_t n_done = 0;
    for (size_t j = 0; j < f->count; j++) {
        const Instr* in = &f->body[j];
        if (in->dst.kind != ADDR_VAR) continue;
        if (in->op != IR_COPY && in->op != IR_READ) continue;
        if (is_param(f, in->dst.s)) continue;
        int dup = 0;
        for (size_t k = 0; k < n_done; k++)
            if (strcmp(done[k], in->dst.s) == 0) { dup = 1; break; }
        if (dup) continue;
        if (n_done < 256) done[n_done++] = in->dst.s;
        fprintf(out, "    ");
        c_type(out, in->dst.type);
        fprintf(out, " k_%s = 0;\n", in->dst.s);
    }
}
```

Emisión de instrucciones (las de este task; el resto cae a un `default` andamio):

```c
static int is_string(const Addr* a) {
    return a->type && a->type->kind == KT_STRING;
}

static void emit_instr(FILE* out, const IRFunction* f, const Instr* in) {
    (void)f;
    switch (in->op) {
        case IR_COPY:
            fprintf(out, "    ");
            c_addr(out, &in->dst); fprintf(out, " = ");
            c_addr(out, &in->op1); fprintf(out, ";\n");
            break;
        case IR_BINOP:
            fprintf(out, "    ");
            c_addr(out, &in->dst); fprintf(out, " = ");
            if (is_string(&in->op1)) {
                if (strcmp(in->sym, "+") == 0) {
                    fprintf(out, "kel_concat(");
                    c_addr(out, &in->op1); fprintf(out, ", ");
                    c_addr(out, &in->op2); fprintf(out, ")");
                } else {
                    /* == o != sobre strings: comparar contenido, no punteros */
                    fprintf(out, "(strcmp(");
                    c_addr(out, &in->op1); fprintf(out, ", ");
                    c_addr(out, &in->op2); fprintf(out, ") %s 0)", in->sym);
                }
            } else {
                c_addr(out, &in->op1);
                fprintf(out, " %s ", in->sym);
                c_addr(out, &in->op2);
            }
            fprintf(out, ";\n");
            break;
        case IR_UNOP:
            fprintf(out, "    ");
            c_addr(out, &in->dst); fprintf(out, " = %s", in->sym);
            c_addr(out, &in->op1); fprintf(out, ";\n");
            break;
        case IR_PRINTLN:
            fprintf(out, "    ");
            switch (in->op1.type ? in->op1.type->kind : KT_INT) {
                case KT_FLOAT:  fprintf(out, "kel_print_float("); c_addr(out, &in->op1); fprintf(out, ");"); break;
                case KT_BOOL:   fprintf(out, "printf(\"%%s\\n\", "); c_addr(out, &in->op1); fprintf(out, " ? \"true\" : \"false\");"); break;
                case KT_STRING: fprintf(out, "printf(\"%%s\\n\", "); c_addr(out, &in->op1); fprintf(out, ");"); break;
                default:        fprintf(out, "printf(\"%%lld\\n\", "); c_addr(out, &in->op1); fprintf(out, ");"); break;
            }
            fprintf(out, "\n");
            break;
        default:
            /* Andamio: si esto llega al .c, gcc no compila y el e2e falla
             * con el mensaje a la vista. Cada task siguiente quita casos. */
            fprintf(out, "    #error \"opcode %d sin emitir\"\n", (int)in->op);
            break;
    }
}
```

Y `emit_function` pasa a:

```c
static void emit_function(FILE* out, const IRFunction* f) {
    emit_fn_signature(out, f);
    fprintf(out, " {\n");
    emit_declarations(out, f);
    for (size_t j = 0; j < f->count; j++)
        emit_instr(out, f, &f->body[j]);
    if (strcmp(f->name, "main") == 0) fprintf(out, "    return 0;\n");
    fprintf(out, "}\n");
}
```

El `#error` es el andamio de este plan: **un opcode sin emitir rompe la compilación del e2e con el número a la vista**, en vez de generar C que hace otra cosa.

- [ ] **Step 4: Verificar** — 25/25, cero warnings. Si `vacio` sigue pasando y `expr` pasa, las declaraciones y el orden son correctos.

- [ ] **Step 5: Commit**

```bash
git add src/emit_c.c tests/run/expr.kel tests/run/expr.expected
git commit -m "Etapa 6: expresiones, println y declaraciones

== sobre strings emite strcmp (en C compararía punteros), + emite
kel_concat, y las constantes float llevan punto: sin él, 1.0/2.0 sería
división entera en el C generado. El andamio de opcodes sin emitir es un
#error: rompe la compilación del e2e en vez de callar."
```

---

## Task 3: Control de flujo — etiquetas, saltos, y `full.kel`

**Files:** `src/emit_c.c`, `tests/run/flujo.kel`, `tests/run/flujo.expected`, `tests/run/full.kel`, `tests/run/full.expected`

- [ ] **Step 1: Los tests**

`tests/run/flujo.kel`:
```kel
fn main() {
  var t = 0
  if t > 0 {
    println("positivo")
  } else {
    println("no positivo")
  }
  while t < 3 {
    t = t + 1
  }
  println(t)
  var suma = 0
  for i in 0..5 {
    suma = suma + i
  }
  println(suma)
  val p = true
  val q = false
  if p || q {
    println("corto")
  }
}
```
`tests/run/flujo.expected`:
```
no positivo
3
10
corto
```

`tests/run/full.kel`: **copia de `tests/ok/full.kel`** (`cp tests/ok/full.kel tests/run/full.kel`). Su salida se razona a mano — no la generes del binario sin pensarla: `i = 42` → ni `> 100` ni... sí `> 10` → `mediano`; después `println(t)` con `t = cadena("hola")` → `hola!`. `tests/run/full.expected`:
```
mediano
hola!
```
**`full.kel` es el test que dispara garantizado la etiqueta colgante** (`while` en última posición → etiqueta antes de `}`): si no emites `L: ;`, gcc lo rechaza aquí.

- [ ] **Step 2: Correr y ver que fallan** (el `#error` del andamio rompe gcc). Pégalo.

- [ ] **Step 3: Implementar** — en `emit_instr`, antes del `default`:

```c
        case IR_LABEL:
            /* Siempre con `;`: una etiqueta ante `}` viola C < C23 y todo
             * while en última posición lo produce (§5.4.2). El ; es gratis. */
            fprintf(out, "L%lld: ;\n", in->dst.i);
            break;
        case IR_GOTO:
            fprintf(out, "    goto ");
            c_addr(out, &in->op1); fprintf(out, ";\n");
            break;
        case IR_IF_GOTO:
            fprintf(out, "    if (");
            c_addr(out, &in->op1); fprintf(out, ") goto ");
            c_addr(out, &in->op2); fprintf(out, ";\n");
            break;
        case IR_IF_FALSE_GOTO:
            fprintf(out, "    if (!(");
            c_addr(out, &in->op1); fprintf(out, ")) goto ");
            c_addr(out, &in->op2); fprintf(out, ";\n");
            break;
```

(Nota: `full.kel` también necesita llamadas — si decides que este task no puede pasar entero sin el Task 4, **muévelo al final del Task 4 y dilo en tu reporte**; el plan lo acepta. `flujo.kel` sí debe pasar aquí.)

- [ ] **Step 4: Verificar** — `flujo` pasa (26); `full` según lo anterior. Cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/emit_c.c tests/run/flujo.kel tests/run/flujo.expected tests/run/full.kel tests/run/full.expected
git commit -m "Etapa 6: etiquetas y saltos

Toda etiqueta se emite L: ; — una etiqueta pegada a } viola C anterior a
C23 y cualquier while en última posición del cuerpo lo produce; full.kel
lo dispara garantizado. El ; sobrante es gratis."
```

---

## Task 4: Llamadas, return y `read_*`

**Files:** `src/emit_c.c`, `tests/run/fib.kel`, `tests/run/fib.expected`

- [ ] **Step 1: El test**

`tests/run/fib.kel`:
```kel
fn fib(n: int) -> int {
  if n < 2 {
    return n
  }
  return fib(n - 1) + fib(n - 2)
}

fn saluda() {
  println("hola desde saluda")
  return
}

fn main() {
  println(fib(10))
  saluda()
}
```
`tests/run/fib.expected`:
```
55
hola desde saluda
```
Recursión + dos llamadas anidadas en la misma expresión (`fib(n-1) + fib(n-2)`): ejercita la pila de params con intercalado. El `return` desnudo en `saluda` (void) y el implícito de `main`.

- [ ] **Step 2: Correr y ver que falla.** Pégalo.

- [ ] **Step 3: Implementar** — pila de params a nivel de archivo en `emit_c.c`:

```c
/* Convenio de gen_call (ir.c): `call f, n` consume los n param más
 * recientes. Una pila de Addr lo reconstruye; las llamadas anidadas
 * funcionan solas porque la interna vacía su parte antes de que la
 * externa apile. 256 sobra: nadie escribe una llamada de 256 argumentos. */
static Addr g_params[256];
static size_t g_nparams = 0;
```

En `emit_instr`, antes del `default` (y `emit_instr` ya recibe `f`, que ahora sí se usa — quita el `(void)f;`):

```c
        case IR_PARAM:
            if (g_nparams < 256) g_params[g_nparams++] = in->op1;
            break;
        case IR_CALL: {
            size_t n = (size_t)in->op2.i;
            fprintf(out, "    ");
            if (in->dst.kind != ADDR_NONE) {
                c_addr(out, &in->dst);
                fprintf(out, " = ");
            }
            c_fn_name(out, in->sym);
            fprintf(out, "(");
            for (size_t k = 0; k < n; k++) {
                if (k) fprintf(out, ", ");
                c_addr(out, &g_params[g_nparams - n + k]);
            }
            g_nparams -= n;
            fprintf(out, ");\n");
            break;
        }
        case IR_RETURN:
            if (in->op1.kind == ADDR_NONE) {
                /* main es void en Kel pero int main(void) en C */
                fprintf(out, strcmp(f->name, "main") == 0
                             ? "    return 0;\n" : "    return;\n");
            } else {
                fprintf(out, "    return ");
                c_addr(out, &in->op1);
                fprintf(out, ";\n");
            }
            break;
        case IR_READ:
            fprintf(out, "    ");
            c_addr(out, &in->dst);
            switch (in->dst.type->kind) {   /* nunca NULL: ret_type real, ver ir.h */
                case KT_FLOAT:  fprintf(out, " = kel_read_float();\n"); break;
                case KT_STRING: fprintf(out, " = kel_read_line();\n"); break;
                default:        fprintf(out, " = kel_read_int();\n"); break;
            }
            break;
```

Si `full.kel` quedó pendiente del Task 3, aquí debe pasar.

- [ ] **Step 4: Verificar** — 27/27 (o 28 si `full` entró aquí), cero warnings.

- [ ] **Step 5: Commit**

```bash
git add src/emit_c.c tests/run/fib.kel tests/run/fib.expected
git commit -m "Etapa 6: llamadas, return y read_*

La pila de params reconstruye los argumentos según el convenio de
gen_call; fib(n-1) + fib(n-2) ejercita el intercalado. El return desnudo
de main emite return 0 porque main es void en Kel pero int en C."
```

---

## Task 5: Arreglos

**Files:** `src/emit_c.c`, `tests/run/arreglos.kel`, `tests/run/arreglos.expected`

- [ ] **Step 1: El test**

`tests/run/arreglos.kel`:
```kel
fn suma(nums: [int], n: int) -> int {
  var total = 0
  for i in 0..n {
    total = total + nums[i]
  }
  return total
}

fn main() {
  var xs: [int] = [10, 20, 30]
  xs[0] = 5
  println(suma(xs, 3))
  println(xs[2])
}
```
`tests/run/arreglos.expected`:
```
55
30
```
Pasa el arreglo a una función (por referencia: el `xs[0] = 5` anterior se ve dentro de `suma` → 5+20+30=55).

- [ ] **Step 2: Correr y ver que falla.** Pégalo.

- [ ] **Step 3: Implementar** — en `emit_instr`, antes del `default`:

```c
        case IR_ARRAY_NEW:
            /* Nada: la declaración del temporal ya reservó el arreglo
             * nativo (emit_declarations). */
            break;
        case IR_INDEX_LOAD:
            fprintf(out, "    ");
            c_addr(out, &in->dst); fprintf(out, " = ");
            c_addr(out, &in->op1); fprintf(out, "[");
            c_addr(out, &in->op2); fprintf(out, "];\n");
            break;
        case IR_INDEX_STORE:
            /* dst es la FUENTE (ver ir.h) */
            fprintf(out, "    ");
            c_addr(out, &in->op1); fprintf(out, "[");
            c_addr(out, &in->op2); fprintf(out, "] = ");
            c_addr(out, &in->dst); fprintf(out, ";\n");
            break;
```

Con esto el `default` del andamio ya no debería dispararse nunca. **No lo quites todavía** — el Task 6 lo verifica con la suite completa y lo convierte en el cierre.

- [ ] **Step 4: Verificar** — 28/28 (o 29), cero warnings. Nota: el arreglo nativo `t1[3]` decae a puntero al copiarse a `k_xs` (puntero) y al pasarse a `k_suma` — la semántica por referencia de Kel sale gratis del modelo de C.

- [ ] **Step 5: Commit**

```bash
git add src/emit_c.c tests/run/arreglos.kel tests/run/arreglos.expected
git commit -m "Etapa 6: arreglos nativos

El temporal del literal se declara como arreglo C de tamaño fijo y decae
a puntero al asignarse o pasarse: la semántica por referencia del SPEC
sale gratis del modelo de C."
```

---

## Task 6: Tests de entrada (`tests/read/`) — los 4 casos de §6.4

**Files:** `tests/run_tests.sh`, `tests/read/eco.kel` + `.stdin` + `.expected`, `tests/read/espacios.kel` + `.stdin` + `.expected`, `tests/read/larga.kel` + `.stdin` + `.expected`, `tests/read/invalido.kel` + `.stdin` + `.expected` + `.exitcode`

- [ ] **Step 1: La sección del runner**

Tras la sección de `tests/run`, añadir en `tests/run_tests.sh`:

```bash
echo
echo "== tests/read (end-to-end con stdin) =="
READ_DIR="$(mktemp -d)"
for f in tests/read/*.kel; do
    [ -e "$f" ] || continue
    exp="${f%.kel}.expected"; sin="${f%.kel}.stdin"
    base="$(basename "${f%.kel}")"
    cfile="$READ_DIR/$base.c"; exe="$READ_DIR/$base.exe"
    want_rc=0
    [ -e "${f%.kel}.exitcode" ] && want_rc=$(cat "${f%.kel}.exitcode")
    if ! "$KELC" --emit-c "$f" > "$cfile" 2>/dev/null; then
        fail=$((fail+1)); fails+=("$f (kelc falló)"); printf "  FAIL %s (kelc falló)\n" "$f"; continue
    fi
    if ! gcc -Wall -Wextra -Werror -o "$exe" "$cfile" 2> "$READ_DIR/$base.gcc.log"; then
        fail=$((fail+1)); fails+=("$f (C no compila)"); printf "  FAIL %s (gcc):\n" "$f"
        sed 's/^/    /' "$READ_DIR/$base.gcc.log"; continue
    fi
    got=$("$exe" < "$sin" 2>/dev/null | tr -d '\r'); rc=$?
    if [ "$rc" = "$want_rc" ] && [ "$got" = "$(cat "$exp" | tr -d '\r')" ]; then
        pass=$((pass+1)); printf "  ok   %s\n" "$f"
    else
        fail=$((fail+1)); fails+=("$f (salida o exit code)")
        printf "  FAIL %s (exit %s, esperado %s)\n    --- esperado ---\n%s\n    --- obtenido ---\n%s\n" \
               "$f" "$rc" "$want_rc" "$(cat "$exp" | tr -d '\r')" "$got"
    fi
done
rm -rf "$READ_DIR"
```

- [ ] **Step 2: Los 4 casos (cada uno caza un fallo concreto de §5.4.1 del diseño)**

**`eco.kel`** — el `LEER A; LEER B` del profesor; con `scanf`, el `\n` residual dejaría `read_line` vacío:
```kel
fn main() {
  val n = read_int()
  val linea = read_line()
  println(n)
  println(linea)
}
```
`eco.stdin`: `42` + salto + `hola` + salto. `eco.expected`:
```
42
hola
```

**`espacios.kel`** — `%s` cortaría en el espacio:
```kel
fn main() {
  println(read_line())
}
```
`espacios.stdin`: `hola mundo cruel` + salto. `espacios.expected`: `hola mundo cruel`.

**`larga.kel`** — mismo programa que `espacios.kel` (cópialo). `larga.stdin`: una línea de >5000 caracteres — genérala en el task, p.ej. `python -c "print('x'*5000)"` o `printf 'x%.0s' $(seq 5000)` + salto. `larga.expected`: la misma línea. Caza truncado o desbordamiento del búfer.

**`invalido.kel`** — entrada no numérica:
```kel
fn main() {
  println(read_int())
}
```
`invalido.stdin`: `no soy un numero`. `invalido.expected`: **vacío**. `invalido.exitcode`: `1`. Caza que `strtoll` detecte el error en vez de dejar basura.

- [ ] **Step 3: Correr** — deben pasar los 4 a la primera (el runtime ya está desde el Task 1). Si alguno falla, **el bug está en el runtime, no en el test** — arréglalo. Total: 32 (o 33 si `full` se movió).

- [ ] **Step 4: Quitar el andamio** — todos los opcodes se emiten ya. Borra el `default` con `#error` de `emit_instr` y verifica que `-Wswitch` no protesta (si protesta, un opcode quedó sin emitir: repórtalo). Igual que `print_instr` en `ir.c`.

- [ ] **Step 5: Verificar y commit** — suite completa, cero warnings.

```bash
git add src/emit_c.c tests/read tests/run_tests.sh
git commit -m "Etapa 6: tests de entrada — los 4 casos del diseño

eco.kel replica el LEER A; LEER B del profesor: con scanf, el \n
residual dejaría el read_line vacío. espacios caza el %s que corta,
larga el desbordamiento del búfer, e invalido que strtoll detecte el
error. Fuera el andamio de emit_instr: -Wswitch vigila desde ahora."
```

---

## Task 7: `-o <exe>` y cierre — la demo estrella

**Files:** `src/main.c`, `SPEC.md`, `README.md`

- [ ] **Step 1: El flag `-o`**

En `usage()`:
```c
        "  -o <exe>    Compila con gcc y produce un ejecutable (Etapa 6)\n"
```
Parseo (ojo: consume el argumento siguiente):
```c
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "kelc: -o requiere un nombre de salida\n"); return 1; }
            out_exe = argv[++i];
        }
```
con `const char* out_exe = NULL;` junto a los otros flags.

Helper en `main.c` (antes de `main`):

```c
/* Escribe <exe>.c y lo compila con gcc. El .c se queda al lado del exe a
 * propósito: en la demo, verlo es la mitad del argumento. */
static int compile_to_exe(const IRProgram* ir, const char* exe) {
    char cpath[512], cmd[1200];
    snprintf(cpath, sizeof cpath, "%s.c", exe);
    FILE* out = fopen(cpath, "w");
    if (!out) { fprintf(stderr, "kelc: no puedo escribir %s\n", cpath); return 1; }
    kel_emit_c(ir, out);
    fclose(out);
    snprintf(cmd, sizeof cmd, "gcc -Wall -Wextra -o \"%s\" \"%s\"", exe, cpath);
    if (system(cmd) != 0) {
        fprintf(stderr, "kelc: gcc falló compilando %s\n", cpath);
        return 1;
    }
    printf("Generado %s (y %s)\n", exe, cpath);
    return 0;
}
```

En el bloque del IR, la condición pasa a `(show_ir || emit_c_flag || out_exe)` y dentro:
```c
                if (out_exe)     rc = compile_to_exe(&ir, out_exe);
```
La condición del resumen suma `&& !out_exe`. **`kel_ir_free` y `kel_free_ast` siguen en su orden.**

- [ ] **Step 2: Probar a mano la cadena completa** (no hay test automatizado del `-o`: el e2e ya cubre la misma cadena vía `--emit-c`; `-o` solo añade el `system(gcc)`):

```bash
./kelc tests/run/fib.kel -o fib && ./fib
```
Expected: `Generado fib (y fib.c)`, luego `55` y `hola desde saluda`. Borra `fib*` después. Pega la salida real en tu reporte.

- [ ] **Step 3: Documentación**

- `SPEC.md` y `README.md`: quitar `(pendiente)` de `emit_c.c`; añadir a los bloques de uso:
  ```
  ./kelc programa.kel --emit-c > programa.c   # imprime el C generado
  ./kelc programa.kel -o programa             # ejecutable vía gcc
  ```
- `README.md` "## Estado": Etapa 6 hecha; queda la 5 (optimización).
- Verificar la invariante: todo archivo listado sin `(pendiente)` existe (`ls src/`). Solo `optimize.*` debe conservar el marcador.

- [ ] **Step 4: Verificación final del plan**

`mingw32-make clean && mingw32-make && bash tests/run_tests.sh` → cero warnings; **todos los tests pasan** (32-33 según dónde quedó `full`). Y el barrido: `./kelc --emit-c` sobre los 5 de `tests/ok/` + gcc `-Werror` compila los 5 (el runner ya lo hace para los de `tests/run`).

- [ ] **Step 5: Commit**

```bash
git add src/main.c SPEC.md README.md
git commit -m "Cerrar el Plan 3: -o produce ejecutables vía gcc

Etapa 6 completa: Kel → TAC → C → gcc → .exe. El .c se queda al lado
del ejecutable a propósito: en la demo, verlo es la mitad del argumento.
Cierra el criterio 10 de la rúbrica."
```

---

## Criterio de terminado

- [ ] `mingw32-make clean && mingw32-make` sin warnings (`-Wall -Wextra -Wpedantic`)
- [ ] `bash tests/run_tests.sh` → todos pasan (≥32), incluidas las secciones `tests/run` y `tests/read`
- [ ] El C generado de TODOS los e2e compila con `gcc -Wall -Wextra -Werror` (lo impone el runner)
- [ ] `./kelc tests/run/fib.kel -o fib && ./fib` imprime `55`
- [ ] `eco.kel` (read_int + read_line) pasa — el patrón `LEER A; LEER B` del profesor
- [ ] Ningún `scanf` en el runtime (grep)
- [ ] `strcmp` para `==`/`!=` de strings; `kel_concat` para `+`
- [ ] El `default` andamio de `emit_instr` eliminado; `-Wswitch` vigila
- [ ] SPEC/README: `emit_c.c` sin `(pendiente)`; solo `optimize.*` lo conserva

## Qué queda fuera

La Etapa 5 (Plan 4): `optimize.c`, `--opt`, la prueba de equivalencia y `tests/opt/`. También `--stats` y los documentos GRAMMAR/AUTOMATA (Plan 5). El C emitido aquí es el del TAC ingenuo — los `IR_COPY` redundantes se ven en el `.c`, y eso es material del antes/después del Plan 4.

## Concerns conocidos que hay que reportar al terminar

- **El optimizador reordenará este archivo**: cuando el Plan 4 borre saltos, aparecerán etiquetas sin referencia (`-Wunused-label` bajo `-Werror` en el e2e). El pase de limpieza de etiquetas es del Plan 4, pero el fallo aflorará aquí — el e2e está diseñado para cazarlo.
- **`system(gcc)` asume gcc en el PATH.** En esta máquina lo está (TDM-GCC); documentado en README como requisito.
- La pila de params (256) y la lista de vars (256) tienen topes fijos con la casa a favor; coherente con el estilo del codebase (realloc sin comprobar).
