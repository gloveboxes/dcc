/*
 * dcc_expr.c - expression code generation (core).
 *
 * The heart of the syntax-directed translator: gen_primary/gen_unary and the
 * many helpers for casts, dereference/address-of, function-call argument
 * marshalling (scalar, struct, variadic), and a large set of recognised
 * fast-path peepholes for common idioms. Drives the operator fragments below.
 *
 * MODULE: compiled as its own translation unit; shared declarations are in dcc.h.
 * Source provenance: monolith src/ddc.c lines 5373-8841.
 */

#include "dcc.h"
int parse_sizeof_expr_operand(void)
{
    int type;
    int sz;
    int rhs_type;
    int rhs_sz;
    int op;

    if (!sizeof_parse_primary_type(&type, &sz))
        return sz;

    /* Consume a simple expression without emitting code.  This is intentionally
     * conservative, but it is enough for C89 sizeof expression cases such as:
     *     sizeof a + b       (as parsed by caller inside parentheses)
     *     sizeof(a + 1L)
     *     sizeof(p[0])
     *     sizeof(s.field)
     * Stop at delimiters that belong to the surrounding grammar. */
    while (tok.kind != TOK_EOF && tok.kind != ')' && tok.kind != ']' &&
           tok.kind != ',' && tok.kind != ';') {
        op = tok.kind;

        if (op == '?' || op == ':')
            break;

        next_token();
        if (!sizeof_parse_primary_type(&rhs_type, &rhs_sz))
            break;

        type = sizeof_common_type(type, rhs_type, op);
        sz = type_size(type);
        if (sz <= 0) sz = 2;
    }

    return sz;
}


void emit_load_from_hl(int type)
{
    if (type_size(type) == 1) {
        emit("\tld l,(hl)\n");
        if (type & TYPE_UNSIGNED)
            emit("\tld h,0\n");
        else
            emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
    } else if (type_size(type) == 4) {
        /* read 4 bytes little-endian from [HL] into DE:HL (DE=high, HL=low) */
        emit("\tld e,(hl)\n");
        emit("\tinc hl\n");
        emit("\tld d,(hl)\n");
        emit("\tinc hl\n");
        emit("\tld a,(hl)\n");
        emit("\tinc hl\n");
        emit("\tld h,(hl)\n");
        emit("\tld l,a\n");
        /* now H=high_hi, L=high_lo, D=low_hi, E=low_lo — need DE:HL swapped */
        emit("\tex de,hl\n"); /* HL=low word, DE=high word */
    } else {
        emit("\tld e,(hl)\n");
        emit("\tinc hl\n");
        emit("\tld d,(hl)\n");
        emit("\tex de,hl\n");
    }
}

void emit_store_de_to_addr_hl(int type)
{
    if (type_size(type) == 1) {
        emit("\tld (hl),e\n");
    } else if (type_size(type) == 4) {
        /* value in DE:HL (DE=high, HL=low), address on stack (2 bytes) */
        emit("\tld b,d\n\tld c,e\n"); /* BC = high word */
        emit("\tpop de\n");           /* DE = address (popped from where caller pushed it) */
        emit("\tex de,hl\n");         /* HL = address, DE = low word */
        emit("\tld (hl),e\n");
        emit("\tinc hl\n");
        emit("\tld (hl),d\n");
        emit("\tinc hl\n");
        emit("\tld (hl),c\n");
        emit("\tinc hl\n");
        emit("\tld (hl),b\n");
    } else {
        emit("\tld (hl),e\n");
        emit("\tinc hl\n");
        emit("\tld (hl),d\n");
    }
}


int type_is_struct_object(int type)
{
    return (type & TYPE_STRUCT) && type_ptr_depth(type) == 0;
}

int same_struct_type(int a, int b)
{
    return type_is_struct_object(a) && type_is_struct_object(b) &&
           type_struct_id(a) == type_struct_id(b);
}

void emit_copy_de_to_hl_bytes(int n)
{
    int lab;

    if (n <= 0)
        return;

    lab = new_label();
    if (n <= 255) {
        fprintf(outf, "\tld b,%d\n", n);
        emit_label(lab);
        emit("\tld a,(de)\n");
        emit("\tld (hl),a\n");
        emit("\tinc de\n");
        emit("\tinc hl\n");
        fprintf(outf, "\tdjnz L%d\n", lab);
    } else {
        fprintf(outf, "\tld bc,%d\n", n);
        emit_label(lab);
        emit("\tld a,(de)\n");
        emit("\tld (hl),a\n");
        emit("\tinc de\n");
        emit("\tinc hl\n");
        emit("\tdec bc\n");
        emit("\tld a,b\n");
        emit("\tor c\n");
        emit_jp_label("jp nz,", lab);
    }
}

void emit_push_struct_arg_from_hl(int n)
{
    if (n <= 0)
        return;
    emit("\tex de,hl\n");          /* DE = source */
    fprintf(outf, "\tld hl,-%d\n", n);
    emit("\tadd hl,sp\n");        /* HL = destination */
    emit("\tld sp,hl\n");
    emit_copy_de_to_hl_bytes(n);
}

void emit_load_hl_from_sp_offset(int off)
{
    if (off == 0) {
        emit("\tpop hl\n\tpush hl\n");
    } else {
        fprintf(outf, "\tld hl,%d\n", off);
        emit("\tadd hl,sp\n");
        emit("\tld e,(hl)\n");
        emit("\tinc hl\n");
        emit("\tld d,(hl)\n");
        emit("\tex de,hl\n");
    }
}

void gen_expr(void);
void gen_expr_no_comma(void);
void gen_unary(void);
void gen_snippet_lvalue_addr(const char *snippet, int *ptype);
int snippet_is_single_pointer_id(const char *s);
void gen_statement(void);
int parse_funcptr_declarator(int *ptype, char *name, int namesz)
{
    int type;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    g_funcptr_decl_array_len = 0;
    g_ptr_array_dim_count = 0;
    g_ptr_array_elem_size = 0;
    memset(g_ptr_array_dims, 0, sizeof(g_ptr_array_dims));

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (!accept('*') || tok.kind != TOK_ID) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    strncpy(name, tok.text, namesz - 1);
    name[namesz - 1] = 0;
    next_token();

    if (accept('[')) {
        if (tok.kind == ']') {
            g_funcptr_decl_array_len = 0;
            next_token();
        } else {
            g_funcptr_decl_array_len = parse_const_int_expr();
            expect(']');
        }
    }

    if (!accept(')')) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        g_funcptr_decl_array_len = 0;
        g_ptr_array_dim_count = 0;
        g_ptr_array_elem_size = 0;
        memset(g_ptr_array_dims, 0, sizeof(g_ptr_array_dims));
        return 0;
    }

    type = type_add_ptr(ptype[0]);

    if (accept('(')) {
        while (tok.kind != ')' && tok.kind != TOK_EOF)
            next_token();
        expect(')');
    } else if (tok.kind == '[') {
        int dims[8];
        int ndims;
        int i;
        int n;
        int elem_bytes;
        int total;

        ndims = 0;
        memset(dims, 0, sizeof(dims));
        while (accept('[')) {
            if (tok.kind == ']') {
                n = 0;
                next_token();
            } else {
                n = parse_const_int_expr();
                expect(']');
            }
            if (n < 0) n = 0;
            if (ndims < 8) dims[ndims++] = n;
        }

        elem_bytes = type_size(ptype[0]);
        if (elem_bytes <= 0) elem_bytes = 2;
        total = 1;
        for (i = 0; i < ndims; ++i) {
            if (dims[i] <= 0) {
                total = 0;
                break;
            }
            total *= dims[i];
        }
        g_ptr_array_dim_count = ndims;
        g_ptr_array_elem_size = total > 0 ? total * elem_bytes : elem_bytes;
        for (i = 0; i < ndims && i < 8; ++i)
            g_ptr_array_dims[i] = dims[i];
    }

    ptype[0] = type;
    return 1;
}


int parse_abstract_funcptr_declarator(int *ptype)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int type;

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (!accept('*')) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (!accept(')')) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    type = type_add_ptr(ptype[0]);

    if (accept('(')) {
        while (tok.kind != ')' && tok.kind != TOK_EOF)
            next_token();
        expect(')');
    } else if (tok.kind == '[') {
        while (accept('[')) {
            if (tok.kind != ']')
                (void)parse_const_int_expr();
            expect(']');
        }
    } else {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    ptype[0] = type;
    return 1;
}

int char_array_string_initializer_size(int base_type)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int n;

    if ((base_type & 15) != TYPE_CHAR || type_ptr_depth(base_type) != 0)
        return 0;
    if (tok.kind != '=')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (tok.kind == TOK_STR) {
        char *lit;
        int is_wide;

        /*
         * Omitted-size char arrays must be sized from the whole C string
         * literal sequence, not just the first token.  The old lookahead used
         * tok.text directly, so:
         *
         *     char t[] = "xy" "z";
         *
         * allocated only sizeof("xy") == 3 bytes, while code generation
         * later emitted the concatenated four-byte initializer.  On CP/M this
         * overwrote the next local slot and sizeof(t) was also wrong.
         */
        lit = read_adjacent_string_literals_ex(&is_wide);
        if (is_wide)
            n = 0;
        else
            n = (int)strlen(lit) + 1;
        free(lit);
    } else {
        n = 0;
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return n;
}

/*
 * Parse one or more array declarator dimensions after the identifier.
 *
 * DCC stores arrays as a flat byte/object allocation.  For multidimensional
 * arrays, total_len is the product of all dimensions, and first_stride_elems
 * is the product of the inner dimensions.  Example:
 *
 *     char bufs[2][256]
 *
 * total_len = 512, first_stride_elems = 256.  Existing array indexing code
 * already uses Sym.elem_size as the stride for the first index, so bufs[i]
 * points at the correct row without needing a full C array type system.
 */
void parse_array_declarator_dims(int base_type,
                                        int *total_len,
                                        int *first_stride_bytes,
                                        int allow_empty_first)
{
    int dims[8];
    int ndims;
    int i;
    int n;
    int elem_bytes;
    int total;
    int inner;

    ndims = 0;
    g_last_array_dim_count = 0;
    memset(g_last_array_dims, 0, sizeof(g_last_array_dims));

    while (accept('[')) {
        if (tok.kind == ']') {
            next_token();
            n = (allow_empty_first && ndims == 0)
                    ? char_array_string_initializer_size(base_type)
                    : 0;
        } else {
            n = parse_const_int_expr();
            expect(']');
        }

        if (n < 0)
            n = 0;
        if (ndims < 8)
            dims[ndims++] = n;
    }

    if (ndims == 0) {
        total_len[0] = 0;
        first_stride_bytes[0] = 0;
        return;
    }

    total = 1;
    for (i = 0; i < ndims; ++i) {
        if (dims[i] <= 0) {
            total = 0;
            break;
        }
        total *= dims[i];
    }

    elem_bytes = type_size(base_type);
    if (elem_bytes <= 0)
        elem_bytes = 2;

    inner = 1;
    for (i = 1; i < ndims; ++i) {
        if (dims[i] <= 0) {
            inner = 0;
            break;
        }
        inner *= dims[i];
    }

    total_len[0] = total;
    first_stride_bytes[0] = (ndims > 1 && inner > 0) ? inner * elem_bytes : elem_bytes;

    g_last_array_dim_count = ndims;
    for (i = 0; i < ndims && i < 8; ++i)
        g_last_array_dims[i] = dims[i];
}




int count_initializer_atoms_level(void)
{
    int n;
    int depth;

    n = 0;

    if (accept('{')) {
        while (tok.kind != TOK_EOF && tok.kind != '}') {
            n += count_initializer_atoms_level();
            if (!accept(','))
                break;
            if (tok.kind == '}')
                break;
        }
        expect('}');
        return n;
    }

    depth = 0;
    while (tok.kind != TOK_EOF) {
        if (depth == 0 && (tok.kind == ',' || tok.kind == '}'))
            break;

        if (tok.kind == '(' || tok.kind == '[' || tok.kind == '{') {
            depth++;
        } else if (tok.kind == ')' || tok.kind == ']' || tok.kind == '}') {
            if (depth > 0)
                depth--;
            else
                break;
        }

        next_token();
    }

    return 1;
}

