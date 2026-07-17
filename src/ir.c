#include "ir.h"
#include "semantic.h"

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

/* `in` se pasa por valor a propósito: se copia desde la pila DESPUÉS del
 * realloc, así que nunca se lee del buffer viejo.
 *
 * AVISO para quien haga backpatching (el if/while del Task 8): esto realloca,
 * así que cualquier `Instr*` a f->body queda colgando en cuanto emitas otra
 * instrucción. Guarda el ÍNDICE, nunca el puntero:
 *     size_t j = f->count; emit(f, jmp); ... ; f->body[j].op2 = addr_label(L);
 * Con el puntero compila igual y falla solo cuando el cuerpo cruza la
 * capacidad — es decir, en los programas grandes y no en tus tests. */
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

/* Una etiqueta no es un valor: no lleva KelType (ver "Tipos" en ir.h). */
static Addr addr_label(long long id) {
    Addr a = addr_none();
    a.kind = ADDR_LABEL;
    a.i = id;
    return a;
}

/* Las etiquetas se numeran por función, igual que los temporales. */
static long long new_label(IRFunction* f) {
    return (long long)(++f->n_labels);
}

static void emit_label(IRFunction* f, long long id) {
    Instr in = instr(IR_LABEL);
    in.dst = addr_label(id);
    emit(f, in);
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

/* Los saltos del control de flujo no necesitan backpatching: el id de la
 * etiqueta se reserva con new_label() ANTES de emitir el salto, así que el
 * destino ya se conoce aunque el IR_LABEL se emita más tarde. Es lo que evita
 * tener que volver sobre una Instr ya emitida — que con el realloc de emit()
 * exigiría guardar el índice, nunca el puntero (ver el aviso en emit()). */
static void emit_goto(IRFunction* f, long long L) {
    Instr in = instr(IR_GOTO);
    in.op1 = addr_label(L);
    emit(f, in);
}

static void emit_if_false_goto(IRFunction* f, Addr cond, long long L) {
    Instr in = instr(IR_IF_FALSE_GOTO);
    in.op1 = cond;
    in.op2 = addr_label(L);
    emit(f, in);
}

/* `&&` y `||` no son IR_BINOP: eso evaluaría los dos lados siempre, y
 * `f() && g()` ejecutaría g() aunque f() fuese falso. La traducción usa
 * cortocircuito con etiquetas:
 *
 *     t = a                 t = a
 *     ifFalse t goto L      if t goto L
 *     t = b                 t = b
 *   L:                    L:
 *      (&&)                  (||)
 *
 * El temporal se reserva antes del lado izquierdo porque ambas ramas
 * escriben en él. */
static Addr gen_shortcircuit(IRFunction* f, Node* n) {
    int is_and = (strcmp(n->op, "&&") == 0);
    Addr t = new_temp(f, n->inferred_type);

    Addr a = gen_expr(f, n->lhs);
    emit_copy(f, t, a);

    long long L = new_label(f);
    Instr jmp = instr(is_and ? IR_IF_FALSE_GOTO : IR_IF_GOTO);
    jmp.op1 = t;
    jmp.op2 = addr_label(L);
    emit(f, jmp);

    Addr b = gen_expr(f, n->rhs);
    emit_copy(f, t, b);

    emit_label(f, L);
    return t;
}

static Addr gen_binop(IRFunction* f, Node* n) {
    if (strcmp(n->op, "&&") == 0 || strcmp(n->op, "||") == 0)
        return gen_shortcircuit(f, n);
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

/* println y los read_* llegan como N_CALL: println se parsea como N_IDENT
 * (parser.c:152) y los read_* son identificadores normales. Se distinguen
 * por nombre con kel_is_println/kel_is_read_builtin, expuestos por
 * semantic.h, para no duplicar aquí la lista de nombres.
 *
 * CONVENIO DE LLAMADA (lo necesita la Etapa 6): `call sym, n` consume los n
 * IR_PARAM *más recientes*. Los args se generan en orden y cada uno emite su
 * param justo después de su propio código, así que una llamada anidada deja
 * los params intercalados — pero nunca ambiguos, porque la llamada de dentro
 * consume los suyos antes de que se emitan los de fuera:
 *
 *     suma(suma(1,2), 3)  ->   param 1
 *                              param 2
 *                              t1 = call suma, 2   ; consume 1 y 2
 *                              param t1
 *                              param 3
 *                              t2 = call suma, 2   ; consume t1 y 3
 *
 * Es decir: emit_c.c puede recoger los args escaneando hacia atrás los n
 * params anteriores al call, sin cruzar nunca los de otra llamada. */
static Addr gen_call(IRFunction* f, Node* n) {
    const char* name = n->lhs->str_val;

    /* item_count == 1 lo garantiza el semántico: --ir solo corre si el
     * análisis fue limpio (main.c). */
    if (kel_is_println(name)) {
        Addr v = gen_expr(f, n->items[0]);
        Instr in = instr(IR_PRINTLN);
        in.op1 = v;
        emit(f, in);
        return addr_none();          /* println es void */
    }

    /* El tipo viaja en dst.type: los read_* se registran con un ret_type real
     * (semantic.c), así que inferred_type nunca es NULL aquí. */
    if (kel_is_read_builtin(name)) {
        Addr d = new_temp(f, n->inferred_type);
        Instr in = instr(IR_READ);
        in.dst = d;
        emit(f, in);
        return d;
    }

    for (size_t i = 0; i < n->item_count; i++) {
        Addr a = gen_expr(f, n->items[i]);
        Instr pin = instr(IR_PARAM);
        pin.op1 = a;
        emit(f, pin);
    }

    Instr in = instr(IR_CALL);
    in.sym = name;                   /* no-owned: apunta al AST */
    /* El nº de argumentos no lleva tipo: el opcode ya lo implica (ver ir.h). */
    in.op2 = addr_const_int((long long)n->item_count, NULL);
    /* Una fn sin `->` tiene ret_type = KT_VOID real (parser.c:418), no NULL;
     * el chequeo de NULL es defensivo. Sin dst, print_instr omite el "=". */
    int is_void = !n->inferred_type || n->inferred_type->kind == KT_VOID;
    Addr d = addr_none();
    if (!is_void) {
        d = new_temp(f, n->inferred_type);
        in.dst = d;
    }
    emit(f, in);
    return d;
}

/* Un literal de array reserva y luego rellena: un IR_ARRAY_NEW y un
 * IR_INDEX_STORE por elemento. El tamaño sale del AST (item_count), no del
 * KelType, que no guarda el número de elementos — por eso `--symbols` cuenta
 * los arreglos como una referencia de 8 bytes.
 *
 * El literal construye en un temporal y el N_VAR_DECL de fuera copia ese
 * temporal a la variable. La copia sobra (nada más lee el temporal), pero es
 * el mismo IR_COPY redundante que ya emiten var_decl y assign: la propagación
 * de copias de la Etapa 5 lo limpia. */
static Addr gen_array_lit(IRFunction* f, Node* n) {
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_ARRAY_NEW);
    in.dst = d;
    /* El tamaño no lleva tipo: el opcode ya lo implica (ver "Tipos" en ir.h). */
    in.op1 = addr_const_int((long long)n->item_count, NULL);
    emit(f, in);

    for (size_t i = 0; i < n->item_count; i++) {
        Addr v = gen_expr(f, n->items[i]);
        Instr st = instr(IR_INDEX_STORE);
        st.op1 = d;                                   /* arreglo */
        st.op2 = addr_const_int((long long)i, NULL);  /* índice literal, sin tipo */
        st.dst = v;                                   /* la FUENTE, no el destino (ver ir.h) */
        emit(f, st);
    }
    return d;
}

static Addr gen_index(IRFunction* f, Node* n) {
    Addr arr = gen_expr(f, n->lhs);   /* el arreglo va en lhs */
    Addr idx = gen_expr(f, n->rhs);   /* el índice, en rhs */
    Addr d = new_temp(f, n->inferred_type);
    Instr in = instr(IR_INDEX_LOAD);
    in.dst = d;
    in.op1 = arr;
    in.op2 = idx;
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
        case N_CALL:      return gen_call(f, n);
        case N_ARRAY_LIT: return gen_array_lit(f, n);
        case N_INDEX:     return gen_index(f, n);
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
        case N_INDEX_ASSIGN: {
            /* lhs es el N_INDEX entero (arreglo + índice); rhs es el valor.
             * No reserva temporal: el store consume las tres direcciones. */
            Addr arr = gen_expr(f, n->lhs->lhs);
            Addr idx = gen_expr(f, n->lhs->rhs);
            Addr v   = gen_expr(f, n->rhs);
            Instr in = instr(IR_INDEX_STORE);
            in.op1 = arr;
            in.op2 = idx;
            in.dst = v;              /* la FUENTE, no el destino (ver ir.h) */
            emit(f, in);
            break;
        }
        case N_BLOCK:
            for (size_t i = 0; i < n->item_count; i++)
                gen_stmt(f, n->items[i]);
            break;
        case N_EXPR_STMT:
            gen_expr(f, n->lhs);
            break;
        case N_RETURN: {
            /* n->lhs es NULL en un `return` pelado: el op1 se queda en
             * ADDR_NONE y es lo que distingue `return` de `return 0`. */
            Instr in = instr(IR_RETURN);
            if (n->lhs) in.op1 = gen_expr(f, n->lhs);
            emit(f, in);
            break;
        }
        case N_IF: {
            /*     ifFalse cond goto L_else
             *     <then>
             *     goto L_end            ; solo si hay else
             *   L_else:
             *     <else>
             *   L_end:
             *
             * Sin else, L_else hace de etiqueta de salida y no hacen falta ni
             * el goto ni la segunda etiqueta. */
            Addr c = gen_expr(f, n->cond);
            long long L_else = new_label(f);
            emit_if_false_goto(f, c, L_else);
            gen_stmt(f, n->then_branch);
            if (n->else_branch) {
                long long L_end = new_label(f);
                emit_goto(f, L_end);
                emit_label(f, L_else);
                gen_stmt(f, n->else_branch);
                emit_label(f, L_end);
            } else {
                emit_label(f, L_else);
            }
            break;
        }
        case N_WHILE: {
            /* La etiqueta va antes de la condición: hay que reevaluarla
             * en cada vuelta. */
            long long L_cond = new_label(f);
            long long L_end  = new_label(f);
            emit_label(f, L_cond);
            Addr c = gen_expr(f, n->cond);
            emit_if_false_goto(f, c, L_end);
            gen_stmt(f, n->body);
            emit_goto(f, L_cond);
            emit_label(f, L_end);
            break;
        }
        case N_FOR: {
            /* for i in a..b  ==>  i = a; Lc: ifFalse i < b goto Lf;
             *                     cuerpo; i = i + 1; goto Lc; Lf:
             * El fin es exclusivo (ver SPEC.md). */
            Addr start = gen_expr(f, n->range_start);
            KelType* it = n->range_start->inferred_type;
            Addr iv = addr_var(n->loop_var, it);
            emit_copy(f, iv, start);

            long long L_cond = new_label(f);
            long long L_end  = new_label(f);
            emit_label(f, L_cond);

            Addr end = gen_expr(f, n->range_end);
            Addr c = new_temp(f, NULL);   /* bool; el AST no tiene nodo para esta comparación */
            Instr cmp = instr(IR_BINOP);
            cmp.dst = c;
            cmp.op1 = iv;
            cmp.op2 = end;
            cmp.sym = "<";
            emit(f, cmp);
            emit_if_false_goto(f, c, L_end);

            gen_stmt(f, n->body);

            Addr inc = new_temp(f, it);
            Instr add = instr(IR_BINOP);
            add.dst = inc;
            add.op1 = iv;
            add.op2 = addr_const_int(1, it);
            add.sym = "+";
            emit(f, add);
            emit_copy(f, iv, inc);

            emit_goto(f, L_cond);
            emit_label(f, L_end);
            break;
        }
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

/* %g suelta el punto: 1.0 saldría como "1". El --ast puede permitírselo porque
 * anota el tipo al lado ("Float 1 : float"), pero el TAC no lleva anotación, así
 * que "t1 = 1 / 2" se leería como división entera — que es justo lo que C haría
 * si la Etapa 6 emitiera eso. Y "2.5 + 1" se lee como un int sumado a un float,
 * la expresión que el propio SPEC declara error de tipos. El punto no es
 * decorativo: es lo que distingue el literal.
 *
 * Los no finitos ("inf", "nan", y los "1.#INF" de MSVCRT) ya traen letra o
 * punto, así que el strpbrk los deja en paz. */
static void print_float(double v) {
    char buf[64];
    snprintf(buf, sizeof buf, "%.15g", v);
    printf("%s", buf);
    if (!strpbrk(buf, ".eEnN")) printf(".0");
}

/* Sin `default` a propósito: así -Wswitch obliga a cubrir cualquier AddrKind
 * que se añada en el futuro. */
static void print_addr(const Addr* a) {
    switch (a->kind) {
        case ADDR_NONE:        break;
        case ADDR_CONST_INT:   printf("%lld", a->i); break;
        case ADDR_CONST_FLOAT: print_float(a->f); break;
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
        case IR_GOTO:
            printf("  goto ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_IF_GOTO:
            printf("  if ");
            print_addr(&in->op1);
            printf(" goto ");
            print_addr(&in->op2);
            printf("\n");
            break;
        case IR_IF_FALSE_GOTO:
            printf("  ifFalse ");
            print_addr(&in->op1);
            printf(" goto ");
            print_addr(&in->op2);
            printf("\n");
            break;
        case IR_PARAM:
            printf("  param ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_CALL:
            printf("  ");
            /* Sin dst (fn void) no se imprime el "=": `call saluda, 0`. */
            if (in->dst.kind != ADDR_NONE) {
                print_addr(&in->dst);
                printf(" = ");
            }
            printf("call %s, %lld\n", in->sym, in->op2.i);
            break;
        case IR_RETURN:
            if (in->op1.kind == ADDR_NONE) {
                printf("  return\n");
            } else {
                printf("  return ");
                print_addr(&in->op1);
                printf("\n");
            }
            break;
        case IR_PRINTLN:
            printf("  println ");
            print_addr(&in->op1);
            printf("\n");
            break;
        case IR_ARRAY_NEW:
            /* Se imprime "alloc int[3]", no "array[3]": `array` no es palabra
             * reservada en Kel, así que `t1 = array[3]` se leería como indexar
             * una variable llamada así.
             *
             * dst.type nunca es NULL, y su elem tampoco: dst.type es el
             * inferred_type del literal y --ir solo corre con el semántico
             * limpio (main.c). check_array_lit solo devuelve tipo si no hubo
             * error, y siempre con elem: en el caso no vacío elem es el tipo
             * del primer elemento, y en `val v: [int] = []` es el clon del
             * hint, que viene de parse_type y siempre trae elem. */
            printf("  ");
            print_addr(&in->dst);
            printf(" = alloc %s[%lld]\n",
                   kel_type_name(in->dst.type->elem), in->op1.i);
            break;
        case IR_INDEX_LOAD:
            printf("  ");
            print_addr(&in->dst);
            printf(" = ");
            print_addr(&in->op1);
            printf("[");
            print_addr(&in->op2);
            printf("]\n");
            break;
        case IR_INDEX_STORE:
            /* dst se imprime como fuente: `op1[op2] = dst` (ver ir.h). */
            printf("  ");
            print_addr(&in->op1);
            printf("[");
            print_addr(&in->op2);
            printf("] = ");
            print_addr(&in->dst);
            printf("\n");
            break;
        case IR_READ:
            /* dst.type nunca es NULL: los read_* tienen ret_type real. */
            printf("  ");
            print_addr(&in->dst);
            printf(" = read %s\n", kel_type_name(in->dst.type));
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
