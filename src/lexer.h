#ifndef KEL_LEXER_H
#define KEL_LEXER_H

#include <stddef.h>

typedef enum {
    TOKEN_VAL, TOKEN_VAR, TOKEN_FN, TOKEN_RETURN,
    TOKEN_IF, TOKEN_ELSE, TOKEN_WHILE, TOKEN_FOR, TOKEN_IN,
    TOKEN_TRUE, TOKEN_FALSE, TOKEN_PRINTLN,
    TOKEN_TYPE_INT, TOKEN_TYPE_FLOAT, TOKEN_TYPE_BOOL, TOKEN_TYPE_STRING,

    TOKEN_PLUS, TOKEN_MINUS, TOKEN_STAR, TOKEN_SLASH, TOKEN_PERCENT,
    TOKEN_ASSIGN, TOKEN_EQ, TOKEN_NEQ,
    TOKEN_LT, TOKEN_GT, TOKEN_LTE, TOKEN_GTE,
    TOKEN_AND, TOKEN_OR, TOKEN_NOT,
    TOKEN_ARROW, TOKEN_RANGE,

    TOKEN_LPAREN, TOKEN_RPAREN,
    TOKEN_LBRACE, TOKEN_RBRACE,
    TOKEN_LBRACKET, TOKEN_RBRACKET,
    TOKEN_COLON, TOKEN_COMMA,

    TOKEN_INT_LIT, TOKEN_FLOAT_LIT, TOKEN_STR_LIT,
    TOKEN_IDENT,
    TOKEN_EOF,
    TOKEN_ERROR
} TokenType;

typedef struct {
    TokenType type;
    char* value;
    int line;
    int col;
} Token;

typedef struct {
    Token* tokens;
    size_t count;
    size_t capacity;
    int had_error;
} TokenList;

/* Lee el archivo completo a memoria (malloc). Retorna NULL si falla. */
char* kel_read_file(const char* path);

/* Tokeniza una cadena de código Kel. El resultado debe liberarse con kel_free_tokens. */
TokenList kel_tokenize(const char* source);

void kel_free_tokens(TokenList* list);

/* Nombre legible del tipo de token (para debug / flag --tokens). */
const char* kel_token_name(TokenType t);

/* Imprime la lista de tokens en formato tabular. */
void kel_print_tokens(const TokenList* list);

#endif
