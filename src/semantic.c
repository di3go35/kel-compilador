#include "semantic.h"
#include "diag.h"
#include "symtab.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <stdarg.h>

/* ---------- Tabla de símbolos ---------- */

typedef struct Symbol {
    char* name;
    KelType* type;       /* owned copy */
    int is_mutable;
    struct Symbol* next;
} Symbol;

typedef struct Scope {
    Symbol* head;
    struct Scope* parent;
    const char* label;   /* "main", "for", ... o NULL en bloques anónimos */
} Scope;

typedef struct FnSig {
    char* name;
    Param* params;         /* shared ref, no free */
    size_t param_count;
    KelType* ret_type;     /* shared ref, no free */
    struct FnSig* next;
} FnSig;

typedef struct {
    Scope* scope;
    FnSig* fns;
    KelType* current_ret;  /* ret_type de la función que se está analizando */
    int errors;
    int frame_offset;      /* cursor del marco; se reinicia por función */
} Sem;

/* ---------- Tipos helpers ---------- */

/* El clon y la liberación de KelType viven en parser.c (ver parser.h). */

static int type_eq(const KelType* a, const KelType* b) {
    if (!a || !b) return 0;
    if (a->kind != b->kind) return 0;
    if (a->kind == KT_ARRAY) return type_eq(a->elem, b->elem);
    return 1;
}

static KelType* mk(KelTypeKind k) {
    KelType* t = (KelType*)calloc(1, sizeof(KelType));
    t->kind = k;
    return t;
}

/* ---------- Error ---------- */

static void err(Sem* S, const Node* n, const char* fmt, ...) {
    S->errors++;
    char buf[512];
    va_list ap;
    va_start(ap, fmt);
    vsnprintf(buf, sizeof(buf), fmt, ap);
    va_end(ap);
    kel_diag_error(n ? n->line : 0, n ? n->col : 0, "%s", buf);
}

/* ---------- Scopes ---------- */

static void scope_push(Sem* S, const char* label) {
    Scope* s = (Scope*)calloc(1, sizeof(Scope));
    s->parent = S->scope;
    s->label = label;
    S->scope = s;
}

/* Construye el ámbito punteado ("main.for") caminando hasta la raíz y
 * saltando los bloques anónimos. Escribe en `out` y lo devuelve.
 *
 * Hay dos topes y truncan en direcciones OPUESTAS:
 *   - `parts` se llena desde dentro hacia fuera, así que al desbordarse
 *     pierde los ámbitos EXTERIORES (se iría el prefijo "main.").
 *   - `out` se escribe de fuera hacia dentro, así que al agotarse pierde
 *     los ámbitos INTERIORES.
 * Los dos casos marcan la truncación con '…': una ruta incompleta que no
 * se distingue de una completa es peor que no imprimir nada, y esta es
 * justo la tabla que se supone que hay que creerse. Hacen falta 32
 * ámbitos con nombre anidados para llegar a esto. */
static const char* scope_path(Sem* S, char* out, size_t cap) {
    const char* parts[32];
    const int max_parts = (int)(sizeof(parts) / sizeof(parts[0]));
    int n = 0;
    int cut_outer = 0, cut_inner = 0;

    for (Scope* s = S->scope; s; s = s->parent) {
        if (!s->label) continue;
        if (n == max_parts) { cut_outer = 1; break; }
        parts[n++] = s->label;
    }

    out[0] = '\0';
    size_t len = 0;
    if (cut_outer && cap > 3) { strcpy(out, "…"); len = 3; }  /* '…' = 3 bytes */

    for (int i = n - 1; i >= 0; i--) {
        size_t need = strlen(parts[i]) + (len ? 1 : 0);
        if (len + need + 1 > cap) { cut_inner = 1; break; }
        if (len) out[len++] = '.';
        strcpy(out + len, parts[i]);
        len += strlen(parts[i]);
    }

    if (cut_inner) {
        const char* mark = ".…";              /* 1 + 3 bytes */
        size_t mlen = strlen(mark);
        if (len + mlen + 1 <= cap) {          /* cabe al final */
            strcpy(out + len, mark);
        } else if (cap > mlen) {              /* no cabe: pisar la cola */
            strcpy(out + (cap - mlen - 1), mark);
        }
    }
    return out;
}

