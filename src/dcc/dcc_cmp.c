/*
 * dcc_cmp.c - comparison and conditional-branch code generation.
 *
 * Relational/equality comparison codegen (signed and unsigned, 16- and 32-bit)
 * and the direct condition-to-branch lowering used by if/while/for. Includes
 * the byte-operand fast comparators that emit a single cp + conditional jump.
 * struct ByteOperand is declared in dcc.h.
 *
 * MODULE: compiled as its own translation unit; shared declarations are in dcc.h.
 * Source provenance: monolith src/ddc.c lines 8842-9235 and 9242-10184
 * (the struct ByteOperand definition at 9236-9241 is hoisted to dcc.h).
 */

#include "dcc.h"
void gen_cmp(int op)
{
    int lt;
    int le;

    lt = new_label();
    le = new_label();

    if (op == '<' || op == TOK_GE || op == '>' || op == TOK_LE) {
        emit("\tld a,h\n");
        emit("\txor 80h\n");
        emit("\tld h,a\n");
        emit("\tld a,d\n");
        emit("\txor 80h\n");
        emit("\tld d,a\n");
    }    

    emit("\tor a\n\tsbc hl,de\n");

    if (op == TOK_EQ) emit_jp_label("jp z,", lt);
    else if (op == TOK_NE) emit_jp_label("jp nz,", lt);
    else if (op == '<') emit_jp_label("jp c,", lt);
    else if (op == TOK_GE) emit_jp_label("jp nc,", lt);
    else if (op == '>') {
        int lfalse_gt = new_label();
        emit_jp_label("jp z,", lfalse_gt);
        emit_jp_label("jp nc,", lt);
        emit_label(lfalse_gt);
    } else if (op == TOK_LE) {
        emit_jp_label("jp z,", lt);
        emit_jp_label("jp c,", lt);
    }

    emit("\tld hl,0\n");
    emit_jp_label("jp", le);
    emit_label(lt);
    emit("\tld hl,1\n");
    emit_label(le);
}

void gen_cmp_typed(int op, int lhs_type)
{
    int lt;
    int le;

    lt = new_label();
    le = new_label();

    if (!(lhs_type & TYPE_UNSIGNED) &&
        (op == '<' || op == TOK_GE || op == '>' || op == TOK_LE)) {
        emit("\tld a,h\n");
        emit("\txor 80h\n");
        emit("\tld h,a\n");
        emit("\tld a,d\n");
        emit("\txor 80h\n");
        emit("\tld d,a\n");
    }

    emit("\tor a\n\tsbc hl,de\n");

    if (op == TOK_EQ) emit_jp_label("jp z,", lt);
    else if (op == TOK_NE) emit_jp_label("jp nz,", lt);
    else if (op == '<') emit_jp_label("jp c,", lt);
    else if (op == TOK_GE) emit_jp_label("jp nc,", lt);
    else if (op == '>') {
        int lfalse_gt = new_label();
        emit_jp_label("jp z,", lfalse_gt);
        emit_jp_label("jp nc,", lt);
        emit_label(lfalse_gt);
    } else if (op == TOK_LE) {
        emit_jp_label("jp z,", lt);
        emit_jp_label("jp c,", lt);
    }

    emit("\tld hl,0\n");
    emit_jp_label("jp", le);
    emit_label(lt);
    emit("\tld hl,1\n");
    emit_label(le);
}

int is_relop_token(int k)
{
    return k == TOK_EQ || k == TOK_NE || k == '<' || k == '>' ||
           k == TOK_LE || k == TOK_GE;
}

void emit_signed_bias_for_relop(int op)
{
    if (op == '<' || op == TOK_GE || op == '>' || op == TOK_LE) {
        emit("\tld a,h\n");
        emit("\txor 80h\n");
        emit("\tld h,a\n");
        emit("\tld a,d\n");
        emit("\txor 80h\n");
        emit("\tld d,a\n");
    }
}

void emit_cmp_branch_false(int op, int lfalse)
{
    int ltrue;

    emit_signed_bias_for_relop(op);
    emit("\tor a\n\tsbc hl,de\n");

    if (op == TOK_EQ) {
        emit_jp_label("jp nz,", lfalse);
    } else if (op == TOK_NE) {
        emit_jp_label("jp z,", lfalse);
    } else if (op == '<') {
        emit_jp_label("jp nc,", lfalse);
    } else if (op == TOK_GE) {
        emit_jp_label("jp c,", lfalse);
    } else if (op == '>') {
        emit_jp_label("jp z,", lfalse);
        emit_jp_label("jp c,", lfalse);
    } else if (op == TOK_LE) {
        ltrue = new_label();
        emit_jp_label("jp z,", ltrue);
        emit_jp_label("jp c,", ltrue);
        emit_jp_label("jp", lfalse);
        emit_label(ltrue);
    }
}

