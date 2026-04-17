#include "parser.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

/* ---------- Estado del parser ---------- */

typedef struct {
    const Token* tokens;
    size_t count;
    size_t pos;
    int had_error;
    int panic;
} Parser;

static const Token* peek(Parser* p)        { return &p->tokens[p->pos]; }
static const Token* advance(Parser* p) {
    const Token* t = &p->tokens[p->pos];
    if (t->type != TOKEN_EOF) p->pos++;
    return t;
}
static int check(Parser* p, TokenType t) { return peek(p)->type == t; }
static int match(Parser* p, TokenType t) {
    if (!check(p, t)) return 0;
    advance(p);
    return 1;
}

static void error_at(Parser* p, const Token* t, const char* msg) {
    if (p->panic) return;
    p->panic = 1;
    p->had_error = 1;
    kel_diag_error(t->line, t->col, "%s (token '%s')", msg, t->value);
}

static const Token* expect(Parser* p, TokenType t, const char* msg) {
    if (check(p, t)) return advance(p);
    error_at(p, peek(p), msg);
    return peek(p);
}

static void sync_to_stmt(Parser* p) {
    p->panic = 0;
    while (!check(p, TOKEN_EOF)) {
        TokenType t = peek(p)->type;
        if (t == TOKEN_VAL || t == TOKEN_VAR || t == TOKEN_IF ||
            t == TOKEN_WHILE || t == TOKEN_FOR || t == TOKEN_RETURN ||
            t == TOKEN_FN || t == TOKEN_RBRACE || t == TOKEN_PRINTLN)
            return;
        advance(p);
    }
}

/* ---------- Constructores ---------- */

static Node* new_node(NodeKind k, int line, int col) {
    Node* n = (Node*)calloc(1, sizeof(Node));
    n->kind = k;
    n->line = line;
    n->col = col;
    return n;
}

static void push_item(Node* n, Node* child) {
    n->items = (Node**)realloc(n->items, sizeof(Node*) * (n->item_count + 1));
    n->items[n->item_count++] = child;
}

static KelType* new_type(KelTypeKind k) {
    KelType* t = (KelType*)calloc(1, sizeof(KelType));
    t->kind = k;
    return t;
}

static void free_type(KelType* t) {
    if (!t) return;
    free_type(t->elem);
    free(t);
}

/* ---------- Forward ---------- */
static Node* parse_expr(Parser* p);
static Node* parse_block(Parser* p);
static Node* parse_stmt(Parser* p);
static KelType* parse_type(Parser* p);

/* ---------- Tipos ---------- */

static KelType* parse_type(Parser* p) {
    const Token* t = peek(p);
    switch (t->type) {
        case TOKEN_TYPE_INT:    advance(p); return new_type(KT_INT);
        case TOKEN_TYPE_FLOAT:  advance(p); return new_type(KT_FLOAT);
        case TOKEN_TYPE_BOOL:   advance(p); return new_type(KT_BOOL);
        case TOKEN_TYPE_STRING: advance(p); return new_type(KT_STRING);
        case TOKEN_LBRACKET: {
            advance(p);
            KelType* elem = parse_type(p);
            expect(p, TOKEN_RBRACKET, "se esperaba ']' en tipo de array");
            KelType* arr = new_type(KT_ARRAY);
            arr->elem = elem;
            return arr;
        }
        default:
            error_at(p, t, "se esperaba un tipo");
            return new_type(KT_UNKNOWN);
    }
}

/* ---------- Expresiones (precedencia de menor a mayor) ---------- */

