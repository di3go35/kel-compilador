#ifndef KEL_SYMTAB_H
#define KEL_SYMTAB_H

#include "parser.h"
#include <stddef.h>

/* Log append-only de declaraciones (--symbols).
 *
 * La tabla de símbolos real vive en semantic.c y sus scopes se liberan en
 * scope_pop al cerrarse, de modo que al terminar la compilación no queda
 * nada que imprimir. Este log graba cada declaración cuando ocurre, sin
 * tocar el ciclo de vida de los scopes.
 *
 * Modelo del desplazamiento (campo `offset`): posición relativa al marco de
 * la función, alineada al tamaño del tipo. No es una dirección absoluta —
 * esas las decide gcc. Los arreglos cuentan como una referencia de 8 bytes
 * porque KelType no guarda el número de elementos. */

typedef struct {
    char*    scope;       /* owned: "main", "main.for", "suma" */
    char*    name;        /* owned */
    KelType* type;        /* owned */
    int      is_mutable;  /* 0 = val, 1 = var */
    int      offset;      /* desplazamiento en el marco */
    int      line;
    char*    value;       /* owned; NULL si no es constante conocida */
} SymEntry;

/* Copia todo lo que recibe: el llamante conserva la propiedad de sus datos.
 * `value` puede ser NULL. */
void   kel_symlog_add(const char* scope, const char* name, const KelType* type,
                      int is_mutable, int offset, int line, const char* value);
void   kel_symlog_print(void);
void   kel_symlog_free(void);
size_t kel_symlog_count(void);

/* Tamaño y alineación del modelo de marco. Expuestos para poder probarlos. */
int kel_type_size(const KelType* t);
int kel_type_align(const KelType* t);

#endif
