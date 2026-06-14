#include "dcc.h"

/*
 * ---------------------------------------------------------------------------
 * Type-only expression oracle.
 *
 * typeof_conditional_arm() resolves the C type of the conditional-expression
 * beginning at the current token WITHOUT emitting code and WITHOUT advancing
 * the parser: the public entry saves and restores all lexer/token state, so a
 * bug in the walker can only produce a wrong *type verdict*, never corrupt the
 * token stream that real codegen re-parses afterwards.
 *
 * The internal to_* ladder mirrors the codegen gen_* precedence ladder
 * one-for-one, so the type it reports matches what codegen ultimately produces
 * under the usual arithmetic conversions (float dominates long dominates
 * unsigned dominates int) and the pointer-arithmetic rules. Unlike
 * peek_simple_unary_type, which classifies only a single leading unary
 * operand, the oracle recurses through parentheses, casts, every binary level
 * and nested conditionals, so a float hidden arbitrarily deep is resolved:
 *   (float)y      x + fa*fb      s->f      a ? 1 : b ? 2 : 3.5f
 * ---------------------------------------------------------------------------
 */
static int to_comma(void);
static int to_assign(void);
static int to_conditional(void);
static int to_unary(void);

/* Usual-arithmetic-conversion balance for the two arms of ?:, including the
 * pointer cases common_arith_type does not model. */
static int to_balance(int a, int b)
{
    if (type_is_float(a) || type_is_float(b))
        return TYPE_FLOAT;
    if (type_ptr_depth(a) > 0)
        return a;
    if (type_ptr_depth(b) > 0)
        return b;
    return common_arith_type(a, b);
}

static int to_postfix(void)
{
    int tt;
    int is_arr;
    int dim_count;
    int base_type;
    int nsubs;
    struct Sym *s;

    if (tok.kind == TOK_NUM) {
        if (tok.val > 0xffffL || tok.val < -32768L || g_tok_long_suffix)
            tt = TYPE_LONG;
        else
            tt = TYPE_INT;
        if (g_tok_unsigned_suffix)
            tt |= TYPE_UNSIGNED;
        next_token();
        return promote_int_type(tt);
    }
    if (tok.kind == TOK_CHARLIT) { next_token(); return TYPE_INT; }
    if (tok.kind == TOK_FLOATLIT) { next_token(); return TYPE_FLOAT; }
    if (tok.kind == TOK_STR || tok.kind == TOK_WSTR) {
        int wide = (tok.kind == TOK_WSTR);
        while (tok.kind == TOK_STR || tok.kind == TOK_WSTR)
            next_token();
        return (wide ? TYPE_INT : TYPE_CHAR) | TYPE_PTR;
    }

    /* Resolve the primary, seeding the array-tracking state used by the
     * shared postfix-suffix walk below. */
    if (tok.kind == '(') {
        next_token();
        tt = to_comma();          /* inner expression: already value-decayed */
        if (tok.kind == ')')
            next_token();
        is_arr = 0;
        dim_count = 0;
        base_type = tt;
        nsubs = 0;
    } else if (tok.kind == TOK_ID) {
        s = find_sym(tok.text);
        if (!s) {
            /* enum constant or unknown identifier behaves like int */
            next_token();
            return TYPE_INT;
        }
        /* Port of peek_simple_unary_type's id + subscript/member chain. */
        tt = s->type;
        is_arr = s->is_array;
        dim_count = s->dim_count;
        base_type = s->type;
        nsubs = 0;
        next_token();
    } else {
        return TYPE_INT;
    }

    for (;;) {
        if (tok.kind == '[') {
            skip_balanced_bracket('[', ']');
            nsubs++;
            if (is_arr) {
                if (dim_count > 0 && nsubs < dim_count)
                    tt = type_add_ptr(base_type);
                else
                    tt = base_type;
                if (dim_count <= 0 || nsubs >= dim_count)
                    is_arr = 0;
                continue;
            } else if (dim_count > 0 && type_ptr_depth(base_type) > 0) {
                if (nsubs <= dim_count)
                    tt = type_add_ptr(type_decay_ptr(base_type));
                else
                    tt = type_decay_ptr(base_type);
                continue;
            } else {
                tt = type_decay_ptr(tt);
                continue;
            }
        }
        if (tok.kind == '.' || tok.kind == TOK_ARROW) {
            struct FieldDef *fd;
            int sid;
            next_token();
            if (tok.kind != TOK_ID)
                break;
            sid = base_struct_id_from_type(tt);
            fd = find_field_def(sid, tok.text);
            if (!fd)
                break;
            tt = fd->is_array ? fd->elem_type : fd->type;
            is_arr = fd->is_array;
            dim_count = fd->dim_count;
            base_type = fd->elem_type;
            nsubs = 0;
            next_token();
            continue;
        }
        if (tok.kind == '(') {
            /* Call: through a pointer yields int (this compiler only
             * models int-returning function pointers); a plain function
             * symbol's stored type is already its return type. */
            skip_balanced_bracket('(', ')');
            if (type_ptr_depth(tt) > 0)
                tt = TYPE_INT;
            is_arr = 0;
            continue;
        }
        if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
            next_token();
            continue;
        }
        break;
    }

    /* An array expression that was never fully subscripted decays to a
     * pointer-to-element in value context, exactly as gen_primary does
     * (g_expr_type = type_add_ptr(val_type)). Without this a bare float
     * array such as fa in c ? fp : fa was reported as float, which made
     * the conditional oracle mis-convert the pointer arms to float. */
    if (is_arr)
        tt = type_add_ptr(tt);
    return tt;
}

