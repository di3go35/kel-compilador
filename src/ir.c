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

/* ---------- Constructores de Addr ---------- */

/* Todo Addr nace en cero: kind == ADDR_NONE por defecto (ver ir.h). */
static Addr addr_none(void) {
    Addr a;
    memset(&a, 0, sizeof(a));
    return a;
}

static Addr addr_var(const char* name, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_VAR;
    a.type = t;
    a.s = name;      /* no-owned: apunta al AST */
    return a;
}

static Addr addr_const_int(long long v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_INT;
    a.type = t;
    a.i = v;
    return a;
}

static Addr addr_const_float(double v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_FLOAT;
    a.type = t;
    a.f = v;
    return a;
}

static Addr addr_const_bool(int v, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_BOOL;
    a.type = t;
    a.b = v;
    return a;
}

static Addr addr_const_str(const char* s, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_CONST_STR;
    a.type = t;
    a.s = s;         /* no-owned: apunta al AST */
    return a;
}

/* ---------- Emisión ---------- */

static void emit(IRFunction* f, Instr in) {
    if (f->count == f->capacity) {
        f->capacity = f->capacity ? f->capacity * 2 : 16;
        f->body = (Instr*)realloc(f->body, f->capacity * sizeof(Instr));
    }
    f->body[f->count++] = in;
}

static Instr instr(IROp op) {
    Instr in;
    memset(&in, 0, sizeof(in));
    in.op = op;
    return in;
}

/* Los temporales se numeran por función (ver ir.h). */
static Addr new_temp(IRFunction* f, KelType* t) {
    Addr a = addr_none();
    a.kind = ADDR_TEMP;
    a.type = t;
    a.i = (long long)(++f->n_temps);
    return a;
}

/* ---------- Generación de expresiones y sentencias ---------- */

static Addr gen_expr(IRFunction* f, Node* n);
static void gen_stmt(IRFunction* f, Node* n);

static void emit_copy(IRFunction* f, Addr dst, Addr src) {
    Instr in = instr(IR_COPY);
    in.dst = dst;
    in.op1 = src;
    emit(f, in);
}

static Addr gen_binop(IRFunction* f, Node* n) {
    Addr a = gen_expr(f, n->lhs);
    Addr b = gen_expr(f, n->rhs);
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_BINOP);
    in.dst = d;
    in.op1 = a;
    in.op2 = b;
    in.sym = n->op;   /* no-owned: apunta al AST */
    emit(f, in);
    return d;
}

static Addr gen_unop(IRFunction* f, Node* n) {
    Addr a = gen_expr(f, n->lhs);   /* el operando va en lhs, no en rhs */
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_UNOP);
    in.dst = d;
    in.op1 = a;
    in.sym = n->op;
    emit(f, in);
    return d;
}

/* El `default` con fprintf es un andamio: cada task siguiente le va quitando
 * casos. Al cerrar la Etapa 4 no debe quedar ningún nodo cayendo aquí. */
static Addr gen_expr(IRFunction* f, Node* n) {
    switch (n->kind) {
        case N_INT_LIT:   return addr_const_int(n->int_val, n->inferred_type);
        case N_FLOAT_LIT: return addr_const_float(n->float_val, n->inferred_type);
        case N_BOOL_LIT:  return addr_const_bool(n->bool_val, n->inferred_type);
        case N_STR_LIT:   return addr_const_str(n->str_val, n->inferred_type);
        case N_IDENT:     return addr_var(n->str_val, n->inferred_type);
        case N_BINOP:     return gen_binop(f, n);
        case N_UNOP:      return gen_unop(f, n);
        default:
            fprintf(stderr, "ir: nodo de expresión no soportado (kind %d)\n",
                    (int)n->kind);
            return addr_none();
    }
}

static void gen_stmt(IRFunction* f, Node* n) {
    switch (n->kind) {
        case N_VAR_DECL: {
            /* El IR_COPY del temporal a la variable es redundante a propósito:
             * la propagación de copias de la Etapa 5 lo limpia. */
            Addr v = gen_expr(f, n->decl_init);
            emit_copy(f, addr_var(n->decl_name, n->decl_init->inferred_type), v);
            break;
        }
        case N_ASSIGN: {
            /* OJO: N_ASSIGN usa decl_name/decl_init, no lhs/rhs (parser.c:346).
             * Leer n->lhs aquí daría NULL y emitiría IR vacío en silencio. */
            Addr v = gen_expr(f, n->decl_init);
            emit_copy(f, addr_var(n->decl_name, n->decl_init->inferred_type), v);
            break;
        }
        case N_BLOCK:
            for (size_t i = 0; i < n->item_count; i++)
                gen_stmt(f, n->items[i]);
            break;
        case N_EXPR_STMT:
            gen_expr(f, n->lhs);
            break;
        default:
            fprintf(stderr, "ir: sentencia no soportada (kind %d)\n", (int)n->kind);
            break;
    }
}

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

    /* body es un N_BLOCK; gen_stmt lo recorre. */
    gen_stmt(f, fn->body);
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

/* Sin `default` a propósito: así -Wswitch obliga a cubrir cualquier AddrKind
 * que se añada en el futuro. */
static void print_addr(const Addr* a) {
    switch (a->kind) {
        case ADDR_NONE:        break;
        case ADDR_CONST_INT:   printf("%lld", a->i); break;
        case ADDR_CONST_FLOAT: printf("%.15g", a->f); break;
        case ADDR_CONST_BOOL:  printf("%s", a->b ? "true" : "false"); break;
        case ADDR_CONST_STR:   printf("\"%s\"", a->s); break;
        case ADDR_VAR:         printf("%s", a->s); break;
        case ADDR_TEMP:        printf("t%lld", a->i); break;
        case ADDR_LABEL:       printf("L%lld", a->i); break;
    }
}

/* Aquí el `default` sí hace falta: todavía hay opcodes sin formato. Es un
 * andamio visible — si un opcode se genera pero no se imprime, sale
 * "<opcode N sin imprimir>" en el golden y el test falla. */
static void print_instr(const Instr* in) {
    switch (in->op) {
        case IR_LABEL:
            /* Sin indentar, en la columna 0, igual que en C. */
            printf("L%lld:\n", in->dst.i);
            break;
        case IR_COPY:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_BINOP:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf(" %s ", in->sym);
            print_addr(&in->op2);
            printf("\n");
            break;
        case IR_UNOP:
            printf("  ");
            print_addr(&in->dst);
            printf(" = %s", in->sym);
            print_addr(&in->op1);
            printf("\n");
            break;
        default:
            printf("  <opcode %d sin imprimir>\n", (int)in->op);
            break;
    }
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