void emit_cmp_branch_true(int op, int ltrue)
{
    int ldone;

    emit_signed_bias_for_relop(op);
    emit("\tor a\n\tsbc hl,de\n");

    if (op == TOK_EQ) {
        emit_jp_label("jp z,", ltrue);
    } else if (op == TOK_NE) {
        emit_jp_label("jp nz,", ltrue);
    } else if (op == '<') {
        emit_jp_label("jp c,", ltrue);
    } else if (op == TOK_GE) {
        emit_jp_label("jp nc,", ltrue);
    } else if (op == '>') {
        ldone = new_label();
        emit_jp_label("jp z,", ldone);
        emit_jp_label("jp nc,", ltrue);
        emit_label(ldone);
    } else if (op == TOK_LE) {
        emit_jp_label("jp z,", ltrue);
        emit_jp_label("jp c,", ltrue);
    }
}


int const_positive_snippet_value(const char *s, long *out)
{
    long v;
    char *endp;
    char name[64];
    int i;

    while (*s && isspace((unsigned char)*s))
        s++;

    if (isdigit((unsigned char)*s)) {
        v = strtol(s, &endp, 0);
        while (*endp && isspace((unsigned char)*endp))
            endp++;
        if (*endp != 0 && *endp != ';')
            return 0;
        if (v <= 0 || v > 32767)
            return 0;
        out[0] = v;
        return 1;
    }

    if (is_ident_start((unsigned char)*s)) {
        i = 0;
        while (is_ident_char((unsigned char)*s) && i < 63)
            name[i++] = *s++;
        name[i] = 0;
        while (*s && isspace((unsigned char)*s))
            s++;
        if (*s != 0 && *s != ';')
            return 0;
        if (!define_number_value(name, &v, 0))
            return 0;
        if (v <= 0 || v > 32767)
            return 0;
        out[0] = v;
        return 1;
    }

    return 0;
}

int snippet_const_expr_value(const char *snippet, struct ConstVal *out)
{
    char *old_src;
    long old_len;
    long old_pos;
    long old_tok_start;
    int old_line;
    int old_tok_line;
    int old_errors;
    int old_long_suffix;
    int old_unsigned_suffix;
    struct Token old_tok;
    int ok;

    old_src = src;
    old_len = src_len;
    old_pos = posi;
    old_tok_start = tok_start_pos;
    old_line = line_no;
    old_tok_line = tok_line;
    old_errors = errors;
    old_long_suffix = g_tok_long_suffix;
    old_unsigned_suffix = g_tok_unsigned_suffix;
    old_tok = tok;

    src = (char *)snippet;
    src_len = (long)strlen(snippet);
    posi = 0;
    line_no = 1;
    next_token();

    ok = 0;
    if (tok.kind != ';' && tok.kind != TOK_EOF) {
        if (try_parse_const_expr_value(out) &&
            (tok.kind == ';' || tok.kind == TOK_EOF) &&
            errors == old_errors) {
            ok = 1;
        }
    }

    src = old_src;
    src_len = old_len;
    posi = old_pos;
    tok_start_pos = old_tok_start;
    line_no = old_line;
    tok_line = old_tok_line;
    errors = old_errors;
    g_tok_long_suffix = old_long_suffix;
    g_tok_unsigned_suffix = old_unsigned_suffix;
    tok = old_tok;

    return ok;
}

int const_rel_result(struct ConstVal lhs, int op, struct ConstVal rhs, int *resultp)
{
    int common;
    int r;

    common = cf_common_arith_type(lhs.type, rhs.type);
    cf_convert_to_type(&lhs, common);
    cf_convert_to_type(&rhs, common);

    if (op == TOK_EQ || op == TOK_NE) {
        r = ((lhs.u & cf_mask_for_type(common)) ==
             (rhs.u & cf_mask_for_type(common)));
        if (op == TOK_NE)
            r = !r;
    } else if (common & TYPE_UNSIGNED) {
        unsigned long a;
        unsigned long b;
        a = lhs.u & cf_mask_for_type(common);
        b = rhs.u & cf_mask_for_type(common);
        if (op == '<') r = a < b;
        else if (op == '>') r = a > b;
        else if (op == TOK_LE) r = a <= b;
        else if (op == TOK_GE) r = a >= b;
        else return 0;
    } else {
        long a;
        long b;
        a = cf_signed_value(lhs);
        b = cf_signed_value(rhs);
        if (op == '<') r = a < b;
        else if (op == '>') r = a > b;
        else if (op == TOK_LE) r = a <= b;
        else if (op == TOK_GE) r = a >= b;
        else return 0;
    }

    *resultp = r ? 1 : 0;
    return 1;
}

void emit_const_rel_branch(int rel_result, int label, int branch_when_true)
{
    if ((rel_result != 0) == (branch_when_true != 0))
        emit_jp_label("jp", label);
}

void emit_cmp_branch_false_unsigned(int op, int lfalse)
{
    int ltrue;

    emit("\tor a\n\tsbc hl,de\n");

    if (op == '<') {
        emit_jp_label("jp nc,", lfalse);
    } else if (op == TOK_GE) {
        emit_jp_label("jp c,", lfalse);
    } else if (op == '>') {
        emit_jp_label("jp z,", lfalse);
        emit_jp_label("jp c,", lfalse);
    } else if (op == TOK_LE) {
        ltrue = new_label();
        emit_jp_label("jp z,", ltrue);
        emit_jp_label("jp c,", ltrue);
        emit_jp_label("jp", lfalse);
        emit_label(ltrue);
    } else if (op == TOK_EQ) {
        emit_jp_label("jp nz,", lfalse);
    } else if (op == TOK_NE) {
        emit_jp_label("jp z,", lfalse);
    }
}

