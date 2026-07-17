#ifndef KEL_EMIT_C_H
#define KEL_EMIT_C_H

#include "ir.h"
#include <stdio.h>

/* Etapa 6: escribe en `out` un programa C completo y autocontenido
 * (runtime inline) equivalente al IRProgram. El IR y el AST deben seguir
 * vivos durante la llamada (ver el aviso de tiempo de vida en ir.h). */
void kel_emit_c(const IRProgram* p, FILE* out);

#endif