static Node* parse_primary(Parser* p) {
    const Token* t = peek(p);
    switch (t->type) {
        case TOKEN_INT_LIT: {
            advance(p);
            Node* n = new_node(N_INT_LIT, t->line, t->col);
            n->int_val = strtoll(t->value, NULL, 10);
            return n;
        }
        case TOKEN_FLOAT_LIT: {
            advance(p);
            Node* n = new_node(N_FLOAT_LIT, t->line, t->col);
            n->float_val = strtod(t->value, NULL);
            return n;
        }
        case TOKEN_TRUE: case TOKEN_FALSE: {
            advance(p);
            Node* n = new_node(N_BOOL_LIT, t->line, t->col);
            n->bool_val = (t->type == TOKEN_TRUE);
            return n;
        }
        case TOKEN_STR_LIT: {
            advance(p);
            Node* n = new_node(N_STR_LIT, t->line, t->col);
            n->str_val = strdup(t->value);
            return n;
        }
        case TOKEN_LPAREN: {
            advance(p);
            Node* e = parse_expr(p);
            expect(p, TOKEN_RPAREN, "se esperaba ')'");
            return e;
        }
        case TOKEN_LBRACKET: {
            advance(p);
            Node* n = new_node(N_ARRAY_LIT, t->line, t->col);
            if (!check(p, TOKEN_RBRACKET)) {
                push_item(n, parse_expr(p));
                while (match(p, TOKEN_COMMA))
                    push_item(n, parse_expr(p));
            }
            expect(p, TOKEN_RBRACKET, "se esperaba ']' en literal de array");
            return n;
        }
        case TOKEN_IDENT: case TOKEN_PRINTLN: {
            advance(p);
            Node* n = new_node(N_IDENT, t->line, t->col);
            n->str_val = strdup(t->value);
            return n;
        }
        default:
            error_at(p, t, "se esperaba una expresión");
            advance(p);
            Node* n = new_node(N_INT_LIT, t->line, t->col);
            return n;
    }
}

static Node* parse_postfix(Parser* p) {
    Node* e = parse_primary(p);
    for (;;) {
        const Token* t = peek(p);
        if (t->type == TOKEN_LPAREN) {
            advance(p);
            Node* call = new_node(N_CALL, t->line, t->col);
            call->lhs = e;
            if (!check(p, TOKEN_RPAREN)) {
                push_item(call, parse_expr(p));
                while (match(p, TOKEN_COMMA))
                    push_item(call, parse_expr(p));
            }
            expect(p, TOKEN_RPAREN, "se esperaba ')' en llamada");
            e = call;
        } else if (t->type == TOKEN_LBRACKET) {
            advance(p);
            Node* idx = new_node(N_INDEX, t->line, t->col);
            idx->lhs = e;
            idx->rhs = parse_expr(p);
            expect(p, TOKEN_RBRACKET, "se esperaba ']' en indexado");
            e = idx;
        } else break;
    }
    return e;
}

static Node* parse_unary(Parser* p) {
    const Token* t = peek(p);
    if (t->type == TOKEN_NOT || t->type == TOKEN_MINUS) {
        advance(p);
        Node* n = new_node(N_UNOP, t->line, t->col);
        n->op = strdup(t->value);
        n->lhs = parse_unary(p);
        return n;
    }
    return parse_postfix(p);
}

static Node* make_binop(const Token* t, Node* l, Node* r) {
    Node* n = new_node(N_BINOP, t->line, t->col);
    n->op = strdup(t->value);
    n->lhs = l;
    n->rhs = r;
    return n;
}