int count_omitted_array_initializer_atoms(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int n;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    n = 0;
    if (accept('=') && tok.kind == '{')
        n = count_initializer_atoms_level();

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return n;
}

/*
 * Count the TOP-LEVEL comma-separated elements inside the outer initializer
 * brace, regardless of how many scalar atoms each element spells.  For
 * { {1},{2},{3} } this returns 3 even though each braced group is a *partial*
 * struct that only initializes its first field.  count_initializer_atoms_level
 * is reused to skip over each element's contents.
 */
int count_initializer_top_elems_level(void)
{
    int n;

    n = 0;
    if (accept('{')) {
        while (tok.kind != TOK_EOF && tok.kind != '}') {
            count_initializer_atoms_level();   /* skip one whole element */
            n++;
            if (!accept(','))
                break;
            if (tok.kind == '}')
                break;
        }
        expect('}');
    }
    return n;
}

int count_omitted_array_initializer_top_elems(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int n;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    n = 0;
    if (accept('=') && tok.kind == '{')
        n = count_initializer_top_elems_level();

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return n;
}

void emit_init_auto_char_array_from_string(struct Sym *s, const char *str)
{
    int i;
    int n;
    int limit;

    n = (int)strlen(str) + 1;
    limit = s->size;
    if (limit <= 0)
        limit = n;

    /*
     * Automatic aggregate initializers must zero-fill any elements not
     * explicitly initialized.  The old code emitted only the string bytes
     * plus the first NUL, leaving the rest of a larger local char array
     * containing old stack contents.
     *
     * For char s[3] = "abc", limit is the declared size and the terminating
     * NUL is correctly not emitted.  For char s[8] = "abc", bytes 4..7 are
     * now explicitly zeroed.
     */
    for (i = 0; i < limit; ++i) {
        int ch;
        ch = (i + 1 < n) ? ((unsigned char)str[i]) : 0;
        emit_load_sym_addr(s);
        emit_add_const_to_hl(i);
        fprintf(outf, "\tld e,%d\n", ch);
        emit_store_de_to_addr_hl(TYPE_CHAR);
    }
}

void parse_typedef_decl(void);
void parse_global_init_list(struct Sym *s);
void scan_static_local_decl_after_type(int base);
char *copy_range(long a, long b);
void gen_snippet_expr(const char *snippet);
void emit_incdec_addr(int type, int op);

/* Look up or pre-allocate a user label ID (for goto / label: targets).
 * Labels are function-scoped; nulabels is reset before each function. */
int find_or_alloc_user_label_index(const char *name)
{
    int i;

    for (i = 0; i < nulabels; ++i)
        if (!strcmp(ulabel_names[i], name))
            return i;

    if (nulabels >= MAX_USER_LABELS) fatal("too many goto labels");
    memset(&ulabel_names[nulabels], 0, sizeof(ulabel_names[nulabels]));
    strncpy(ulabel_names[nulabels], name, sizeof(ulabel_names[nulabels]) - 1);
    ulabel_ids[nulabels] = new_label();
    ulabel_defined[nulabels] = 0;
    ulabel_referenced[nulabels] = 0;
    return nulabels++;
}

int mark_user_label_reference(const char *name)
{
    int i;

    i = find_or_alloc_user_label_index(name);
    ulabel_referenced[i] = 1;
    return ulabel_ids[i];
}

int define_user_label(const char *name)
{
    int i;

    i = find_or_alloc_user_label_index(name);
    if (ulabel_defined[i])
        error_here("duplicate goto label");
    ulabel_defined[i] = 1;
    return ulabel_ids[i];
}

void check_undefined_user_labels(void)
{
    int i;

    for (i = 0; i < nulabels; ++i) {
        if (ulabel_referenced[i] && !ulabel_defined[i]) {
            fprintf(stderr, "%s:%d: error: undefined goto label '%s'\n",
                    tok.file[0] ? tok.file : (input_name ? input_name : "<input>"),
                    tok_line, ulabel_names[i]);
            errors++;
            if (errors > 40) fatal("too many errors");
        }
    }
}

/* Parse an integer constant expression used in enum bodies.
 * Keep the signed target value in an int-sized host object; code generation
 * masks it back to the 16-bit target representation when emitted. */
int parse_enum_const_value(void)
{
    return (int)parse_const_long_expr();
}



int bracket_expr_has_field_access(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int depth;
    int found;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    depth = 1;
    found = 0;
    while (tok.kind != TOK_EOF && depth > 0) {
        if (tok.kind == '[' || tok.kind == '(')
            depth++;
        else if (tok.kind == ']' || tok.kind == ')')
            depth--;
        else if (tok.kind == TOK_ARROW || tok.kind == '.')
            found = 1;

        if (depth > 0)
            next_token();
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return found;
}

int try_fast_global_char_array_condition(int false_label)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    char name[64];
    struct Sym *s;

    if (tok.kind != TOK_ID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    next_token();

    s = find_global(name);
    if (!is_global_char_array_sym(s) || !accept('[')) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (bracket_expr_has_field_access()) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (!emit_simple_local_index_to_hl())
        gen_expr();             /* index result in HL */
    expect(']');

    /*
     * Only optimize the bare condition form:
     *     if (flags[i])
     * Do not consume ==, !=, &&, etc.
     */
    if (tok.kind != ')') {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    emit_test_global_char_index_zero(s, false_label);
    return 1;
}

int is_assignment_token(int k)
{
    return k == '=' || k == TOK_ADDEQ || k == TOK_SUBEQ || k == TOK_MULEQ ||
           k == TOK_DIVEQ || k == TOK_MODEQ || k == TOK_ANDEQ || k == TOK_OREQ ||
           k == TOK_XOREQ || k == TOK_SHLEQ || k == TOK_SHREQ;
}

int skip_lvalue_syntax(void)
{
    int depth;

    /*
     * Lookahead-only parser for the left side of assignments and
     * statement-level postfix ++/--.
     *
     * Return nonzero only if a complete lvalue syntax was skipped.  This
     * matters for expressions such as:
     *
     *     while ((dir = readdir(d)) != NULL)
     *
     * The previous version accepted the leading '(' and then stopped at the
     * inner '=', so lookahead_is_assignment() misclassified the whole
     * parenthesized assignment expression as an lvalue assignment and the
     * real parser later failed near '='.
     */
    if (tok.kind == '(') {
        next_token();
        if (!skip_lvalue_syntax())
            return 0;
        if (tok.kind != ')')
            return 0;
        next_token();
    } else if (tok.kind == '*') {
        next_token();
        if (tok.kind == '(') {
            depth = 1;
            next_token();
            while (tok.kind != TOK_EOF && depth > 0) {
                if (tok.kind == '(' || tok.kind == '[') depth++;
                else if (tok.kind == ')' || tok.kind == ']') depth--;
                next_token();
            }
            if (depth != 0)
                return 0;
            /* This may have been a cast in a dereference lvalue, e.g.
             *     *(struct S *)p = rhs;
             * Consume the simple cast operand so lookahead reaches '='. */
            if (tok.kind == TOK_ID) {
                next_token();
            } else if (tok.kind == '(') {
                depth = 1;
                next_token();
                while (tok.kind != TOK_EOF && depth > 0) {
                    if (tok.kind == '(' || tok.kind == '[') depth++;
                    else if (tok.kind == ')' || tok.kind == ']') depth--;
                    next_token();
                }
                if (depth != 0)
                    return 0;
            }
        } else if (tok.kind == TOK_ID) {
            next_token();
            /*
             * *p++ and *p-- are valid lvalues: the address being assigned is
             * the old value of p, then p is updated.  This form is common in
             * code such as:
             *
             *     *p++ = (unsigned char)c;
             *
             * The statement-level inc/dec lookahead added for (*exp)++ must
             * not make assignment lookahead reject this lvalue before it sees
             * the following '='.
             */
            if (tok.kind == TOK_INC || tok.kind == TOK_DEC)
                next_token();
        } else {
            return 0;
        }
    } else {
        if (tok.kind != TOK_ID)
            return 0;
        next_token();
    }

    for (;;) {
        if (tok.kind == '[') {
            depth = 1;
            next_token();
            while (tok.kind != TOK_EOF && depth > 0) {
                if (tok.kind == '[' || tok.kind == '(') depth++;
                else if (tok.kind == ']' || tok.kind == ')') depth--;
                next_token();
            }
            if (depth != 0)
                return 0;
        } else if (tok.kind == '.' || tok.kind == TOK_ARROW) {
            next_token();
            if (tok.kind != TOK_ID)
                return 0;
            next_token();
        } else {
            break;
        }
    }

    return 1;
}

int lookahead_is_assignment(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int r;

    if (tok.kind != TOK_ID && tok.kind != '*' && tok.kind != '(') return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind == TOK_ID) {
        next_token();
        if (tok.kind == '(') {
            posi = save_pos;
            tok_start_pos = save_tok_start;
            line_no = save_line;
            tok_line = save_tok_line;
            tok = save_tok;
            return 0;
        }
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
    }

    if (!skip_lvalue_syntax()) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }
    r = is_assignment_token(tok.kind);

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return r;
}


int try_fast_global_char_array_store(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    char name[64];
    struct Sym *s;
    int op;
    int rhs_is_const;
    long rhs_val;

    if (tok.kind != TOK_ID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    next_token();

    s = find_global(name);
    if (!is_global_char_array_sym(s) || !accept('[')) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (bracket_expr_has_field_access()) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (!emit_simple_local_index_to_hl()) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }
    expect(']');

    op = tok.kind;
    if (op != '=') {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }
    next_token();

    rhs_is_const = 0;
    rhs_val = 0;
    if (tok.kind == TOK_FLOATLIT) {
        error_float_unsupported("float literal not supported yet");
        next_token();
        return 0;
    }

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        rhs_is_const = 1;
        rhs_val = tok.val & 255;
        next_token();
    }

    if (!rhs_is_const) {
        /*
         * Keep this path narrow.  General RHS expression stores still use
         * the normal lvalue path.
         */
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    emit_global_char_index_addr(s);

    if (rhs_val == 0) {
        emit("\tld (hl),0\n");
    } else if (rhs_val == 1) {
        emit("\tld (hl),1\n");
    } else {
        fprintf(outf, "\tld (hl),%ld\n", rhs_val & 255);
    }

    return 1;
}


void gen_post_update_symbol_addr_value(struct Sym *s, int op)
{
    int t;

    t = s->type;

    emit_load_sym_addr(s);          /* HL = address of pointer variable */
    emit("\tpush hl\n");            /* save pointer variable address */
    emit_load_from_hl(t);           /* HL = old pointer value */
    emit("\tpush hl\n");            /* save old pointer value for lvalue */

    {
        int elem = type_index_elem_size(t);
        if (op == TOK_INC) {
            emit("\tinc hl\n");
            if (elem >= 2) emit("\tinc hl\n");
            if (elem >= 4) { emit("\tinc hl\n"); emit("\tinc hl\n"); }
        } else {
            emit("\tdec hl\n");
            if (elem >= 2) emit("\tdec hl\n");
            if (elem >= 4) { emit("\tdec hl\n"); emit("\tdec hl\n"); }
        }
    }

    emit("\tex de,hl\n");           /* DE = new pointer value */
    emit("\tpop hl\n");             /* HL = old pointer value */
    emit("\tex (sp),hl\n");         /* HL = pointer variable address, stack = old pointer */
    emit_store_de_to_addr_hl(t);    /* store new pointer */
    emit("\tpop hl\n");             /* HL = old pointer, used as lvalue address */
    g_expr_type = t;
}

/*
 * Try lvalue forms used heavily by string code:
 *     *p = ...
 *     *p++ = ...
 *     *p-- = ...
 * This returns the address being assigned in HL.
 */
int try_gen_deref_postinc_lvalue_addr(int *ptype)
{
    struct Sym *s;
    char name[64];
    int op;
    int base;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    if (tok.kind != '*')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();

    if (tok.kind != TOK_ID) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;

    s = find_sym(name);
    if (!s) {
        error_here("undefined symbol");
        s = add_global(name, TYPE_INT | TYPE_PTR, SC_GLOBAL);
    }

    next_token();

    if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
        op = tok.kind;
        next_token();
        gen_post_update_symbol_addr_value(s, op);
    } else {
        emit_load_sym_addr(s);
        emit_load_from_hl(s->type);
    }

    if (ptype) {
        base = type_decay_ptr(s->type);
        if ((base & 15) == TYPE_VOID)
            base = TYPE_CHAR;
        ptype[0] = base;
    }

    return 1;
}


