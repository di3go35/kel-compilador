#include "lexer.h"
#include "diag.h"

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

/* ---------- Utilidades internas ---------- */

typedef struct {
    const char* src;
    size_t pos;
    size_t len;
    int line;
    int col;
    TokenList out;
} Lexer;

static void list_push(TokenList* list, Token tok) {
    if (list->count + 1 > list->capacity) {
        size_t ncap = list->capacity == 0 ? 64 : list->capacity * 2;
        list->tokens = (Token*)realloc(list->tokens, ncap * sizeof(Token));
        list->capacity = ncap;
    }
    list->tokens[list->count++] = tok;
}

static char* str_dup_n(const char* s, size_t n) {
    char* r = (char*)malloc(n + 1);
    memcpy(r, s, n);
    r[n] = '\0';
    return r;
}

static char peek(Lexer* L)      { return L->pos < L->len ? L->src[L->pos] : '\0'; }
static char peek_at(Lexer* L, size_t o) { return (L->pos + o) < L->len ? L->src[L->pos + o] : '\0'; }

static char advance(Lexer* L) {
    char c = L->src[L->pos++];
    if (c == '\n') { L->line++; L->col = 1; }
    else           { L->col++; }
    return c;
}

static int match(Lexer* L, char expected) {
    if (peek(L) != expected) return 0;
    advance(L);
    return 1;
}

static void emit(Lexer* L, TokenType type, const char* start, size_t n, int line, int col) {
    Token t;
    t.type = type;
    t.value = str_dup_n(start, n);
    t.line = line;
    t.col = col;
    list_push(&L->out, t);
}

static void emit_lit(Lexer* L, TokenType type, const char* literal, int line, int col) {
    Token t;
    t.type = type;
    t.value = str_dup_n(literal, strlen(literal));
    t.line = line;
    t.col = col;
    list_push(&L->out, t);
}

static void error_at(Lexer* L, int line, int col, const char* msg, char c) {
    if (c) kel_diag_error(line, col, "%s '%c'", msg, c);
    else   kel_diag_error(line, col, "%s", msg);
    L->out.had_error = 1;
}

/* ---------- Keywords ---------- */

typedef struct { const char* kw; TokenType type; } Keyword;

static const Keyword KEYWORDS[] = {
    {"val", TOKEN_VAL}, {"var", TOKEN_VAR},
    {"fn", TOKEN_FN}, {"return", TOKEN_RETURN},
    {"if", TOKEN_IF}, {"else", TOKEN_ELSE},
    {"while", TOKEN_WHILE}, {"for", TOKEN_FOR}, {"in", TOKEN_IN},
    {"true", TOKEN_TRUE}, {"false", TOKEN_FALSE},
    {"println", TOKEN_PRINTLN},
    {"int", TOKEN_TYPE_INT}, {"float", TOKEN_TYPE_FLOAT},
    {"bool", TOKEN_TYPE_BOOL}, {"string", TOKEN_TYPE_STRING},
    {NULL, TOKEN_IDENT}
};

static TokenType keyword_lookup(const char* s, size_t n) {
    for (int i = 0; KEYWORDS[i].kw != NULL; i++) {
        if (strlen(KEYWORDS[i].kw) == n && strncmp(KEYWORDS[i].kw, s, n) == 0)
            return KEYWORDS[i].type;
    }
    return TOKEN_IDENT;
}

/* ---------- Scanners ---------- */

static int is_ident_start(char c) { return isalpha((unsigned char)c) || c == '_'; }
static int is_ident_cont(char c)  { return isalnum((unsigned char)c) || c == '_'; }

static void scan_identifier(Lexer* L) {
    int line = L->line, col = L->col;
    size_t start = L->pos;
    while (is_ident_cont(peek(L))) advance(L);
    size_t n = L->pos - start;
    TokenType t = keyword_lookup(L->src + start, n);
    emit(L, t, L->src + start, n, line, col);
}

/* Números: solo dígitos (+ punto para floats). El signo '-' NO se consume
 * aquí: se emite como TOKEN_MINUS y el parser lo trata como unario. Esto
 * evita ambigüedad en expresiones como `a-5` (resta) vs `a -5` (error).
 */
static void scan_number(Lexer* L) {
    int line = L->line, col = L->col;
    size_t start = L->pos;
    while (isdigit((unsigned char)peek(L))) advance(L);

    int is_float = 0;
    if (peek(L) == '.' && isdigit((unsigned char)peek_at(L, 1))) {
        is_float = 1;
        advance(L); /* . */
        while (isdigit((unsigned char)peek(L))) advance(L);
    }
    size_t n = L->pos - start;
    emit(L, is_float ? TOKEN_FLOAT_LIT : TOKEN_INT_LIT, L->src + start, n, line, col);
}

