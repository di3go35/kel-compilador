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
