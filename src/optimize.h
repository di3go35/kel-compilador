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