static void scan_string(Lexer* L) {
    int line = L->line, col = L->col;
    advance(L); /* consume " */
    size_t start = L->pos;
    while (peek(L) != '\0' && peek(L) != '"') {
        if (peek(L) == '\n') {
            error_at(L, L->line, L->col, "cadena sin cerrar", 0);
            return;
        }
        advance(L);
    }
    if (peek(L) == '\0') {
        error_at(L, line, col, "cadena sin cerrar al final del archivo", 0);
        return;
    }
    size_t n = L->pos - start;
    emit(L, TOKEN_STR_LIT, L->src + start, n, line, col);
    advance(L); /* consume " */
}

static void skip_block_comment(Lexer* L) {
    int line = L->line, col = L->col;
    advance(L); advance(L); /* consume los dos chars de apertura */
    while (!(peek(L) == '*' && peek_at(L, 1) == '/')) {
        if (peek(L) == '\0') {
            error_at(L, line, col, "comentario de bloque sin cerrar", 0);
            return;
        }
        advance(L);
    }
    advance(L); advance(L); /* consume */
}

static void skip_line_comment(Lexer* L) {
    while (peek(L) != '\0' && peek(L) != '\n') advance(L);
}

/* ---------- API pública ---------- */

char* kel_read_file(const char* path) {
    FILE* f = fopen(path, "rb");
    if (!f) return NULL;
    fseek(f, 0, SEEK_END);
    long sz = ftell(f);
    fseek(f, 0, SEEK_SET);
    char* buf = (char*)malloc((size_t)sz + 1);
    size_t rd = fread(buf, 1, (size_t)sz, f);
    buf[rd] = '\0';
    fclose(f);
    return buf;
}

TokenList kel_tokenize(const char* source) {
    Lexer L;
    L.src = source;
    L.pos = 0;
    L.len = strlen(source);
    L.line = 1;
    L.col = 1;
    L.out.tokens = NULL;
    L.out.count = 0;
    L.out.capacity = 0;
    L.out.had_error = 0;

    while (L.pos < L.len) {
        char c = peek(&L);

        /* whitespace */
        if (c == ' ' || c == '\t' || c == '\r' || c == '\n') {
            advance(&L);
            continue;
        }

        /* comentarios */
        if (c == '/' && peek_at(&L, 1) == '/') { skip_line_comment(&L); continue; }
        if (c == '/' && peek_at(&L, 1) == '*') { skip_block_comment(&L); continue; }

        /* identificadores / keywords */
        if (is_ident_start(c)) { scan_identifier(&L); continue; }

        /* números */
        if (isdigit((unsigned char)c)) { scan_number(&L); continue; }

        /* strings */
        if (c == '"') { scan_string(&L); continue; }

        int line = L.line, col = L.col;

        switch (c) {
            case '+': advance(&L); emit_lit(&L, TOKEN_PLUS, "+", line, col); break;
            case '-':
                advance(&L);
                if (match(&L, '>')) emit_lit(&L, TOKEN_ARROW, "->", line, col);
                else                emit_lit(&L, TOKEN_MINUS, "-", line, col);
                break;
            case '*': advance(&L); emit_lit(&L, TOKEN_STAR, "*", line, col); break;
            case '/': advance(&L); emit_lit(&L, TOKEN_SLASH, "/", line, col); break;
            case '%': advance(&L); emit_lit(&L, TOKEN_PERCENT, "%", line, col); break;
            case '=':
                advance(&L);
                if (match(&L, '=')) emit_lit(&L, TOKEN_EQ, "==", line, col);
                else                emit_lit(&L, TOKEN_ASSIGN, "=", line, col);
                break;
            case '!':
                advance(&L);
                if (match(&L, '=')) emit_lit(&L, TOKEN_NEQ, "!=", line, col);
                else                emit_lit(&L, TOKEN_NOT, "!", line, col);
                break;
            case '<':
                advance(&L);
                if (match(&L, '=')) emit_lit(&L, TOKEN_LTE, "<=", line, col);
                else                emit_lit(&L, TOKEN_LT, "<", line, col);
                break;
            case '>':
                advance(&L);
                if (match(&L, '=')) emit_lit(&L, TOKEN_GTE, ">=", line, col);
                else                emit_lit(&L, TOKEN_GT, ">", line, col);
                break;
            case '&':
                advance(&L);
                if (match(&L, '&')) emit_lit(&L, TOKEN_AND, "&&", line, col);
                else                error_at(&L, line, col, "se esperaba '&&', caracter suelto", '&');
                break;
            case '|':
                advance(&L);
                if (match(&L, '|')) emit_lit(&L, TOKEN_OR, "||", line, col);
                else                error_at(&L, line, col, "se esperaba '||', caracter suelto", '|');
                break;
            case '.':
                advance(&L);
                if (match(&L, '.')) emit_lit(&L, TOKEN_RANGE, "..", line, col);
                else                error_at(&L, line, col, "caracter inesperado", '.');
                break;
            case '(': advance(&L); emit_lit(&L, TOKEN_LPAREN, "(", line, col); break;
            case ')': advance(&L); emit_lit(&L, TOKEN_RPAREN, ")", line, col); break;
            case '{': advance(&L); emit_lit(&L, TOKEN_LBRACE, "{", line, col); break;
            case '}': advance(&L); emit_lit(&L, TOKEN_RBRACE, "}", line, col); break;
            case '[': advance(&L); emit_lit(&L, TOKEN_LBRACKET, "[", line, col); break;
            case ']': advance(&L); emit_lit(&L, TOKEN_RBRACKET, "]", line, col); break;
            case ':': advance(&L); emit_lit(&L, TOKEN_COLON, ":", line, col); break;
            case ',': advance(&L); emit_lit(&L, TOKEN_COMMA, ",", line, col); break;
            default:
                error_at(&L, line, col, "caracter desconocido", c);
                advance(&L);
                break;
        }
    }

    emit_lit(&L, TOKEN_EOF, "", L.line, L.col);
    return L.out;
}