void emit_cmp_branch_true_unsigned(int op, int ltrue)
{
    int ldone;

    emit("\tor a\n\tsbc hl,de\n");

    if (op == '<') {
        emit_jp_label("jp c,", ltrue);
    } else if (op == TOK_GE) {
        emit_jp_label("jp nc,", ltrue);
    } else if (op == '>') {
        ldone = new_label();
        emit_jp_label("jp z,", ldone);
        emit_jp_label("jp nc,", ltrue);
        emit_label(ldone);
    } else if (op == TOK_LE) {
        emit_jp_label("jp z,", ltrue);
        emit_jp_label("jp c,", ltrue);
    } else if (op == TOK_EQ) {
        emit_jp_label("jp z,", ltrue);
    } else if (op == TOK_NE) {
        emit_jp_label("jp nz,", ltrue);
    }
}


void scan_until_stop_at_depth0(int stop_kind)
{
    int depth;

    depth = 0;

    while (tok.kind != TOK_EOF) {
        if (depth == 0 && tok.kind == stop_kind)
            break;

        if (tok.kind == '(' || tok.kind == '[') {
            depth++;
        } else if (tok.kind == ')' || tok.kind == ']') {
            if (depth > 0)
                depth--;
            else if (tok.kind == stop_kind)
                break;
        }

        next_token();
    }
}

int invert_relop_for_swap(int op)
{
    if (op == '<') return '>';
    if (op == '>') return '<';
    if (op == TOK_LE) return TOK_GE;
    if (op == TOK_GE) return TOK_LE;
    return op;
}

int parse_byte_const_operand(long *vp)
{
    long v;

    if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT)
        return 0;
    v = tok.val;
    if (v < 0 || v > 255)
        return 0;
    vp[0] = v;
    next_token();
    return 1;
}



int parse_global_byte_array_index_operand(struct ByteOperand *op)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *s;
    struct Sym *idx;

    if (tok.kind != TOK_ID)
        return 0;

    s = find_sym(tok.text);
    if (!s || s->storage != SC_GLOBAL || !s->is_array)
        return 0;
    if (type_size(s->type) != 1)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (!accept('['))
        goto fail;

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        op->idx_sym = NULL;
        op->val = tok.val;
        next_token();
    } else if (tok.kind == TOK_ID) {
        idx = find_sym(tok.text);
        if (!sym_can_ix_direct(idx) || type_size(idx->type) != 1)
            goto fail;
        op->idx_sym = idx;
        op->val = 0;
        next_token();
    } else {
        goto fail;
    }

    if (!accept(']'))
        goto fail;

    op->kind = 3;
    op->sym = s;
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

int parse_byte_operand_fast(struct ByteOperand *op)
{
    struct Sym *s;
    long v;

    memset(op, 0, sizeof(*op));

    if (parse_global_byte_array_index_operand(op))
        return 1;

    if (tok.kind == TOK_ID) {
        s = find_sym(tok.text);
        if (sym_can_ix_direct(s) && type_size(s->type) == 1 && (s->type & TYPE_UNSIGNED)) {
            op->kind = 1;
            op->sym = s;
            next_token();
            return 1;
        }
    }

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        v = tok.val;
        if (v >= 0 && v <= 255) {
            op->kind = 2;
            op->val = v;
            next_token();
            return 1;
        }
    }

    return 0;
}

void emit_byte_operand_to_a(struct ByteOperand *op)
{
    if (op->kind == 1) {
        fprintf(outf, "\tld a,(ix%+d)\n", op->sym->offset);
    } else if (op->kind == 2) {
        fprintf(outf, "\tld a,%ld\n", op->val & 255);
    } else if (op->kind == 3) {
        emit_extrn_if_needed(op->sym);
        if (op->idx_sym) {
            fprintf(outf, "\tld hl,%s\n", asm_name_for(sym_asm_name(op->sym)));
            fprintf(outf, "\tld e,(ix%+d)\n", op->idx_sym->offset);
            emit("\tld d,0\n");
            emit("\tadd hl,de\n");
            emit("\tld a,(hl)\n");
        } else {
            fprintf(outf, "\tld a,(%s+%ld)\n", asm_name_for(sym_asm_name(op->sym)), op->val & 0xffffL);
        }
    }
}

void emit_cp_byte_operand(struct ByteOperand *op)
{
    if (op->kind == 1) {
        fprintf(outf, "\tcp (ix%+d)\n", op->sym->offset);
    } else if (op->kind == 2) {
        fprintf(outf, "\tcp %ld\n", op->val & 255);
    } else if (op->kind == 3) {
        emit_extrn_if_needed(op->sym);
        if (op->idx_sym) {
            emit("\tld b,a\n");
            fprintf(outf, "\tld hl,%s\n", asm_name_for(sym_asm_name(op->sym)));
            fprintf(outf, "\tld e,(ix%+d)\n", op->idx_sym->offset);
            emit("\tld d,0\n");
            emit("\tadd hl,de\n");
            emit("\tld a,b\n");
            emit("\tcp (hl)\n");
        } else {
            fprintf(outf, "\tcp (%s+%ld)\n", asm_name_for(sym_asm_name(op->sym)), op->val & 0xffffL);
        }
    }
}

