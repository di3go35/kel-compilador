#ifndef KEL_SEMANTIC_H
#define KEL_SEMANTIC_H

#include "parser.h"

typedef struct {
    int had_error;
    int errors;
} SemResult;

SemResult kel_analyze(Node* program);

/* ¿`name` es un built-in del lenguaje (println, read_int, read_float,
 * read_line)? La Etapa 4 lo necesita para emitir IR_PRINTLN / IR_READ en
 * vez de IR_CALL, sin duplicar los nombres. */
int kel_is_builtin(const char* name);

/* ¿`name` es uno de los read_*? Los tres generan IR_READ; println no. */
int kel_is_read_builtin(const char* name);

#endif
