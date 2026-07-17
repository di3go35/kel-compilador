#include "ir.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ============================================================
 * Etapa 4 — Generación de código intermedio (TAC).
 *
 * gen_expr() traduce una expresión y devuelve la Addr donde quedó su
 * valor, emitiendo instrucciones por el camino. gen_stmt() traduce una
 * sentencia y no devuelve nada.
 *
 * Los Addr.type y los nombres (ADDR_VAR, ADDR_CONST_STR) apuntan al AST
 * sin poseerlo: el AST debe sobrevivir al IRProgram. Ver ir.h.
 * ============================================================ */

/* ---------- Generación por función ---------- */

static void gen_function(IRProgram* p, Node* fn) {
    if (p->count == p->capacity) {
        p->capacity = p->capacity ? p->capacity * 2 : 8;
        p->fns = (IRFunction*)realloc(p->fns, p->capacity * sizeof(IRFunction));
    }
    IRFunction* f = &p->fns[p->count++];
    memset(f, 0, sizeof(*f));
    f->name        = strdup(fn->fn_name);
    f->params      = fn->params;      /* no-owned: apunta al AST */
    f->param_count = fn->param_count;
    f->ret_type    = fn->ret_type;    /* no-owned */

    /* El cuerpo se genera en los tasks siguientes. */
}

IRProgram kel_gen(Node* program) {
    IRProgram p;
    memset(&p, 0, sizeof(p));
    for (size_t i = 0; i < program->item_count; i++)
        gen_function(&p, program->items[i]);
    return p;
}

void kel_ir_free(IRProgram* p) {
    for (size_t i = 0; i < p->count; i++) {
        free(p->fns[i].name);
        free(p->fns[i].body);
    }
    free(p->fns);
    p->fns = NULL;
    p->count = p->capacity = 0;
}

/* ---------- Impresión ---------- */

static void print_instr(const Instr* in) {
    /* Cada task siguiente añade aquí el formato de sus opcodes. Hoy ninguna
     * función tiene cuerpo, así que esto no llega a ejecutarse. */
    printf("  <opcode %d sin imprimir>\n", (int)in->op);
}

/* Cabecera con forma de Kel: hace inmediato comparar --ir con el fuente. */
static void print_fn_header(const IRFunction* f) {
    printf("fn %s(", f->name);
    for (size_t i = 0; i < f->param_count; i++) {
        if (i) printf(", ");
        printf("%s: %s", f->params[i].name, kel_type_name(f->params[i].type));
    }
    printf(")");
    if (f->ret_type && f->ret_type->kind != KT_VOID)
        printf(" -> %s", kel_type_name(f->ret_type));
    printf(" {\n");
}

void kel_ir_print(const IRProgram* p) {
    for (size_t i = 0; i < p->count; i++) {
        if (i) printf("\n");
        const IRFunction* f = &p->fns[i];
        print_fn_header(f);
        for (size_t j = 0; j < f->count; j++)
            print_instr(&f->body[j]);
        printf("}\n");
    }
}
