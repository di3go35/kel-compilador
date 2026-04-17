#include "diag.h"

#include <stdio.h>
#include <stdarg.h>
#include <string.h>

static const char* g_src = NULL;

void kel_diag_init(const char* source) { g_src = source; }

/* Busca el inicio de la línea n (1-indexed) en g_src y su longitud (sin \n). */
static int find_line(int target, const char** out_start, int* out_len) {
    if (!g_src) return 0;
    int line = 1;
    const char* p = g_src;
    const char* line_start = p;
    while (*p) {
        if (line == target) {
            const char* end = p;
            while (*end && *end != '\n') end++;
            *out_start = line_start;
            *out_len = (int)(end - line_start);
            return 1;
        }
        if (*p == '\n') { line++; line_start = p + 1; }
        p++;
    }
    if (line == target) {
        const char* end = p;
        *out_start = line_start;
        *out_len = (int)(end - line_start);
        return 1;
    }
    return 0;
}

void kel_diag_error(int line, int col, const char* fmt, ...) {
    va_list ap;
    fprintf(stderr, "[Kel Error] línea %d, col %d: ", line, col);
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
    fputc('\n', stderr);

    const char* ls = NULL;
    int len = 0;
    if (find_line(line, &ls, &len) && len > 0) {
        fprintf(stderr, "  %5d | %.*s\n", line, len, ls);
        fprintf(stderr, "        | ");
        for (int i = 1; i < col; i++) {
            char c = (i - 1 < len) ? ls[i - 1] : ' ';
            fputc(c == '\t' ? '\t' : ' ', stderr);
        }
        fputs("^\n", stderr);
    }
}
