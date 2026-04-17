#ifndef KEL_CODEGEN_H
#define KEL_CODEGEN_H

/* ============================================================
 * Etapa 4 — Código intermedio (TAC: Three Address Code)
 *
 * Pipeline previsto:
 *   AST anotado (semantic.c)  →  IR (lista lineal de Instr)
 *                             →  optimización (Etapa 5)
 *                             →  C de salida (Etapa 6)
 *
 * Cada instrucción tiene como máximo 3 direcciones: dst, op1, op2.
 * Los operandos son "Addr": variables, temporales (t1, t2, ...),
 * constantes o etiquetas.
 *
 * Formato de impresión por instrucción (flag `--ir` futuro):
 *
 *     t1 = a + b              ; IR_BINOP
 *     t2 = -t1                ; IR_UNOP
 *     t3 = t1                 ; IR_COPY
 *     t4 = arr[i]             ; IR_INDEX_LOAD
 *     arr[i] = t1             ; IR_INDEX_STORE
 *     param t1                ; IR_PARAM
 *     t5 = call suma, 2       ; IR_CALL  (nargs = 2)
 *     if t1 goto L2           ; IR_IF_GOTO
 *     ifFalse t1 goto L3      ; IR_IF_FALSE_GOTO
 *     goto L1                 ; IR_GOTO
 *   L2:                       ; IR_LABEL
 *     return t5               ; IR_RETURN
 *     println t1              ; IR_PRINTLN
 *
 * Convenciones:
 *   - Cada función empieza con un IR_LABEL con el nombre de la función.
 *   - Temporales son t1, t2, ... numerados por función.
 *   - Etiquetas: L1, L2, ... únicas por función.
 *   - Tipos: cada Addr lleva un KelType para facilitar la Etapa 6
 *     (emitir C requiere saber si un `+` es aritmético o concatenación).
 *
 * Esta fase aprovecha `Node.inferred_type` anotado en semántico.
 * ============================================================ */

#include "parser.h"

typedef enum {
    ADDR_CONST_INT,
    ADDR_CONST_FLOAT,
    ADDR_CONST_BOOL,
    ADDR_CONST_STR,
    ADDR_VAR,        /* variable del programa, por nombre */
    ADDR_TEMP,       /* temporal t1, t2... */
    ADDR_LABEL       /* L1, L2, ...  (solo como target de saltos) */
} AddrKind;

typedef struct {
    AddrKind kind;
    KelType* type;        /* no-owned; apunta a inferred_type del AST */
    long long    i;       /* ADDR_CONST_INT, ADDR_TEMP id, ADDR_LABEL id */
    double       f;       /* ADDR_CONST_FLOAT */
    int          b;       /* ADDR_CONST_BOOL */
    const char*  s;       /* ADDR_CONST_STR, ADDR_VAR name */
} Addr;

typedef enum {
    IR_LABEL,
    IR_COPY,              /* dst = op1 */
    IR_BINOP,             /* dst = op1 <op> op2 ; op string "+" ... */
    IR_UNOP,              /* dst = <op> op1 */
    IR_INDEX_LOAD,        /* dst = op1[op2] */
    IR_INDEX_STORE,       /* op1[op2] = dst  (dst es la fuente) */
    IR_PARAM,             /* push arg op1 */
    IR_CALL,              /* dst = call <op1.s>, n_params (usar 'i' en op2) */
    IR_GOTO,              /* goto op1 (label) */
    IR_IF_GOTO,           /* if op1 goto op2 */
    IR_IF_FALSE_GOTO,     /* ifFalse op1 goto op2 */
    IR_RETURN,            /* return op1 (o void si op1.kind == 0 sentinel) */
    IR_PRINTLN            /* println op1 */
} IROp;

typedef struct {
    IROp op;
    Addr dst, op1, op2;
    const char* sym;      /* "+", "-", ... para BINOP/UNOP; o nombre de fn */
} Instr;

typedef struct {
    Instr* data;
    size_t count, capacity;
} IRProgram;

/* API a implementar en la Etapa 4.
 *
 *   IRProgram kel_gen(Node* program);   // genera TAC desde el AST
 *   void      kel_ir_print(const IRProgram*);
 *   void      kel_ir_free(IRProgram*);
 */

#endif
