#ifndef KEL_DIAG_H
#define KEL_DIAG_H

/* Módulo de diagnósticos: errores con formato uniforme y línea fuente. */

void kel_diag_init(const char* source);
void kel_diag_error(int line, int col, const char* fmt, ...);

#endif
