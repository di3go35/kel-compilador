#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* Privado: nada fuera de este archivo lee una entrada, sólo se añaden y se
 * imprimen. El modelo del campo `offset` está documentado en symtab.h. */
typedef struct {
    char*    scope;       /* owned: "main", "main.for", "suma" */
    char*    name;        /* owned */
    KelType* type;        /* owned */
    int      is_mutable;  /* 0 = val, 1 = var */
    int      offset;      /* desplazamiento en el marco */
    int      line;
    char*    value;       /* owned; NULL si no es constante conocida */
} SymEntry;

static SymEntry* g_log = NULL;
static size_t    g_count = 0;
static size_t    g_cap = 0;

/* Modelo de tamaños del marco. Ver el comentario de symtab.h.
 * Los arreglos son referencias de 8 bytes: KelType no guarda cuántos
 * elementos tiene, así que n×elemento no es computable desde el tipo. */
int kel_type_size(const KelType* t) {
    if (!t) return 0;
    switch (t->kind) {
        case KT_INT:    return 4;
        case KT_FLOAT:  return 8;
        case KT_BOOL:   return 1;
        case KT_STRING: return 8;   /* char* */
        case KT_ARRAY:  return 8;   /* referencia */
        default:        return 0;   /* KT_VOID, KT_UNKNOWN */
    }
}

int kel_type_align(const KelType* t) {
    int s = kel_type_size(t);
    return s > 0 ? s : 1;
}

void kel_symlog_add(const char* scope, const char* name, const KelType* type,
                    int is_mutable, int offset, int line, const char* value) {
    if (g_count == g_cap) {
        g_cap = g_cap ? g_cap * 2 : 16;
        g_log = (SymEntry*)realloc(g_log, g_cap * sizeof(SymEntry));
    }
    SymEntry* e = &g_log[g_count++];
    e->scope      = strdup(scope ? scope : "");
    e->name       = strdup(name ? name : "");
    e->type       = kel_type_clone(type);
    e->is_mutable = is_mutable;
    e->offset     = offset;
    e->line       = line;
    e->value      = value ? strdup(value) : NULL;
}

size_t kel_symlog_count(void) { return g_count; }

/* La cabecera va escrita a mano, no con %-16s.
 *
 * printf rellena contando BYTES, no caracteres. "Ámbito" ocupa 7 bytes en
 * UTF-8 pero 6 columnas visuales (Á son 2 bytes), y "Línea" lo mismo. Con
 * %-16s la cabecera saldría desplazada una columna respecto a los datos.
 * Las filas de datos sí usan %-Ns sin problema: los identificadores, los
 * tipos y los ámbitos son todos ASCII. El "—" de la columna Valor es UTF-8
 * pero es la última columna y no se rellena.
 *
 * Ámbito son 16 columnas porque "main.for.for.for" —tres bucles anidados,
 * un programa normal— ya son 16 caracteres. */
static const char* SYM_HEADER =
    "Ámbito           Identificador  Tipo      Mut   Despl  Línea  Valor";

void kel_symlog_print(void) {
    puts(SYM_HEADER);
    for (size_t i = 0; i < g_count; i++) {
        SymEntry* e = &g_log[i];
        printf("%-16s %-14s %-9s %-4s %6d %6d  %s\n",
               e->scope, e->name, kel_type_name(e->type),
               e->is_mutable ? "var" : "val",
               e->offset, e->line,
               e->value ? e->value : "—");
    }
}

void kel_symlog_free(void) {
    for (size_t i = 0; i < g_count; i++) {
        free(g_log[i].scope);
        free(g_log[i].name);
        kel_type_free(g_log[i].type);
        free(g_log[i].value);
    }
    free(g_log);
    g_log = NULL;
    g_count = g_cap = 0;
}
