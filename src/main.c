#include "lexer.h"
#include "parser.h"
#include "semantic.h"
#include "diag.h"
#include "symtab.h"
#include "ir.h"
#include "emit_c.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static void usage(FILE* out, const char* prog) {
    fprintf(out,
        "kelc — compilador del lenguaje Kel\n"
        "\n"
        "Uso:\n"
        "  %s [flags] archivo.kel\n"
        "\n"
        "Flags:\n"
        "  --tokens    Muestra el stream de tokens (Etapa 1)\n"
        "  --ast       Muestra el AST con tipos inferidos (Etapa 2 + 3)\n"
        "  --symbols   Muestra la tabla de símbolos (Etapa 3)\n"
        "  --ir        Muestra el código intermedio TAC (Etapa 4)\n"
        "  --emit-c    Imprime el C generado (Etapa 6)\n"
        "  -o <exe>    Compila con gcc y produce un ejecutable (Etapa 6)\n"
        "  --sem       Solo reporta resultado del análisis semántico\n"
        "  -h, --help  Muestra esta ayuda\n"
        "\n"
        "Ejemplos:\n"
        "  %s programa.kel              compila y reporta fases\n"
        "  %s --tokens programa.kel     debug léxico\n"
        "  %s --ast programa.kel        debug sintáctico/semántico\n"
        "\n"
        "Nota: los enteros negativos (p.ej. -5) se procesan como operador\n"
        "unario '-' aplicado a un literal entero, no como un solo token.\n",
        prog, prog, prog, prog);
}

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

int main(int argc, char** argv) {
    const char* path = NULL;
    const char* out_exe = NULL;
    int show_tokens = 0, show_ast = 0, show_sem = 0, show_symbols = 0, show_ir = 0;
    int emit_c_flag = 0;

    for (int i = 1; i < argc; i++) {
        if      (strcmp(argv[i], "--tokens") == 0) show_tokens = 1;
        else if (strcmp(argv[i], "--ast") == 0)    show_ast = 1;
        else if (strcmp(argv[i], "--sem") == 0)    show_sem = 1;
        else if (strcmp(argv[i], "--symbols") == 0) show_symbols = 1;
        else if (strcmp(argv[i], "--ir") == 0)      show_ir = 1;
        else if (strcmp(argv[i], "--emit-c") == 0)  emit_c_flag = 1;
        else if (strcmp(argv[i], "-o") == 0) {
            if (i + 1 >= argc) { fprintf(stderr, "kelc: -o requiere un nombre de salida\n"); return 1; }
            out_exe = argv[++i];
        }
        else if (strcmp(argv[i], "--help") == 0 || strcmp(argv[i], "-h") == 0) {
            usage(stdout, argv[0]);
            return 0;
        }
        else if (argv[i][0] == '-') {
            fprintf(stderr, "flag desconocido: %s\n", argv[i]);
            usage(stderr, argv[0]);
            return 1;
        }
        else path = argv[i];
    }

    if (!path) { usage(stderr, argv[0]); return 1; }

    char* src = kel_read_file(path);
    if (!src) {
        fprintf(stderr, "no se pudo abrir el archivo: %s\n", path);
        return 1;
    }
    kel_diag_init(src);

    TokenList tokens = kel_tokenize(src);
    if (show_tokens) kel_print_tokens(&tokens);

    int rc = 0;
    if (tokens.had_error) {
        fprintf(stderr, "Lexer: errores encontrados\n");
        rc = 1;
    } else {
        ParseResult pr = kel_parse(&tokens);
        if (pr.had_error) {
            if (show_ast) kel_print_ast(pr.root);
            fprintf(stderr, "Parser: errores encontrados\n");
            rc = 1;
        } else {
            SemResult sr = kel_analyze(pr.root);
            if (show_ast) kel_print_ast(pr.root);  /* con tipos inferidos */
            if (show_symbols) kel_symlog_print();
            if (sr.had_error) {
                fprintf(stderr, "Semántico: %d error(es)\n", sr.errors);
                rc = 1;
            } else if (show_sem) {
                printf("Semántico OK\n");
            } else if (!show_tokens && !show_ast && !show_symbols && !show_ir && !emit_c_flag && !out_exe) {
                printf("Lexer OK — %zu tokens\nParser OK\nSemántico OK\n", tokens.count);
            }
            /* El IR se genera ANTES de kel_free_ast: IRFunction.params,
             * ret_type y los Addr.type apuntan al AST sin poseerlo. */
            if (!sr.had_error && (show_ir || emit_c_flag || out_exe)) {
                IRProgram ir = kel_gen(pr.root);
                if (show_ir)     kel_ir_print(&ir);
                if (emit_c_flag) kel_emit_c(&ir, stdout);
                if (out_exe)     rc = compile_to_exe(&ir, out_exe);
                kel_ir_free(&ir);
            }
        }
        kel_free_ast(pr.root);
    }

    kel_symlog_free();
    kel_free_tokens(&tokens);
    free(src);
    return rc;
}