static void scope_pop(Sem* S) {
    Scope* s = S->scope;
    Symbol* sym = s->head;
    while (sym) {
        Symbol* nx = sym->next;
        free(sym->name);
        kel_type_free(sym->type);
        free(sym);
        sym = nx;
    }
    S->scope = s->parent;
    free(s);
}

static Symbol* scope_find_local(Scope* s, const char* name) {
    for (Symbol* sy = s->head; sy; sy = sy->next)
        if (strcmp(sy->name, name) == 0) return sy;
    return NULL;
}

static Symbol* lookup(Sem* S, const char* name) {
    for (Scope* s = S->scope; s; s = s->parent) {
        Symbol* sy = scope_find_local(s, name);
        if (sy) return sy;
    }
    return NULL;
}

/* Devuelve el literal del inicializador como texto, o NULL si no es una
 * constante conocida en compilación. El llamante libera. */
static char* const_text(const Node* init) {
    if (!init) return NULL;
    char buf[128];
    switch (init->kind) {
        case N_INT_LIT:   snprintf(buf, sizeof(buf), "%lld", init->int_val); break;
        /* %.15g y no %g: %g da 6 dígitos significativos, así que
         * 3.14159265358979 saldría como 3.14159 y parecería que el
         * compilador leyó mal el literal. %.15g va y vuelve sin
         * artefactos donde %.17g daría 3.1415000000000002. */
        case N_FLOAT_LIT: snprintf(buf, sizeof(buf), "%.15g", init->float_val); break;
        case N_BOOL_LIT:  snprintf(buf, sizeof(buf), "%s", init->bool_val ? "true" : "false"); break;
        case N_STR_LIT: {
            /* Truncar con snprintf a secas se comería la comilla de cierre
             * y la cadena parecería sin terminar. Cortamos explícitamente y
             * marcamos con '…' dentro de las comillas. */
            const char* s = init->str_val ? init->str_val : "";
            const size_t max_bytes = 100;   /* cabe en buf con margen */
            if (strlen(s) <= max_bytes) {
                snprintf(buf, sizeof(buf), "\"%s\"", s);
            } else {
                /* No partir un carácter UTF-8: retroceder mientras el byte
                 * de corte sea una continuación (10xxxxxx). */
                size_t cut = max_bytes;
                while (cut > 0 && ((unsigned char)s[cut] & 0xC0) == 0x80) cut--;
                snprintf(buf, sizeof(buf), "\"%.*s…\"", (int)cut, s);
            }
            break;
        }
        default: return NULL;
    }
    return strdup(buf);
}

static int declare(Sem* S, const char* name, KelType* type, int mutable, const Node* where) {
    if (scope_find_local(S->scope, name)) {
        err(S, where, "'%s' ya está declarado en este ámbito", name);
        return 0;
    }
    Symbol* sy = (Symbol*)calloc(1, sizeof(Symbol));
    sy->name = strdup(name);
    sy->type = kel_type_clone(type);
    sy->is_mutable = mutable;
    sy->next = S->scope->head;
    S->scope->head = sy;

    /* Grabar en el log para --symbols. Alineamos el cursor del marco al
     * tamaño del tipo antes de asignar el desplazamiento. */
    int align = kel_type_align(type);
    S->frame_offset = (S->frame_offset + align - 1) / align * align;
    int off = S->frame_offset;
    S->frame_offset += kel_type_size(type);

    char path[128];
    scope_path(S, path, sizeof(path));
    char* val = (where && where->kind == N_VAR_DECL) ? const_text(where->decl_init) : NULL;
    kel_symlog_add(path, name, type, mutable, off, where ? where->line : 0, val);
    free(val);

    return 1;
}

/* ---------- Funciones ---------- */

static FnSig* fn_find(Sem* S, const char* name) {
    for (FnSig* f = S->fns; f; f = f->next)
        if (strcmp(f->name, name) == 0) return f;
    return NULL;
}