int try_parse_const_subscript(long *out)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    long v;

    if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    v = tok.val;
    next_token();
    if (accept(']')) {
        out[0] = v;
        return 1;
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

/*
 * Snapshot the current_field_array_* dimension metadata into caller-owned
 * locals.  An index expression evaluated via gen_expr() may perform its own
 * struct field accesses, which overwrite these globals; capturing them before
 * any index is evaluated keeps multidimensional field-array strides correct.
 */
static void snapshot_field_array_dims(int *dim_count, int *dims)
{
    int di;
    *dim_count = current_field_array_dim_count;
    for (di = 0; di < 4; ++di)
        dims[di] = current_field_array_dims[di];
}

/*
 * Byte stride of array index `index_count` (0-based) for a field array whose
 * base element size is `base_size` and whose dimensions are the snapshot
 * `dim_count`/`dims`.  For a[d0][d1]...[dn-1], advancing index k moves by
 * base_size * d[k+1] * ... * d[n-1] bytes.
 */
static int field_array_index_stride(int base_size, int dim_count,
                                    const int *dims, int index_count)
{
    int stride;
    int di;
    stride = base_size;
    for (di = index_count + 1; di < dim_count; ++di)
        stride *= dims[di];
    return stride;
}

void gen_lvalue_addr(int *ptype)
{
    current_field_bit_width = 0;
    current_field_bit_shift = 0;
    current_field_bit_mask = 0;
    char name[64];
    struct Sym *s;

    if (try_gen_deref_postinc_lvalue_addr(ptype))
        return;

    int elem_size;
    int addr_is_array;
    int addr_array_index_count;
    int cur_type;
    int arrow;
    int field_is_array;
    int fa_dimc;
    int fa_dims[4];

    if (tok.kind == TOK_ID) {
        strncpy(name, tok.text, sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;
        next_token();

        s = find_sym(name);
        if (!s) {
            error_here("undefined symbol");
            s = add_global(name, TYPE_INT, SC_GLOBAL);
        }

        {
        int lva_global_ptr_preloaded = 0;
        /* For a global pointer variable immediately subscripted, load the
         * pointer value directly with ld hl,(nn) and skip the later deref. */
        if (is_global_word_sym(s) && !s->is_array &&
            type_ptr_depth(s->type) > 0 && tok.kind == '[') {
            emit_load_global_word_direct(s);
            lva_global_ptr_preloaded = 1;
        } else {
            emit_load_sym_addr(s);
        }
        if (ptype) {
            ptype[0] = s->type;
        }
        addr_is_array = s->is_array;
        addr_array_index_count = 0;

        while (accept('[')) {
            cur_type = ptype ? *ptype : s->type;

            /*
             * For a real array, the symbol address is already the base.
             * For a pointer variable/parameter, the symbol address names the
             * pointer object, so load the pointer value before indexing.
             */
            if (!s->is_array && type_ptr_depth(cur_type) > 0) {
                if (!lva_global_ptr_preloaded)
                    emit_load_from_hl(cur_type);
                lva_global_ptr_preloaded = 0;
            }

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (s->is_array)
                    elem_size = sym_array_index_elem_size(s, addr_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, addr_array_index_count);
                addr_array_index_count++;

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                int old_dead;
                emit("\tpush hl\n");
                old_dead = expr_result_dead;
                expr_result_dead = 0;
                gen_expr();
                expr_result_dead = old_dead;
                expect(']');

                if (s->is_array)
                    elem_size = sym_array_index_elem_size(s, addr_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, addr_array_index_count);
                addr_array_index_count++;

                scale_hl_by_elem_size(elem_size);
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (ptype) {
                if (s->is_array)
                    ptype[0] = cur_type;
                else
                    ptype[0] = type_decay_ptr(cur_type);
            }
        }

        while (tok.kind == '.' || tok.kind == TOK_ARROW) {
            arrow = tok.kind == TOK_ARROW;
            next_token();

            cur_type = ptype ? *ptype : s->type;
            cur_type = apply_field_access_from_addr(cur_type, arrow,
                                                    &field_is_array);
            addr_is_array = field_is_array;
            addr_array_index_count = 0;
            if (ptype) {
                ptype[0] = cur_type;
            }
        }

        snapshot_field_array_dims(&fa_dimc, fa_dims);
        while (accept('[')) {
            cur_type = ptype ? *ptype : s->type;

            if (!addr_is_array && type_ptr_depth(cur_type) > 0)
                emit_load_from_hl(cur_type);

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (addr_is_array)
                    elem_size = field_array_index_stride(type_size(cur_type),
                                                         fa_dimc, fa_dims,
                                                         addr_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, addr_array_index_count);

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                int old_dead;
                emit("\tpush hl\n");
                old_dead = expr_result_dead;
                expr_result_dead = 0;
                gen_expr();
                expr_result_dead = old_dead;
                expect(']');

                if (addr_is_array)
                    elem_size = field_array_index_stride(type_size(cur_type),
                                                         fa_dimc, fa_dims,
                                                         addr_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, addr_array_index_count);

                scale_hl_by_elem_size(elem_size);
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (addr_is_array) {
                addr_array_index_count++;
                if (addr_array_index_count >= fa_dimc)
                    addr_is_array = 0;
            } else {
                cur_type = type_decay_ptr(cur_type);
                if (ptype) {
                ptype[0] = cur_type;
            }
            }
        }

        /*
         * Handle postfix field access after subscripted pointer-to-struct
         * lvalues, e.g. rs[rp].kind when rs macro-expands to G->s_rs.
         * The field loop above handles G->s_rs before the bracket; this one
         * handles .kind after the bracket.
         */
        while (tok.kind == '.' || tok.kind == TOK_ARROW) {
            arrow = tok.kind == TOK_ARROW;
            next_token();

            cur_type = ptype ? *ptype : s->type;
            cur_type = apply_field_access_from_addr(cur_type, arrow,
                                                    &field_is_array);
            addr_is_array = field_is_array;
            addr_array_index_count = 0;
            if (ptype) {
                ptype[0] = cur_type;
            }

            snapshot_field_array_dims(&fa_dimc, fa_dims);
            while (accept('[')) {
                cur_type = ptype ? *ptype : s->type;

                if (!addr_is_array && type_ptr_depth(cur_type) > 0)
                    emit_load_from_hl(cur_type);

                {
                    long const_index;

                    if (try_parse_const_subscript(&const_index)) {
                        if (addr_is_array)
                            elem_size = field_array_index_stride(type_size(cur_type),
                                                                 fa_dimc, fa_dims,
                                                                 addr_array_index_count);
                        else
                            elem_size = type_index_elem_size(cur_type);
                        emit_add_const_to_hl(const_index * elem_size);
                    } else {
                        int old_dead;
                        emit("\tpush hl\n");
                        old_dead = expr_result_dead;
                        expr_result_dead = 0;
                        gen_expr();
                        expr_result_dead = old_dead;
                        expect(']');
                        if (addr_is_array)
                            elem_size = field_array_index_stride(type_size(cur_type),
                                                                 fa_dimc, fa_dims,
                                                                 addr_array_index_count);
                        else
                            elem_size = type_index_elem_size(cur_type);
                        scale_hl_by_elem_size(elem_size);
                        emit("\tex de,hl\n");
                        emit("\tpop hl\n");
                        emit("\tadd hl,de\n");
                    }
                }

                if (addr_is_array) {
                    addr_array_index_count++;
                    if (addr_array_index_count >= fa_dimc)
                        addr_is_array = 0;
                } else {
                    cur_type = type_decay_ptr(cur_type);
                    if (ptype) {
                        ptype[0] = cur_type;
                    }
                }
            }
        }

        return;
        } /* end lva_global_ptr_preloaded block */
    }

    if (accept('*')) {
        /* For lvalue dereference, parse only a unary expression for the
         * pointer operand.  The old code used gen_expr(), so in cases like
         *     *(struct S *)p = s;
         * the pointer operand accidentally consumed the following '=' as
         * part of the address expression. */
        gen_unary();
        if (ptype) {
            ptype[0] = type_decay_ptr(g_expr_type);
            if ((ptype[0] & 15) == TYPE_VOID)
                ptype[0] = TYPE_CHAR;
        }
        return;
    }

    if (accept('(')) {
        gen_lvalue_addr(ptype);
        expect(')');
        return;
    }

    error_here("lvalue required");
    emit("\tld hl,0\n");
    if (ptype) ptype[0] = TYPE_INT;
}

void gen_post_update_from_addr(int type, int op)
{
    if (type_is_long(type)) {
        if (expr_result_dead) {
            /* Statement context: just increment in place, no old value needed */
            emit_incdec_addr(type, op);
            return;
        }
        /* Expression context: return old value in DE:HL, store new value.
         * Save address in BC (HL will be destroyed by load), then store
         * the new value byte-by-byte using BC as the address. */
        int no_carry = new_label();
        emit("\tld b,h\n\tld c,l\n");    /* BC = address */
        emit_load_from_hl(type);          /* HL=low16_old, DE=high16_old */
        emit("\tpush de\n");              /* save high16_old for return */
        emit("\tpush hl\n");              /* save low16_old for return */
        if (op == TOK_INC) {
            emit("\tinc hl\n");
            emit("\tld a,h\n\tor l\n");
            emit_jp_label("jp nz,", no_carry);
            emit("\tinc de\n");           /* carry from low to high word */
        } else {
            emit("\tdec hl\n");
            emit("\tld a,h\n\tand l\n\tinc a\n"); /* A=0 only if HL==0xFFFF (borrow) */
            emit_jp_label("jp nz,", no_carry);
            emit("\tdec de\n");           /* borrow from high word */
        }
        emit_label(no_carry);
        /* HL=new_low, DE=new_high; stack: [..., high16_old, low16_old] */
        emit("\tpush hl\n");              /* save new_low */
        emit("\tld h,b\n\tld l,c\n");    /* HL = address */
        emit("\tpop bc\n");               /* BC = new_low */
        emit("\tld (hl),c\n\tinc hl\n"); /* store new_low[0] */
        emit("\tld (hl),b\n\tinc hl\n"); /* store new_low[1] */
        emit("\tld (hl),e\n\tinc hl\n"); /* store new_high[0] */
        emit("\tld (hl),d\n");           /* store new_high[1] */
        emit("\tpop hl\n");              /* HL = low16_old  (return value low) */
        emit("\tpop de\n");              /* DE = high16_old (return value high) */
        g_expr_type = type;
        return;
    }

    emit("\tpush hl\n");             /* address */
    emit_load_from_hl(type);
    emit("\tpush hl\n");             /* old value */

    if (op == TOK_INC) {
        emit("\tinc hl\n");
    } else {
        emit("\tdec hl\n");
    }

    emit("\tex de,hl\n");            /* DE = new */
    emit("\tpop hl\n");              /* HL = old */
    emit("\tex (sp),hl\n");          /* HL = addr, stack = old */
    emit_store_de_to_addr_hl(type);
    emit("\tpop hl\n");              /* expression result = old */
    g_expr_type = type;
}


int paren_starts_indirect_call(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int r;

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    r = 0;
    next_token();
    if (accept('*')) {
        if (tok.kind == TOK_ID) {
            next_token();
            if (accept(')') && tok.kind == '(')
                r = 1;
        }
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return r;
}


int try_inline_strcpy_call(char *name, long *arg_start, long *arg_end, int argc)
{
    char *arg_code;
    int loop;

    if (strcmp(name, "strcpy") != 0 || argc != 2)
        return 0;

    arg_code = copy_range(arg_start[0], arg_end[0]);
    gen_snippet_expr(arg_code);      /* HL = dest */
    free(arg_code);
    emit("\tpush hl\n");

    arg_code = copy_range(arg_start[1], arg_end[1]);
    gen_snippet_expr(arg_code);      /* HL = src */
    free(arg_code);

    emit("\tpop de\n");             /* DE = dest, HL = src */
    emit("\tpush de\n");            /* strcpy return value */
    loop = new_label();
    emit_label(loop);
    emit("\tld a,(hl)\n");
    emit("\tld (de),a\n");
    emit("\tinc hl\n");
    emit("\tinc de\n");
    emit("\tor a\n");
    emit_jp_label("jp nz,", loop);
    emit("\tpop hl\n");
    return 1;
}


void emit_promote_byte_to_int(int actual_type)
{
    if ((actual_type & 15) != TYPE_CHAR || type_ptr_depth(actual_type) != 0)
        return;

    if (actual_type & TYPE_UNSIGNED)
        emit("\tld h,0\n");
    else
        emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
}

void emit_promote_int_to_long(int actual_type, int expected_type)
{
    (void)expected_type;

    /*
     * A byte-typed expression, especially a function call returning
     * unsigned char, is only guaranteed to have its value in L.  Normalize
     * HL before forming DE:HL; otherwise stale/sign bits in H turn uint8_t
     * 255 into 65535 or 0xffffffff when widened.
     */
    emit_promote_byte_to_int(actual_type);

    if ((actual_type & TYPE_UNSIGNED) || type_ptr_depth(actual_type)) {
        emit("\tld de,0\n");
    } else {
        /* Sign-extend signed 16-bit HL into DE. */
        emit("\tld a,h\n");
        emit("\trlca\n");
        emit("\tsbc a,a\n");
        emit("\tld d,a\n");
        emit("\tld e,a\n");
    }
}


void emit_convert_int_to_float(int actual_type)
{
    if (type_is_long(actual_type)) {
        if (actual_type & TYPE_UNSIGNED)
            emit_runtime_call("__fulf");
        else
            emit_runtime_call("__flf");
        g_expr_type = TYPE_FLOAT;
        return;
    }
    if ((actual_type & TYPE_UNSIGNED) || type_ptr_depth(actual_type))
        emit_runtime_call("__fuf");
    else
        emit_runtime_call("__fif");
    g_expr_type = TYPE_FLOAT;
}

void emit_convert_float_to_intlike(int target_type)
{
    if (type_is_long(target_type)) {
        if (target_type & TYPE_UNSIGNED)
            emit_runtime_call("__fful");
        else
            emit_runtime_call("__ffl");
        g_expr_type = target_type;
        return;
    }

    if ((target_type & TYPE_UNSIGNED) || type_ptr_depth(target_type))
        emit_runtime_call("__ffu");
    else
        emit_runtime_call("__ffi");

    if (type_size(target_type) == 1) {
        if (target_type & TYPE_UNSIGNED)
            emit("\tld h,0\n");
        else
            emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
    }

    g_expr_type = target_type;
}

int expected_arg_type(struct Sym *fn, int arg_index, int *ptype)
{
    if (!fn || !fn->has_proto)
        return 0;
    if (arg_index < 0)
        return 0;
    if (arg_index >= fn->proto_nargs)
        return 0;          /* variadic or excess args use normal/default behavior */
    ptype[0] = fn->proto_types[arg_index];
    return 1;
}


void trim_small(char *s)
{
    int i, j, n;
    i = 0;
    while (s[i] == ' ' || s[i] == '\t' || s[i] == '\n' || s[i] == '\r')
        i++;
    if (i) {
        j = 0;
        while (s[i]) s[j++] = s[i++];
        s[j] = 0;
    }
    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ' ' || s[n - 1] == '\t' ||
                     s[n - 1] == '\n' || s[n - 1] == '\r'))
        s[--n] = 0;
}

void strip_line_directives_and_semi(char *s)
{
    char *p;
    int n;

    for (;;) {
        p = s;
        while (*p == ' ' || *p == '\t' || *p == '\r' || *p == '\n')
            p++;
        if (strncmp(p, "#line", 5) != 0)
            break;
        while (*p && *p != '\n')
            p++;
        if (*p == '\n')
            p++;
        memmove(s, p, strlen(p) + 1);
    }

    trim_small(s);
    n = (int)strlen(s);
    while (n > 0 && (s[n - 1] == ';' || s[n - 1] == ' ' ||
                     s[n - 1] == '\t' || s[n - 1] == '\r' || s[n - 1] == '\n')) {
        s[--n] = 0;
    }
    trim_small(s);
}

int parse_simple_ident_text(const char *s, char *out)
{
    int i;
    char tmp[128];
    strncpy(tmp, s, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    strip_line_directives_and_semi(tmp);
    if (!is_ident_start((unsigned char)tmp[0]))
        return 0;
    i = 0;
    while (is_ident_char((unsigned char)tmp[i])) {
        if (i < 63) out[i] = tmp[i];
        i++;
    }
    if (i >= 64) i = 63;
    out[i] = 0;
    while (tmp[i] == ' ' || tmp[i] == '\t') i++;
    return tmp[i] == 0;
}

int va_size_from_text(const char *s)
{
    char tmp[128];
    char *p;
    long v;
    strncpy(tmp, s, sizeof(tmp) - 1);
    tmp[sizeof(tmp) - 1] = 0;
    strip_line_directives_and_semi(tmp);

    v = strtol(tmp, &p, 0);
    if (p != tmp) {
        while (*p == ' ' || *p == '\t') p++;
        if (*p == 0 && v > 0 && v < 256)
            return (int)v;
    }

    if (strstr(tmp, "*") != NULL)
        return 2;
    if (strstr(tmp, "long") != NULL)
        return 4;
    if (strstr(tmp, "char") != NULL)
        return 1;
    return 2;
}

int try_builtin_stdarg_call(const char *name, long *arg_start, long *arg_end, int argc)
{
    char *a0;
    char *a1;
    char id0[64];
    char id1[64];
    struct Sym *ap;
    struct Sym *last;
    int sz;

    if (strcmp(name, "__va_start") == 0) {
        if (argc != 2)
            return 0;
        a0 = copy_range(arg_start[0], arg_end[0]);
        a1 = copy_range(arg_start[1], arg_end[1]);
        if (!parse_simple_ident_text(a0, id0) || !parse_simple_ident_text(a1, id1)) {
            free(a0); free(a1); return 0;
        }
        free(a0); free(a1);
        ap = find_sym(id0);
        last = find_sym(id1);
        if (!ap || !last)
            return 0;
        sz = type_size(last->type);
        if (sz < 2) sz = 2;
        emit_load_sym_addr(last);       /* HL = &last */
        emit_add_const_to_hl(sz);       /* HL = first unnamed arg */
        emit("\tex de,hl\n");
        emit_load_sym_addr(ap);         /* HL = &ap */
        emit_store_de_to_addr_hl(ap->type);
        emit("\tld hl,0\n");
        g_expr_type = TYPE_INT;
        return 1;
    }

    if (strcmp(name, "__va_arg") == 0) {
        if (argc != 2)
            return 0;
        a0 = copy_range(arg_start[0], arg_end[0]);
        a1 = copy_range(arg_start[1], arg_end[1]);
        if (!parse_simple_ident_text(a0, id0)) {
            free(a0); free(a1); return 0;
        }
        sz = va_size_from_text(a1);
        free(a0); free(a1);
        ap = find_sym(id0);
        if (!ap)
            return 0;
        emit_load_sym_addr(ap);         /* HL = &ap */
        emit("\tpush hl\n");
        emit_load_from_hl(ap->type);    /* HL = old ap */
        emit("\tpush hl\n");          /* save old ap as result */
        emit_add_const_to_hl(sz);       /* HL = new ap */
        emit("\tex de,hl\n");
        emit("\tpop bc\n");          /* BC = old ap */
        emit("\tpop hl\n");          /* HL = &ap */
        emit_store_de_to_addr_hl(ap->type);
        emit("\tld h,b\n\tld l,c\n"); /* result = old ap */
        g_expr_type = TYPE_CHAR | TYPE_PTR;
        return 1;
    }

    if (strcmp(name, "__va_end") == 0) {
        if (argc != 1)
            return 0;
        a0 = copy_range(arg_start[0], arg_end[0]);
        if (!parse_simple_ident_text(a0, id0)) {
            free(a0); return 0;
        }
        free(a0);
        ap = find_sym(id0);
        if (!ap)
            return 0;
        emit("\tld de,0\n");
        emit_load_sym_addr(ap);
        emit_store_de_to_addr_hl(ap->type);
        emit("\tld hl,0\n");
        g_expr_type = TYPE_INT;
        return 1;
    }

    return 0;
}

int parse_call_args_after_lparen(long *arg_start, long *arg_end, int *argcp)
{
    int argc;
    int depth;

    if (!accept('('))
        return 0;

    argc = 0;
    if (tok.kind != ')') {
        for (;;) {
            if (argc >= MAX_SNAPSHOT) fatal("too many call args");
            arg_start[argc] = tok_start_pos;
            depth = 0;
            while (tok.kind != TOK_EOF) {
                if (depth == 0 && (tok.kind == ',' || tok.kind == ')'))
                    break;
                if (tok.kind == '(' || tok.kind == '[')
                    depth++;
                else if (tok.kind == ')' || tok.kind == ']') {
                    if (depth == 0) break;
                    depth--;
                }
                next_token();
            }
            arg_end[argc] = tok_start_pos;
            argc++;
            if (!accept(',')) break;
        }
    }
    expect(')');
    argcp[0] = argc;
    return 1;
}


int try_emit_fast_byte_add_const_snippet(const char *snippet)
{
    const char *p;
    char name[64];
    int ni;
    struct Sym *s;
    int op;
    long k;

    /* copy_range() prefixes snippets with #line.  For this tiny fast path,
     * parse the source text directly and skip that logical-line marker. */
    p = snippet;
    while (*p == '#' || *p == '\n' || *p == '\r') {
        if (*p == '#') {
            while (*p && *p != '\n') p++;
        } else {
            p++;
        }
    }
    while (*p == ' ' || *p == '\t') p++;

    if (!is_ident_start((unsigned char)*p))
        return 0;
    ni = 0;
    while (is_ident_char((unsigned char)*p) && ni < 63)
        name[ni++] = *p++;
    name[ni] = 0;

    s = find_sym(name);
    if (!sym_can_ix_direct(s) || type_size(s->type) != 1 || !(s->type & TYPE_UNSIGNED))
        return 0;

    while (*p == ' ' || *p == '\t') p++;
    if (*p != '+' && *p != '-')
        return 0;
    op = *p++;
    while (*p == ' ' || *p == '\t') p++;

    if (*p < '0' || *p > '9')
        return 0;
    k = 0;
    while (*p >= '0' && *p <= '9') {
        k = k * 10 + (*p - '0');
        p++;
    }
    if (k < 0 || k > 255)
        return 0;

    {
        int has_u;
        int has_l;

        has_u = 0;
        has_l = 0;
        while (*p == ' ' || *p == '\t') p++;
        while (*p == 'u' || *p == 'U' || *p == 'l' || *p == 'L') {
            if (*p == 'u' || *p == 'U') has_u = 1;
            if (*p == 'l' || *p == 'L') has_l = 1;
            p++;
        }
        while (*p == ' ' || *p == '\t') p++;

        /* Do not use the byte fast path for long constants.  The result of
         * e.g. unsigned-char + 1UL must be unsigned long, not unsigned int.
         * Let normal expression code do the 32-bit usual-arithmetic conversion.
         */
        if (has_l)
            return 0;

        if (*p != ';')
            return 0;

        /* unsigned char promotes to int on this 16-bit target, so the add/sub
         * must be 16-bit.  The old byte add wrapped 255 + 1 to 0, which broke
         * call arguments such as ck("uc+ul", uc + 1UL, 256UL) before the long
         * case was rejected above, and also made plain uc + 1 wrong.
         */
        fprintf(outf, "\tld l,(ix%+d)\n", s->offset);
        emit("\tld h,0\n");
        if (k != 0) {
            fprintf(outf, "\tld de,%ld\n", k & 0xffffL);
            if (op == '+')
                emit("\tadd hl,de\n");
            else
                emit("\tor a\n\tsbc hl,de\n");
        }
        g_expr_type = has_u ? (TYPE_UNSIGNED | TYPE_INT) : TYPE_INT;
        return 1;
    }
}

int emit_default_call_args(long *arg_start, long *arg_end, int argc)
{
    int i;
    int arg_bytes;
    int old_dead;

    arg_bytes = 0;
    old_dead = expr_result_dead;
    expr_result_dead = 0;
    for (i = argc - 1; i >= 0; --i) {
        char *arg_code;
        arg_code = copy_range(arg_start[i], arg_end[i]);
        if (!try_emit_fast_byte_add_const_snippet(arg_code))
            gen_snippet_expr(arg_code);
        free(arg_code);
        if (type_is_long(g_expr_type) || type_is_float(g_expr_type)) {
            emit("\tpush de\n\tpush hl\n");
            arg_bytes += 4;
        } else {
            emit("\tpush hl\n");
            arg_bytes += 2;
        }
    }
    expr_result_dead = old_dead;
    return arg_bytes;
}

void emit_cleanup_stack_bytes(int bytes)
{
    int k;

    /*
     * POP BC is 1 byte and discards 2 bytes from the stack without touching
     * HL, DE, or flags — safe regardless of whether the callee returned a
     * 16-bit value (in HL) or a 32-bit value (in DE:HL).  Arguments are
     * always pushed in 2- or 4-byte units so bytes is always even; the
     * trailing inc sp is a safety net only.
     */
    for (k = bytes; k >= 2; k -= 2)
        emit("\tpop bc\n");
    if (k > 0)
        emit("\tinc sp\n");
}


int try_emit_push_struct_return_call_arg(const char *snippet, int want_type);

int try_emit_struct_return_call_assignment(int lhs_type)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    char name[64];
    struct Sym *fn;
    long arg_start[MAX_SNAPSHOT];
    long arg_end[MAX_SNAPSHOT];
    int argc;
    int i;
    int arg_bytes;
    int old_dead;

    if (!type_is_struct_object(lhs_type) || tok.kind != TOK_ID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    fn = find_global(name);
    next_token();

    if (!fn || fn->storage != SC_FUNC || !same_struct_type(lhs_type, fn->type) ||
        tok.kind != '(') {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    parse_call_args_after_lparen(arg_start, arg_end, &argc);

    arg_bytes = 0;
    old_dead = expr_result_dead;
    expr_result_dead = 0;
    for (i = argc - 1; i >= 0; --i) {
        char *arg_code;
        int actual_type;
        int want_type;
        int have_want;

        have_want = expected_arg_type(fn, i, &want_type);
        if (have_want && type_is_struct_object(want_type)) {
            int atype;
            arg_code = copy_range(arg_start[i], arg_end[i]);
            if (try_emit_push_struct_return_call_arg(arg_code, want_type)) {
                arg_bytes += type_size(want_type);
                free(arg_code);
                continue;
            }
            gen_snippet_lvalue_addr(arg_code, &atype);
            free(arg_code);
            if (!same_struct_type(want_type, atype))
                error_here("incompatible struct argument");
            emit_push_struct_arg_from_hl(type_size(want_type));
            arg_bytes += type_size(want_type);
        } else {
            arg_code = copy_range(arg_start[i], arg_end[i]);
            gen_snippet_expr(arg_code);
            free(arg_code);
            actual_type = g_expr_type;
            if (have_want && type_is_long(want_type) && !type_is_long(actual_type)) {
                emit_promote_int_to_long(actual_type, want_type);
                emit("\tpush de\n\tpush hl\n");
                arg_bytes += 4;
            } else if (type_is_long(actual_type) || type_is_float(actual_type)) {
                emit("\tpush de\n\tpush hl\n");
                arg_bytes += 4;
            } else {
                emit("\tpush hl\n");
                arg_bytes += 2;
            }
        }
    }
    expr_result_dead = old_dead;

    /* Original destination pointer is below the ordinary arguments.  Duplicate
     * it as the hidden first argument so the callee sees it at ix+4. */
    emit_load_hl_from_sp_offset(arg_bytes);
    emit("\tpush hl\n");
    emit_extrn_if_needed(fn);
    fprintf(outf, "\tcall %s\n", asm_name_for(name));
    emit_cleanup_stack_bytes(arg_bytes + 2);
    emit("\tpop bc\n");          /* discard saved destination pointer */
    g_expr_type = lhs_type;
    g_long_from16 = 0;
    return 1;
}


int try_emit_push_struct_return_call_arg(const char *snippet, int want_type)
{
    char *old_src;
    long old_len;
    long old_pos;
    long old_tok_start;
    int old_line;
    int old_tok_line;
    struct Token old_tok;
    char name[64];
    struct Sym *fn;
    long arg_start[MAX_SNAPSHOT];
    long arg_end[MAX_SNAPSHOT];
    int argc;
    int i;
    int arg_bytes;
    int old_dead;
    int n;
    int ok;

    if (!type_is_struct_object(want_type))
        return 0;

    old_src = src;
    old_len = src_len;
    old_pos = posi;
    old_tok_start = tok_start_pos;
    old_line = line_no;
    old_tok_line = tok_line;
    old_tok = tok;

    src = (char *)snippet;
    src_len = (long)strlen(snippet);
    posi = 0;
    line_no = 1;
    next_token();

    ok = 0;
    if (tok.kind == TOK_ID) {
        strncpy(name, tok.text, sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;
        fn = find_global(name);
        next_token();
        if (fn && fn->storage == SC_FUNC && same_struct_type(want_type, fn->type) &&
            tok.kind == '(') {
            parse_call_args_after_lparen(arg_start, arg_end, &argc);
            if (tok.kind == ';') {
                n = type_size(want_type);

                /* Reserve the outer by-value struct-argument bytes on the
                 * caller's stack, and save that destination pointer while
                 * the inner call's ordinary arguments are pushed above it.
                 * After the inner call is cleaned up, the struct bytes remain
                 * as the already-pushed outer argument. */
                fprintf(outf, "\tld hl,-%d\n", n);
                emit("\tadd hl,sp\n");
                emit("\tld sp,hl\n");
                emit("\tpush hl\n");

                arg_bytes = 0;
                old_dead = expr_result_dead;
                expr_result_dead = 0;
                for (i = argc - 1; i >= 0; --i) {
                    char *arg_code;
                    int actual_type;
                    int inner_want;
                    int have_want;

                    have_want = expected_arg_type(fn, i, &inner_want);
                    arg_code = copy_range(arg_start[i], arg_end[i]);
                    if (have_want && type_is_struct_object(inner_want) &&
                        try_emit_push_struct_return_call_arg(arg_code, inner_want)) {
                        arg_bytes += type_size(inner_want);
                    } else if (have_want && type_is_struct_object(inner_want)) {
                        int atype;
                        gen_snippet_lvalue_addr(arg_code, &atype);
                        if (!same_struct_type(inner_want, atype))
                            error_here("incompatible struct argument");
                        emit_push_struct_arg_from_hl(type_size(inner_want));
                        arg_bytes += type_size(inner_want);
                    } else {
                        gen_snippet_expr(arg_code);
                        actual_type = g_expr_type;
                        if (have_want && type_is_long(inner_want) && !type_is_long(actual_type)) {
                            emit_promote_int_to_long(actual_type, inner_want);
                            emit("\tpush de\n\tpush hl\n");
                            arg_bytes += 4;
                        } else if (type_is_long(actual_type) || type_is_float(actual_type)) {
                            emit("\tpush de\n\tpush hl\n");
                            arg_bytes += 4;
                        } else {
                            emit("\tpush hl\n");
                            arg_bytes += 2;
                        }
                    }
                    free(arg_code);
                }
                expr_result_dead = old_dead;

                emit_load_hl_from_sp_offset(arg_bytes);
                emit("\tpush hl\n");
                emit_extrn_if_needed(fn);
                fprintf(outf, "\tcall %s\n", asm_name_for(name));
                emit_cleanup_stack_bytes(arg_bytes + 2);
                emit("\tpop bc\n");
                g_long_from16 = 0;
                ok = 1;
            }
        }
    }

    src = old_src;
    src_len = old_len;
    posi = old_pos;
    tok_start_pos = old_tok_start;
    line_no = old_line;
    tok_line = old_tok_line;
    tok = old_tok;

    return ok;
}

void emit_call_hl_from_stack_offset(int off)
{
    fprintf(outf, "\tld hl,%d\n", off);
    emit("\tadd hl,sp\n");
    emit("\tld e,(hl)\n");
    emit("\tinc hl\n");
    emit("\tld d,(hl)\n");
    emit("\tex de,hl\n");
    emit_runtime_call("__call_hl");
}


void emit_extract_bitfield(void)
{
    int i;
    unsigned int mask;
    int out_type;

    if (current_field_bit_width <= 0)
        return;

    out_type = (g_expr_type & TYPE_UNSIGNED) ? (TYPE_UNSIGNED | TYPE_INT) : TYPE_INT;

    for (i = 0; i < current_field_bit_shift; ++i)
        emit("\tsrl h\n\trr l\n");

    mask = (unsigned int)((1UL << current_field_bit_width) - 1UL);
    fprintf(outf, "\tld de,%u\n", mask & 0xffffU);
    emit("\tld a,l\n\tand e\n\tld l,a\n");
    emit("\tld a,h\n\tand d\n\tld h,a\n");

    if (!(out_type & TYPE_UNSIGNED) && current_field_bit_width < 16) {
        int lab;
        unsigned int signbit;
        unsigned int extend_mask;

        lab = new_label();
        signbit = (unsigned int)(1UL << (current_field_bit_width - 1));
        extend_mask = (~mask) & 0xffffU;

        fprintf(outf, "\tld de,%u\n", signbit & 0xffffU);
        emit("\tld a,l\n\tand e\n\tld e,a\n");
        emit("\tld a,h\n\tand d\n\tor e\n");
        fprintf(outf, "\tjp z,L%d\n", lab);
        fprintf(outf, "\tld de,%u\n", extend_mask);
        emit("\tld a,l\n\tor e\n\tld l,a\n");
        emit("\tld a,h\n\tor d\n\tld h,a\n");
        emit_label(lab);
    }

    g_expr_type = out_type;
}

void emit_store_bitfield_from_hl(void)
{
    int i;
    unsigned int clear_mask;
    unsigned int mask;

    mask = current_field_bit_mask & 0xffffU;
    clear_mask = (~mask) & 0xffffU;

    /* Stack top is the field storage-unit address, value is in HL. */
    emit("\tex de,hl\n");       /* DE = new field value */
    emit("\tpop hl\n");        /* HL = storage-unit address */
    emit("\tpush hl\n");       /* keep address for final store */
    emit("\tpush de\n");       /* keep raw field value */
    emit_load_from_hl(TYPE_INT); /* HL = old storage-unit word */

    fprintf(outf, "\tld de,%u\n", clear_mask);
    emit("\tld a,l\n\tand e\n\tld l,a\n");
    emit("\tld a,h\n\tand d\n\tld h,a\n");

    emit("\tpop de\n");        /* DE = raw field value */
    for (i = 0; i < current_field_bit_shift; ++i)
        emit("\tsla e\n\trl d\n");

    fprintf(outf, "\tld bc,%u\n", mask);
    emit("\tld a,e\n\tand c\n\tld e,a\n");
    emit("\tld a,d\n\tand b\n\tld d,a\n");
    emit("\tld a,l\n\tor e\n\tld l,a\n");
    emit("\tld a,h\n\tor d\n\tld h,a\n");

    emit("\tex de,hl\n");       /* DE = merged storage-unit word */
    emit("\tpop hl\n");        /* HL = address */
    emit_store_de_to_addr_hl(TYPE_INT);
}

void emit_load_float_bits(unsigned long bits);
void emit_load_const_sym_value(struct Sym *s);
void emit_float_compare_call(int op);



int try_emit_string_fastcall(const char *name,
                                    long arg_start[MAX_SNAPSHOT],
                                    long arg_end[MAX_SNAPSHOT],
                                    int argc)
{
    char *arg_code;
    int old_dead;

    if (!strcmp(name, "strlen") && argc == 1) {
        old_dead = expr_result_dead;
        expr_result_dead = 0;
        arg_code = copy_range(arg_start[0], arg_end[0]);
        gen_snippet_expr(arg_code);
        free(arg_code);
        expr_result_dead = old_dead;

        emit_runtime_call("__slf");
        g_expr_type = TYPE_UNSIGNED | TYPE_INT;
        return 1;
    }

    if (!strcmp(name, "strchr") && argc == 2) {
        old_dead = expr_result_dead;
        expr_result_dead = 0;

        /* HL = string argument.  Save it across evaluation of the character
         * expression, then pass the low byte of that expression in A. */
        arg_code = copy_range(arg_start[0], arg_end[0]);
        gen_snippet_expr(arg_code);
        free(arg_code);
        emit("\tpush hl\n");

        arg_code = copy_range(arg_start[1], arg_end[1]);
        gen_snippet_expr(arg_code);
        free(arg_code);
        emit("\tld a,l\n");
        emit("\tpop hl\n");

        expr_result_dead = old_dead;
        emit_runtime_call("__chf");
        g_expr_type = TYPE_PTR | TYPE_CHAR;
        return 1;
    }

    return 0;
}

int try_inline_cb_is_zero_call(const char *name,
                                      long arg_start[MAX_SNAPSHOT],
                                      long arg_end[MAX_SNAPSHOT],
                                      int argc)
{
    char *arg_code;
    int ptr_type;
    int obj_type;
    int nbytes;
    int lloop;
    int lfalse;
    int ldone;

    if (strcmp(name, "cb_is_zero") != 0 || argc != 1)
        return 0;

    /*
     * crc.c calls cb_is_zero() in the packed-input hot path.  The function
     * simply tests all bytes of a counter_t.  Inline only when the argument
     * is a pointer to a sized object, so this remains a semantic inline of
     * the actual object size rather than a hard-coded CRC-only constant.
     */
    arg_code = copy_range(arg_start[0], arg_end[0]);
    gen_snippet_expr(arg_code);
    free(arg_code);

    ptr_type = g_expr_type;
    if (type_ptr_depth(ptr_type) <= 0)
        return 0;

    obj_type = type_decay_ptr(ptr_type);
    nbytes = type_size(obj_type);
    if (nbytes <= 0 || nbytes > 255)
        return 0;

    lloop = new_label();
    lfalse = new_label();
    ldone = new_label();

    if (nbytes == 1) {
        emit("\tld a,(hl)\n\tor a\n");
        emit_jp_label("jp nz,", lfalse);
    } else {
        fprintf(outf, "\tld b,%d\n", nbytes);
        emit_label(lloop);
        emit("\tld a,(hl)\n\tor a\n");
        emit_jp_label("jp nz,", lfalse);
        emit("\tinc hl\n\tdjnz L");
        if (!scan_mode)
            fprintf(outf, "%d\n", lloop);
    }

    emit("\tld hl,1\n");
    emit_jp_label("jp", ldone);
    emit_label(lfalse);
    emit("\tld hl,0\n");
    emit_label(ldone);
    g_expr_type = TYPE_INT;
    return 1;
}


int try_gen_parenthesized_const_size_expr(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    int save_errors;
    int save_long_suffix;
    int save_unsigned_suffix;
    struct Token save_tok;
    long scan;
    int depth;
    int saw_sizeof;
    long v;

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_errors = errors;
    save_long_suffix = g_tok_long_suffix;
    save_unsigned_suffix = g_tok_unsigned_suffix;
    save_tok = tok;

    /* This optimization is intentionally limited to parenthesized expressions
     * containing sizeof.  It folds macro forms such as:
     *     ( sizeof(x) / sizeof(x[0]) )
     * but avoids touching ordinary parentheses/casts like:
     *     ((unsigned int)rand() % 300)
     */
    scan = tok_start_pos + 1;
    depth = 1;
    saw_sizeof = 0;
    while (scan < src_len && depth > 0) {
        if (src[scan] == '(') {
            depth++;
            scan++;
            continue;
        }
        if (src[scan] == ')') {
            depth--;
            scan++;
            continue;
        }

        /*
         * Keep this speculative fold extremely conservative.  It is meant for
         * pure constant macros such as:
         *     ( sizeof(a) / sizeof(a[0]) )
         *
         * Do not try to fold mixed runtime/sizeof expressions such as:
         *     (buf + sizeof(struct Pair))
         * or stdarg.h va_arg macro internals.  The real parser can handle
         * those; the speculative constant parser would emit diagnostics before
         * it discovers the expression is not constant.
         */
        if (src[scan] == '*')
            return 0;

        if (is_ident_start((unsigned char)src[scan])) {
            char word[64];
            int wi;
            long p;

            wi = 0;
            p = scan;
            while (p < src_len && is_ident_char((unsigned char)src[p])) {
                if (wi < (int)sizeof(word) - 1)
                    word[wi++] = src[p];
                p++;
            }
            word[wi] = 0;

            if (!strcmp(word, "sizeof")) {
                int sd;

                saw_sizeof = 1;
                scan = p;
                while (scan < src_len && isspace((unsigned char)src[scan]))
                    scan++;

                /* Skip sizeof(type-or-expression) as one opaque constant
                 * operand for this pre-scan.  parse_const_long_expr() will
                 * validate it for real after the scan succeeds. */
                if (scan < src_len && src[scan] == '(') {
                    sd = 1;
                    scan++;
                    while (scan < src_len && sd > 0) {
                        if (src[scan] == '(')
                            sd++;
                        else if (src[scan] == ')')
                            sd--;
                        scan++;
                    }
                    continue;
                }

                /* sizeof identifier */
                if (scan < src_len && is_ident_start((unsigned char)src[scan])) {
                    scan++;
                    while (scan < src_len && is_ident_char((unsigned char)src[scan]))
                        scan++;
                    continue;
                }

                continue;
            }

            /* Any other identifier means this is not a pure constant sizeof
             * expression.  Leave it for normal expression codegen. */
            return 0;
        }

        scan++;
    }

    if (!saw_sizeof || depth != 0)
        return 0;

    next_token();
    v = parse_const_long_expr();
    if (tok.kind != ')')
        goto fail;

    next_token();
    fprintf(outf, "\tld hl,%ld\n", v & 0xffffL);
    g_expr_type = TYPE_INT;
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    errors = save_errors;
    g_tok_long_suffix = save_long_suffix;
    g_tok_unsigned_suffix = save_unsigned_suffix;
    tok = save_tok;
    return 0;
}


int try_gen_parenthesized_deref_array_value(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    char name[64];
    struct Sym *s;
    int index_count;
    int elem_bytes;
    int elem_size;
    int cur_type;

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (!accept('*') || tok.kind != TOK_ID)
        goto fail;

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;
    s = find_sym(name);
    if (!s || type_ptr_depth(s->type) <= 0 || s->dim_count <= 0)
        goto fail;

    next_token();
    if (!accept(')') || tok.kind != '[')
        goto fail;

    if (is_global_word_sym(s)) {
        emit_load_global_word_direct(s);
    } else {
        emit_load_sym_addr(s);
        emit_load_from_hl(s->type);
    }

    elem_bytes = type_size(type_decay_ptr(s->type));
    if (elem_bytes <= 0)
        elem_bytes = 2;

    index_count = 0;
    cur_type = type_decay_ptr(s->type);

    while (accept('[')) {
        long const_index;

        elem_size = elem_bytes;
        if (index_count < s->dim_count)
            elem_size = sym_array_inner_count_from(s, index_count + 1) * elem_bytes;
        if (elem_size <= 0)
            elem_size = elem_bytes;

        if (try_parse_const_subscript(&const_index)) {
            emit_add_const_to_hl(const_index * elem_size);
        } else {
            emit("\tpush hl\n");
            gen_expr();
            expect(']');
            scale_hl_by_elem_size(elem_size);
            emit("\tex de,hl\n");
            emit("\tpop hl\n");
            emit("\tadd hl,de\n");
        }

        index_count++;
    }

    if (index_count < s->dim_count) {
        g_expr_type = type_add_ptr(cur_type);
    } else {
        g_expr_type = cur_type;
        emit_load_from_hl(cur_type);
    }
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

void gen_primary(void)
{
    current_field_bit_width = 0;
    current_field_bit_shift = 0;
    current_field_bit_mask = 0;
    char name[64];
    struct Sym *s;
    int sid;
    int argc;
    int indexed;
    int elem_size;
    int val_type;
    int val_is_array;
    int val_array_index_count;
    long arg_start[MAX_SNAPSHOT];
    long arg_end[MAX_SNAPSHOT];
    int depth;
    int i;
    int cur_type;
    int arrow;
    int field_is_array;
    int fa_dimc;
    int fa_dims[4];

    if (tok.kind == TOK_ID && strcmp(tok.text, "__offsetof") == 0) {
        int ov;
        ov = parse_offsetof_value();
        if (!scan_mode)
            fprintf(outf, "\tld hl,%d\n", ov & 0xffff);
        g_expr_type = TYPE_INT | TYPE_UNSIGNED;
        return;
    }

    if (tok.kind == TOK_FLOATLIT) {
        unsigned long bits;
        bits = parse_float_literal_bits(tok.text);
        emit_load_float_bits(bits);
        g_expr_type = TYPE_FLOAT;
        next_token();
        return;
    }

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        if (tok.val > 0xffffL || tok.val < -32768L ||
                (tok.kind == TOK_NUM && g_tok_long_suffix)) {
            /* 32-bit literal: emit DE:HL */
            fprintf(outf, "\tld hl,%ld\n", tok.val & 0xffffL);
            fprintf(outf, "\tld de,%ld\n", (tok.val >> 16) & 0xffffL);
            g_expr_type = TYPE_LONG;
            if (tok.kind == TOK_NUM && g_tok_unsigned_suffix)
                g_expr_type |= TYPE_UNSIGNED;
        } else {
            fprintf(outf, "\tld hl,%ld\n", tok.val & 0xffffL);
            g_expr_type = TYPE_INT;
            if (tok.kind == TOK_NUM && g_tok_unsigned_suffix)
                g_expr_type |= TYPE_UNSIGNED;
        }
        next_token();
        return;
    }

    if (tok.kind == TOK_STR || tok.kind == TOK_WSTR) {
        char *lit;
        int is_wide;
        lit = read_adjacent_string_literals_ex(&is_wide);
        sid = add_string_ex(lit, is_wide);
        free(lit);
        fprintf(outf, "\tld hl,S%d\n", sid);
        g_expr_type = TYPE_INT;
        return;
    }

    if (try_gen_parenthesized_const_size_expr())
        return;

    if (try_gen_parenthesized_deref_array_value())
        return;

    if (paren_starts_indirect_call()) {
        long fp_arg_start[MAX_SNAPSHOT];
        long fp_arg_end[MAX_SNAPSHOT];
        int fp_argc;
        int fp_arg_bytes;

        expect('(');
        expect('*');
        gen_expr();
        expect(')');
        parse_call_args_after_lparen(fp_arg_start, fp_arg_end, &fp_argc);
        emit("\tpush hl\n");
        fp_arg_bytes = emit_default_call_args(fp_arg_start, fp_arg_end, fp_argc);
        emit_call_hl_from_stack_offset(fp_arg_bytes);
        emit_cleanup_stack_bytes(fp_arg_bytes + 2);
        g_expr_type = TYPE_INT;
        g_long_from16 = 0;
        return;
    }

    if (accept('(')) {
        gen_expr();
        expect(')');

        while (tok.kind == TOK_ARROW) {
            struct FieldDef *fd;
            int sid2;
            int base_type;

            next_token();
            if (tok.kind != TOK_ID) {
                error_here("field name expected");
                return;
            }

            base_type = type_decay_ptr(g_expr_type);
            sid2 = base_struct_id_from_type(base_type);
            fd = find_field_def(sid2, tok.text);
            if (!fd) {
                error_here("unknown struct field");
                next_token();
                return;
            }
            next_token();
            emit_add_field_offset(fd);
            g_expr_type = fd->is_array ? type_add_ptr(fd->elem_type) : fd->type;
            if (!fd->is_array) {
                emit_load_from_hl(fd->type);
                if (fd->bit_width > 0) {
                    current_field_bit_width = fd->bit_width;
                    current_field_bit_shift = fd->bit_shift;
                    current_field_bit_mask = fd->bit_mask;
                    emit_extract_bitfield();
                }
            }
        }

        if (tok.kind == '(' && type_ptr_depth(g_expr_type) > 0) {
            long fp_arg_start[MAX_SNAPSHOT];
            long fp_arg_end[MAX_SNAPSHOT];
            int fp_argc;
            int fp_arg_bytes;

            parse_call_args_after_lparen(fp_arg_start, fp_arg_end, &fp_argc);
            emit("\tpush hl\n");
            fp_arg_bytes = emit_default_call_args(fp_arg_start, fp_arg_end, fp_argc);
            emit_call_hl_from_stack_offset(fp_arg_bytes);
            emit_cleanup_stack_bytes(fp_arg_bytes + 2);
            g_expr_type = type_decay_ptr(g_expr_type);
            g_long_from16 = 0;
        }
        return;
    }

    if (tok.kind == TOK_ID) {
        strncpy(name, tok.text, sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;
        next_token();

        if (accept('(')) {
            argc = 0;

            if (tok.kind != ')') {
                for (;;) {
                    if (argc >= MAX_SNAPSHOT) fatal("too many call args");

                    arg_start[argc] = tok_start_pos;
                    depth = 0;

                    while (tok.kind != TOK_EOF) {
                        if (depth == 0 && (tok.kind == ',' || tok.kind == ')'))
                            break;

                        if (tok.kind == '(' || tok.kind == '[')
                            depth++;
                        else if (tok.kind == ')' || tok.kind == ']') {
                            if (depth == 0) break;
                            depth--;
                        }

                        next_token();
                    }

                    arg_end[argc] = tok_start_pos;
                    argc++;

                    if (!accept(',')) break;
                }
            }

            expect(')');

            if (try_builtin_stdarg_call(name, arg_start, arg_end, argc))
                return;

            if (try_inline_cb_is_zero_call(name, arg_start, arg_end, argc))
                return;

            if (try_emit_string_fastcall(name, arg_start, arg_end, argc))
                return;

            /*
             * Do not inline user functions by name.  A previous nmfpart()
             * special-case was unsafe because it bypassed the actual function
             * body, including assertions and debug checks added by the user.
             * Keep builtins/intrinsics explicit; ordinary same-file functions
             * must remain callable unless a real semantic inliner is added.
             */
            if (try_inline_strcpy_call(name, arg_start, arg_end, argc))
                return;

            {
                int arg_bytes = 0;
                int old_dead = expr_result_dead;
                struct Sym *fn_sym = find_global(name);
                struct Sym *call_sym = find_sym(name);
                int indirect_call = 0;

                /*
                 * C89 implicit function declaration: a call to an undeclared
                 * identifier declares an external function returning int.
                 * Record it so deferred EXTRN emission can happen if no local
                 * definition appears later.
                 */
                if (!fn_sym) {
                    fn_sym = add_global(name, TYPE_INT, SC_FUNC);
                    fn_sym->needs_extrn = 1;
                }

                if (call_sym && call_sym->storage != SC_FUNC && type_ptr_depth(call_sym->type) > 0)
                    indirect_call = 1;
                expr_result_dead = 0; /* call args always need their value */
                for (i = argc - 1; i >= 0; --i) {
                    char *arg_code;
                    int actual_type;
                    int want_type;
                    int have_want;

                    have_want = expected_arg_type(fn_sym, i, &want_type);

                    if (have_want && type_is_struct_object(want_type)) {
                        int atype;
                        arg_code = copy_range(arg_start[i], arg_end[i]);
                        if (try_emit_push_struct_return_call_arg(arg_code, want_type)) {
                            arg_bytes += type_size(want_type);
                            free(arg_code);
                            continue;
                        }
                        gen_snippet_lvalue_addr(arg_code, &atype);
                        free(arg_code);
                        if (!same_struct_type(want_type, atype))
                            error_here("incompatible struct argument");
                        emit_push_struct_arg_from_hl(type_size(want_type));
                        arg_bytes += type_size(want_type);
                    } else {
                        arg_code = copy_range(arg_start[i], arg_end[i]);
                        if (!try_emit_fast_byte_add_const_snippet(arg_code))
                            gen_snippet_expr(arg_code);
                        free(arg_code);

                        actual_type = g_expr_type;

                        if (have_want && type_is_float(want_type)) {
                            if (!type_is_float(actual_type))
                                emit_convert_int_to_float(actual_type);
                            emit("\tpush de\n\tpush hl\n");
                            arg_bytes += 4;
                        } else if (have_want && type_is_long(want_type)) {
                            if (!type_is_long(actual_type))
                                emit_promote_int_to_long(actual_type, want_type);
                            emit("\tpush de\n\tpush hl\n");
                            arg_bytes += 4;
                        } else if (have_want && !type_is_long(want_type) && !type_is_float(want_type)) {
                            /* Prototype says this is a 16-bit argument.  If the
                             * expression produced a long/float, pass its low word. */
                            emit("\tpush hl\n");
                            arg_bytes += 2;
                        } else if (type_is_long(actual_type) || type_is_float(actual_type)) {
                            emit("\tpush de\n\tpush hl\n");
                            arg_bytes += 4;
                        } else {
                            emit("\tpush hl\n");
                            arg_bytes += 2;
                        }
                    }
                }
                expr_result_dead = old_dead;

                if (indirect_call) {
                    if (sym_can_ix_direct(call_sym))
                        emit_load_sym_value_direct(call_sym);
                    else if (is_global_word_sym(call_sym))
                        emit_load_global_word_direct(call_sym);
                    else {
                        emit_load_sym_addr(call_sym);
                        emit_load_from_hl(call_sym->type);
                    }
                    emit_runtime_call("__call_hl");
                    if (call_sym)
                        g_expr_type = type_decay_ptr(call_sym->type);
                    else
                        g_expr_type = TYPE_INT;
                } else {
                    emit_extrn_if_needed(fn_sym);
                    fprintf(outf, "\tcall %s\n", asm_name_for(name));
                    g_expr_type = fn_sym ? fn_sym->type : TYPE_INT;
                }
                g_long_from16 = 0;

                emit_cleanup_stack_bytes(arg_bytes);
            }
            return;
        }

        /*
         * The CP/M RTL represents stdin/stdout/stderr as FILE values 0/1/2.
         * It also exports _stdin/_stdout/_stderr data objects, but using those
         * external variables makes M80 print a noisy 'U' for every reference.
         * In ordinary rvalue expression context, emit the immediate FILE value.
         */
        if (tok.kind != '[' && tok.kind != '.' && tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC && tok.kind != TOK_DEC) {
            if (!strcmp(name, "stdin")) {
                emit("\tld hl,0\n");
                g_expr_type = TYPE_INT;
                return;
            } else if (!strcmp(name, "stdout")) {
                emit("\tld hl,1\n");
                g_expr_type = TYPE_INT;
                return;
            } else if (!strcmp(name, "stderr")) {
                emit("\tld hl,2\n");
                g_expr_type = TYPE_INT;
                return;
            }
        }

        s = find_sym(name);
        if (!s) {
            /* Check for an enum constant before reporting an error */
            int ei;
            for (ei = 0; ei < nenum_consts; ++ei) {
                if (!strcmp(enum_const_names[ei], name)) {
                    long ev = (long)(int)enum_const_values[ei];
                    if (!scan_mode)
                        fprintf(outf, "\tld hl,%ld\n", ev & 0xffffL);
                    g_expr_type = TYPE_INT;
                    return;
                }
            }
            error_here("undefined symbol");
            s = add_global(name, TYPE_INT, SC_GLOBAL);
        }

        if (s->is_const_value &&
            tok.kind != '[' && tok.kind != '.' && tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC && tok.kind != TOK_DEC && tok.kind != '(') {
            emit_load_const_sym_value(s);
            return;
        }

        if (s->storage == SC_FUNC &&
            tok.kind != '[' && tok.kind != '.' && tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC && tok.kind != TOK_DEC) {
            fprintf(outf, "\tld hl,%s\n", asm_name_for(name));
            g_expr_type = type_add_ptr(s->type);
            return;
        }

        if ((tok.kind == TOK_INC || tok.kind == TOK_DEC) &&
            try_emit_post_update_sym_direct(s, tok.kind)) {
            next_token();
            return;
        }

        if (sym_can_ix_direct(s) &&
            tok.kind != '[' &&
            tok.kind != '.' &&
            tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC &&
            tok.kind != TOK_DEC) {
            emit_load_sym_value_direct(s);
            g_expr_type = s->type;
            return;
        }

        /* Z80: ld hl,(nn) — direct 16-bit load from global/extern without
         * subscript/field/update following.  Replaces the 5-instruction
         * ld hl,_name / ld e,(hl) / inc hl / ld d,(hl) / ex de,hl sequence. */
        if (is_global_word_sym(s) &&
            tok.kind != '[' && tok.kind != '.' && tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC && tok.kind != TOK_DEC && tok.kind != '(') {
            emit_load_global_word_direct(s);
            g_expr_type = s->type;
            return;
        }

        {
        int global_ptr_preloaded = 0;
        /* For a global pointer variable immediately subscripted, load the
         * pointer value directly with ld hl,(nn) and skip the later deref. */
        if (is_global_word_sym(s) && !s->is_array &&
            type_ptr_depth(s->type) > 0 && tok.kind == '[') {
            emit_load_global_word_direct(s);
            global_ptr_preloaded = 1;
        } else {
            emit_load_sym_addr(s);
        }
        indexed = 0;
        val_type = s->type;
        val_is_array = s->is_array;
        val_array_index_count = 0;

        while (tok.kind == '[' && (!s->is_array || val_is_array)) {
            accept('[');
            indexed = 1;
            cur_type = val_type;

            /*
             * Array symbols decay to their address; pointer variables must be
             * loaded first so argv[1] indexes the pointed-to argv vector, not
             * the stack slot holding argv.
             */
            if (!s->is_array && type_ptr_depth(cur_type) > 0) {
                if (!global_ptr_preloaded)
                    emit_load_from_hl(cur_type);
                global_ptr_preloaded = 0;
            }

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (val_is_array)
                    elem_size = sym_array_index_elem_size(s, val_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, val_array_index_count);

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (val_is_array)
                    elem_size = sym_array_index_elem_size(s, val_array_index_count);
                else
                    elem_size = sym_pointer_array_index_elem_size(s, cur_type, val_array_index_count);

                scale_hl_by_elem_size(elem_size);
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (s->is_array) {
                val_type = cur_type;
                val_array_index_count++;
                if (s->dim_count <= 0 || val_array_index_count >= s->dim_count)
                    val_is_array = 0;
            } else {
                val_array_index_count++;
                val_type = type_decay_ptr(cur_type);
            }
        }

        while (tok.kind == '.' || tok.kind == TOK_ARROW) {
            indexed = 1;
            arrow = tok.kind == TOK_ARROW;
            next_token();
            val_type = apply_field_access_from_addr(val_type, arrow,
                                                    &field_is_array);
            val_is_array = field_is_array;
            val_array_index_count = 0;
        }

        snapshot_field_array_dims(&fa_dimc, fa_dims);
        while (accept('[')) {
            indexed = 1;
            cur_type = val_type;

            if (!val_is_array && type_ptr_depth(cur_type) > 0)
                emit_load_from_hl(cur_type);

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (val_is_array) {
                    elem_size = field_array_index_stride(type_size(cur_type),
                                                         fa_dimc, fa_dims,
                                                         val_array_index_count);
                } else {
                    elem_size = type_index_elem_size(cur_type);
                }

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (val_is_array) {
                    elem_size = field_array_index_stride(type_size(cur_type),
                                                         fa_dimc, fa_dims,
                                                         val_array_index_count);
                } else {
                    elem_size = type_index_elem_size(cur_type);
                }

                scale_hl_by_elem_size(elem_size);
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (val_is_array) {
                val_array_index_count++;
                if (val_array_index_count >= fa_dimc)
                    val_is_array = 0;
            } else {
                val_type = type_decay_ptr(cur_type);
            }
        }

        /*
         * Handle postfix field access after a subscripted pointer-to-struct,
         * e.g. rs[rp].kind when rs is a macro for G->s_rs.  The earlier
         * field loop handles G->s_rs before the bracket; this one handles
         * the .kind after the bracket.
         */
        while (tok.kind == '.' || tok.kind == TOK_ARROW) {
            indexed = 1;
            arrow = tok.kind == TOK_ARROW;
            next_token();
            val_type = apply_field_access_from_addr(val_type, arrow,
                                                    &field_is_array);
            val_is_array = field_is_array;
            val_array_index_count = 0;

            snapshot_field_array_dims(&fa_dimc, fa_dims);
            while (accept('[')) {
                indexed = 1;
                cur_type = val_type;

                if (!val_is_array && type_ptr_depth(cur_type) > 0)
                    emit_load_from_hl(cur_type);

                {
                    long const_index;

                    if (try_parse_const_subscript(&const_index)) {
                        if (val_is_array) {
                            elem_size = field_array_index_stride(type_size(cur_type),
                                                                 fa_dimc, fa_dims,
                                                                 val_array_index_count);
                        } else {
                            elem_size = type_index_elem_size(cur_type);
                        }
                        emit_add_const_to_hl(const_index * elem_size);
                    } else {
                        emit("\tpush hl\n");
                        gen_expr();
                        expect(']');

                        if (val_is_array) {
                            elem_size = field_array_index_stride(type_size(cur_type),
                                                                 fa_dimc, fa_dims,
                                                                 val_array_index_count);
                        } else {
                            elem_size = type_index_elem_size(cur_type);
                        }

                        scale_hl_by_elem_size(elem_size);
                        emit("\tex de,hl\n");
                        emit("\tpop hl\n");
                        emit("\tadd hl,de\n");
                    }
                }

                if (val_is_array) {
                    val_array_index_count++;
                    if (val_array_index_count >= fa_dimc)
                        val_is_array = 0;
                } else {
                    val_type = type_decay_ptr(cur_type);
                }
            }
        }

        if (tok.kind == '(' && indexed && type_ptr_depth(val_type) > 0) {
            long fp_arg_start[MAX_SNAPSHOT];
            long fp_arg_end[MAX_SNAPSHOT];
            int fp_argc;
            int fp_arg_bytes;

            emit_load_from_hl(val_type);
            parse_call_args_after_lparen(fp_arg_start, fp_arg_end, &fp_argc);
            emit("\tpush hl\n");
            fp_arg_bytes = emit_default_call_args(fp_arg_start, fp_arg_end, fp_argc);
            emit_call_hl_from_stack_offset(fp_arg_bytes);
            emit_cleanup_stack_bytes(fp_arg_bytes + 2);
            g_expr_type = type_decay_ptr(val_type);
            return;
        }

        if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
            int op;
            op = tok.kind;
            next_token();
            gen_post_update_from_addr(val_type, op);
            return;
        }

        if ((s->is_array && !indexed) || val_is_array) {
            /* array expression decays to pointer-to-element */
            g_expr_type = type_add_ptr(val_type);
        } else {
            g_expr_type = val_type;
            emit_load_from_hl(val_type);
            if (current_field_bit_width > 0)
                emit_extract_bitfield();
        }
        return;
        } /* end global_ptr_preloaded block */
    }

    error_here("primary expression expected");
    emit("\tld hl,0\n");
    next_token();
}

int paren_starts_cast(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int r;

    if (tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    r = starts_type();

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;

    return r;
}


int try_gen_simple_deref_value(void)
{
    struct Sym *s;
    char name[64];
    int op;
    int base;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    if (tok.kind != '*')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();

    if (tok.kind != TOK_ID)
        goto fail;

    strncpy(name, tok.text, sizeof(name) - 1);
    name[sizeof(name) - 1] = 0;

    s = find_sym(name);
    if (!s) {
        error_here("undefined symbol");
        s = add_global(name, TYPE_INT | TYPE_PTR, SC_GLOBAL);
    }

    next_token();

    if (tok.kind == '[' || tok.kind == '.' || tok.kind == TOK_ARROW ||
        tok.kind == '(') {
        goto fail;
    }

    if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
        op = tok.kind;
        next_token();
        gen_post_update_symbol_addr_value(s, op);
    } else {
        if (is_global_word_sym(s)) {
            emit_load_global_word_direct(s);
        } else {
            emit_load_sym_addr(s);
            emit_load_from_hl(s->type);
        }
    }

    base = type_decay_ptr(s->type);
    if ((base & 15) == TYPE_VOID)
        base = TYPE_CHAR;

    emit_load_from_hl(base);
    g_expr_type = base;
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}



void emit_incdec_value_in_dehl(int type, int op)
{
    int no_carry;

    if (type_ptr_depth(type) > 0) {
        int elem;
        elem = type_index_elem_size(type);
        if (op == TOK_INC) {
            emit_add_const_to_hl(elem);
        } else {
            emit_ld_de_const(elem);
            emit("\tor a\n\tsbc hl,de\n");
        }
        return;
    }

    if (type_is_long(type)) {
        no_carry = new_label();
        if (op == TOK_INC) {
            emit("\tinc hl\n");
            emit("\tld a,h\n\tor l\n");
            emit_jp_label("jp nz,", no_carry);
            emit("\tinc de\n");
        } else {
            emit("\tld a,h\n\tor l\n");
            emit_jp_label("jp nz,", no_carry);
            emit("\tdec de\n");
            emit_label(no_carry);
            emit("\tdec hl\n");
            return;
        }
        emit_label(no_carry);
    } else {
        if (op == TOK_INC)
            emit("\tinc hl\n");
        else
            emit("\tdec hl\n");
    }
}

void emit_pre_incdec_lvalue(int type, int op)
{
    if (type_is_long(type)) {
        /* Address is in HL.  Save it, load DE:HL, update full 32-bit value,
         * then store DE:HL back through saved address. */
        emit("\tpush hl\n");
        emit_load_from_hl(type);
        emit_incdec_value_in_dehl(type, op);
        emit_store_de_to_addr_hl(type);
    } else {
        emit("\tpush hl\n");
        emit_load_from_hl(type);
        emit_incdec_value_in_dehl(type, op);
        emit("\tex de,hl\n\tpop hl\n");
        emit_store_de_to_addr_hl(type);
        emit("\tex de,hl\n");
    }
    g_expr_type = type;
}

void gen_unary(void)
{
    int t;
    int sz;

    /* Each gen_unary call produces exactly one operand value.  Clear the
     * "freshly widened from 16-bit" marker on entry; only an explicit
     * widening (emit_extend_to_long_typed) below sets it again.  This lets
     * gen_mul recognize (long)int * int and use the cheap 16x16->32 helper. */
    g_long_from16 = 0;

    if (paren_starts_cast()) {
        expect('(');
        parse_type_name_decl(&t, &sz);
        expect(')');

        gen_unary();
        if (type_is_float(t)) {
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            g_expr_type = t;
            return;
        }
        if (type_is_float(g_expr_type)) {
            emit_convert_float_to_intlike(t);
            return;
        }
        if (type_is_long(t)) {
            /* Widen only when the source was not already long.  Re-extending
             * an existing DE:HL long from just HL corrupts values whose low
             * word has bit 15 set, e.g. (int32_t)37000L. */
            if (!type_is_long(g_expr_type))
                emit_extend_to_long_typed(g_expr_type);
        } else if (type_size(t) == 1) {
            if (t & TYPE_UNSIGNED)
                emit("\tld h,0\n");
            else
                emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
        } else if (!type_is_long(g_expr_type)) {
            /* nothing needed for 16-bit to 16-bit */
        }
        g_expr_type = t;
        return;
    }

    if (accept(TOK_SIZEOF)) {
        sz = 0;

        if (accept('(')) {
            if (starts_type()) {
                parse_type_name_decl(&t, &sz);
                expect(')');
            } else {
                sz = parse_sizeof_expr_operand();
                expect(')');
            }
        } else {
            sz = parse_sizeof_expr_operand();
        }

        fprintf(outf, "\tld hl,%d\n", sz);
        g_expr_type = TYPE_INT;
        return;
    }

    if (accept('+')) {
        gen_unary();
        /* C89 integer promotions apply to unary +.  This matters for casts
         * of the result: +(unsigned char) must have type int, not unsigned
         * char, because later widening to long depends on g_expr_type. */
        if (!type_is_float(g_expr_type) && !type_is_long(g_expr_type))
            g_expr_type = promote_int_type(g_expr_type);
        return;
    }

    if (accept('-')) {
        gen_unary();
        if (type_is_float(g_expr_type)) {
            /* unary minus for opaque IEEE single: toggle the sign bit in D.
             * The old integer-negate path only changed HL, so -2.0f stayed
             * +2.0f because the float sign/exponent live in DE. */
            emit("\tld a,d\n\txor 80h\n\tld d,a\n");
        } else if (type_is_long(g_expr_type)) {
            /* negate DE:HL: ~DE:HL + 1 */
            int lneg_skip = new_label();
            emit("\tld a,l\n\tcpl\n\tld l,a\n");
            emit("\tld a,h\n\tcpl\n\tld h,a\n");
            emit("\tld a,e\n\tcpl\n\tld e,a\n");
            emit("\tld a,d\n\tcpl\n\tld d,a\n");
            emit("\tinc hl\n");
            emit("\tld a,h\n\tor l\n");
            emit_jp_label("jp nz,", lneg_skip);
            emit("\tinc de\n");
            emit_label(lneg_skip);
            /* The negated long is a computed value, not a faithful
             * sign/zero-extension of a 16-bit operand: e.g. -(long)(-32768)
             * is 0x00008000 (low word 0x8000 is not a signed 16-bit -32768),
             * and -(unsigned long)x has a 0xffff high word.  Clear the marker
             * so a following multiply does not use the 16x16 helper. */
            g_long_from16 = 0;
        } else {
            emit("\txor a\n\tsub l\n\tld l,a\n\tld a,0\n\tsbc a,h\n\tld h,a\n");
            /* C89 integer promotions apply to unary -. */
            g_expr_type = promote_int_type(g_expr_type);
        }
        return;
    }

    if (accept('!')) {
        int lt;
        int le;
        lt = new_label();
        le = new_label();
        gen_unary();
        emit_test_expr_nonzero(g_expr_type, lt, 0);
        emit("\tld hl,0\n");
        emit_jp_label("jp", le);
        emit_label(lt);
        emit("\tld hl,1\n");
        emit_label(le);
        g_expr_type = TYPE_INT;
        return;
    }

    if (accept('~')) {
        gen_unary();
        if (type_is_long(g_expr_type)) {
            emit("\tld a,h\n\tcpl\n\tld h,a\n\tld a,l\n\tcpl\n\tld l,a\n");
            emit("\tld a,d\n\tcpl\n\tld d,a\n\tld a,e\n\tcpl\n\tld e,a\n");
            /* ~ of a zero-extended unsigned long sets the high word to 0xffff,
             * so the result is no longer a faithful widening of a 16-bit
             * value; clear the marker before any following multiply. */
            g_long_from16 = 0;
        } else {
            emit("\tld a,h\n\tcpl\n\tld h,a\n\tld a,l\n\tcpl\n\tld l,a\n");
            /* C89 integer promotions apply before unary ~.  Without this,
             * ~uint8_t left g_expr_type as unsigned char; a following cast
             * to long zero-extended only L and turned 0xff37 into 55. */
            g_expr_type = promote_int_type(g_expr_type);
        }
        return;
    }

    if (accept('&')) {
        gen_lvalue_addr(&t);
        g_expr_type = type_add_ptr(t);
        return;
    }

    if (tok.kind == '*') {
        if (try_gen_simple_deref_value())
            return;

        accept('*');
        gen_unary();
        t = type_decay_ptr(g_expr_type);
        if ((t & 15) == TYPE_VOID)
            t = TYPE_CHAR;
        emit_load_from_hl(t);
        g_expr_type = t;
        return;
    }

    if (accept(TOK_INC)) {
        gen_lvalue_addr(&t);
        emit_pre_incdec_lvalue(t, TOK_INC);
        return;
    }

    if (accept(TOK_DEC)) {
        gen_lvalue_addr(&t);
        emit_pre_incdec_lvalue(t, TOK_DEC);
        return;
    }

    gen_primary();
}

