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

/* ---------- Entorno de propagación local ----------
 * Por cada ubicación escrita (variable o temporal), la Addr a la que es igual
 * ahora mismo. En este task solo constantes; el Task 3 añade copias. */

static int is_const(const Addr* a) {
    return a->kind == ADDR_CONST_INT || a->kind == ADDR_CONST_FLOAT
        || a->kind == ADDR_CONST_BOOL || a->kind == ADDR_CONST_STR;
}

/* Igualdad de UBICACIONES (var por nombre, temporal por id). Las constantes
 * no son ubicaciones: devuelven 0. */
static int addr_same_loc(const Addr* a, const Addr* b) {
    if (a->kind != b->kind) return 0;
    if (a->kind == ADDR_TEMP) return a->i == b->i;
    if (a->kind == ADDR_VAR)  return strcmp(a->s, b->s) == 0;
    return 0;
}

typedef struct { Addr key; Addr val; } EnvEntry;
typedef struct { EnvEntry* v; size_t n, cap; } Env;

static void env_init(Env* e)  { e->v = NULL; e->n = e->cap = 0; }
static void env_free(Env* e)  { free(e->v); e->v = NULL; e->n = e->cap = 0; }
static void env_clear(Env* e) { e->n = 0; }

/* Devuelve el valor asociado a la ubicación `k`, o NULL. */
static const Addr* env_lookup(const Env* e, const Addr* k) {
    if (k->kind != ADDR_VAR && k->kind != ADDR_TEMP) return NULL;
    for (size_t i = 0; i < e->n; i++)
        if (addr_same_loc(&e->v[i].key, k)) return &e->v[i].val;
    return NULL;
}

/* Borra toda entrada cuya CLAVE o cuyo VALOR sea la ubicación `w`: cuando w se
 * reescribe, ni «w vale X» ni «Y vale w» siguen siendo ciertos. */
static void env_invalidate(Env* e, const Addr* w) {
    size_t k = 0;
    for (size_t i = 0; i < e->n; i++) {
        if (addr_same_loc(&e->v[i].key, w) || addr_same_loc(&e->v[i].val, w))
            continue;
        e->v[k++] = e->v[i];
    }
    e->n = k;
}

static void env_set(Env* e, Addr key, Addr val) {
    if (e->n == e->cap) {
        e->cap = e->cap ? e->cap * 2 : 8;
        e->v = (EnvEntry*)realloc(e->v, e->cap * sizeof(EnvEntry));
    }
    e->v[e->n].key = key;
    e->v[e->n].val = val;
    e->n++;
}

/* ---------- Plegado de constantes ----------
 * dst y su tipo se conservan; solo cambia la operación a IR_COPY de la
 * constante. La constante toma el tipo del dst para que la Etapa 6 lo emita
 * bien. */
static void make_copy(Instr* in, Addr c) {
    in->op = IR_COPY;
    in->op1 = c;
    memset(&in->op2, 0, sizeof(in->op2));
    in->sym = NULL;
}
static Addr const_int(long long v, KelType* t)  { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_INT;  a.type=t; a.i=v; return a; }
static Addr const_float(double v, KelType* t)   { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_FLOAT;a.type=t; a.f=v; return a; }
static Addr const_bool(int v, KelType* t)       { Addr a; memset(&a,0,sizeof a); a.kind=ADDR_CONST_BOOL; a.type=t; a.b=v; return a; }