void kel_free_tokens(TokenList* list) {
    if (!list || !list->tokens) return;
    for (size_t i = 0; i < list->count; i++) free(list->tokens[i].value);
    free(list->tokens);
    list->tokens = NULL;
    list->count = list->capacity = 0;
}

const char* kel_token_name(TokenType t) {
    switch (t) {
        case TOKEN_VAL: return "VAL";
        case TOKEN_VAR: return "VAR";
        case TOKEN_FN: return "FN";
        case TOKEN_RETURN: return "RETURN";
        case TOKEN_IF: return "IF";
        case TOKEN_ELSE: return "ELSE";
        case TOKEN_WHILE: return "WHILE";
        case TOKEN_FOR: return "FOR";
        case TOKEN_IN: return "IN";
        case TOKEN_TRUE: return "TRUE";
        case TOKEN_FALSE: return "FALSE";
        case TOKEN_PRINTLN: return "PRINTLN";
        case TOKEN_TYPE_INT: return "TYPE_INT";
        case TOKEN_TYPE_FLOAT: return "TYPE_FLOAT";
        case TOKEN_TYPE_BOOL: return "TYPE_BOOL";
        case TOKEN_TYPE_STRING: return "TYPE_STRING";
        case TOKEN_PLUS: return "PLUS";
        case TOKEN_MINUS: return "MINUS";
        case TOKEN_STAR: return "STAR";
        case TOKEN_SLASH: return "SLASH";
        case TOKEN_PERCENT: return "PERCENT";
        case TOKEN_ASSIGN: return "ASSIGN";
        case TOKEN_EQ: return "EQ";
        case TOKEN_NEQ: return "NEQ";
        case TOKEN_LT: return "LT";
        case TOKEN_GT: return "GT";
        case TOKEN_LTE: return "LTE";
        case TOKEN_GTE: return "GTE";
        case TOKEN_AND: return "AND";
        case TOKEN_OR: return "OR";
        case TOKEN_NOT: return "NOT";
        case TOKEN_ARROW: return "ARROW";
        case TOKEN_RANGE: return "RANGE";
        case TOKEN_LPAREN: return "LPAREN";
        case TOKEN_RPAREN: return "RPAREN";
        case TOKEN_LBRACE: return "LBRACE";
        case TOKEN_RBRACE: return "RBRACE";
        case TOKEN_LBRACKET: return "LBRACKET";
        case TOKEN_RBRACKET: return "RBRACKET";
        case TOKEN_COLON: return "COLON";
        case TOKEN_COMMA: return "COMMA";
        case TOKEN_INT_LIT: return "INT_LIT";
        case TOKEN_FLOAT_LIT: return "FLOAT_LIT";
        case TOKEN_STR_LIT: return "STR_LIT";
        case TOKEN_IDENT: return "IDENT";
        case TOKEN_EOF: return "EOF";
        case TOKEN_ERROR: return "ERROR";
    }
    return "?";
}

void kel_print_tokens(const TokenList* list) {
    printf("%-5s %-5s %-14s %s\n", "LINE", "COL", "TYPE", "LEXEME");
    printf("-----------------------------------------------\n");
    for (size_t i = 0; i < list->count; i++) {
        const Token* t = &list->tokens[i];
        printf("%-5d %-5d %-14s %s\n",
               t->line, t->col, kel_token_name(t->type),
               t->type == TOKEN_STR_LIT ? t->value : t->value);
    }
}
