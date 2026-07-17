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
