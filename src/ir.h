#ifndef KEL_IR_H
#define KEL_IR_H

/* ============================================================
 * Etapa 4 — Código intermedio (TAC: Three Address Code)
 *
 * Pipeline previsto:
 *   AST anotado (semantic.c)  →  IR (lista lineal de Instr)
 *                             →  optimización (Etapa 5)
 *                             →  C de salida (Etapa 6, emit_c.c)
 *
 * Cada instrucción tiene como máximo 3 direcciones: dst, op1, op2.
 * Los operandos son "Addr": variables, temporales (t1, t2, ...),
 * constantes o etiquetas.
 *
 * Formato de impresión por instrucción (flag `--ir`):
 *
 *     t1 = a + b              ; IR_BINOP
 *     t2 = -t1                ; IR_UNOP
 *     t3 = t1                 ; IR_COPY
 *     t4 = arr[i]             ; IR_INDEX_LOAD
 *     arr[i] = t1             ; IR_INDEX_STORE
 *     t6 = alloc int[3]       ; IR_ARRAY_NEW  (reserva, no inicializa)
 *     param t1                ; IR_PARAM
 *     t5 = call suma, 2       ; IR_CALL  (nargs = 2)
 *     if t1 goto L2           ; IR_IF_GOTO
 *     ifFalse t1 goto L3      ; IR_IF_FALSE_GOTO
 *     goto L1                 ; IR_GOTO
 *   L2:                       ; IR_LABEL
 *     return t5               ; IR_RETURN
 *     println t1              ; IR_PRINTLN
 *     t7 = read int           ; IR_READ
 *
 * Convenciones:
 *   - IRFunction lleva nombre, params y ret_type: no hace falta un IR_LABEL
 *     con el nombre de la función. --ir imprime la cabecera desde la struct.
 *   - IR_LABEL solo lleva etiquetas numéricas L1, L2..., en dst (ADDR_LABEL).
 *   - Temporales son t1, t2, ... numerados por función.
 *   - Etiquetas: L1, L2, ... únicas por función.
 *   - Tipos: un Addr que representa un *valor* lleva su KelType, porque la
 *     Etapa 6 lo necesita (emitir C requiere saber si un `+` es aritmético o
 *     concatenación). Pero `type` NO siempre está: es NULL en ADDR_NONE, en
 *     ADDR_LABEL, y en las constantes que son pura maquinaria de la
 *     instrucción y cuyo tipo ya implica el opcode — el nº de argumentos de
 *     IR_CALL, el tamaño de IR_ARRAY_NEW, el índice literal de un
 *     IR_INDEX_STORE. **La Etapa 6 debe comprobar NULL antes de mirar
 *     `Addr.type`.** Ver también el hueco conocido del `for` más abajo.
 *
 * Esta fase aprovecha `Node.inferred_type` anotado en semántico.
 * ============================================================ */

#include "parser.h"

typedef enum {
    ADDR_NONE = 0,   /* operando ausente. Un Addr en cero es ADDR_NONE. */
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
    IR_ARRAY_NEW,         /* dst = alloc <tipo>[op1]  (op1 = nº elementos, const int) */
    IR_INDEX_LOAD,        /* dst = op1[op2] */
    IR_INDEX_STORE,       /* op1[op2] = dst  (dst es la fuente) */
    IR_PARAM,             /* push arg op1 */
    IR_CALL,              /* dst = call <sym>, op2.i args  (dst ADDR_NONE si void) */
    IR_GOTO,              /* goto op1 (label) */
    IR_IF_GOTO,           /* if op1 goto op2 */
    IR_IF_FALSE_GOTO,     /* ifFalse op1 goto op2 */
    IR_RETURN,            /* return op1; op1.kind == ADDR_NONE si es void */
    IR_PRINTLN,           /* println op1 */
    IR_READ               /* dst = read <tipo de dst>  (entrada estándar) */
} IROp;

typedef struct {
    IROp op;
    Addr dst, op1, op2;
    const char* sym;      /* "+", "-", ... para BINOP/UNOP; o nombre de fn */
} Instr;

/* Una función compilada. Los temporales (t1..t_temps) y las etiquetas
 * (L1..L_labels) se numeran por función, no globalmente. */
typedef struct {
    char*    name;          /* owned */
    Param*   params;        /* no-owned; apunta al AST */
    size_t   param_count;
    KelType* ret_type;      /* no-owned; apunta al AST */
    Instr*   body;          /* owned */
    size_t   count, capacity;
    size_t   n_temps;       /* temporales usados: t1..t_temps */
    size_t   n_labels;      /* etiquetas usadas: L1..L_labels */
} IRFunction;

typedef struct {
    IRFunction* fns;        /* owned */
    size_t      count, capacity;
} IRProgram;

/* ---------- API de la Etapa 4 (ir.c) ----------
 *
 * CUIDADO — tiempo de vida: IRFunction.params, ret_type y los Addr.type
 * apuntan al AST sin poseerlo, y los ADDR_VAR / ADDR_CONST_STR apuntan a
 * cadenas del AST. El AST debe seguir vivo mientras se use el IRProgram:
 * llama a kel_free_ast() DESPUÉS de kel_ir_free(), nunca antes.
 *
 * HUECO CONOCIDO — el temporal de la condición del `for`: `for i in a..b` se
 * desazucara a una comparación `i < b` que **no existe en el AST**, así que no
 * hay `inferred_type` del que copiar y su Addr queda con `type == NULL`. Es un
 * bool. La Etapa 6 puede asumirlo por el contexto (es el op1 de un
 * IR_IF_FALSE_GOTO). Si algún día no bastara, la salida limpia es que ir.c
 * construya su propio KelType — y entonces IRFunction pasaría a poseer
 * KelTypes y kel_ir_free tendría que liberarlos, que hoy no hace.
 *
 * API de la Etapa 5 (optimize.c) — Plan 4.
 * API de la Etapa 6 (emit_c.c)   — Plan 3.
 */

IRProgram kel_gen(Node* program);          /* genera TAC desde el AST anotado */
void      kel_ir_print(const IRProgram*);  /* imprime el TAC (--ir) */
void      kel_ir_free(IRProgram*);

#endif
