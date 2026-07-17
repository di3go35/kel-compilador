#include "emit_c.h"
#include <string.h>
#include <stdlib.h>

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
        /* __attribute__((unused)): una variable de Kel puede calcularse y no
         * usarse nunca (p.ej. `val d = div(10, 2)` sin imprimir d). Es código
         * muerto legal en el fuente, no un defecto del codegen; sin la marca,
         * -Wunused-but-set-variable con -Werror tumbaría el .c generado. Lo
         * limpiará el optimizador del Plan 4; hoy la marca lo hace compilable. */
        fprintf(out, "    ");
        c_type(out, in->dst.type);
        fprintf(out, " k_%s __attribute__((unused)) = 0;\n", in->dst.s);
    }
}

static int is_string(const Addr* a) {
    return a->type && a->type->kind == KT_STRING;
}

/* Convenio de gen_call (ir.c): `call f, n` consume los n param más
 * recientes. Una pila de Addr lo reconstruye; las llamadas anidadas
 * funcionan solas porque la interna vacía su parte antes de que la
 * externa apile. 256 sobra: nadie escribe una llamada de 256 argumentos. */
static Addr g_params[256];
static size_t g_nparams = 0;

static void emit_instr(FILE* out, const IRFunction* f, const Instr* in) {
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
    }
    /* Sin default: -Wswitch vigila que todo IROp tenga su caso, igual que
     * print_instr en ir.c. Un opcode nuevo sin emitir será un warning aquí. */
}

static void emit_function(FILE* out, const IRFunction* f) {
    emit_fn_signature(out, f);
    fprintf(out, " {\n");
    emit_declarations(out, f);
    for (size_t j = 0; j < f->count; j++)
        emit_instr(out, f, &f->body[j]);
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