int byte_operand_can_be_lhs(struct ByteOperand *op)
{
    return op->kind == 1 || op->kind == 3;
}

void emit_byte_cmp_branch_after_cp(int op, int label, int branch_when_true)
{
    int ldone;

    if (branch_when_true) {
        if (op == TOK_EQ) {
            emit_jp_label("jp z,", label);
        } else if (op == TOK_NE) {
            emit_jp_label("jp nz,", label);
        } else if (op == '<') {
            emit_jp_label("jp c,", label);
        } else if (op == TOK_GE) {
            emit_jp_label("jp nc,", label);
        } else if (op == '>') {
            ldone = new_label();
            emit_jp_label("jp z,", ldone);
            emit_jp_label("jp nc,", label);
            emit_label(ldone);
        } else if (op == TOK_LE) {
            emit_jp_label("jp z,", label);
            emit_jp_label("jp c,", label);
        }
    } else {
        if (op == TOK_EQ) {
            emit_jp_label("jp nz,", label);
        } else if (op == TOK_NE) {
            emit_jp_label("jp z,", label);
        } else if (op == '<') {
            emit_jp_label("jp nc,", label);
        } else if (op == TOK_GE) {
            emit_jp_label("jp c,", label);
        } else if (op == '>') {
            emit_jp_label("jp z,", label);
            emit_jp_label("jp c,", label);
        } else if (op == TOK_LE) {
            ldone = new_label();
            emit_jp_label("jp z,", ldone);
            emit_jp_label("jp c,", ldone);
            emit_jp_label("jp", label);
            emit_label(ldone);
        }
    }
}

int gen_direct_byte_rel_branch_until(int label, int branch_when_true, int stop_kind)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct ByteOperand lhs;
    struct ByteOperand rhs;
    struct ByteOperand tmp;
    int op;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (!parse_byte_operand_fast(&lhs))
        goto fail;

    if (!is_relop_token(tok.kind))
        goto fail;
    op = tok.kind;
    next_token();

    if (!parse_byte_operand_fast(&rhs))
        goto fail;

    if (tok.kind != stop_kind)
        goto fail;

    /*
     * We need a real byte value on the left for "cp rhs".  If the left is
     * a constant and the right is a byte lvalue, swap and invert the relation:
     *     0 == g_board[p]  =>  g_board[p] == 0
     *     4 <= depth       =>  depth >= 4
     */
    if (!byte_operand_can_be_lhs(&lhs) && byte_operand_can_be_lhs(&rhs)) {
        op = invert_relop_for_swap(op);
        tmp = lhs;
        lhs = rhs;
        rhs = tmp;
    }

    if (!byte_operand_can_be_lhs(&lhs))
        goto fail;

    emit_byte_operand_to_a(&lhs);
    emit_cp_byte_operand(&rhs);
    emit_byte_cmp_branch_after_cp(op, label, branch_when_true);
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

int simple_direct_condition_until(int stop_kind)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int depth;
    int found;
    int bad;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    depth = 0;
    found = 0;
    bad = 0;

    while (tok.kind != TOK_EOF) {
        if (depth == 0 && tok.kind == stop_kind)
            break;

        /*
         * A float operand anywhere in the condition -- including inside
         * parentheses such as  if (x < (fa * fb))  -- means the integer
         * direct-branch comparison path cannot represent it.  Check at every
         * depth (not just depth 0) so it falls back to the general expression
         * path, which converts and compares as float.
         */
        if (tok.kind == TOK_FLOATLIT) {
            bad = 1;
        } else if (tok.kind == TOK_ID) {
            struct Sym *fs;
            fs = find_sym(tok.text);
            if (fs && type_is_float(fs->type))
                bad = 1;
        }

        if (depth == 0) {
            if (is_relop_token(tok.kind)) {
                if (found)
                    bad = 1;
                found = 1;
            } else if (tok.kind == TOK_ANDAND || tok.kind == TOK_OROR ||
                       tok.kind == '?' || is_assignment_token(tok.kind)) {
                bad = 1;
            }
        }

        if (tok.kind == '(' || tok.kind == '[')
            depth++;
        else if (tok.kind == ')' || tok.kind == ']') {
            if (depth > 0)
                depth--;
            else
                break;
        }

        next_token();
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;

    return found && !bad;
}


int snippet_is_single_pointer_id(const char *s)
{
    char name[64];
    int i;
    struct Sym *sym;

    /* copy_range() prefixes snippets with #line; skip those directives. */
    while (*s == '#' || *s == '\n' || *s == '\r') {
        if (*s == '#') {
            while (*s && *s != '\n') s++;
        } else {
            s++;
        }
    }

    while (*s == ' ' || *s == '\t')
        s++;

    if (!is_ident_start((unsigned char)*s))
        return 0;

    i = 0;
    while (is_ident_char((unsigned char)*s) && i < 63)
        name[i++] = *s++;
    name[i] = 0;

    while (*s == ' ' || *s == '\t' || *s == '\n' || *s == '\r')
        s++;

    if (*s != 0 && *s != ';')
        return 0;

    sym = find_sym(name);
    return sym && (sym->type & (TYPE_PTR | TYPE_PTR2));
}