static Node* parse_mul(Parser* p) {
    Node* l = parse_unary(p);
    while (check(p, TOKEN_STAR) || check(p, TOKEN_SLASH) || check(p, TOKEN_PERCENT)) {
        const Token* t = advance(p);
        Node* r = parse_unary(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_add(Parser* p) {
    Node* l = parse_mul(p);
    while (check(p, TOKEN_PLUS) || check(p, TOKEN_MINUS)) {
        const Token* t = advance(p);
        Node* r = parse_mul(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_cmp(Parser* p) {
    Node* l = parse_add(p);
    while (check(p, TOKEN_LT) || check(p, TOKEN_GT) ||
           check(p, TOKEN_LTE) || check(p, TOKEN_GTE)) {
        const Token* t = advance(p);
        Node* r = parse_add(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_eq(Parser* p) {
    Node* l = parse_cmp(p);
    while (check(p, TOKEN_EQ) || check(p, TOKEN_NEQ)) {
        const Token* t = advance(p);
        Node* r = parse_cmp(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_and(Parser* p) {
    Node* l = parse_eq(p);
    while (check(p, TOKEN_AND)) {
        const Token* t = advance(p);
        Node* r = parse_eq(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_or(Parser* p) {
    Node* l = parse_and(p);
    while (check(p, TOKEN_OR)) {
        const Token* t = advance(p);
        Node* r = parse_and(p);
        l = make_binop(t, l, r);
    }
    return l;
}

static Node* parse_expr(Parser* p) { return parse_or(p); }

/* ---------- Statements ---------- */

static Node* parse_var_decl(Parser* p, int is_mutable) {
    const Token* kw = advance(p); /* val|var */
    Node* n = new_node(N_VAR_DECL, kw->line, kw->col);
    n->is_mutable = is_mutable;

    const Token* name = expect(p, TOKEN_IDENT, "se esperaba nombre de variable");
    n->decl_name = strdup(name->value);

    if (match(p, TOKEN_COLON))
        n->decl_type = parse_type(p);

    expect(p, TOKEN_ASSIGN, "se esperaba '=' en declaración");
    n->decl_init = parse_expr(p);
    return n;
}

static Node* parse_if(Parser* p) {
    const Token* kw = advance(p);
    Node* n = new_node(N_IF, kw->line, kw->col);
    n->cond = parse_expr(p);
    n->then_branch = parse_block(p);
    if (match(p, TOKEN_ELSE)) {
        if (check(p, TOKEN_IF))
            n->else_branch = parse_if(p);
        else
            n->else_branch = parse_block(p);
    }
    return n;
}

static Node* parse_while(Parser* p) {
    const Token* kw = advance(p);
    Node* n = new_node(N_WHILE, kw->line, kw->col);
    n->cond = parse_expr(p);
    n->body = parse_block(p);
    return n;
}

static Node* parse_for(Parser* p) {
    const Token* kw = advance(p);
    Node* n = new_node(N_FOR, kw->line, kw->col);
    const Token* v = expect(p, TOKEN_IDENT, "se esperaba variable del for");
    n->loop_var = strdup(v->value);
    expect(p, TOKEN_IN, "se esperaba 'in' en for");
    n->range_start = parse_expr(p);
    expect(p, TOKEN_RANGE, "se esperaba '..' en rango de for");
    n->range_end = parse_expr(p);
    n->body = parse_block(p);
    return n;
}

static Node* parse_return(Parser* p) {
    const Token* kw = advance(p);
    Node* n = new_node(N_RETURN, kw->line, kw->col);
    if (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF))
        n->lhs = parse_expr(p);
    return n;
}

static Node* parse_expr_or_assign_stmt(Parser* p) {
    int line = peek(p)->line, col = peek(p)->col;
    Node* lhs = parse_expr(p);

    if (check(p, TOKEN_ASSIGN)) {
        advance(p);
        Node* rhs = parse_expr(p);

        if (lhs->kind == N_IDENT) {
            Node* n = new_node(N_ASSIGN, line, col);
            n->decl_name = strdup(lhs->str_val);
            n->decl_init = rhs;
            kel_free_ast(lhs);
            return n;
        }
        if (lhs->kind == N_INDEX) {
            Node* n = new_node(N_INDEX_ASSIGN, line, col);
            n->lhs = lhs;
            n->rhs = rhs;
            return n;
        }
        error_at(p, peek(p), "lado izquierdo inválido en asignación");
        return lhs;
    }

    Node* stmt = new_node(N_EXPR_STMT, line, col);
    stmt->lhs = lhs;
    return stmt;
}

static Node* parse_stmt(Parser* p) {
    const Token* t = peek(p);
    switch (t->type) {
        case TOKEN_VAL: return parse_var_decl(p, 0);
        case TOKEN_VAR: return parse_var_decl(p, 1);
        case TOKEN_IF:  return parse_if(p);
        case TOKEN_WHILE: return parse_while(p);
        case TOKEN_FOR: return parse_for(p);
        case TOKEN_RETURN: return parse_return(p);
        default:        return parse_expr_or_assign_stmt(p);
    }
}

static Node* parse_block(Parser* p) {
    const Token* lb = expect(p, TOKEN_LBRACE, "se esperaba '{'");
    Node* blk = new_node(N_BLOCK, lb->line, lb->col);
    while (!check(p, TOKEN_RBRACE) && !check(p, TOKEN_EOF)) {
        Node* s = parse_stmt(p);
        if (s) push_item(blk, s);
        if (p->panic) sync_to_stmt(p);
    }
    expect(p, TOKEN_RBRACE, "se esperaba '}'");
    return blk;
}

/* ---------- Funciones y programa ---------- */

static Node* parse_function(Parser* p) {
    const Token* kw = expect(p, TOKEN_FN, "se esperaba 'fn'");
    Node* fn = new_node(N_FUNCTION, kw->line, kw->col);

    const Token* name = expect(p, TOKEN_IDENT, "se esperaba nombre de función");
    fn->fn_name = strdup(name->value);

    expect(p, TOKEN_LPAREN, "se esperaba '(' en función");
    if (!check(p, TOKEN_RPAREN)) {
        do {
            const Token* pn = expect(p, TOKEN_IDENT, "se esperaba nombre de parámetro");
            expect(p, TOKEN_COLON, "se esperaba ':' tras nombre de parámetro");
            KelType* pt = parse_type(p);
            fn->params = (Param*)realloc(fn->params, sizeof(Param) * (fn->param_count + 1));
            fn->params[fn->param_count].name = strdup(pn->value);
            fn->params[fn->param_count].type = pt;
            fn->param_count++;
        } while (match(p, TOKEN_COMMA));
    }
    expect(p, TOKEN_RPAREN, "se esperaba ')' en función");

    if (match(p, TOKEN_ARROW))
        fn->ret_type = parse_type(p);
    else
        fn->ret_type = new_type(KT_VOID);

    fn->body = parse_block(p);
    return fn;
}

ParseResult kel_parse(const TokenList* tokens) {
    Parser p;
    p.tokens = tokens->tokens;
    p.count = tokens->count;
    p.pos = 0;
    p.had_error = 0;
    p.panic = 0;

    Node* prog = new_node(N_PROGRAM, 1, 1);
    while (!check(&p, TOKEN_EOF)) {
        Node* fn = parse_function(&p);
        if (fn) push_item(prog, fn);
        if (p.panic) {
            /* resync a siguiente fn */
            while (!check(&p, TOKEN_EOF) && !check(&p, TOKEN_FN)) advance(&p);
            p.panic = 0;
        }
    }

    ParseResult r;
    r.root = prog;
    r.had_error = p.had_error;
    return r;
}

/* ---------- Free ---------- */

static void free_params(Param* ps, size_t n) {
    for (size_t i = 0; i < n; i++) {
        free(ps[i].name);
        free_type(ps[i].type);
    }
    free(ps);
}

void kel_free_ast(Node* n) {
    if (!n) return;
    kel_free_ast(n->lhs);
    kel_free_ast(n->rhs);
    kel_free_ast(n->cond);
    kel_free_ast(n->then_branch);
    kel_free_ast(n->else_branch);
    kel_free_ast(n->decl_init);
    kel_free_ast(n->range_start);
    kel_free_ast(n->range_end);
    kel_free_ast(n->body);
    for (size_t i = 0; i < n->item_count; i++) kel_free_ast(n->items[i]);
    free(n->items);
    free(n->str_val);
    free(n->op);
    free(n->decl_name);
    free_type(n->decl_type);
    free(n->loop_var);
    free(n->fn_name);
    free_params(n->params, n->param_count);
    free_type(n->ret_type);
    free_type(n->inferred_type);
    free(n);
}

/* ---------- Pretty printer ---------- */

const char* kel_type_name(const KelType* t) {
    if (!t) return "<null>";
    switch (t->kind) {
        case KT_INT: return "int";
        case KT_FLOAT: return "float";
        case KT_BOOL: return "bool";
        case KT_STRING: return "string";
        case KT_VOID: return "void";
        case KT_UNKNOWN: return "?";
        case KT_ARRAY: {
            static char buf[64];
            snprintf(buf, sizeof(buf), "[%s]", kel_type_name(t->elem));
            return buf;
        }
    }
    return "?";
}

static void indent(int d) { for (int i = 0; i < d; i++) printf("  "); }

static void print_type_suffix(const Node* n) {
    if (n && n->inferred_type)
        printf("  : %s", kel_type_name(n->inferred_type));
}

static void print_node(const Node* n, int d) {
    if (!n) { indent(d); printf("<null>\n"); return; }
    indent(d);
    switch (n->kind) {
        case N_PROGRAM:
            printf("Program\n");
            for (size_t i = 0; i < n->item_count; i++) print_node(n->items[i], d + 1);
            break;
        case N_FUNCTION:
            printf("Function '%s' -> %s\n", n->fn_name, kel_type_name(n->ret_type));
            for (size_t i = 0; i < n->param_count; i++) {
                indent(d + 1);
                printf("Param '%s': %s\n", n->params[i].name, kel_type_name(n->params[i].type));
            }
            print_node(n->body, d + 1);
            break;
        case N_BLOCK:
            printf("Block\n");
            for (size_t i = 0; i < n->item_count; i++) print_node(n->items[i], d + 1);
            break;
        case N_VAR_DECL:
            printf("VarDecl %s '%s'%s%s\n",
                   n->is_mutable ? "var" : "val",
                   n->decl_name,
                   n->decl_type ? " : " : "",
                   n->decl_type ? kel_type_name(n->decl_type) : "");
            print_node(n->decl_init, d + 1);
            break;
        case N_ASSIGN:
            printf("Assign '%s'\n", n->decl_name);
            print_node(n->decl_init, d + 1);
            break;
        case N_INDEX_ASSIGN:
            printf("IndexAssign\n");
            print_node(n->lhs, d + 1);
            print_node(n->rhs, d + 1);
            break;
        case N_IF:
            printf("If\n");
            indent(d + 1); printf("cond:\n");   print_node(n->cond, d + 2);
            indent(d + 1); printf("then:\n");   print_node(n->then_branch, d + 2);
            if (n->else_branch) { indent(d + 1); printf("else:\n"); print_node(n->else_branch, d + 2); }
            break;
        case N_WHILE:
            printf("While\n");
            indent(d + 1); printf("cond:\n");  print_node(n->cond, d + 2);
            indent(d + 1); printf("body:\n");  print_node(n->body, d + 2);
            break;
        case N_FOR:
            printf("For '%s' in\n", n->loop_var);
            print_node(n->range_start, d + 1);
            indent(d + 1); printf("..\n");
            print_node(n->range_end, d + 1);
            print_node(n->body, d + 1);
            break;
        case N_RETURN:
            printf("Return\n");
            if (n->lhs) print_node(n->lhs, d + 1);
            break;
        case N_EXPR_STMT:
            printf("ExprStmt\n");
            print_node(n->lhs, d + 1);
            break;
        case N_BINOP:
            printf("BinOp '%s'", n->op); print_type_suffix(n); printf("\n");
            print_node(n->lhs, d + 1);
            print_node(n->rhs, d + 1);
            break;
        case N_UNOP:
            printf("UnOp '%s'", n->op); print_type_suffix(n); printf("\n");
            print_node(n->lhs, d + 1);
            break;
        case N_CALL:
            printf("Call"); print_type_suffix(n); printf("\n");
            print_node(n->lhs, d + 1);
            for (size_t i = 0; i < n->item_count; i++) print_node(n->items[i], d + 1);
            break;
        case N_INDEX:
            printf("Index"); print_type_suffix(n); printf("\n");
            print_node(n->lhs, d + 1);
            print_node(n->rhs, d + 1);
            break;
        case N_ARRAY_LIT:
            printf("ArrayLit"); print_type_suffix(n); printf("\n");
            for (size_t i = 0; i < n->item_count; i++) print_node(n->items[i], d + 1);
            break;
        case N_INT_LIT:   printf("Int %lld", n->int_val); print_type_suffix(n); printf("\n"); break;
        case N_FLOAT_LIT: printf("Float %g", n->float_val); print_type_suffix(n); printf("\n"); break;
        case N_BOOL_LIT:  printf("Bool %s", n->bool_val ? "true" : "false"); print_type_suffix(n); printf("\n"); break;
        case N_STR_LIT:   printf("Str \"%s\"", n->str_val); print_type_suffix(n); printf("\n"); break;
        case N_IDENT:     printf("Ident '%s'", n->str_val); print_type_suffix(n); printf("\n"); break;
        case N_PRINTLN:   printf("Println\n"); break;
    }
}

void kel_print_ast(const Node* n) { print_node(n, 0); }