static int to_unary(void)
{
    int t;

    if (paren_starts_cast()) {
        int ct;
        int sz;
        expect('(');
        parse_type_name_decl(&ct, &sz);
        expect(')');
        (void)to_unary();             /* cast operand; its type is discarded */
        return ct;                    /* a cast value has the cast's type */
    }
    if (tok.kind == '!') { next_token(); (void)to_unary(); return TYPE_INT; }
    if (tok.kind == '~') { next_token(); return promote_int_type(to_unary()); }
    if (tok.kind == '-' || tok.kind == '+') { next_token(); return promote_int_type(to_unary()); }
    if (tok.kind == '*') {
        next_token();
        t = type_decay_ptr(to_unary());
        if ((t & 15) == TYPE_VOID)
            t = TYPE_CHAR;
        return t;
    }
    if (tok.kind == '&') { next_token(); return type_add_ptr(to_unary()); }
    if (tok.kind == TOK_INC || tok.kind == TOK_DEC) { next_token(); return to_unary(); }
    if (tok.kind == TOK_SIZEOF) {
        next_token();
        if (tok.kind == '(') {
            next_token();
            if (starts_type()) {
                int tt;
                int ss;
                parse_type_name_decl(&tt, &ss);
                if (tok.kind == ')')
                    next_token();
            } else {
                (void)to_comma();
                if (tok.kind == ')')
                    next_token();
            }
        } else {
            (void)to_unary();
        }
        return TYPE_INT | TYPE_UNSIGNED;
    }
    return to_postfix();
}

static int to_mul(void)
{
    int t = to_unary();
    while (tok.kind == '*' || tok.kind == '/' || tok.kind == '%') {
        next_token();
        t = common_arith_type(t, to_unary());
    }
    return t;
}

static int to_add(void)
{
    int t = to_mul();
    while (tok.kind == '+' || tok.kind == '-') {
        int op = tok.kind;
        int r;
        next_token();
        r = to_mul();
        if (type_ptr_depth(t) > 0) {
            if (op == '-' && type_ptr_depth(r) > 0)
                t = TYPE_INT;             /* ptr - ptr -> ptrdiff (int) */
            /* else ptr +/- int -> ptr (t unchanged) */
        } else if (op == '+' && type_ptr_depth(r) > 0) {
            t = r;                        /* int + ptr -> ptr */
        } else {
            t = common_arith_type(t, r);
        }
    }
    return t;
}

static int to_shift(void)
{
    int t = to_add();
    while (tok.kind == TOK_SHL || tok.kind == TOK_SHR) {
        next_token();
        (void)to_add();
        t = promote_int_type(t);          /* result is the promoted left operand */
    }
    return t;
}

static int to_rel(void)
{
    int t = to_shift();
    while (tok.kind == '<' || tok.kind == '>' || tok.kind == TOK_LE || tok.kind == TOK_GE) {
        next_token();
        (void)to_shift();
        t = TYPE_INT;
    }
    return t;
}

static int to_eq(void)
{
    int t = to_rel();
    while (tok.kind == TOK_EQ || tok.kind == TOK_NE) {
        next_token();
        (void)to_rel();
        t = TYPE_INT;
    }
    return t;
}

static int to_band(void)
{
    int t = to_eq();
    while (tok.kind == '&') {
        next_token();
        t = common_arith_type(t, to_eq());
    }
    return t;
}

static int to_bxor(void)
{
    int t = to_band();
    while (tok.kind == '^') {
        next_token();
        t = common_arith_type(t, to_band());
    }
    return t;
}

static int to_bor(void)
{
    int t = to_bxor();
    while (tok.kind == '|') {
        next_token();
        t = common_arith_type(t, to_bxor());
    }
    return t;
}

static int to_land(void)
{
    int t = to_bor();
    while (tok.kind == TOK_ANDAND) {
        next_token();
        (void)to_bor();
        t = TYPE_INT;
    }
    return t;
}

static int to_lor(void)
{
    int t = to_land();
    while (tok.kind == TOK_OROR) {
        next_token();
        (void)to_land();
        t = TYPE_INT;
    }
    return t;
}

static int to_conditional(void)
{
    int t = to_lor();
    if (tok.kind == '?') {
        int mt;
        int ft;
        next_token();
        mt = to_comma();              /* middle arm is a full expression */
        if (tok.kind == ':')
            next_token();
        ft = to_conditional();        /* false arm is a conditional-expression */
        t = to_balance(mt, ft);
    }
    return t;
}

static int to_assign(void)
{
    int t = to_conditional();
    /* An assignment's value has the (unconverted) type of its left operand,
     * which is the conditional-expression just parsed. */
    if (is_assignment_token(tok.kind)) {
        next_token();
        (void)to_assign();
    }
    return t;
}

static int to_comma(void)
{
    int t = to_assign();
    while (tok.kind == ',') {
        next_token();
        t = to_assign();
    }
    return t;
}

int typeof_conditional_arm(void)
{
    long save_pos = posi;
    long save_tok_start = tok_start_pos;
    int save_line = line_no;
    int save_tok_line = tok_line;
    int save_long_suffix = g_tok_long_suffix;
    int save_unsigned_suffix = g_tok_unsigned_suffix;
    struct Token save_tok = tok;
    int t;

    t = to_conditional();

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    g_tok_long_suffix = save_long_suffix;
    g_tok_unsigned_suffix = save_unsigned_suffix;
    tok = save_tok;
    return t;
}