static void fn_register(Sem* S, Node* fn) {
    if (fn_find(S, fn->fn_name)) {
        err(S, fn, "función '%s' ya definida", fn->fn_name);
        return;
    }
    FnSig* sig = (FnSig*)calloc(1, sizeof(FnSig));
    sig->name = fn->fn_name;
    sig->params = fn->params;
    sig->param_count = fn->param_count;
    sig->ret_type = fn->ret_type;
    sig->next = S->fns;
    S->fns = sig;
}

/* ---------- Check de expresiones: retorna KelType* owned ---------- */

static KelType* check_expr(Sem* S, Node* n);
static KelType* check_expr_h(Sem* S, Node* n, const KelType* hint);

static KelType* check_binop(Sem* S, Node* n) {
    KelType* l = check_expr(S, n->lhs);
    KelType* r = check_expr(S, n->rhs);
    KelType* res = NULL;
    const char* op = n->op;

    if (!l || !r) goto out;

    /* aritméticos + - * / % */
    if (!strcmp(op, "+") || !strcmp(op, "-") || !strcmp(op, "*") ||
        !strcmp(op, "/") || !strcmp(op, "%")) {
        if (!strcmp(op, "+") && l->kind == KT_STRING && r->kind == KT_STRING) {
            res = mk(KT_STRING);
        } else if ((l->kind == KT_INT && r->kind == KT_INT)) {
            res = mk(KT_INT);
        } else if (!strcmp(op, "%")) {
            err(S, n, "'%%' requiere operandos int (encontrado %s y %s)",
                kel_type_name(l), kel_type_name(r));
        } else if (l->kind == KT_FLOAT && r->kind == KT_FLOAT) {
            res = mk(KT_FLOAT);
        } else {
            err(S, n, "operador '%s' no aplicable a %s y %s",
                op, kel_type_name(l), kel_type_name(r));
        }
        goto out;
    }
    /* comparación < > <= >= */
    if (!strcmp(op, "<") || !strcmp(op, ">") || !strcmp(op, "<=") || !strcmp(op, ">=")) {
        if (type_eq(l, r) && (l->kind == KT_INT || l->kind == KT_FLOAT)) {
            res = mk(KT_BOOL);
        } else {
            err(S, n, "comparación '%s' requiere dos int o dos float (encontrado %s y %s)",
                op, kel_type_name(l), kel_type_name(r));
        }
        goto out;
    }
    /* igualdad == != */
    if (!strcmp(op, "==") || !strcmp(op, "!=")) {
        if (type_eq(l, r)) res = mk(KT_BOOL);
        else err(S, n, "'%s' requiere operandos del mismo tipo (encontrado %s y %s)",
                 op, kel_type_name(l), kel_type_name(r));
        goto out;
    }
    /* lógicos && || */
    if (!strcmp(op, "&&") || !strcmp(op, "||")) {
        if (l->kind == KT_BOOL && r->kind == KT_BOOL) res = mk(KT_BOOL);
        else err(S, n, "'%s' requiere operandos bool (encontrado %s y %s)",
                 op, kel_type_name(l), kel_type_name(r));
        goto out;
    }

    err(S, n, "operador desconocido '%s'", op);
out:
    kel_type_free(l);
    kel_type_free(r);
    return res;
}

static KelType* check_unop(Sem* S, Node* n) {
    KelType* t = check_expr(S, n->lhs);
    if (!t) return NULL;
    KelType* res = NULL;
    if (!strcmp(n->op, "!")) {
        if (t->kind == KT_BOOL) res = mk(KT_BOOL);
        else err(S, n, "'!' requiere bool, encontrado %s", kel_type_name(t));
    } else if (!strcmp(n->op, "-")) {
        if (t->kind == KT_INT || t->kind == KT_FLOAT) res = kel_type_clone(t);
        else err(S, n, "'-' unario requiere int o float, encontrado %s", kel_type_name(t));
    }
    kel_type_free(t);
    return res;
}

static int is_printable(KelType* t) {
    return t && (t->kind == KT_INT || t->kind == KT_FLOAT ||
                 t->kind == KT_BOOL || t->kind == KT_STRING);
}