static int try_fold(Instr* in) {
    if (in->op != IR_BINOP) return 0;
    const Addr* a = &in->op1; const Addr* b = &in->op2; const char* o = in->sym;
    if (a->kind == ADDR_CONST_INT && b->kind == ADDR_CONST_INT) {
        long long x = a->i, y = b->i;
        if      (strcmp(o,"+")==0) { make_copy(in, const_int(x+y, in->dst.type)); return 1; }
        else if (strcmp(o,"-")==0) { make_copy(in, const_int(x-y, in->dst.type)); return 1; }
        else if (strcmp(o,"*")==0) { make_copy(in, const_int(x*y, in->dst.type)); return 1; }
        else if (strcmp(o,"/")==0) { if (y==0) return 0; make_copy(in, const_int(x/y, in->dst.type)); return 1; }
        else if (strcmp(o,"%")==0) { if (y==0) return 0; make_copy(in, const_int(x%y, in->dst.type)); return 1; }
        else {
            int c;
            if      (strcmp(o,"<")==0)  c = x <  y;
            else if (strcmp(o,">")==0)  c = x >  y;
            else if (strcmp(o,"<=")==0) c = x <= y;
            else if (strcmp(o,">=")==0) c = x >= y;
            else if (strcmp(o,"==")==0) c = x == y;
            else if (strcmp(o,"!=")==0) c = x != y;
            else return 0;
            make_copy(in, const_bool(c, in->dst.type)); return 1;
        }
    }
    if (a->kind == ADDR_CONST_FLOAT && b->kind == ADDR_CONST_FLOAT) {
        double x = a->f, y = b->f;
        if      (strcmp(o,"+")==0) { make_copy(in, const_float(x+y, in->dst.type)); return 1; }
        else if (strcmp(o,"-")==0) { make_copy(in, const_float(x-y, in->dst.type)); return 1; }
        else if (strcmp(o,"*")==0) { make_copy(in, const_float(x*y, in->dst.type)); return 1; }
        else if (strcmp(o,"/")==0) { if (y==0.0) return 0; make_copy(in, const_float(x/y, in->dst.type)); return 1; }
        else {
            int c;
            if      (strcmp(o,"<")==0)  c = x <  y;
            else if (strcmp(o,">")==0)  c = x >  y;
            else if (strcmp(o,"<=")==0) c = x <= y;
            else if (strcmp(o,">=")==0) c = x >= y;
            else if (strcmp(o,"==")==0) c = x == y;
            else if (strcmp(o,"!=")==0) c = x != y;
            else return 0;
            make_copy(in, const_bool(c, in->dst.type)); return 1;
        }
    }
    return 0;
}

/* ¿Escribe esta instrucción una ubicación (dst como DESTINO)? Devuelve la Addr
 * escrita o NULL. OJO: IR_INDEX_STORE NO escribe dst (dst es su fuente, ver
 * ir.h), y IR_CALL void tampoco. */
static const Addr* written_loc(const Instr* in) {
    switch (in->op) {
        case IR_COPY: case IR_BINOP: case IR_UNOP:
        case IR_ARRAY_NEW: case IR_INDEX_LOAD: case IR_READ:
            return &in->dst;
        case IR_CALL:
            return in->dst.kind == ADDR_NONE ? NULL : &in->dst;
        default:
            return NULL;
    }
}

/* Reescribe un operando fuente con el entorno. Devuelve 1 si cambió. */
static int rewrite_operand(const Env* e, Addr* op) {
    const Addr* v = env_lookup(e, op);
    if (!v) return 0;
    KelType* keep = op->type;      /* conserva el tipo del operando original */
    *op = *v;
    if (!op->type) op->type = keep;
    return 1;
}

/* Actualiza el entorno según el efecto de la instrucción ya reescrita. */
static void update_env(Env* e, const Instr* in) {
    const Addr* w = written_loc(in);
    if (w) env_invalidate(e, w);
    /* Solo se registran copias de CONSTANTE en este task (Task 3 añade copias
     * de ubicación). Un plegado ya dejó la instrucción como IR_COPY de const. */
    if (in->op == IR_COPY && is_const(&in->op1))
        env_set(e, in->dst, in->op1);
}

static int is_block_end(IROp op) {
    return op == IR_GOTO || op == IR_IF_GOTO
        || op == IR_IF_FALSE_GOTO || op == IR_RETURN;
}

/* Un recorrido hacia adelante: reescribe operandos, pliega, actualiza entorno,
 * y vacía el entorno en cada frontera de bloque. Devuelve 1 si algo cambió. */
static int local_pass(IRFunction* f) {
    int changed = 0;
    Env e; env_init(&e);
    for (size_t j = 0; j < f->count; j++) {
        Instr* in = &f->body[j];
        if (in->op == IR_LABEL) { env_clear(&e); continue; }   /* nueva frontera */
        changed |= rewrite_operand(&e, &in->op1);
        changed |= rewrite_operand(&e, &in->op2);
        changed |= try_fold(in);
        update_env(&e, in);
        if (is_block_end(in->op)) env_clear(&e);
    }
    env_free(&e);
    return changed;
}

OptStats kel_optimize(IRProgram* p) {
    OptStats st;
    st.instr_before = count_instrs(p);
    for (size_t i = 0; i < p->count; i++)
        local_pass(&p->fns[i]);
    st.instr_after = count_instrs(p);
    return st;
}
