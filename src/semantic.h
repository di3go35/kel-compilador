#ifndef KEL_SEMANTIC_H
#define KEL_SEMANTIC_H

#include "parser.h"

typedef struct {
    int had_error;
    int errors;
} SemResult;

SemResult kel_analyze(Node* program);

#endif