static KelType* check_call(Sem* S, Node* n) {
    /* Callee debe ser Ident */
    if (n->lhs->kind != N_IDENT) {
        err(S, n, "solo se pueden llamar identificadores");
        return NULL;
    }
    const char* name = n->lhs->str_val;

    /* println built-in */
    if (strcmp(name, "println") == 0) {
        if (n->item_count != 1) {
            err(S, n, "println requiere exactamente 1 argumento (recibió %zu)", n->item_count);
        } else {
            KelType* at = check_expr(S, n->items[0]);
            if (at && !is_printable(at))
                err(S, n, "println no acepta argumentos de tipo %s", kel_type_name(at));
            kel_type_free(at);
        }
        return mk(KT_VOID);
    }

    FnSig* f = fn_find(S, name);
    if (!f) {
        err(S, n, "función '%s' no definida", name);
        /* aún así chequear args */
        for (size_t i = 0; i < n->item_count; i++) kel_type_free(check_expr(S, n->items[i]));
        return NULL;
    }
    if (n->item_count != f->param_count) {
        err(S, n, "función '%s' espera %zu argumento(s), recibió %zu",
            name, f->param_count, n->item_count);
    }
    size_t min = n->item_count < f->param_count ? n->item_count : f->param_count;
    for (size_t i = 0; i < n->item_count; i++) {
        const KelType* hint = (i < min) ? f->params[i].type : NULL;
        KelType* at = check_expr_h(S, n->items[i], hint);
        if (i < min && at && !type_eq(at, f->params[i].type)) {
            err(S, n->items[i], "argumento %zu de '%s': esperado %s, encontrado %s",
                i + 1, name, kel_type_name(f->params[i].type), kel_type_name(at));
        }
        kel_type_free(at);
    }
    return kel_type_clone(f->ret_type);
}

static KelType* check_index(Sem* S, Node* n) {
    KelType* arr = check_expr(S, n->lhs);
    KelType* idx = check_expr(S, n->rhs);
    KelType* res = NULL;
    if (arr && arr->kind != KT_ARRAY)
        err(S, n, "se intentó indexar valor de tipo %s", kel_type_name(arr));
    else if (arr)
        res = kel_type_clone(arr->elem);
    if (idx && idx->kind != KT_INT)
        err(S, n->rhs, "índice debe ser int, encontrado %s", kel_type_name(idx));
    kel_type_free(arr);
    kel_type_free(idx);
    return res;
}

static KelType* check_array_lit(Sem* S, Node* n, const KelType* hint) {
    /* Arrays vacíos: requieren hint (tipo esperado del contexto). */
    if (n->item_count == 0) {
        if (hint && hint->kind == KT_ARRAY)
            return kel_type_clone(hint);
        err(S, n, "literal de array vacío sin tipo esperado del contexto");
        return NULL;
    }
    KelType* first = check_expr(S, n->items[0]);
    if (!first) return NULL;
    for (size_t i = 1; i < n->item_count; i++) {
        KelType* t = check_expr(S, n->items[i]);
        if (t && !type_eq(first, t))
            err(S, n->items[i], "elemento %zu del array es %s, esperado %s",
                i + 1, kel_type_name(t), kel_type_name(first));
        kel_type_free(t);
    }
    KelType* arr = mk(KT_ARRAY);
    arr->elem = first;
    return arr;
}

static KelType* check_expr_inner(Sem* S, Node* n, const KelType* hint) {
    if (!n) return NULL;
    switch (n->kind) {
        case N_INT_LIT:   return mk(KT_INT);
        case N_FLOAT_LIT: return mk(KT_FLOAT);
        case N_BOOL_LIT:  return mk(KT_BOOL);
        case N_STR_LIT:   return mk(KT_STRING);
        case N_IDENT: {
            Symbol* sy = lookup(S, n->str_val);
            if (!sy) { err(S, n, "variable '%s' no declarada", n->str_val); return NULL; }
            return kel_type_clone(sy->type);
        }
        case N_BINOP:     return check_binop(S, n);
        case N_UNOP:      return check_unop(S, n);
        case N_CALL:      return check_call(S, n);
        case N_INDEX:     return check_index(S, n);
        case N_ARRAY_LIT: return check_array_lit(S, n, hint);
        default:
            err(S, n, "nodo de expresión inesperado");
            return NULL;
    }
}

static KelType* check_expr_h(Sem* S, Node* n, const KelType* hint) {
    KelType* t = check_expr_inner(S, n, hint);
    if (n && t) {
        kel_type_free(n->inferred_type);
        n->inferred_type = kel_type_clone(t);
    }
    return t;
}

