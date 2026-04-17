#ifndef KEL_PARSER_H
#define KEL_PARSER_H

#include "lexer.h"
#include <stddef.h>

/* ---------- Tipos ---------- */

typedef enum {
    KT_INT, KT_FLOAT, KT_BOOL, KT_STRING,
    KT_ARRAY,      /* wrapper: elem */
    KT_VOID,       /* función sin retorno */
    KT_UNKNOWN     /* inferencia pendiente */
} KelTypeKind;

typedef struct KelType {
    KelTypeKind kind;
    struct KelType* elem; /* para KT_ARRAY */
} KelType;

/* ---------- Nodos de AST ---------- */

typedef enum {
    /* expresiones */
    N_INT_LIT, N_FLOAT_LIT, N_BOOL_LIT, N_STR_LIT,
    N_IDENT,
    N_BINOP, N_UNOP,
    N_CALL,
    N_INDEX,
    N_ARRAY_LIT,
    /* statements */
    N_VAR_DECL,
    N_ASSIGN,
    N_INDEX_ASSIGN,
    N_IF,
    N_WHILE,
    N_FOR,
    N_RETURN,
    N_EXPR_STMT,
    N_BLOCK,
    N_PRINTLN,
    /* top level */
    N_FUNCTION,
    N_PROGRAM
} NodeKind;

typedef struct Node Node;

typedef struct {
    char* name;
    KelType* type;
} Param;

struct Node {
    NodeKind kind;
    int line, col;

    /* literales */
    long long int_val;
    double     float_val;
    int        bool_val;
    char*      str_val;   /* también usado como nombre para IDENT */

    /* binop/unop: value = operador (token lexema) */
    char* op;

    /* hijos genéricos */
    Node*  lhs;
    Node*  rhs;
    Node*  cond;
    Node*  then_branch;
    Node*  else_branch;

    /* listas */
    Node** items;       /* array_lit elems, call args, block stmts, program funcs */
    size_t item_count;

    /* var_decl */
    int      is_mutable;   /* 0 = val, 1 = var */
    char*    decl_name;
    KelType* decl_type;    /* puede ser NULL si inferido */
    Node*    decl_init;

    /* for */
    char* loop_var;
    Node* range_start;
    Node* range_end;

    /* function */
    char*    fn_name;
    Param*   params;
    size_t   param_count;
    KelType* ret_type;
    Node*    body;

    /* Anotación de la fase semántica (solo para nodos de expresión). */
    KelType* inferred_type;
};

typedef struct {
    Node* root;       /* N_PROGRAM */
    int   had_error;
} ParseResult;

ParseResult kel_parse(const TokenList* tokens);
void kel_free_ast(Node* n);
void kel_print_ast(const Node* n);

/* util para otras fases */
const char* kel_type_name(const KelType* t);

#endif
