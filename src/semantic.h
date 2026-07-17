#ifndef KEL_SEMANTIC_H
#define KEL_SEMANTIC_H

#include "parser.h"

typedef struct {
    int had_error;
    int errors;
} SemResult;

SemResult kel_analyze(Node* program);

/* ¿`name` es el built-in println? La Etapa 4 lo necesita para emitir
 * IR_PRINTLN en vez de IR_CALL sin hardcodear el nombre en ir.c. */
int kel_is_println(const char* name);

/* ¿`name` es uno de los read_*? Los tres generan IR_READ; println no. */
int kel_is_read_builtin(const char* name);

#endif