const char *skip_snippet_ws_and_lines(const char *s)
{
    for (;;) {
        while (*s == ' ' || *s == '\t' || *s == '\r' || *s == '\n')
            s++;
        if (*s == '#') {
            while (*s && *s != '\n')
                s++;
            continue;
        }
        break;
    }
    return s;
}

int snippet_simple_type(const char *s, int *typep)
{
    char name[64];
    int n;
    int td;
    struct Sym *sym;
    const char *p;

    s = skip_snippet_ws_and_lines(s);

    if (*s == '(') {
        s++;
        s = skip_snippet_ws_and_lines(s);

        n = 0;
        while (is_ident_char((unsigned char)*s) && n < 63)
            name[n++] = *s++;
        name[n] = 0;

        if (!strcmp(name, "unsigned")) {
            s = skip_snippet_ws_and_lines(s);
            n = 0;
            while (is_ident_char((unsigned char)*s) && n < 63)
                name[n++] = *s++;
            name[n] = 0;
            if (!strcmp(name, "long")) {
                while (*s == ' ' || *s == '\t') s++;
                if (*s == ')') {
                    typep[0] = TYPE_LONG | TYPE_UNSIGNED;
                    return 1;
                }
            }
            if (!strcmp(name, "int") || name[0] == 0) {
                while (*s == ' ' || *s == '\t') s++;
                if (*s == ')') {
                    typep[0] = TYPE_INT | TYPE_UNSIGNED;
                    return 1;
                }
            }
            return 0;
        }

        if (!strcmp(name, "long")) {
            while (*s == ' ' || *s == '\t') s++;
            if (*s == ')') {
                typep[0] = TYPE_LONG;
                return 1;
            }
        }

        if (!strcmp(name, "int")) {
            while (*s == ' ' || *s == '\t') s++;
            if (*s == ')') {
                typep[0] = TYPE_INT;
                return 1;
            }
        }

        td = find_typedef(name);
        if (td >= 0) {
            int t;
            t = typedefs[td].type;
            while (*s == ' ' || *s == '\t') s++;
            while (*s == '*') {
                t = type_add_ptr(t);
                s++;
                while (*s == ' ' || *s == '\t') s++;
                if (!strncmp(s, "const", 5) && !is_ident_char((unsigned char)s[5])) {
                    s += 5;
                    while (*s == ' ' || *s == '\t') s++;
                }
            }
            if (*s == ')') {
                typep[0] = t;
                return 1;
            }
        }

        return 0;
    }

    if (isdigit((unsigned char)*s)) {
        struct ConstVal cv;

        if (!snippet_const_expr_value(s, &cv))
            return 0;
        typep[0] = promote_int_type(cv.type);
        return 1;
    }

    if (!is_ident_start((unsigned char)*s))
        return 0;

    n = 0;
    while (is_ident_char((unsigned char)*s) && n < 63)
        name[n++] = *s++;
    name[n] = 0;

    p = skip_snippet_ws_and_lines(s);
    if (*p == '[') {
        /* Subscripted expression: return element type of array/pointer.
         * This allows snippet_needs_long_compare to detect that expressions
         * like arr[i] have type long when arr is long * or long arr[N]. */
        sym = find_sym(name);
        if (!sym)
            return 0;
        if (sym->is_array) {
            /* Array variable: sym->type already is the element type */
            typep[0] = sym->type;
            return 1;
        }
        if (type_ptr_depth(sym->type) == 0)
            return 0;
        /* Pointer variable: decay one level to get the element type */
        typep[0] = type_decay_ptr(sym->type);
        return 1;
    }
    if (*p != 0 && *p != ';')
        return 0;

    sym = find_sym(name);
    if (!sym)
        return 0;

    typep[0] = sym->type;
    return 1;
}

int snippet_needs_long_compare(const char *lhs, const char *rhs, int *commonp)
{
    int lt;
    int rt;

    lt = TYPE_INT;
    rt = TYPE_INT;

    snippet_simple_type(lhs, &lt);
    snippet_simple_type(rhs, &rt);

    if (!type_is_long(lt) && !type_is_long(rt))
        return 0;

    commonp[0] = common_arith_type(lt, rt);
    return type_is_long(commonp[0]);
}

void emit_branch_on_bool_hl(int label, int branch_when_true)
{
    emit("\tld a,h\n\tor l\n");
    if (branch_when_true)
        emit_jp_label("jp nz,", label);
    else
        emit_jp_label("jp z,", label);
}

/*
 * Inline a 32-bit ordered comparison of a long value against a compile-time
 * constant, branching to `label`, instead of calling the __lts/__lgs/...
 * runtime helpers.  The long value must already be in DE:HL (DE = high word,
 * HL = low word); `rhs_code` is the constant operand's source text.
 *
 * Returns 1 if it emitted the comparison+branch, 0 if it declined (RHS not a
 * constant, a non-ordered relop, or a constant at the domain maximum where the
 * C+1 normalization below would wrap) so the caller can fall back to the
 * runtime path.  On the 0 path nothing is emitted here, but the caller has
 * already emitted the LHS into DE:HL (needed by both paths).
 *
 * Technique:
 *   1. Normalize `>` to `>= C+1` and `<=` to `< C+1`.  Because C is an integer
 *      constant, this makes the boundary exact and leaves only `<` / `>=`,
 *      neither of which needs to detect full 32-bit equality.
 *   2. For signed comparisons, flip bit 31 of both operands (the value via
 *      `ld a,d / xor 80h / ld d,a` at run time, the constant at compile time)
 *      so signed ordering becomes unsigned ordering.
 *   3. Compute value - C as an unsigned 32-bit subtract; the borrow (carry)
 *      out of the high word is set exactly when value < C.
 */