static KelType* check_expr(Sem* S, Node* n) { return check_expr_h(S, n, NULL); }

/* ---------- Statements ---------- */

static void check_stmt(Sem* S, Node* n);

static void check_block(Sem* S, Node* n) {
    scope_push(S, NULL);
    for (size_t i = 0; i < n->item_count; i++) check_stmt(S, n->items[i]);
    scope_pop(S);
}

static void check_var_decl(Sem* S, Node* n) {
    KelType* init_t = check_expr_h(S, n->decl_init, n->decl_type);
    KelType* final_t = NULL;
    if (n->decl_type) {
        if (init_t && !type_eq(init_t, n->decl_type))
            err(S, n, "inicializador de '%s': esperado %s, encontrado %s",
                n->decl_name, kel_type_name(n->decl_type), kel_type_name(init_t));
        final_t = kel_type_clone(n->decl_type);
    } else if (init_t) {
        final_t = kel_type_clone(init_t);
    }
    if (final_t) declare(S, n->decl_name, final_t, n->is_mutable, n);
    kel_type_free(init_t);
    kel_type_free(final_t);
}

static void check_assign(Sem* S, Node* n) {
    Symbol* sy = lookup(S, n->decl_name);
    KelType* rhs = check_expr_h(S, n->decl_init, sy ? sy->type : NULL);
    if (!sy) {
        err(S, n, "variable '%s' no declarada", n->decl_name);
    } else {
        if (!sy->is_mutable)
            err(S, n, "no se puede reasignar 'val %s'", n->decl_name);
        if (rhs && !type_eq(rhs, sy->type))
            err(S, n, "asignación a '%s': esperado %s, encontrado %s",
                n->decl_name, kel_type_name(sy->type), kel_type_name(rhs));
    }
    kel_type_free(rhs);
}

static void check_index_assign(Sem* S, Node* n) {
    /* lhs es N_INDEX */
    Node* idx = n->lhs;
    KelType* elem_t = check_expr(S, idx); /* ya reporta si mal */
    KelType* rhs = check_expr(S, n->rhs);
    if (elem_t && rhs && !type_eq(elem_t, rhs))
        err(S, n, "asignación a elemento de array: esperado %s, encontrado %s",
            kel_type_name(elem_t), kel_type_name(rhs));
    /* también hay que verificar mutabilidad del array base */
    Node* base = idx->lhs;
    if (base && base->kind == N_IDENT) {
        Symbol* sy = lookup(S, base->str_val);
        if (sy && !sy->is_mutable)
            err(S, n, "no se puede modificar elemento de 'val %s'", base->str_val);
    }
    kel_type_free(elem_t);
    kel_type_free(rhs);
}

static void check_if(Sem* S, Node* n) {
    KelType* c = check_expr(S, n->cond);
    if (c && c->kind != KT_BOOL)
        err(S, n->cond, "condición de 'if' debe ser bool, encontrado %s", kel_type_name(c));
    kel_type_free(c);
    check_stmt(S, n->then_branch);
    if (n->else_branch) check_stmt(S, n->else_branch);
}

static void check_while(Sem* S, Node* n) {
    KelType* c = check_expr(S, n->cond);
    if (c && c->kind != KT_BOOL)
        err(S, n->cond, "condición de 'while' debe ser bool, encontrado %s", kel_type_name(c));
    kel_type_free(c);
    check_stmt(S, n->body);
}

static void check_for(Sem* S, Node* n) {
    KelType* a = check_expr(S, n->range_start);
    KelType* b = check_expr(S, n->range_end);
    if (a && a->kind != KT_INT)
        err(S, n->range_start, "inicio de rango debe ser int, encontrado %s", kel_type_name(a));
    if (b && b->kind != KT_INT)
        err(S, n->range_end, "fin de rango debe ser int, encontrado %s", kel_type_name(b));
    kel_type_free(a); kel_type_free(b);

    scope_push(S, "for");
    KelType* it = mk(KT_INT);
    declare(S, n->loop_var, it, 0, n);
    kel_type_free(it);
    /* El body es un N_BLOCK; check_block abre su propio scope anidado. */
    check_stmt(S, n->body);
    scope_pop(S);
}