int try_emit_long_cmp_const_branch(int op, const char *rhs_code, int common_type,
                                   int label, int branch_when_true)
{
    struct ConstVal rcv;
    unsigned long cbits;
    int is_unsigned;
    int op_eff;
    int branch_on_carry;
    unsigned long clo;
    unsigned long chi;

    if (op != '<' && op != '>' && op != TOK_LE && op != TOK_GE)
        return 0;
    if (!snippet_const_expr_value(rhs_code, &rcv))
        return 0;

    is_unsigned = (common_type & TYPE_UNSIGNED) != 0;
    cf_convert_to_type(&rcv, common_type);
    cbits = rcv.u & 0xffffffffUL;

    op_eff = op;
    if (op == '>' || op == TOK_LE) {
        /* value > C  <=>  value >= C+1 ;  value <= C  <=>  value < C+1 */
        if (is_unsigned) {
            if (cbits == 0xffffffffUL)
                return 0;               /* C+1 wraps past UINT32_MAX */
        } else if (cbits == 0x7fffffffUL) {
            return 0;                   /* C+1 wraps past INT32_MAX */
        }
        cbits = (cbits + 1UL) & 0xffffffffUL;
        op_eff = (op == '>') ? TOK_GE : '<';
    }

    if (!is_unsigned) {
        emit("\tld a,d\n\txor 80h\n\tld d,a\n");
        cbits ^= 0x80000000UL;
    }

    clo = cbits & 0xffffUL;
    chi = (cbits >> 16) & 0xffffUL;

    /* value - C (unsigned 32-bit): carry set out of the high word <=> value < C */
    if (!scan_mode)
        fprintf(outf, "\tld bc,%lu\n", clo);
    emit("\tor a\n\tsbc hl,bc\n");
    emit("\tex de,hl\n");               /* HL = value high word, DE = low result (dead) */
    if (!scan_mode)
        fprintf(outf, "\tld bc,%lu\n", chi);
    emit("\tsbc hl,bc\n");

    /* op_eff is '<' or TOK_GE.  value < C  <=>  carry set. */
    branch_on_carry = (op_eff == '<') == (branch_when_true != 0);
    if (branch_on_carry)
        emit_jp_label("jp c,", label);
    else
        emit_jp_label("jp nc,", label);
    return 1;
}

void gen_direct_rel_branch_until(int op, int label, int branch_when_true,
                                        int stop_kind)
{
    long lhs_start;
    long lhs_end;
    long rhs_start;
    long rhs_end;
    char *lhs_code;
    char *rhs_code;
    long rhs_const;

    lhs_start = tok_start_pos;

    for (;;) {
        if (is_relop_token(tok.kind))
            break;
        next_token();
    }

    lhs_end = tok_start_pos;
    op = tok.kind;
    next_token();
    rhs_start = tok_start_pos;

    scan_until_stop_at_depth0(stop_kind);

    rhs_end = tok_start_pos;

    lhs_code = copy_range(lhs_start, lhs_end);
    rhs_code = copy_range(rhs_start, rhs_end);

    {
        struct ConstVal lhs_cv;
        struct ConstVal rhs_cv;
        int rel_result;
        if (snippet_const_expr_value(lhs_code, &lhs_cv) &&
            snippet_const_expr_value(rhs_code, &rhs_cv) &&
            const_rel_result(lhs_cv, op, rhs_cv, &rel_result)) {
            emit_const_rel_branch(rel_result, label, branch_when_true);
            free(lhs_code);
            free(rhs_code);
            return;
        }
    }

    if (const_positive_snippet_value(rhs_code, &rhs_const) &&
        (op == '<' || op == TOK_GE || op == '>' || op == TOK_LE)) {
        int ptr_cmp;
        ptr_cmp = snippet_is_single_pointer_id(lhs_code);
        gen_snippet_expr(lhs_code);
        emit_ld_de_const(rhs_const);
        if ((g_expr_type & TYPE_UNSIGNED) || ptr_cmp) {
            if (branch_when_true) emit_cmp_branch_true_unsigned(op, label);
            else emit_cmp_branch_false_unsigned(op, label);
        } else {
            if (branch_when_true) emit_cmp_branch_true(op, label);
            else emit_cmp_branch_false(op, label);
        }
    } else {
        int lhs_type;
        int rhs_type;
        int ptr_cmp;
        int common_type;

        if (snippet_needs_long_compare(lhs_code, rhs_code, &common_type)) {
            gen_snippet_expr(lhs_code);
            lhs_type = g_expr_type;
            emit_cast_16_to_common(lhs_type, common_type);
            /* Prefer an inline compare against a constant RHS; the LHS is now
             * in DE:HL, which both that path and the runtime fallback need. */
            if (!try_emit_long_cmp_const_branch(op, rhs_code, common_type,
                                                label, branch_when_true)) {
                emit("\tpush de\n\tpush hl\n");
                gen_snippet_expr(rhs_code);
                emit_cast_16_to_common(g_expr_type, common_type);
                gen_cmp32(op, common_type);
                emit_branch_on_bool_hl(label, branch_when_true);
            }
        } else {
            ptr_cmp = snippet_is_single_pointer_id(lhs_code) ||
                      snippet_is_single_pointer_id(rhs_code);
            gen_snippet_expr(lhs_code);
            lhs_type = g_expr_type;

            if (type_is_long(lhs_type)) {
                /*
                 * The LHS turned out to be a long expression that the snippet
                 * pre-analysis (snippet_simple_type) could not classify, e.g.
                 * a compound "a + b".  Compare as 32-bit so the high word is
                 * not silently dropped; the RHS is widened to the common type.
                 */
                emit("\tpush de\n\tpush hl\n");
                gen_snippet_expr(rhs_code);
                rhs_type = g_expr_type;
                common_type = common_arith_type(lhs_type, rhs_type);
                emit_cast_16_to_common(rhs_type, common_type);
                gen_cmp32(op, common_type);
                emit_branch_on_bool_hl(label, branch_when_true);
            } else {
                emit("\tpush hl\n");
                gen_snippet_expr(rhs_code);
                rhs_type = g_expr_type;
                common_type = common_arith_type(lhs_type, rhs_type);

                if (type_is_long(rhs_type)) {
                    /*
                     * The RHS is long but the LHS was generated as a 16-bit
                     * value (only HL was pushed).  Reclaim the 16-bit LHS,
                     * widen it to 32 bits, and compare as long with the
                     * operands swapped (the RHS is already in DE:HL, so it
                     * becomes gen_cmp32's stacked left operand and the relop
                     * is inverted).
                     */
                    emit("\tpop bc\n");              /* BC = 16-bit LHS */
                    emit("\tpush de\n\tpush hl\n");  /* stack = RHS (cmp32 LHS slot) */
                    emit("\tld h,b\n\tld l,c\n");    /* HL = 16-bit LHS */
                    emit_cast_16_to_common(lhs_type, common_type);
                    gen_cmp32(invert_relop_for_swap(op), common_type);
                    emit_branch_on_bool_hl(label, branch_when_true);
                } else {
                    emit("\tex de,hl\n\tpop hl\n");
                    if ((common_type & TYPE_UNSIGNED) || ptr_cmp) {
                        if (branch_when_true) emit_cmp_branch_true_unsigned(op, label);
                        else emit_cmp_branch_false_unsigned(op, label);
                    } else {
                        if (branch_when_true) emit_cmp_branch_true(op, label);
                        else emit_cmp_branch_false(op, label);
                    }
                }
            }
        }
    }

    free(lhs_code);
    free(rhs_code);
}


int gen_direct_byte_bitand_branch_until(int label, int branch_when_true, int stop_kind)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *s;
    long mask;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind != TOK_ID)
        goto fail;

    s = find_sym(tok.text);
    if (!sym_can_ix_direct(s) || type_size(s->type) != 1)
        goto fail;

    next_token();
    if (!accept('&'))
        goto fail;

    if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT)
        goto fail;
    mask = tok.val;
    if (mask < 0 || mask > 255)
        goto fail;
    next_token();

    if (tok.kind != stop_kind)
        goto fail;

    fprintf(outf, "\tld a,(ix%+d)\n", s->offset);
    fprintf(outf, "\tand %ld\n", mask & 255);
    if (branch_when_true)
        emit_jp_label("jp nz,", label);
    else
        emit_jp_label("jp z,", label);
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}


int gen_const_and_byte_condition_branch_until(int label, int branch_when_true, int stop_kind)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    long lhs_const;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (!parse_byte_const_operand(&lhs_const))
        goto fail;

    if (!accept(TOK_ANDAND))
        goto fail;

    /*
     * Constant-fold the common macro case:
     *     if (1 && byte_relation)
     * and the degenerate:
     *     if (0 && anything)
     *
     * This covers ttt's hot WinLosePrune/ABPrune-style tests after macro
     * expansion, without trying to implement general short-circuit lowering.
     */
    if (lhs_const == 0) {
        scan_until_stop_at_depth0(stop_kind);
        if (tok.kind != stop_kind)
            goto fail;
        if (!branch_when_true)
            emit_jp_label("jp", label);
        return 1;
    }

    if (gen_direct_byte_rel_branch_until(label, branch_when_true, stop_kind))
        return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}