/* ---------- Análisis de rutas de retorno (sug. 3) ---------- */

static int stmt_always_returns(const Node* s) {
    if (!s) return 0;
    switch (s->kind) {
        case N_RETURN: return 1;
        case N_BLOCK:
            for (size_t i = 0; i < s->item_count; i++)
                if (stmt_always_returns(s->items[i])) return 1;
            return 0;
        case N_IF:
            return s->else_branch &&
                   stmt_always_returns(s->then_branch) &&
                   stmt_always_returns(s->else_branch);
        default:
            return 0;
    }
}

static void check_return(Sem* S, Node* n) {
    KelType* t = n->lhs ? check_expr_h(S, n->lhs, S->current_ret) : mk(KT_VOID);
    if (!S->current_ret) { kel_type_free(t); return; }
    if (S->current_ret->kind == KT_VOID) {
        if (t && t->kind != KT_VOID)
            err(S, n, "función void no debe retornar valor");
    } else {
        if (!t || !type_eq(t, S->current_ret))
            err(S, n, "return: esperado %s, encontrado %s",
                kel_type_name(S->current_ret), t ? kel_type_name(t) : "void");
    }
    kel_type_free(t);
}

static void check_stmt(Sem* S, Node* n) {
    if (!n) return;
    switch (n->kind) {
        case N_VAR_DECL:     check_var_decl(S, n); break;
        case N_ASSIGN:       check_assign(S, n); break;
        case N_INDEX_ASSIGN: check_index_assign(S, n); break;
        case N_IF:           check_if(S, n); break;
        case N_WHILE:        check_while(S, n); break;
        case N_FOR:          check_for(S, n); break;
        case N_RETURN:       check_return(S, n); break;
        case N_EXPR_STMT: {
            KelType* t = check_expr(S, n->lhs);
            kel_type_free(t);
            break;
        }
        case N_BLOCK:        check_block(S, n); break;
        default:
            err(S, n, "nodo de statement inesperado");
            break;
    }
}

/* ---------- Función y programa ---------- */

static void check_function(Sem* S, Node* fn) {
    S->current_ret = fn->ret_type;
    S->frame_offset = 0;
    scope_push(S, fn->fn_name);
    for (size_t i = 0; i < fn->param_count; i++)
        declare(S, fn->params[i].name, fn->params[i].type, 1, fn);
    /* body es N_BLOCK pero ya estamos en un scope; recorrer directo */
    for (size_t i = 0; i < fn->body->item_count; i++)
        check_stmt(S, fn->body->items[i]);
    scope_pop(S);

    /* Sug. 3: toda función no-void debe retornar en todas las rutas. */
    if (fn->ret_type && fn->ret_type->kind != KT_VOID) {
        if (!stmt_always_returns(fn->body))
            err(S, fn, "la función '%s' no retorna en todas las rutas (esperado %s)",
                fn->fn_name, kel_type_name(fn->ret_type));
    }

    S->current_ret = NULL;
}

SemResult kel_analyze(Node* prog) {
    Sem S;
    S.scope = NULL;
    S.fns = NULL;
    S.current_ret = NULL;
    S.errors = 0;

    /* Pass 1: registrar funciones */
    int has_main = 0;
    for (size_t i = 0; i < prog->item_count; i++) {
        Node* fn = prog->items[i];
        fn_register(&S, fn);
        if (strcmp(fn->fn_name, "main") == 0) {
            has_main = 1;
            if (fn->param_count != 0)
                err(&S, fn, "main no debe tener parámetros");
            if (fn->ret_type->kind != KT_VOID)
                err(&S, fn, "main debe ser void");
        }
    }
    if (!has_main)
        fprintf(stderr, "[Kel Error] falta función 'main'\n"), S.errors++;

    /* Pass 2: cuerpos */
    for (size_t i = 0; i < prog->item_count; i++)
        check_function(&S, prog->items[i]);

    /* Libera fn signatures (solo contenedor) */
    while (S.fns) { FnSig* nx = S.fns->next; free(S.fns); S.fns = nx; }

    SemResult r;
    r.errors = S.errors;
    r.had_error = S.errors > 0;
    return r;
}