int emit_cmp_const_branch_for_signed_local16(struct Sym *s, int op, long c,
                                                    int label, int branch_when_true)
{
    int ldone;

    if (!sym_can_ix_direct(s))
        return 0;
    if (type_size(s->type) != 2)
        return 0;
    if (s->type & TYPE_UNSIGNED)
        return 0;
    if (c < 0 || c > 255)
        return 0;

    /*
     * Fast signed 16-bit local/param compare against a small non-negative
     * constant.  This intentionally starts with the hot loop form only:
     *
     *     for (i = 0; i < 20; i++)
     *
     * Signed correctness is preserved:
     *   negative lhs       => lhs < c is true
     *   positive hi != 0   => lhs >= 256, so lhs < c is false
     *   hi == 0            => compare low byte with c
     */
    if (op == TOK_GE) {
        /*
         * var >= c  — only c == 0 is handled here.
         * var >= 0  ↔  var is non-negative  ↔  sign bit clear in hi byte.
         * This catches  for (i = N; 0 <= i; i--)  written as  const <= var.
         */
        if (c != 0)
            return 0;
        if (branch_when_true) {
            /* branch to label when var >= 0 (non-negative) */
            ldone = new_label();
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 1);
            emit("\tor a\n");
            emit_jp_label("jp m,", ldone);   /* negative: not >= 0, skip */
            emit_jp_label("jp", label);
            emit_label(ldone);
        } else {
            /* branch to label when var < 0 (negative) */
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 1);
            emit("\tor a\n");
            emit_jp_label("jp m,", label);   /* negative: branch */
        }
        return 1;
    }

    if (op != '<')
        return 0;

    if (branch_when_true) {
        ldone = new_label();
        fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 1);
        emit("\tor a\n");
        emit_jp_label("jp m,", label);     /* negative < c */
        emit_jp_label("jp nz,", ldone);    /* positive >= 256 */
        fprintf(outf, "\tld a,(ix%+d)\n", s->offset);
        fprintf(outf, "\tcp %ld\n", c & 255);
        emit_jp_label("jp c,", label);
        emit_label(ldone);
    } else {
        ldone = new_label();
        fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 1);
        emit("\tor a\n");
        emit_jp_label("jp m,", ldone);     /* negative => true, so not false */
        emit_jp_label("jp nz,", label);    /* positive >= 256 => false */
        fprintf(outf, "\tld a,(ix%+d)\n", s->offset);
        fprintf(outf, "\tcp %ld\n", c & 255);
        emit_jp_label("jp nc,", label);
        emit_label(ldone);
    }

    return 1;
}

int gen_direct_small_const_int_rel_branch_until(int label, int branch_when_true,
                                                       int stop_kind)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *s;
    int op;
    long c;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind != TOK_ID)
        goto try_const_op_var;
    s = find_sym(tok.text);
    if (!s)
        goto try_const_op_var;
    next_token();

    if (!is_relop_token(tok.kind))
        goto fail;
    op = tok.kind;
    next_token();

    if (!parse_byte_const_operand(&c))
        goto fail;

    if (tok.kind != stop_kind)
        goto fail;

    if (!emit_cmp_const_branch_for_signed_local16(s, op, c, label, branch_when_true))
        goto fail;

    return 1;

try_const_op_var:
    /* Handle const op var forms by flipping the operator:
     *   const >  var  →  var <  const  (e.g., 12 > i)
     *   const <= var  →  var >= const  (e.g.,  0 <= i, reverse loops)
     */
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;

    if (!parse_byte_const_operand(&c))
        goto fail;

    /* Accept '>' (const > var → var < const) or TOK_LE (const <= var → var >= const) */
    if (tok.kind == '>') {
        int flipped_op = '<';
        next_token();
        if (tok.kind != TOK_ID) goto fail;
        s = find_sym(tok.text); if (!s) goto fail;
        next_token();
        if (tok.kind != stop_kind) goto fail;
        if (!emit_cmp_const_branch_for_signed_local16(s, flipped_op, c, label, branch_when_true))
            goto fail;
        return 1;
    }

    if (tok.kind == TOK_LE) {
        int flipped_op = TOK_GE;
        next_token();
        if (tok.kind != TOK_ID) goto fail;
        s = find_sym(tok.text); if (!s) goto fail;
        next_token();
        if (tok.kind != stop_kind) goto fail;
        if (!emit_cmp_const_branch_for_signed_local16(s, flipped_op, c, label, branch_when_true))
            goto fail;
        return 1;
    }

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

int gen_condition_branch_false_until(int lfalse, int stop_kind)
{
    if (gen_const_and_byte_condition_branch_until(lfalse, 0, stop_kind))
        return 1;

    if (gen_direct_byte_bitand_branch_until(lfalse, 0, stop_kind))
        return 1;

    if (gen_direct_byte_rel_branch_until(lfalse, 0, stop_kind))
        return 1;

    if (gen_direct_small_const_int_rel_branch_until(lfalse, 0, stop_kind))
        return 1;

    if (!simple_direct_condition_until(stop_kind))
        return 0;

    gen_direct_rel_branch_until(0, lfalse, 0, stop_kind);
    return 1;
}

int gen_condition_branch_true_until(int ltrue, int stop_kind)
{
    if (gen_const_and_byte_condition_branch_until(ltrue, 1, stop_kind))
        return 1;

    if (gen_direct_byte_bitand_branch_until(ltrue, 1, stop_kind))
        return 1;

    if (gen_direct_byte_rel_branch_until(ltrue, 1, stop_kind))
        return 1;

    if (gen_direct_small_const_int_rel_branch_until(ltrue, 1, stop_kind))
        return 1;

    if (!simple_direct_condition_until(stop_kind))
        return 0;

    gen_direct_rel_branch_until(0, ltrue, 1, stop_kind);
    return 1;
}

int gen_condition_branch_false(int lfalse)
{
    return gen_condition_branch_false_until(lfalse, ')');
}

int gen_condition_branch_true(int ltrue)
{
    return gen_condition_branch_true_until(ltrue, ')');
}


