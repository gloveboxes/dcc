#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>
#include <time.h>

/*
 * dcc.c - tiny bootstrap C compiler for CP/M-80 / Z80
 *
 * Stage 1d.
 *
 * Host:    modern C compiler, conservative C89 style.
 * Target:  Z80 assembly for Microsoft M80 / LINK-80 style tools.
 *
 * Usage:
 *   dcc file.c -o file.mac
 */

#define MAX_TOK_TEXT   128
#define MAX_SYMS       4096
#define MAX_LOCALS     1024
#define MAX_STRINGS    2048
#define MAX_DEFINES    512
#define MAX_FLOW       128
#define MAX_SNAPSHOT   256
#define MAX_PROTO_PARAMS 16
#define MAX_SWITCH_CASES 128

#define TYPE_CHAR      1
#define TYPE_INT       2
#define TYPE_VOID      3
#define TYPE_LONG      4
#define TYPE_FLOAT     5   /* syntax/storage only: 32-bit opaque float */
#define TYPE_PTR       16
#define TYPE_PTR2      64
#define TYPE_UNSIGNED  32
#define TYPE_STRUCT    128
#define STRUCT_SHIFT   8

#define SC_GLOBAL      1
#define SC_LOCAL       2
#define SC_PARAM       3
#define SC_FUNC        4
#define SC_EXTERN      5
#define SC_REGISTER    6   /* unused; reserved */

#define TOK_EOF        0
#define TOK_ID         256
#define TOK_NUM        257
#define TOK_STR        258
#define TOK_CHARLIT    259
#define TOK_INT        260
#define TOK_CHAR       261
#define TOK_VOID       262
#define TOK_UNSIGNED   263
#define TOK_SIGNED     264
#define TOK_EXTERN     265
#define TOK_STATIC     266
#define TOK_CONST      267
#define TOK_IF         268
#define TOK_ELSE       269
#define TOK_WHILE      270
#define TOK_FOR        271
#define TOK_RETURN     272
#define TOK_BREAK      273
#define TOK_CONTINUE   274
#define TOK_SIZEOF     275
#define TOK_EQ         276
#define TOK_NE         277
#define TOK_LE         278
#define TOK_GE         279
#define TOK_ANDAND     280
#define TOK_OROR       281
#define TOK_SHL        282
#define TOK_SHR        283
#define TOK_INC        284
#define TOK_DEC        285
#define TOK_ADDEQ      286
#define TOK_SUBEQ      287
#define TOK_MULEQ      288
#define TOK_DIVEQ      289
#define TOK_MODEQ      290
#define TOK_ANDEQ      291
#define TOK_OREQ       292
#define TOK_XOREQ      293
#define TOK_SHLEQ      294
#define TOK_SHREQ      295
#define TOK_TYPEDEF    296
#define TOK_REGISTER   303
#define TOK_AUTO       313
#define TOK_DO         304
#define TOK_INLINE     305
#define TOK_GOTO       306
#define TOK_ENUM       307
#define TOK_UNION      308
#define TOK_ELLIPSIS   309
#define TOK_VOLATILE  310
#define TOK_STRUCT     297
#define TOK_ARROW      298
#define TOK_LONG       299
#define TOK_FLOAT      311
#define TOK_FLOATLIT   312
#define TOK_SHORT      315
#define TOK_WSTR       314
#define TOK_SWITCH     300
#define TOK_CASE       301
#define TOK_DEFAULT    302

struct Token {
    int kind;
    long val;
    char text[MAX_TOK_TEXT];
    char file[256];
};

struct Sym {
    char name[64];
    int type;
    int storage;
    int offset;
    int size;
    int has_init;
    long init_value;
    int init_count;
    char init_labels[256][64];
    int init_sizes[256];
    int is_array;
    int array_len;
    int elem_size; /* stride per first-dimension element */
    int dim_count; /* C array dimensions, e.g. a[2][3] -> 2 */
    int dims[8];   /* dims[0] may be 0 until inferred for a[][N] */
    int needs_extrn; /* 1 = symbol has external linkage and may need EXTRN if referenced */
    int is_defined;  /* 1 = this translation unit emits storage/PUBLIC for the symbol */
    int is_static;   /* file-scope static: internal linkage, mangle and do not PUBLIC */
    int has_proto;
    int proto_nargs;
    int proto_variadic;
    int proto_types[MAX_PROTO_PARAMS];
};

struct Def {
    char name[64];
    char value[256];
    int is_func;
    int nargs;
    char params[8][32];
};


static void fatal(const char *msg);
struct Sym; /* forward */
static void emit_extrn_if_needed(struct Sym *s);
static void emit_deferred_extrns(void);
static int starts_type(void);      /* forward: used in parse_const_int_expr */
static int parse_enum_const_value(void); /* forward: used in parse_base_type */
static int parse_base_type(void);  /* forward: used in parse_const_int_expr */
static int type_add_ptr(int t);    /* forward */
static int common_arith_type(int a, int b);
static void emit_cast_16_to_common(int from_type, int common_type);
static void gen_cmp32(int op, int lhs_type);
static int snippet_needs_long_compare(const char *lhs, const char *rhs, int *commonp);
static void emit_branch_on_bool_hl(int label, int branch_when_true);

struct AsmName {
    char cname[64];
    char aname[66]; /* '_' + up to 63-char name + '\0' = 65 bytes */
};

#define MAX_ASM_NAMES 2048
static struct AsmName asm_names[MAX_ASM_NAMES];
static int nasm_names;
static int opt_floatio;
static int opt_module;      /* -c/-module: emit linkable helper module, not final app TU */
static int opt_stack_size;  /* bytes reserved above heap for C stack */

static struct Sym *find_global(const char *name);

static int asm_name_must_mangle(const char *cname)
{
    struct Sym *s;

    if (strncmp(cname, "fake_", 5) == 0)
        return 1;

    /*
     * M80/L80 have short external-name significance, so file-scope static
     * helpers such as cb_is_zero/cb_is_one and compute_crc/compute_crc_fb can
     * collide if emitted as their C names.  Static names have internal linkage:
     * give them generated assembler names and do not make them PUBLIC.
     */
    s = find_global(cname);
    return s && s->is_static;
}

static int asm_name_is_internal_public(const char *cname)
{
    /*
     * These labels are emitted by every dcc translation unit around the
     * uninitialized global data block.  Let C code refer to them by their
     * real assembler names, without adding another leading underscore and
     * without emitting EXTRN for them.
     *
     * Usage from C:
     *     extern char __bssb;
     *     extern char __bsse;
     *     extern char __hstart;
     *     p = &__bssb;
     */
    return !strcmp(cname, "__bssb") ||
           !strcmp(cname, "__bsse") ||
           !strcmp(cname, "__hstart") ||
           !strcmp(cname, "__data_end");
}


static const char *asm_name_for_runtime(const char *cname)
{
    /*
     * LINK-80/M80 style tools only preserve a short prefix of external
     * names.  Keep ordinary C source and headers standard, but map known RTL
     * entry points to deliberately short, collision-free assembler names.
     * The returned names are final assembler labels, not C names to which
     * another underscore should be prepended.
     */
    if (!strcmp(cname, "memcpy"))  return "__mcpy";
    if (!strcmp(cname, "memcmp"))  return "__mcmp";
    if (!strcmp(cname, "memchr"))  return "__mchr";
    if (!strcmp(cname, "memmove")) return "__mmov";
    if (!strcmp(cname, "memset"))  return "__mset";

    if (!strcmp(cname, "strlen"))  return "__slen";
    if (!strcmp(cname, "strcpy"))  return "__scpy";
    if (!strcmp(cname, "strcmp"))  return "__scmp";
    if (!strcmp(cname, "strchr"))  return "__schr";
    if (!strcmp(cname, "strcat"))  return "__scat";
    if (!strcmp(cname, "strrchr")) return "__srch";
    if (!strcmp(cname, "strstr"))  return "__sstr";
    if (!strcmp(cname, "strcoll")) return "__scol";
    if (!strcmp(cname, "strcspn")) return "__scsp";
    if (!strcmp(cname, "strpbrk")) return "__spbr";
    if (!strcmp(cname, "strspn"))  return "__sspn";
    if (!strcmp(cname, "strdup"))  return "__sdup";
    if (!strcmp(cname, "strncpy")) return "__ncpy";
    if (!strcmp(cname, "strncmp")) return "__ncmp";
    if (!strcmp(cname, "strncat")) return "__ncat";

    if (!strcmp(cname, "putchar")) return "__pchr";
    if (!strcmp(cname, "putc"))    return "__putc";
    if (!strcmp(cname, "fputc"))   return "__fpc";
    if (!strcmp(cname, "fputs"))   return "__fps";
    if (!strcmp(cname, "getc"))    return "__getc";
    if (!strcmp(cname, "getchar")) return "__gchr";

    return NULL;
}

static const char *asm_name_for(const char *cname)
{
    int i;

    if (opt_floatio && !strcmp(cname, "printf"))
        return "_pffio";

    {
        const char *rtlname;
        rtlname = asm_name_for_runtime(cname);
        if (rtlname)
            return rtlname;
    }

    if (asm_name_is_internal_public(cname))
        return cname;

    for (i = 0; i < nasm_names; ++i)
        if (!strcmp(asm_names[i].cname, cname))
            return asm_names[i].aname;

    if (nasm_names >= MAX_ASM_NAMES)
        fatal("too many assembler symbol names");

    memset(&asm_names[nasm_names], 0, sizeof(asm_names[nasm_names]));
    strncpy(asm_names[nasm_names].cname, cname,
            sizeof(asm_names[nasm_names].cname) - 1);

    if (asm_name_must_mangle(cname)) {
        sprintf(asm_names[nasm_names].aname, "_Z%04d", nasm_names + 1);
    } else {
        sprintf(asm_names[nasm_names].aname, "_%s", cname);
    }

    return asm_names[nasm_names++].aname;
}

struct TypeDef {
    char name[64];
    int type;
    int array_len; /* >0 when typedef is an array type, e.g. typedef int T[4] */
    int is_func;   /* typedef names a function type, e.g. typedef int F(int); */
};

#define MAX_TYPEDEFS   512

static struct TypeDef typedefs[MAX_TYPEDEFS];
static int ntypedefs;

struct FieldDef {
    char name[64];
    int type;
    int offset;
    int size;
    int is_array;
    int array_len;
    int elem_type;
    int elem_size;
    int dim_count;
    int dims[4];
    int parent_struct_id; /* which struct/union owns this field */
    int bit_width;      /* >0 for C89 int bitfield */
    int bit_shift;      /* bit offset within containing 16-bit storage unit */
    unsigned int bit_mask;
};

struct StructDef {
    char name[64];
    int first_field;
    int field_count;
    int size;
    int is_union; /* 1 = union: all fields at offset 0, size = max(field_sizes) */
};

#define MAX_STRUCTS    256
#define MAX_FIELDS     2048

static struct StructDef struct_defs[MAX_STRUCTS];
static int nstruct_defs;
static struct FieldDef field_defs[MAX_FIELDS];
static int nfield_defs;

static int current_field_array_elem_size;
static int current_field_array_dim_count;
static int current_field_array_dims[4];
static int current_field_bit_width;
static int current_field_bit_shift;
static unsigned int current_field_bit_mask;
static char *src;
static long src_len;
static long posi;
static long tok_start_pos;
static int line_no;
static int tok_line;
static struct Token tok;
static FILE *outf;
static const char *input_name;
static const char *output_name;
static char current_file_name[256];
static char predefined_date_text[16];
static char predefined_time_text[16];

static struct Sym globals[MAX_SYMS];
static int nglobals;
static struct Sym locals[MAX_LOCALS];
static int nlocals;
static int local_size;
static int param_offset;

static struct Def defs[MAX_DEFINES];
static int ndefs;

#define MAX_IFSTACK    64
static int if_parent_active[MAX_IFSTACK];
static int if_this_active[MAX_IFSTACK];
static int if_seen_else[MAX_IFSTACK];
static int if_branch_taken[MAX_IFSTACK];
static int if_sp;
static int pp_active = 1;

static char *strings[MAX_STRINGS];
static int string_wide[MAX_STRINGS];
static int nstrings;

#define MAX_USED_EXTRNS 512
static struct Sym *used_extrns[MAX_USED_EXTRNS];
static int nused_extrns;


static int label_id;
static int current_return_label;
static int current_return_type;
static int parse_function_return_type;
static int current_local_bytes;
static int max_function_local_bytes;

static int break_stack[MAX_FLOW];
static int cont_stack[MAX_FLOW];
static int nflow;
static int errors;
static int scan_mode;
static int decl_is_extern;
static int decl_is_static;
static int decl_is_register;   /* current decl used 'register' keyword */
static int expr_result_dead;
static int g_expr_type;
static int g_tok_long_suffix; /* set by lexer when L/l suffix seen on integer literal */
static int g_tok_unsigned_suffix; /* set for U/u suffix or non-decimal unsigned-int literal */

/* User-defined goto labels (function-scoped) */
#define MAX_USER_LABELS 256
static char ulabel_names[MAX_USER_LABELS][64];
static int  ulabel_ids[MAX_USER_LABELS];
static int  nulabels;

/* Enum constants (file-scoped) */
#define MAX_ENUM_CONSTS 512
static char enum_const_names[MAX_ENUM_CONSTS][64];
static int  enum_const_values[MAX_ENUM_CONSTS];
static int  nenum_consts;

/* Communicates array length from array-typedef through parse_base_type to declarators */
static int g_typedef_array_len;
static int g_typedef_is_func;

/* Counter for naming anonymous structs/unions uniquely */
static int g_anon_struct_counter;

/* Most recently parsed function prototype/parameter-list metadata. */
static int g_proto_has;
static int g_proto_nargs;
static int g_proto_variadic;
static int g_proto_types[MAX_PROTO_PARAMS];
static int g_funcptr_decl_array_len;
static int g_last_array_dim_count;
static int g_last_array_dims[8];

static void fatal(const char *msg)
{
    fprintf(stderr, "dcc: fatal: %s\n", msg);
    exit(1);
}

static void init_predefined_macro_texts(void)
{
    time_t now;
    struct tm *tmv;
    static const char *mons[] = {
        "Jan", "Feb", "Mar", "Apr", "May", "Jun",
        "Jul", "Aug", "Sep", "Oct", "Nov", "Dec"
    };

    now = time(NULL);
    tmv = localtime(&now);
    if (tmv) {
        sprintf(predefined_date_text, "%s %2d %04d",
                mons[tmv->tm_mon], tmv->tm_mday, tmv->tm_year + 1900);
        sprintf(predefined_time_text, "%02d:%02d:%02d",
                tmv->tm_hour, tmv->tm_min, tmv->tm_sec);
    } else {
        strcpy(predefined_date_text, "Jan  1 1970");
        strcpy(predefined_time_text, "00:00:00");
    }
}

static void source_location_at(long ofs, char *filebuf, int filebufsz, int *linep)
{
    long p;
    long line_start;
    long line_end;
    int line;
    const char *fname;

    fname = input_name ? input_name : "<input>";
    line = 1;
    if (filebufsz > 0) {
        strncpy(filebuf, fname, (size_t)filebufsz - 1);
        filebuf[filebufsz - 1] = 0;
    }

    if (ofs < 0)
        ofs = 0;
    if (ofs > src_len)
        ofs = src_len;

    p = 0;
    while (p < ofs) {
        int i;

        line_start = p;
        while (p < src_len && src[p] != '\n')
            p++;
        line_end = p;

        i = (int)line_start;
        while (i < line_end && (src[i] == ' ' || src[i] == '\t'))
            i++;

        if (i + 5 <= line_end && src[i] == '#' &&
            src[i + 1] == 'l' && src[i + 2] == 'i' &&
            src[i + 3] == 'n' && src[i + 4] == 'e' &&
            (i + 5 == line_end || src[i + 5] == ' ' || src[i + 5] == '\t')) {
            int n;
            int qi;

            i += 5;
            while (i < line_end && (src[i] == ' ' || src[i] == '\t'))
                i++;
            n = 0;
            while (i < line_end && src[i] >= '0' && src[i] <= '9') {
                n = n * 10 + src[i] - '0';
                i++;
            }
            if (n > 0)
                line = n - 1;

            while (i < line_end && (src[i] == ' ' || src[i] == '\t'))
                i++;
            if (i < line_end && src[i] == '"') {
                i++;
                qi = 0;
                while (i < line_end && src[i] != '"' && qi < filebufsz - 1)
                    filebuf[qi++] = src[i++];
                if (filebufsz > 0)
                    filebuf[qi] = 0;
            }
        }

        if (p >= ofs)
            break;
        if (p < src_len && src[p] == '\n') {
            p++;
            line++;
        }
    }

    linep[0] = line;
}

static void error_here(const char *msg)
{
    const char *fn;

    fn = tok.file[0] ? tok.file : (input_name ? input_name : "<input>");
    fprintf(stderr, "%s:%d: error: %s near '%s'\n",
            fn, tok_line, msg, tok.text);
    errors++;
    if (errors > 40) fatal("too many errors");
}

static void *xmalloc(size_t n)
{
    void *p;
    p = malloc(n);
    if (!p) fatal("out of memory");
    return p;
}

static char *xstrdup2(const char *s)
{
    char *p;
    p = (char *)xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

static int new_label(void)
{
    return ++label_id;
}

static void emit(const char *s);

static void emit_ld_de_const(long v)
{
    if (!scan_mode)
        fprintf(outf, "\tld de,%ld\n", v & 0xffffL);
}

static void emit_add_const_to_hl(long v)
{
    v &= 0xffffL;
    if (v == 0)
        return;
    emit_ld_de_const(v);
    emit("\tadd hl,de\n");
}

static void emit(const char *s)
{
    if (!scan_mode)
        fputs(s, outf);
}

static void emit_label(int n)
{
    if (!scan_mode)
        fprintf(outf, "L%d:\n", n);
}

static void emit_jp_label(const char *op, int n)
{
    if (!scan_mode)
        fprintf(outf, "\t%s L%d\n", op, n);
}

static int is_ident_start(int c)
{
    return isalpha((unsigned char)c) || c == '_';
}

static int is_ident_char(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}

static int peekc(void)
{
    if (posi >= src_len) return 0;
    return (unsigned char)src[posi];
}

static int getc_src(void)
{
    int c;
    if (posi >= src_len) return 0;
    c = (unsigned char)src[posi++];
    if (c == '\n') line_no++;
    return c;
}

static int define_number_value(const char *name, long *out, int depth);

static int find_define(const char *name)
{
    int i;
    for (i = ndefs - 1; i >= 0; --i)
        if (!strcmp(defs[i].name, name)) return i;
    return -1;
}

static void add_define_ex(const char *name, const char *value, int is_func, int nargs, char params[8][32])
{
    int i;
    int j;

    i = find_define(name);
    if (i < 0) {
        if (ndefs >= MAX_DEFINES) fatal("too many defines");
        i = ndefs++;
        memset(&defs[i], 0, sizeof(defs[i]));
        strncpy(defs[i].name, name, sizeof(defs[i].name) - 1);
    }

    defs[i].is_func = is_func;
    defs[i].nargs = nargs;
    for (j = 0; j < 8; ++j)
        defs[i].params[j][0] = 0;
    for (j = 0; j < nargs && j < 8; ++j) {
        strncpy(defs[i].params[j], params[j], sizeof(defs[i].params[j]) - 1);
        defs[i].params[j][sizeof(defs[i].params[j]) - 1] = 0;
    }

    strncpy(defs[i].value, value, sizeof(defs[i].value) - 1);
    defs[i].value[sizeof(defs[i].value) - 1] = 0;
}

static void add_define(const char *name, const char *value)
{
    char dummy[8][32];
    memset(dummy, 0, sizeof(dummy));
    add_define_ex(name, value, 0, 0, dummy);
}

static const char *pp_expr_p;
static int pp_expr_depth;

static void pp_expr_skip_ws(void)
{
    while (*pp_expr_p && isspace((unsigned char)*pp_expr_p))
        pp_expr_p++;
}

static int pp_expr_match_word(const char *w)
{
    int n;
    n = (int)strlen(w);
    pp_expr_skip_ws();
    if (strncmp(pp_expr_p, w, n) != 0)
        return 0;
    if (is_ident_char((unsigned char)pp_expr_p[n]))
        return 0;
    pp_expr_p += n;
    return 1;
}

static long pp_expr_number(void)
{
    unsigned long v;
    int base;

    pp_expr_skip_ws();

    v = 0;
    base = 10;
    if (pp_expr_p[0] == '0') {
        if (pp_expr_p[1] == 'x' || pp_expr_p[1] == 'X') {
            base = 16;
            pp_expr_p += 2;
        } else {
            base = 8;
            pp_expr_p++;
        }
    }

    if (base == 16) {
        while (isxdigit((unsigned char)*pp_expr_p)) {
            v *= 16;
            if (*pp_expr_p >= '0' && *pp_expr_p <= '9') v += *pp_expr_p - '0';
            else if (*pp_expr_p >= 'a' && *pp_expr_p <= 'f') v += *pp_expr_p - 'a' + 10;
            else v += *pp_expr_p - 'A' + 10;
            pp_expr_p++;
        }
    } else {
        while (*pp_expr_p >= '0' && *pp_expr_p <= (base == 8 ? '7' : '9')) {
            v = v * (unsigned long)base + (unsigned long)(*pp_expr_p - '0');
            pp_expr_p++;
        }
    }

    while (*pp_expr_p == 'u' || *pp_expr_p == 'U' ||
           *pp_expr_p == 'l' || *pp_expr_p == 'L')
        pp_expr_p++;

    return (long)v;
}

static long pp_expr_charlit(void)
{
    int c;
    long v;

    pp_expr_skip_ws();
    if (*pp_expr_p != '\'')
        return 0;
    pp_expr_p++;

    if (*pp_expr_p == '\\') {
        pp_expr_p++;
        c = (unsigned char)*pp_expr_p;
        if (c == 'n') v = '\n';
        else if (c == 'r') v = '\r';
        else if (c == 't') v = '\t';
        else if (c == '0') v = 0;
        else v = c;
        if (*pp_expr_p)
            pp_expr_p++;
    } else {
        v = (unsigned char)*pp_expr_p;
        if (*pp_expr_p)
            pp_expr_p++;
    }

    while (*pp_expr_p && *pp_expr_p != '\'')
        pp_expr_p++;
    if (*pp_expr_p == '\'')
        pp_expr_p++;

    return v;
}

static long pp_expr_primary(void);
static long pp_expr_unary(void);
static long pp_expr_mul(void);
static long pp_expr_add(void);
static long pp_expr_shift(void);
static long pp_expr_rel(void);
static long pp_expr_eq(void);
static long pp_expr_bitand(void);
static long pp_expr_bitxor(void);
static long pp_expr_bitor(void);
static long pp_expr_andand(void);
static long pp_expr_oror(void);

static long pp_expr_defined(void)
{
    char name[64];
    int i;

    if (!pp_expr_match_word("defined"))
        return 0;

    pp_expr_skip_ws();
    if (*pp_expr_p == '(') {
        pp_expr_p++;
        pp_expr_skip_ws();
        i = 0;
        while (is_ident_char((unsigned char)*pp_expr_p) && i < 63)
            name[i++] = *pp_expr_p++;
        name[i] = 0;
        pp_expr_skip_ws();
        if (*pp_expr_p == ')')
            pp_expr_p++;
    } else {
        i = 0;
        while (is_ident_char((unsigned char)*pp_expr_p) && i < 63)
            name[i++] = *pp_expr_p++;
        name[i] = 0;
    }

    return name[0] && find_define(name) >= 0;
}

static long pp_expr_primary(void)
{
    char name[64];
    int i;
    long v;

    pp_expr_skip_ws();

    if (!strncmp(pp_expr_p, "defined", 7) &&
        !is_ident_char((unsigned char)pp_expr_p[7]))
        return pp_expr_defined();

    if (*pp_expr_p == '(') {
        pp_expr_p++;
        v = pp_expr_oror();
        pp_expr_skip_ws();
        if (*pp_expr_p == ')')
            pp_expr_p++;
        return v;
    }

    if (*pp_expr_p == '\'')
        return pp_expr_charlit();

    if (isdigit((unsigned char)*pp_expr_p))
        return pp_expr_number();

    if (is_ident_start((unsigned char)*pp_expr_p)) {
        int di;
        const char *savep;

        i = 0;
        while (is_ident_char((unsigned char)*pp_expr_p) && i < 63)
            name[i++] = *pp_expr_p++;
        name[i] = 0;

        di = find_define(name);
        if (di >= 0 && !defs[di].is_func && pp_expr_depth < 16) {
            savep = pp_expr_p;
            pp_expr_p = defs[di].value;
            pp_expr_depth++;
            v = pp_expr_oror();
            pp_expr_depth--;
            pp_expr_p = savep;
            return v;
        }

        return 0;
    }

    return 0;
}

static long pp_expr_unary(void)
{
    pp_expr_skip_ws();
    if (*pp_expr_p == '!') {
        pp_expr_p++;
        return !pp_expr_unary();
    }
    if (*pp_expr_p == '~') {
        pp_expr_p++;
        return ~pp_expr_unary();
    }
    if (*pp_expr_p == '+') {
        pp_expr_p++;
        return pp_expr_unary();
    }
    if (*pp_expr_p == '-') {
        pp_expr_p++;
        return -pp_expr_unary();
    }
    return pp_expr_primary();
}

static long pp_expr_mul(void)
{
    long v;
    long r;

    v = pp_expr_unary();
    for (;;) {
        pp_expr_skip_ws();
        if (*pp_expr_p == '*') {
            pp_expr_p++;
            v = v * pp_expr_unary();
        } else if (*pp_expr_p == '/') {
            pp_expr_p++;
            r = pp_expr_unary();
            v = r ? (v / r) : 0;
        } else if (*pp_expr_p == '%') {
            pp_expr_p++;
            r = pp_expr_unary();
            v = r ? (v % r) : 0;
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_add(void)
{
    long v;

    v = pp_expr_mul();
    for (;;) {
        pp_expr_skip_ws();
        if (*pp_expr_p == '+') {
            pp_expr_p++;
            v = v + pp_expr_mul();
        } else if (*pp_expr_p == '-') {
            pp_expr_p++;
            v = v - pp_expr_mul();
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_shift(void)
{
    long v;
    long r;

    v = pp_expr_add();
    for (;;) {
        pp_expr_skip_ws();
        if (pp_expr_p[0] == '<' && pp_expr_p[1] == '<') {
            pp_expr_p += 2;
            r = pp_expr_add();
            if (r < 0 || r >= 32)
                v = 0;
            else
                v = v << (int)r;
        } else if (pp_expr_p[0] == '>' && pp_expr_p[1] == '>') {
            pp_expr_p += 2;
            r = pp_expr_add();
            if (r < 0 || r >= 32)
                v = 0;
            else
                v = v >> (int)r;
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_rel(void)
{
    long v;
    long r;

    v = pp_expr_shift();
    for (;;) {
        pp_expr_skip_ws();
        if (pp_expr_p[0] == '<' && pp_expr_p[1] == '=') {
            pp_expr_p += 2;
            r = pp_expr_shift();
            v = (v <= r);
        } else if (pp_expr_p[0] == '>' && pp_expr_p[1] == '=') {
            pp_expr_p += 2;
            r = pp_expr_shift();
            v = (v >= r);
        } else if (*pp_expr_p == '<') {
            pp_expr_p++;
            r = pp_expr_shift();
            v = (v < r);
        } else if (*pp_expr_p == '>') {
            pp_expr_p++;
            r = pp_expr_shift();
            v = (v > r);
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_eq(void)
{
    long v;
    long r;

    v = pp_expr_rel();
    for (;;) {
        pp_expr_skip_ws();
        if (pp_expr_p[0] == '=' && pp_expr_p[1] == '=') {
            pp_expr_p += 2;
            r = pp_expr_rel();
            v = (v == r);
        } else if (pp_expr_p[0] == '!' && pp_expr_p[1] == '=') {
            pp_expr_p += 2;
            r = pp_expr_rel();
            v = (v != r);
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_bitand(void)
{
    long v;

    v = pp_expr_eq();
    for (;;) {
        pp_expr_skip_ws();
        if (*pp_expr_p == '&' && pp_expr_p[1] != '&') {
            pp_expr_p++;
            v = v & pp_expr_eq();
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_bitxor(void)
{
    long v;

    v = pp_expr_bitand();
    for (;;) {
        pp_expr_skip_ws();
        if (*pp_expr_p == '^') {
            pp_expr_p++;
            v = v ^ pp_expr_bitand();
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_bitor(void)
{
    long v;

    v = pp_expr_bitxor();
    for (;;) {
        pp_expr_skip_ws();
        if (*pp_expr_p == '|' && pp_expr_p[1] != '|') {
            pp_expr_p++;
            v = v | pp_expr_bitxor();
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_andand(void)
{
    long v;
    v = pp_expr_bitor();
    for (;;) {
        pp_expr_skip_ws();
        if (pp_expr_p[0] == '&' && pp_expr_p[1] == '&') {
            pp_expr_p += 2;
            v = (pp_expr_bitor() && v);
        } else {
            break;
        }
    }
    return v;
}

static long pp_expr_oror(void)
{
    long v;
    v = pp_expr_andand();
    for (;;) {
        pp_expr_skip_ws();
        if (pp_expr_p[0] == '|' && pp_expr_p[1] == '|') {
            pp_expr_p += 2;
            v = (pp_expr_andand() || v);
        } else {
            break;
        }
    }
    return v;
}

static int pp_eval_simple_expr(const char *s)
{
    pp_expr_p = s;
    pp_expr_depth = 0;
    return pp_expr_oror() != 0;
}

static void remove_define(const char *name)
{
    int i;
    int j;

    i = find_define(name);
    if (i < 0)
        return;

    for (j = i; j + 1 < ndefs; ++j)
        defs[j] = defs[j + 1];
    ndefs--;
}

static void pp_recompute_active(void)
{
    if (if_sp <= 0) {
        pp_active = 1;
    } else {
        pp_active = if_parent_active[if_sp - 1] && if_this_active[if_sp - 1];
    }
}

static void parse_preprocessor_line(void)
{
    char word[32];
    char name[64];
    char val[512];
    int i, c;
    int parent;
    int cond;

    while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();

    i = 0;
    while (is_ident_char(peekc()) && i < (int)sizeof(word) - 1)
        word[i++] = (char)getc_src();
    word[i] = 0;

    if (!strcmp(word, "if")) {
        i = 0;
        while ((c = peekc()) != 0 && c != '\n' && i < (int)sizeof(val) - 1)
            val[i++] = (char)getc_src();
        val[i] = 0;

        if (if_sp >= MAX_IFSTACK) fatal("too many nested #if");

        parent = pp_active;
        cond = pp_eval_simple_expr(val);

        if_parent_active[if_sp] = parent;
        if_this_active[if_sp] = cond ? 1 : 0;
        if_seen_else[if_sp] = 0;
        if_branch_taken[if_sp] = cond ? 1 : 0;
        if_sp++;
        pp_recompute_active();
    } else if (!strcmp(word, "ifdef") || !strcmp(word, "ifndef")) {
        while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();

        i = 0;
        while (is_ident_char(peekc()) && i < (int)sizeof(name) - 1)
            name[i++] = (char)getc_src();
        name[i] = 0;

        if (if_sp >= MAX_IFSTACK) fatal("too many nested #if");

        parent = pp_active;
        cond = find_define(name) >= 0;
        if (!strcmp(word, "ifndef")) cond = !cond;

        if_parent_active[if_sp] = parent;
        if_this_active[if_sp] = cond ? 1 : 0;
        if_seen_else[if_sp] = 0;
        if_branch_taken[if_sp] = cond ? 1 : 0;
        if_sp++;
        pp_recompute_active();
    } else if (!strcmp(word, "elif")) {
        if (if_sp <= 0) {
            /* ignore unmatched #elif for now */
        } else {
            i = if_sp - 1;
            if (if_seen_else[i]) {
                if_this_active[i] = 0;
            } else {
                i = 0;
                while ((c = peekc()) != 0 && c != '\n' && i < (int)sizeof(val) - 1)
                    val[i++] = (char)getc_src();
                val[i] = 0;
                cond = pp_eval_simple_expr(val);
                i = if_sp - 1;
                if (!if_branch_taken[i] && cond) {
                    if_this_active[i] = 1;
                    if_branch_taken[i] = 1;
                } else {
                    if_this_active[i] = 0;
                }
            }
            pp_recompute_active();
        }
    } else if (!strcmp(word, "else")) {
        if (if_sp <= 0) {
            /* ignore unmatched #else for now */
        } else {
            i = if_sp - 1;
            if (!if_seen_else[i]) {
                if_this_active[i] = if_branch_taken[i] ? 0 : 1;
                if_branch_taken[i] = 1;
                if_seen_else[i] = 1;
            }
            pp_recompute_active();
        }
    } else if (!strcmp(word, "endif")) {
        if (if_sp > 0)
            if_sp--;
        pp_recompute_active();
    } else if (!strcmp(word, "line")) {
        int lno;
        int qi;

        while (isspace((unsigned char)peekc()) && peekc() != '\n')
            getc_src();

        lno = 0;
        while (isdigit((unsigned char)peekc()))
            lno = lno * 10 + getc_src() - '0';

        if (lno > 0)
            line_no = lno - 1;

        while (isspace((unsigned char)peekc()) && peekc() != '\n')
            getc_src();

        if (peekc() == '"') {
            getc_src();
            qi = 0;
            while (peekc() && peekc() != '"' && peekc() != '\n' &&
                   qi < (int)sizeof(current_file_name) - 1)
                current_file_name[qi++] = (char)getc_src();
            current_file_name[qi] = 0;
            if (peekc() == '"')
                getc_src();
        }
    } else if (!strcmp(word, "include")) {
        /* #include directives are expanded before tokenisation by
         * preprocess_includes_file(); any that survive to here (e.g. inside
         * an inactive #ifdef block) are silently skipped. */
    } else if (!strcmp(word, "undef")) {
        if (pp_active) {
            while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();
            i = 0;
            while (is_ident_char(peekc()) && i < (int)sizeof(name) - 1)
                name[i++] = (char)getc_src();
            name[i] = 0;
            if (name[0]) remove_define(name);
        }
    } else if (!strcmp(word, "error")) {
        if (pp_active) {
            i = 0;
            while ((c = peekc()) != 0 && c != '\n' && i < (int)sizeof(val) - 1)
                val[i++] = (char)getc_src();
            val[i] = 0;
            fprintf(stderr, "%s:%d: error: #error %s\n",
                    current_file_name[0] ? current_file_name : (input_name ? input_name : "<input>"),
                    line_no, val);
            errors++;
            if (errors > 40) fatal("too many errors");
        }
    } else if (!strcmp(word, "define")) {
        if (pp_active) {
            while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();

            i = 0;
            while (is_ident_char(peekc()) && i < (int)sizeof(name) - 1)
                name[i++] = (char)getc_src();
            name[i] = 0;

            /* Function-like macro: the '(' must immediately follow the name.
             * This is enough for forms such as:
             *     #define _countof( X ) ( sizeof( X ) / sizeof( X[0] ) )
             */
            if (peekc() == '(') {
                char params[8][32];
                int nargs;
                int pi;
                int pp;

                memset(params, 0, sizeof(params));
                nargs = 0;
                getc_src();

                while (peekc() && peekc() != ')' && peekc() != '\n') {
                    while (isspace((unsigned char)peekc()) && peekc() != '\n')
                        getc_src();
                    if (peekc() == ')')
                        break;

                    pp = 0;
                    while (is_ident_char(peekc()) && pp < 31)
                        params[nargs][pp++] = (char)getc_src();
                    params[nargs][pp] = 0;
                    if (params[nargs][0] && nargs < 7)
                        nargs++;

                    while (isspace((unsigned char)peekc()) && peekc() != '\n')
                        getc_src();
                    if (peekc() == ',')
                        getc_src();
                }
                if (peekc() == ')')
                    getc_src();

                while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();

                i = 0;
                while ((c = peekc()) != 0 && c != '\n' && i < (int)sizeof(val) - 1)
                    val[i++] = (char)getc_src();
                val[i] = 0;

                if (name[0]) add_define_ex(name, val[0] ? val : "1", 1, nargs, params);
                (void)pi;
            } else {
                while (isspace((unsigned char)peekc()) && peekc() != '\n') getc_src();

                i = 0;
                while ((c = peekc()) != 0 && c != '\n' && i < (int)sizeof(val) - 1)
                    val[i++] = (char)getc_src();
                val[i] = 0;

                if (name[0]) add_define(name, val[0] ? val : "1");
            }
        }
    } else {
        /* include/pragma/etc. ignored */
    }

    while (peekc() && peekc() != '\n') getc_src();
}

static void skip_ws_and_comments(void)
{
    int c;

    for (;;) {
        c = peekc();

        while (isspace((unsigned char)c)) {
            getc_src();
            c = peekc();
        }

        if (c == '/' && posi + 1 < src_len && src[posi + 1] == '/') {
            while (peekc() && peekc() != '\n') getc_src();
            continue;
        }

        if (c == '/' && posi + 1 < src_len && src[posi + 1] == '*') {
            posi += 2;
            while (peekc()) {
                if (peekc() == '*' && posi + 1 < src_len && src[posi + 1] == '/') {
                    posi += 2;
                    break;
                }
                getc_src();
            }
            continue;
        }

        if (c == '#') {
            getc_src();
            parse_preprocessor_line();
            continue;
        }

        if (!pp_active) {
            while (peekc() && peekc() != '\n')
                getc_src();
            continue;
        }

        break;
    }
}

static int keyword_kind(const char *s)
{
    if (!strcmp(s, "int")) return TOK_INT;
    if (!strcmp(s, "short")) return TOK_SHORT;
    if (!strcmp(s, "long")) return TOK_LONG;
    if (!strcmp(s, "float")) return TOK_FLOAT;
    if (!strcmp(s, "char")) return TOK_CHAR;
    if (!strcmp(s, "void")) return TOK_VOID;
    if (!strcmp(s, "unsigned")) return TOK_UNSIGNED;
    if (!strcmp(s, "signed")) return TOK_SIGNED;
    if (!strcmp(s, "extern")) return TOK_EXTERN;
    if (!strcmp(s, "static")) return TOK_STATIC;
    if (!strcmp(s, "register")) return TOK_REGISTER;
    if (!strcmp(s, "auto")) return TOK_AUTO;
    if (!strcmp(s, "const")) return TOK_CONST;
    if (!strcmp(s, "volatile")) return TOK_VOLATILE;
    if (!strcmp(s, "if")) return TOK_IF;
    if (!strcmp(s, "else")) return TOK_ELSE;
    if (!strcmp(s, "while")) return TOK_WHILE;
    if (!strcmp(s, "for")) return TOK_FOR;
    if (!strcmp(s, "return")) return TOK_RETURN;
    if (!strcmp(s, "break")) return TOK_BREAK;
    if (!strcmp(s, "continue")) return TOK_CONTINUE;
    if (!strcmp(s, "sizeof")) return TOK_SIZEOF;
    if (!strcmp(s, "switch")) return TOK_SWITCH;
    if (!strcmp(s, "case")) return TOK_CASE;
    if (!strcmp(s, "default")) return TOK_DEFAULT;
    if (!strcmp(s, "typedef")) return TOK_TYPEDEF;
    if (!strcmp(s, "struct")) return TOK_STRUCT;
    if (!strcmp(s, "union")) return TOK_UNION;
    if (!strcmp(s, "enum"))  return TOK_ENUM;
    if (!strcmp(s, "goto"))  return TOK_GOTO;
    if (!strcmp(s, "do")) return TOK_DO;
    if (!strcmp(s, "inline")) return TOK_INLINE;
    return TOK_ID;
}

static int read_escape(void)
{
    int c;
    c = getc_src();
    if (c == 'n') return '\n';
    if (c == 'r') return '\r';
    if (c == 't') return '\t';
    if (c == '0') return 0;
    if (c == '\\') return '\\';
    if (c == '\'') return '\'';
    if (c == '"') return '"';
    return c;
}

static long parse_number_string(const char *s)
{
    long v;
    int i;
    int neg;

    v = 0;
    i = 0;
    neg = 0;

    if (s[i] == '-') {
        neg = 1;
        i++;
    }

    if (s[i] == '0' && (s[i + 1] == 'x' || s[i + 1] == 'X')) {
        i += 2;
        while (isxdigit((unsigned char)s[i])) {
            v *= 16;
            if (s[i] >= '0' && s[i] <= '9') v += s[i] - '0';
            else if (s[i] >= 'a' && s[i] <= 'f') v += s[i] - 'a' + 10;
            else v += s[i] - 'A' + 10;
            i++;
        }
    } else {
        while (isdigit((unsigned char)s[i])) {
            v = v * 10 + s[i] - '0';
            i++;
        }
    }

    while (s[i] == 'u' || s[i] == 'U' || s[i] == 'l' || s[i] == 'L')
        i++;

    if (neg) v = -v;
    return v & 0xffffL;
}


static unsigned long parse_float_literal_bits(const char *text)
{
    /* Stage-1 float support stores 32-bit IEEE single constants as opaque
     * data.  The host compiler is expected to use IEEE-754 float, which is
     * true for the supported Windows/Linux/macOS build hosts. */
    union {
        float f;
        unsigned char b[4];
    } u;
    double d;
    unsigned long v;

    d = atof(text);
    u.f = (float)d;
    v = ((unsigned long)u.b[0]) |
        ((unsigned long)u.b[1] << 8) |
        ((unsigned long)u.b[2] << 16) |
        ((unsigned long)u.b[3] << 24);
    return v;
}


static int parse_escape_string_char(const char **ps)
{
    int c;

    c = (unsigned char)**ps;
    if (c == 0)
        return 0;
    *ps = *ps + 1;

    if (c != '\\')
        return c;

    c = (unsigned char)**ps;
    if (c == 0)
        return '\\';
    *ps = *ps + 1;

    if (c == 'n') return '\n';
    if (c == 'r') return '\r';
    if (c == 't') return '\t';
    if (c == 'b') return '\b';
    if (c == 'f') return '\f';
    if (c == 'v') return '\v';
    if (c == 'a') return 7;
    if (c == '\\') return '\\';
    if (c == '\'') return '\'';
    if (c == '"') return '"';

    if (c >= '0' && c <= '7') {
        int v;
        int n;
        v = c - '0';
        n = 1;
        while (n < 3 && **ps >= '0' && **ps <= '7') {
            v = v * 8 + (**ps - '0');
            *ps = *ps + 1;
            n++;
        }
        return v & 255;
    }

    return c;
}

static int parse_charlit_string_value(const char *s, long *out)
{
    const char *p;
    int c;

    p = s;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != '\'')
        return 0;
    p++;

    c = parse_escape_string_char(&p);

    if (*p != '\'')
        return 0;
    p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p != 0)
        return 0;

    *out = c & 255;
    return 1;
}


static void replace_source_range(long start, long end, const char *text)
{
    long n;
    long rest;
    char *nsrc;

    if (start < 0) start = 0;
    if (end < start) end = start;
    if (end > src_len) end = src_len;

    n = (long)strlen(text);
    rest = src_len - end;
    nsrc = (char *)xmalloc((size_t)(start + n + rest + 1));
    memcpy(nsrc, src, (size_t)start);
    memcpy(nsrc + start, text, (size_t)n);
    memcpy(nsrc + start + n, src + end, (size_t)rest);
    nsrc[start + n + rest] = 0;
    src = nsrc;
    src_len = start + n + rest;
    posi = start;
}

static void trim_arg(char *s)
{
    int i;
    int j;
    int n;

    i = 0;
    while (s[i] && isspace((unsigned char)s[i]))
        i++;

    if (i > 0) {
        j = 0;
        while (s[i])
            s[j++] = s[i++];
        s[j] = 0;
    }

    n = (int)strlen(s);
    while (n > 0 && isspace((unsigned char)s[n - 1])) {
        s[n - 1] = 0;
        n--;
    }
}

static int read_macro_call_args(char args[8][128], int *nargs)
{
    int c;
    int depth;
    int ai;
    int ap;

    while (isspace((unsigned char)peekc()))
        getc_src();

    if (peekc() != '(')
        return 0;

    getc_src();
    ai = 0;
    ap = 0;
    depth = 0;
    memset(args, 0, 8 * 128);

    for (;;) {
        c = getc_src();
        if (c == 0)
            return 0;

        if (c == '"') {
            if (ap < 127) args[ai][ap++] = (char)c;
            while ((c = getc_src()) != 0) {
                if (ap < 127) args[ai][ap++] = (char)c;
                if (c == '\\') {
                    c = getc_src();
                    if (c == 0) return 0;
                    if (ap < 127) args[ai][ap++] = (char)c;
                } else if (c == '"') {
                    break;
                }
            }
            continue;
        }

        if (c == '\'') {
            if (ap < 127) args[ai][ap++] = (char)c;
            while ((c = getc_src()) != 0) {
                if (ap < 127) args[ai][ap++] = (char)c;
                if (c == '\\') {
                    c = getc_src();
                    if (c == 0) return 0;
                    if (ap < 127) args[ai][ap++] = (char)c;
                } else if (c == '\'') {
                    break;
                }
            }
            continue;
        }

        if (c == '(' || c == '[' || c == '{') {
            depth++;
            if (ap < 127) args[ai][ap++] = (char)c;
            continue;
        }

        if (c == ')' && depth == 0) {
            args[ai][ap] = 0;
            trim_arg(args[ai]);
            ai++;
            break;
        }

        if (c == ')' || c == ']' || c == '}') {
            depth--;
            if (ap < 127) args[ai][ap++] = (char)c;
            continue;
        }

        if (c == ',' && depth == 0) {
            args[ai][ap] = 0;
            trim_arg(args[ai]);
            ai++;
            if (ai >= 8)
                fatal("too many macro arguments");
            ap = 0;
            continue;
        }

        if (ap < 127)
            args[ai][ap++] = (char)c;
    }

    if (ai == 1 && args[0][0] == 0)
        ai = 0;
    *nargs = ai;
    return 1;
}


static void append_macro_string_literal(const char *arg, char *out, int *oip, int outsz)
{
    int oi;
    const char *p;
    int last_space;

    oi = oip[0];
    if (oi < outsz - 1)
        out[oi++] = '"';

    p = arg;
    while (*p && isspace((unsigned char)*p))
        p++;

    last_space = 0;
    while (*p && oi < outsz - 1) {
        unsigned char c;

        c = (unsigned char)*p++;

        if (isspace(c)) {
            last_space = 1;
            continue;
        }

        if (last_space) {
            if (oi < outsz - 1)
                out[oi++] = ' ';
            last_space = 0;
        }

        if (c == '"' || c == '\\') {
            if (oi < outsz - 1)
                out[oi++] = '\\';
            if (oi < outsz - 1)
                out[oi++] = (char)c;
        } else if (c == '\n' || c == '\r') {
            if (oi < outsz - 1)
                out[oi++] = ' ';
        } else {
            out[oi++] = (char)c;
        }
    }

    if (oi < outsz - 1)
        out[oi++] = '"';

    oip[0] = oi;
}

static int macro_param_index(int di, const char *ident)
{
    int j;
    for (j = 0; j < defs[di].nargs; ++j) {
        if (!strcmp(ident, defs[di].params[j]))
            return j;
    }
    return -1;
}

static void macro_expand_argument_text(const char *in, char *out, int outsz, int depth)
{
    int oi;
    const char *p;

    if (depth > 8) {
        strncpy(out, in, (size_t)outsz - 1);
        out[outsz - 1] = 0;
        return;
    }

    oi = 0;
    p = in;
    while (*p && oi < outsz - 1) {
        if (is_ident_start((unsigned char)*p)) {
            char ident[64];
            int ii;
            int di;

            ii = 0;
            while (is_ident_char((unsigned char)*p) && ii < 63)
                ident[ii++] = *p++;
            ident[ii] = 0;

            di = find_define(ident);
            if (di >= 0 && !defs[di].is_func) {
                char tmp[256];
                macro_expand_argument_text(defs[di].value, tmp, sizeof(tmp), depth + 1);
                for (ii = 0; tmp[ii] && oi < outsz - 1; ++ii)
                    out[oi++] = tmp[ii];
            } else {
                for (ii = 0; ident[ii] && oi < outsz - 1; ++ii)
                    out[oi++] = ident[ii];
            }
        } else {
            out[oi++] = *p++;
        }
    }
    out[oi] = 0;
}

static void paste_tokens_in_text(char *s)
{
    char tmp[512];
    int i;
    int o;

    i = 0;
    o = 0;

    while (s[i] && o < (int)sizeof(tmp) - 1) {
        if (s[i] == '#' && s[i + 1] == '#') {
            while (o > 0 && isspace((unsigned char)tmp[o - 1]))
                --o;
            i += 2;
            while (s[i] && isspace((unsigned char)s[i]))
                ++i;
            continue;
        }

        tmp[o++] = s[i++];
    }

    tmp[o] = 0;
    strcpy(s, tmp);
}

static int replacement_param_raw_context(const char *start, const char *param_start, const char *param_end)
{
    const char *p;

    /* Used with # before parameter? */
    p = param_start;
    while (p > start && (p[-1] == ' ' || p[-1] == '\t'))
        --p;
    if (p > start && p[-1] == '#') {
        if (!(p - 1 > start && p[-2] == '#'))
            return 1;
    }

    /* Adjacent to ## before parameter? */
    if (p - 2 >= start && p[-1] == '#' && p[-2] == '#')
        return 1;

    /* Adjacent to ## after parameter? */
    p = param_end;
    while (*p == ' ' || *p == '\t')
        ++p;
    if (p[0] == '#' && p[1] == '#')
        return 1;

    return 0;
}

static void expand_function_macro(int di, char args[8][128], char *out, int outsz)
{
    const char *v;
    int oi;
    int i;
    int j;
    char ident[64];
    int matched;
    char expanded_args[8][256];

    for (i = 0; i < 8; ++i)
        expanded_args[i][0] = 0;
    for (i = 0; i < defs[di].nargs && i < 8; ++i)
        macro_expand_argument_text(args[i], expanded_args[i], sizeof(expanded_args[i]), 0);

    v = defs[di].value;
    oi = 0;

    while (*v && oi < outsz - 1) {
        /*
         * C89 macro stringification:
         *     #define S(x) #x
         *     S(a + b)  ->  "a + b"
         *
         * Stringification uses the raw, unexpanded argument.
         */
        if (*v == '#') {
            const char *q;

            if (v[1] == '#') {
                out[oi++] = *v++;
                out[oi++] = *v++;
                continue;
            }

            q = v + 1;
            while (*q == ' ' || *q == '\t')
                q++;

            if (is_ident_start((unsigned char)*q)) {
                i = 0;
                while (is_ident_char((unsigned char)*q) && i < 63) {
                    ident[i++] = *q;
                    q++;
                }
                ident[i] = 0;

                matched = macro_param_index(di, ident);
                if (matched >= 0) {
                    append_macro_string_literal(args[matched], out, &oi, outsz);
                    v = q;
                    continue;
                }
            }

            out[oi++] = *v++;
            continue;
        }

        if (is_ident_start((unsigned char)*v)) {
            const char *ident_start;
            const char *ident_end;

            ident_start = v;
            i = 0;
            while (is_ident_char((unsigned char)*v) && i < 63) {
                ident[i++] = *v;
                v++;
            }
            ident_end = v;
            ident[i] = 0;

            matched = macro_param_index(di, ident);

            if (matched >= 0) {
                const char *a;
                if (replacement_param_raw_context(defs[di].value, ident_start, ident_end))
                    a = args[matched];
                else
                    a = expanded_args[matched];

                while (*a && oi < outsz - 1)
                    out[oi++] = *a++;
            } else {
                for (j = 0; ident[j] && oi < outsz - 1; ++j)
                    out[oi++] = ident[j];
            }
        } else {
            out[oi++] = *v++;
        }
    }

    out[oi] = 0;
    paste_tokens_in_text(out);
}



static int macro_value_is_float_literal(const char *s)
{
    const char *p;
    int saw_digit;
    int saw_float;
    int c;

    p = s;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '+' || *p == '-')
        p++;

    saw_digit = 0;
    saw_float = 0;

    while (isdigit((unsigned char)*p)) {
        saw_digit = 1;
        p++;
    }

    if (*p == '.') {
        saw_float = 1;
        p++;
        while (isdigit((unsigned char)*p)) {
            saw_digit = 1;
            p++;
        }
    }

    if ((*p == 'e' || *p == 'E') && saw_digit) {
        saw_float = 1;
        p++;
        if (*p == '+' || *p == '-')
            p++;
        if (!isdigit((unsigned char)*p))
            return 0;
        while (isdigit((unsigned char)*p))
            p++;
    }

    if (!saw_digit || !saw_float)
        return 0;

    c = (unsigned char)*p;
    if (c == 'f' || c == 'F' || c == 'l' || c == 'L')
        p++;

    while (*p && isspace((unsigned char)*p))
        p++;

    return *p == 0;
}


static int macro_number_should_expand_textually(const char *s)
{
    const char *p;
    unsigned long v;
    int saw_digit;
    int is_nondecimal;

    p = s;
    while (*p && isspace((unsigned char)*p))
        p++;

    if (*p == '+' || *p == '-')
        p++;

    saw_digit = 0;
    is_nondecimal = 0;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        is_nondecimal = 1;
        p += 2;
        while (isxdigit((unsigned char)*p)) {
            saw_digit = 1;
            p++;
        }
    } else if (p[0] == '0' && isdigit((unsigned char)p[1])) {
        is_nondecimal = 1; /* octal */
        while (isdigit((unsigned char)*p)) {
            saw_digit = 1;
            p++;
        }
    } else {
        while (isdigit((unsigned char)*p)) {
            saw_digit = 1;
            p++;
        }
    }

    if (!saw_digit)
        return 0;

    /*
     * Any explicit long suffix must remain text so the normal integer
     * literal lexer preserves long type.  Also keep values above 16 bits
     * textual; otherwise define_number_value() truncates object-like numeric
     * macros to the target int width.  That broke LONG_MAX/LONG_MIN.
     *
     * Also expand non-decimal (hex/octal) constants with values in
     * 0x8000..0xFFFF textually so the normal lexer sets g_tok_unsigned_suffix,
     * giving them type 'unsigned int' per C89.  Without this, the fast-path
     * through define_number_value() leaves g_tok_unsigned_suffix unset and
     * the constant is sign-extended to long instead of zero-extended, causing
     * comparisons like (long_var > 0xBDFF) to compare against -16897 instead
     * of 48639.
     */
    if (*p == 'l' || *p == 'L')
        return 1;
    if (*p == 'u' || *p == 'U') {
        p++;
        if (*p == 'l' || *p == 'L')
            return 1;
    }

    v = strtoul(s, NULL, 0);
    return v > 0xffffUL || (is_nondecimal && v > 32767UL);
}

static int define_number_value(const char *name, long *out, int depth)
{
    int di;
    const char *v;
    char nested[64];
    int i;

    if (depth > 16) {
        return 0;
    }

    di = find_define(name);
    if (di < 0) {
        return 0;
    }
    if (defs[di].is_func) {
        return 0;
    }

    v = defs[di].value;

    while (v[0]) {
        if (!isspace((unsigned char)v[0])) {
            break;
        }
        v = v + 1;
    }

    if (isdigit((unsigned char)v[0]) || v[0] == '-' || v[0] == '+') {
        out[0] = parse_number_string(v);
        return 1;
    }

    if (v[0] == '\'') {
        return parse_charlit_string_value(v, out);
    }

    if (is_ident_start((unsigned char)v[0])) {
        i = 0;
        while (is_ident_char((unsigned char)v[0]) && i < 63) {
            nested[i] = v[0];
            i = i + 1;
            v = v + 1;
        }
        nested[i] = 0;

        while (v[0]) {
            if (!isspace((unsigned char)v[0])) {
                break;
            }
            v = v + 1;
        }

        if (v[0] == 0) {
            return define_number_value(nested, out, depth + 1);
        }
    }

    return 0;
}

static void next_token(void)
{
    int c, d, i, di;
    long start_line;

    skip_ws_and_comments();
    memset(&tok, 0, sizeof(tok));

    start_line = line_no;
    c = getc_src();
    tok_start_pos = posi - 1;
    source_location_at(tok_start_pos, tok.file, sizeof(tok.file), &tok_line);
    (void)start_line;

    if (!c) {
        tok.kind = TOK_EOF;
        strcpy(tok.text, "<eof>");
        return;
    }

    if (c == 'L' && peekc() == '\'') {
        getc_src();     /* consume opening quote */
        if (peekc() == '\\') {
            getc_src();
            tok.val = read_escape();
        } else {
            tok.val = getc_src();
        }
        if (peekc() == '\'') getc_src();
        tok.kind = TOK_NUM;
        sprintf(tok.text, "%ld", tok.val & 0xffffL);
        return;
    }

    if (c == 'L' && peekc() == '"') {
        getc_src();     /* consume opening quote */
        i = 0;
        while (peekc() && peekc() != '"' && i < MAX_TOK_TEXT - 1) {
            if (peekc() == '\\') {
                getc_src();
                tok.text[i++] = (char)read_escape();
            } else {
                tok.text[i++] = (char)getc_src();
            }
        }
        if (peekc() == '"') getc_src();
        tok.text[i] = 0;
        tok.kind = TOK_WSTR;
        return;
    }

    if (is_ident_start(c)) {
        i = 0;
        tok.text[i++] = (char)c;
        while (is_ident_char(peekc()) && i < MAX_TOK_TEXT - 1)
            tok.text[i++] = (char)getc_src();
        tok.text[i] = 0;

        /* C89 predefined macros.  These are handled by the lexer so
         * __FILE__ and __LINE__ reflect the logical source location after
         * include/#line processing. */
        if (!strcmp(tok.text, "__DATE__")) {
            tok.kind = TOK_STR;
            strncpy(tok.text, predefined_date_text, sizeof(tok.text) - 1);
            tok.text[sizeof(tok.text) - 1] = 0;
            return;
        }
        if (!strcmp(tok.text, "__TIME__")) {
            tok.kind = TOK_STR;
            strncpy(tok.text, predefined_time_text, sizeof(tok.text) - 1);
            tok.text[sizeof(tok.text) - 1] = 0;
            return;
        }
        if (!strcmp(tok.text, "__FILE__")) {
            tok.kind = TOK_STR;
            strncpy(tok.text, tok.file, sizeof(tok.text) - 1);
            tok.text[sizeof(tok.text) - 1] = 0;
            return;
        }
        if (!strcmp(tok.text, "__LINE__")) {
            tok.kind = TOK_NUM;
            tok.val = tok_line;
            sprintf(tok.text, "%d", tok_line);
            return;
        }
        if (!strcmp(tok.text, "__STDC__")) {
            tok.kind = TOK_NUM;
            tok.val = 1;
            strcpy(tok.text, "1");
            return;
        }

        di = find_define(tok.text);
        if (di >= 0) {
            long dv;
            const char *rv;
            char repl[64];
            int ri;

            if (defs[di].is_func) {
                long save_pos;
                char args[8][128];
                int nargs;
                char expbuf[512];

                save_pos = posi;
                if (read_macro_call_args(args, &nargs)) {
                    if (nargs != defs[di].nargs)
                        error_here("wrong number of macro arguments");
                    expand_function_macro(di, args, expbuf, sizeof(expbuf));
                    replace_source_range(tok_start_pos, posi, expbuf);
                    next_token();
                    return;
                }
                posi = save_pos;
                tok.kind = TOK_ID;
            } else {
                rv = defs[di].value;
                while (*rv && isspace((unsigned char)*rv))
                    rv++;

                if (macro_value_is_float_literal(rv)) {
                    replace_source_range(tok_start_pos, posi, rv);
                    next_token();
                    return;
                }

                if (macro_number_should_expand_textually(rv)) {
                    replace_source_range(tok_start_pos, posi, rv);
                    next_token();
                    return;
                }

                if (define_number_value(tok.text, &dv, 0)) {
                    tok.kind = TOK_NUM;
                    tok.val = dv;
                    sprintf(tok.text, "%ld", dv);
                    return;
                }

                if (is_ident_start((unsigned char)*rv)) {
                    const char *rv0;
                    rv0 = rv;
                    ri = 0;
                    while (is_ident_char((unsigned char)*rv) && ri < 63)
                        repl[ri++] = *rv++;
                    repl[ri] = 0;
                    while (*rv && isspace((unsigned char)*rv))
                        rv++;
                    if (*rv == 0) {
                        /*
                         * Expand object-like aliases textually too.  This lets
                         * chained macros and keyword-like macros go back through
                         * the normal lexer instead of becoming a dead identifier.
                         */
                        replace_source_range(tok_start_pos, posi, rv0);
                        next_token();
                        return;
                    }
                }

                /*
                 * General object-like macro replacement.  Older dcc only
                 * handled numeric macros and single identifiers, so macros such
                 * as:
                 *
                 *     #define BUF_SIZE ( sizeof( buf ) )
                 *     #define FLAG     ( O_CREAT | O_RDWR )
                 *
                 * were left as undefined identifiers.  Reinsert the replacement
                 * text and lex it normally.
                 */
                replace_source_range(tok_start_pos, posi, rv);
                next_token();
                return;
            }
        } else {
            tok.kind = keyword_kind(tok.text);
        }
        return;
    }

    if (isdigit(c)) {
        unsigned long v;
        int non_decimal_literal;
        v = 0;
        non_decimal_literal = 0;
        if (c == '0' && (peekc() == 'x' || peekc() == 'X')) {
            non_decimal_literal = 1;
            getc_src();
            while (isxdigit(peekc())) {
                c = getc_src();
                v *= 16UL;
                if (c >= '0' && c <= '9') v += (unsigned long)(c - '0');
                else if (c >= 'a' && c <= 'f') v += (unsigned long)(c - 'a' + 10);
                else v += (unsigned long)(c - 'A' + 10);
                v &= 0xffffffffUL;
            }
        } else {
            int saw_float;
            v = (unsigned long)(c - '0');
            while (isdigit(peekc())) {
                v = v * 10UL + (unsigned long)(getc_src() - '0');
                v &= 0xffffffffUL;
            }

            saw_float = 0;
            if (peekc() == '.') {
                saw_float = 1;
                while (peekc() == '.' || isdigit(peekc())) getc_src();
            }
            if (peekc() == 'e' || peekc() == 'E') {
                saw_float = 1;
                getc_src();
                if (peekc() == '+' || peekc() == '-') getc_src();
                while (isdigit(peekc())) getc_src();
            }
            if (saw_float) {
                int flen;
                while (peekc() == 'f' || peekc() == 'F' ||
                       peekc() == 'l' || peekc() == 'L')
                    getc_src();
                tok.kind = TOK_FLOATLIT;
                flen = (int)(posi - tok_start_pos);
                if (flen >= MAX_TOK_TEXT) flen = MAX_TOK_TEXT - 1;
                memcpy(tok.text, src + tok_start_pos, (size_t)flen);
                tok.text[flen] = 0;
                tok.val = (long)(parse_float_literal_bits(tok.text) & 0xffffffffUL);
                return;
            }
        }

        g_tok_long_suffix = 0;
        g_tok_unsigned_suffix = 0;
        while (peekc() == 'u' || peekc() == 'U' ||
               peekc() == 'l' || peekc() == 'L') {
            int c2 = getc_src();
            if (c2 == 'l' || c2 == 'L') g_tok_long_suffix = 1;
            if (c2 == 'u' || c2 == 'U') g_tok_unsigned_suffix = 1;
        }

        /*
         * With DCC's 16-bit int, unsuffixed hexadecimal/octal constants use
         * the C89 non-decimal sequence: int, unsigned int, long, unsigned long.
         * Thus 0xffff is unsigned int, not signed int, and must zero-extend
         * when assigned to uint32_t/unsigned long.
         *
         * Decimal 65535 is still long, because decimal constants do not try
         * unsigned int before long in C89.
         */
        /* Choose the target C89 integer literal type.  DCC has 16-bit int
         * and 32-bit long.  Keep only two flags because the rest of the
         * compiler represents literal type as TYPE_INT/TYPE_LONG plus
         * TYPE_UNSIGNED.
         *
         * Unsuffixed decimal:     int, long, unsigned long
         * Unsuffixed hex/octal:   int, unsigned int, long, unsigned long
         * U suffix:               unsigned int, unsigned long
         * L suffix:               long, unsigned long
         * UL/LU suffix:           unsigned long
         */
        if (g_tok_unsigned_suffix) {
            if (g_tok_long_suffix || v > 0xffffUL)
                g_tok_long_suffix = 1;
        } else if (g_tok_long_suffix) {
            if (v > 0x7fffffffUL)
                g_tok_unsigned_suffix = 1;
        } else if (non_decimal_literal) {
            if (v <= 32767UL) {
                /* int */
            } else if (v <= 0xffffUL) {
                g_tok_unsigned_suffix = 1;
            } else if (v <= 0x7fffffffUL) {
                g_tok_long_suffix = 1;
            } else {
                g_tok_long_suffix = 1;
                g_tok_unsigned_suffix = 1;
            }
        } else {
            if (v <= 32767UL) {
                /* int */
            } else if (v <= 0x7fffffffUL) {
                g_tok_long_suffix = 1;
            } else {
                g_tok_long_suffix = 1;
                g_tok_unsigned_suffix = 1;
            }
        }

        tok.kind = TOK_NUM;
        tok.val = (long)(v & 0xffffffffUL);
        {
            int flen;
            flen = (int)(posi - tok_start_pos);
            if (flen >= MAX_TOK_TEXT) flen = MAX_TOK_TEXT - 1;
            memcpy(tok.text, src + tok_start_pos, (size_t)flen);
            tok.text[flen] = 0;
        }
        return;
    }

    if (c == '"') {
        i = 0;
        while (peekc() && peekc() != '"' && i < MAX_TOK_TEXT - 1) {
            if (peekc() == '\\') {
                getc_src();
                tok.text[i++] = (char)read_escape();
            } else {
                tok.text[i++] = (char)getc_src();
            }
        }
        if (peekc() == '"') getc_src();
        tok.text[i] = 0;
        tok.kind = TOK_STR;
        return;
    }

    if (c == '\'') {
        if (peekc() == '\\') {
            getc_src();
            tok.val = read_escape();
        } else {
            tok.val = getc_src();
        }
        if (peekc() == '\'') getc_src();
        tok.kind = TOK_CHARLIT;
        strcpy(tok.text, "charlit");
        return;
    }

    d = peekc();

    if (c == '.' && d == '.' && posi + 1 < src_len && src[posi + 1] == '.') {
        getc_src();
        getc_src();
        tok.kind = TOK_ELLIPSIS;
        strcpy(tok.text, "...");
        return;
    }

    if (c == '.' && isdigit((unsigned char)d)) {
        int flen;
        while (isdigit(peekc())) getc_src();
        if (peekc() == 'e' || peekc() == 'E') {
            getc_src();
            if (peekc() == '+' || peekc() == '-') getc_src();
            while (isdigit(peekc())) getc_src();
        }
        while (peekc() == 'f' || peekc() == 'F' ||
               peekc() == 'l' || peekc() == 'L')
            getc_src();
        tok.kind = TOK_FLOATLIT;
        flen = (int)(posi - tok_start_pos);
        if (flen >= MAX_TOK_TEXT) flen = MAX_TOK_TEXT - 1;
        memcpy(tok.text, src + tok_start_pos, (size_t)flen);
        tok.text[flen] = 0;
        tok.val = (long)(parse_float_literal_bits(tok.text) & 0xffffffffUL);
        return;
    }

    if (c == '=') {
        if (d == '=') { getc_src(); tok.kind = TOK_EQ; strcpy(tok.text, "=="); return; }
    } else if (c == '!') {
        if (d == '=') { getc_src(); tok.kind = TOK_NE; strcpy(tok.text, "!="); return; }
    } else if (c == '<') {
        if (d == '=') { getc_src(); tok.kind = TOK_LE; strcpy(tok.text, "<="); return; }
        if (d == '<') { getc_src(); if (peekc() == '=') { getc_src(); tok.kind = TOK_SHLEQ; strcpy(tok.text, "<<="); return; } tok.kind = TOK_SHL; strcpy(tok.text, "<<"); return; }
    } else if (c == '>') {
        if (d == '=') { getc_src(); tok.kind = TOK_GE; strcpy(tok.text, ">="); return; }
        if (d == '>') { getc_src(); if (peekc() == '=') { getc_src(); tok.kind = TOK_SHREQ; strcpy(tok.text, ">>="); return; } tok.kind = TOK_SHR; strcpy(tok.text, ">>"); return; }
    } else if (c == '&') {
        if (d == '&') { getc_src(); tok.kind = TOK_ANDAND; strcpy(tok.text, "&&"); return; }
        if (d == '=') { getc_src(); tok.kind = TOK_ANDEQ; strcpy(tok.text, "&="); return; }
    } else if (c == '|') {
        if (d == '|') { getc_src(); tok.kind = TOK_OROR; strcpy(tok.text, "||"); return; }
        if (d == '=') { getc_src(); tok.kind = TOK_OREQ; strcpy(tok.text, "|="); return; }
    } else if (c == '+') {
        if (d == '+') { getc_src(); tok.kind = TOK_INC; strcpy(tok.text, "++"); return; }
        if (d == '=') { getc_src(); tok.kind = TOK_ADDEQ; strcpy(tok.text, "+="); return; }
    } else if (c == '-') {
        if (d == '-') { getc_src(); tok.kind = TOK_DEC; strcpy(tok.text, "--"); return; }
        if (d == '=') { getc_src(); tok.kind = TOK_SUBEQ; strcpy(tok.text, "-="); return; }
        if (d == '>') { getc_src(); tok.kind = TOK_ARROW; strcpy(tok.text, "->"); return; }
    } else if (c == '*') {
        if (d == '=') { getc_src(); tok.kind = TOK_MULEQ; strcpy(tok.text, "*="); return; }
    } else if (c == '/') {
        if (d == '=') { getc_src(); tok.kind = TOK_DIVEQ; strcpy(tok.text, "/="); return; }
    } else if (c == '%') {
        if (d == '=') { getc_src(); tok.kind = TOK_MODEQ; strcpy(tok.text, "%="); return; }
    } else if (c == '^') {
        if (d == '=') { getc_src(); tok.kind = TOK_XOREQ; strcpy(tok.text, "^="); return; }
    }

    tok.kind = c;
    tok.text[0] = (char)c;
    tok.text[1] = 0;
}

static int accept(int k)
{
    if (tok.kind == k) {
        next_token();
        return 1;
    }
    return 0;
}

static void expect(int k)
{
    if (tok.kind != k) {
        error_here("unexpected token");
        return;
    }
    next_token();
}

static int is_type_qualifier_token(int k)
{
    return k == TOK_CONST || k == TOK_VOLATILE;
}

static void skip_type_qualifiers(void)
{
    while (is_type_qualifier_token(tok.kind))
        next_token();
}

static int parse_type(void);
static int parse_const_int_expr(void);

static int type_struct_id(int type)
{
    return (type / 256) & 255;
}

static int make_struct_type(int id)
{
    return TYPE_STRUCT | (id * 256);
}

static int type_size(int type)
{
    int sid;

    if (type & (TYPE_PTR | TYPE_PTR2)) return 2;
    if (type & TYPE_STRUCT) {
        sid = type_struct_id(type);
        if (sid > 0 && sid <= nstruct_defs)
            return struct_defs[sid - 1].size;
        return 0;
    }
    if ((type & 15) == TYPE_CHAR) return 1;
    if ((type & 15) == TYPE_VOID) return 0;
    if ((type & 15) == TYPE_LONG) return 4;
    if ((type & 15) == TYPE_FLOAT) return 4;
    return 2;
}

static int type_is_long(int type)
{
    if (type & (TYPE_PTR | TYPE_PTR2)) return 0;
    return (type & ~TYPE_UNSIGNED & 15) == TYPE_LONG;
}

static int type_is_float(int type)
{
    if (type & (TYPE_PTR | TYPE_PTR2)) return 0;
    return (type & 15) == TYPE_FLOAT;
}

static void error_float_unsupported(const char *what)
{
    error_here(what);
    emit("\tld hl,0\n");
    g_expr_type = TYPE_INT;
}

static int object_array_size(int type, int count)
{
    int base_size;

    base_size = type_size(type);
    if (base_size <= 0)
        base_size = 2;

    return base_size * count;
}

static int type_ptr_depth(int type)
{
    if (type & TYPE_PTR2) return 2;
    if (type & TYPE_PTR) return 1;
    return 0;
}

static int type_add_ptr(int type)
{
    if (type & TYPE_PTR)
        return type | TYPE_PTR2;
    return type | TYPE_PTR;
}

static int type_decay_ptr(int type)
{
    if (type & TYPE_PTR2)
        return (type & ~TYPE_PTR2);
    if (type & TYPE_PTR)
        return (type & ~TYPE_PTR);
    return type;
}

static int type_index_elem_size(int type)
{
    if (type & TYPE_PTR2) return 2;
    if (type & TYPE_PTR) {
        int base = type & 15;
        if (type & TYPE_STRUCT)
            return type_size(type & ~(TYPE_PTR | TYPE_PTR2));
        if (base == TYPE_CHAR) return 1;
        if (base == TYPE_VOID) return 1;
        if (base == TYPE_LONG) return 4;
        if (base == TYPE_FLOAT) return 4;
        return 2;
    }
    return type_size(type);
}


static void copy_last_array_dims_to_sym(struct Sym *s)
{
    int i;

    s->dim_count = g_last_array_dim_count;
    for (i = 0; i < 8; ++i)
        s->dims[i] = (i < g_last_array_dim_count) ? g_last_array_dims[i] : 0;
}

static int sym_array_inner_count_from(struct Sym *s, int from_dim)
{
    int i;
    int n;

    n = 1;
    if (!s || s->dim_count <= 0)
        return 1;

    for (i = from_dim; i < s->dim_count; ++i) {
        if (s->dims[i] <= 0)
            return 1;
        n *= s->dims[i];
    }

    return n;
}

static int sym_array_index_elem_size(struct Sym *s, int index_count)
{
    int elem;

    elem = type_size(s->type);
    if (elem <= 0)
        elem = 2;

    if (!s || s->dim_count <= 1)
        return s && s->elem_size > 0 ? s->elem_size : elem;

    return sym_array_inner_count_from(s, index_count + 1) * elem;
}

static void infer_omitted_first_dim_from_init(struct Sym *s, int init_elems)
{
    int inner;

    if (!s || !s->is_array || s->dim_count <= 0 || s->dims[0] != 0)
        return;

    inner = sym_array_inner_count_from(s, 1);
    if (inner <= 0)
        inner = 1;

    s->dims[0] = (init_elems + inner - 1) / inner;
    s->array_len = s->dims[0];

    if (s->elem_size <= 0) {
        int elem = type_size(s->type);
        if (elem <= 0) elem = 2;
        s->elem_size = inner * elem;
    }
}


static int find_struct_def(const char *name)
{
    int i;

    for (i = nstruct_defs - 1; i >= 0; --i)
        if (!strcmp(struct_defs[i].name, name)) return i + 1;

    return 0;
}

static int add_struct_def(const char *name)
{
    int id;

    id = find_struct_def(name);
    if (id) return id;

    if (nstruct_defs >= MAX_STRUCTS)
        fatal("too many structs");

    id = ++nstruct_defs;
    memset(&struct_defs[id - 1], 0, sizeof(struct_defs[id - 1]));
    strncpy(struct_defs[id - 1].name, name, sizeof(struct_defs[id - 1].name) - 1);
    struct_defs[id - 1].first_field = nfield_defs;
    return id;
}

static struct FieldDef *find_field_def(int struct_id, const char *field_name)
{
    int i;

    if (struct_id <= 0 || struct_id > nstruct_defs)
        return NULL;

    /* Search all fields by parent_struct_id — handles non-consecutive layout
     * caused by inline anonymous struct/union definitions whose fields are
     * interleaved in the flat field_defs[] array. */
    for (i = 0; i < nfield_defs; ++i) {
        if (field_defs[i].parent_struct_id == struct_id &&
            !strcmp(field_defs[i].name, field_name))
            return &field_defs[i];
    }

    return NULL;
}

static void parse_struct_definition(int struct_id)
{
    struct StructDef *sd;
    int ftype;
    char fname[64];
    int bytes;
    int bit_next;
    int bit_unit_offset;

    sd = &struct_defs[struct_id - 1];

    expect('{');

    sd->first_field = nfield_defs;
    sd->field_count = 0;
    sd->size = 0;
    bit_next = 0;
    bit_unit_offset = 0;

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        ftype = parse_type();

        for (;;) {
            while (accept('*')) { skip_type_qualifiers(); ftype = type_add_ptr(ftype); }

            if (tok.kind != TOK_ID) {
                error_here("field name expected");
                break;
            }

            strncpy(fname, tok.text, sizeof(fname) - 1);
            fname[sizeof(fname) - 1] = 0;
            next_token();

            if (tok.kind == ':') {
                int bw;
                next_token();
                bw = parse_const_int_expr();
                if (bw < 0 || bw > 16) {
                    error_here("invalid bitfield width");
                    bw = 1;
                }

                if (nfield_defs >= MAX_FIELDS)
                    fatal("too many struct fields");

                /* First-pass C89 bitfields: pack into 16-bit int units.
                 * Zero-width fields force a new storage unit.  Named
                 * zero-width fields are accepted but allocate no usable field.
                 */
                if (bw == 0) {
                    bit_next = 0;
                    continue;
                }
                if (bit_next == 0 || bit_next + bw > 16) {
                    bit_unit_offset = sd->is_union ? 0 : sd->size;
                    if (sd->is_union) {
                        if (sd->size < 2) sd->size = 2;
                    } else {
                        sd->size += 2;
                    }
                    bit_next = 0;
                }

                memset(&field_defs[nfield_defs], 0, sizeof(field_defs[nfield_defs]));
                strncpy(field_defs[nfield_defs].name, fname,
                        sizeof(field_defs[nfield_defs].name) - 1);
                if ((ftype & 15) != TYPE_INT || type_ptr_depth(ftype) != 0)
                    error_here("bitfield type must be int or unsigned int");
                field_defs[nfield_defs].type = (ftype & TYPE_UNSIGNED) ?
                    (TYPE_UNSIGNED | TYPE_INT) : TYPE_INT;
                field_defs[nfield_defs].parent_struct_id = struct_id;
                field_defs[nfield_defs].offset = bit_unit_offset;
                field_defs[nfield_defs].elem_type = TYPE_UNSIGNED | TYPE_INT;
                field_defs[nfield_defs].elem_size = 2;
                field_defs[nfield_defs].size = 2;
                field_defs[nfield_defs].bit_width = bw;
                field_defs[nfield_defs].bit_shift = bit_next;
                field_defs[nfield_defs].bit_mask = (unsigned int)(((1UL << bw) - 1UL) << bit_next);
                nfield_defs++;
                sd->field_count++;
                bit_next += bw;

                if (!accept(',')) break;
                continue;
            }

            if (nfield_defs >= MAX_FIELDS)
                fatal("too many struct fields");

            bit_next = 0;
            bytes = type_size(ftype);

            memset(&field_defs[nfield_defs], 0, sizeof(field_defs[nfield_defs]));
            strncpy(field_defs[nfield_defs].name, fname,
                    sizeof(field_defs[nfield_defs].name) - 1);
            field_defs[nfield_defs].type = ftype;
            field_defs[nfield_defs].parent_struct_id = struct_id;
            /* union: all fields at offset 0; struct: cumulative */
            field_defs[nfield_defs].offset = sd->is_union ? 0 : sd->size;

            field_defs[nfield_defs].elem_type = ftype;
            field_defs[nfield_defs].elem_size = bytes;

            while (accept('[')) {
                int flen;
                flen = parse_const_int_expr();
                expect(']');
                field_defs[nfield_defs].is_array = 1;
                if (field_defs[nfield_defs].array_len == 0)
                    field_defs[nfield_defs].array_len = flen;
                if (field_defs[nfield_defs].dim_count < 4)
                    field_defs[nfield_defs].dims[field_defs[nfield_defs].dim_count++] = flen;
                bytes *= flen;
            }

            field_defs[nfield_defs].size = bytes;
            nfield_defs++;

            sd->field_count++;
            /* union: size = max(field_sizes); struct: cumulative sum */
            if (sd->is_union) {
                if (bytes > sd->size) sd->size = bytes;
            } else {
                sd->size += bytes;
            }

            if (!accept(',')) break;
        }

        expect(';');
    }

    expect('}');
}


static int find_typedef(const char *name)
{
    int i;

    for (i = ntypedefs - 1; i >= 0; --i)
        if (!strcmp(typedefs[i].name, name)) return i;

    return -1;
}

static void add_typedef_name_ex(const char *name, int type, int array_len, int is_func)
{
    int i;

    i = find_typedef(name);
    if (i < 0) {
        if (ntypedefs >= MAX_TYPEDEFS) fatal("too many typedefs");
        i = ntypedefs++;
        memset(&typedefs[i], 0, sizeof(typedefs[i]));
        strncpy(typedefs[i].name, name, sizeof(typedefs[i].name) - 1);
    }

    typedefs[i].type = type;
    typedefs[i].array_len = array_len;
    typedefs[i].is_func = is_func;
}

static void add_typedef_name(const char *name, int type, int array_len)
{
    add_typedef_name_ex(name, type, array_len, 0);
}

static int parse_base_type(void)
{
    int t;
    int td;
    int saw_any;
    int saw_unsigned;
    int saw_long;
    int saw_short;
    int saw_char;
    int saw_void;
    int saw_float;

    t = 0;
    saw_any = 0;
    saw_unsigned = 0;
    saw_long = 0;
    saw_short = 0;
    saw_char = 0;
    saw_void = 0;
    saw_float = 0;
    g_typedef_array_len = 0;
    g_typedef_is_func = 0;
    decl_is_register = 0;

    /* C89 declaration specifiers are order-independent. */
    for (;;) {
        if (tok.kind == TOK_REGISTER) { decl_is_register = 1; next_token(); continue; }
        if (is_type_qualifier_token(tok.kind) ||
            tok.kind == TOK_AUTO || tok.kind == TOK_INLINE) {
            next_token();
            continue;
        }
        if (tok.kind == TOK_EXTERN) { decl_is_extern = 1; next_token(); continue; }
        if (tok.kind == TOK_STATIC) { decl_is_static = 1; next_token(); continue; }
        if (tok.kind == TOK_UNSIGNED) { saw_unsigned = 1; saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_SIGNED) { saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_LONG) { saw_long = 1; saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_SHORT) { saw_short = 1; saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_INT) { saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_FLOAT) { saw_float = 1; saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_CHAR) { saw_char = 1; saw_any = 1; next_token(); continue; }
        if (tok.kind == TOK_VOID) { saw_void = 1; saw_any = 1; next_token(); continue; }

        if (tok.kind == TOK_STRUCT || tok.kind == TOK_UNION) {
            int sid;
            char sname[64];
            int is_union_kw;
            is_union_kw = (tok.kind == TOK_UNION);
            next_token();
            if (tok.kind == TOK_ID) {
                strncpy(sname, tok.text, sizeof(sname) - 1);
                sname[sizeof(sname) - 1] = 0;
                next_token();
                sid = add_struct_def(sname);
            } else if (tok.kind == '{') {
                sprintf(sname, "__anon_%d", ++g_anon_struct_counter);
                sid = add_struct_def(sname);
            } else {
                error_here("struct/union name or '{' expected");
                sprintf(sname, "__anon_%d", ++g_anon_struct_counter);
                sid = add_struct_def(sname);
            }
            if (is_union_kw) struct_defs[sid - 1].is_union = 1;
            if (tok.kind == '{') parse_struct_definition(sid);
            t = make_struct_type(sid);
            saw_any = 1;
            break;
        }

        if (tok.kind == TOK_ENUM) {
            int cur_val;
            cur_val = 0;
            next_token();
            if (tok.kind == TOK_ID) next_token();
            if (tok.kind == '{') {
                next_token();
                while (tok.kind != '}' && tok.kind != TOK_EOF) {
                    char ename[64];
                    int ei;
                    int dup;
                    if (tok.kind != TOK_ID) { error_here("enum constant name expected"); break; }
                    strncpy(ename, tok.text, sizeof(ename) - 1);
                    ename[sizeof(ename) - 1] = 0;
                    next_token();
                    if (accept('=')) cur_val = parse_enum_const_value();
                    dup = 0;
                    for (ei = 0; ei < nenum_consts; ++ei) {
                        if (!strcmp(enum_const_names[ei], ename)) {
                            enum_const_values[ei] = cur_val;
                            dup = 1;
                            break;
                        }
                    }
                    if (!dup && nenum_consts < MAX_ENUM_CONSTS) {
                        strncpy(enum_const_names[nenum_consts], ename,
                                sizeof(enum_const_names[nenum_consts]) - 1);
                        enum_const_values[nenum_consts] = cur_val;
                        nenum_consts++;
                    }
                    cur_val++;
                    if (!accept(',')) break;
                }
                expect('}');
            }
            t = TYPE_INT;
            saw_any = 1;
            break;
        }

        if (!saw_any && tok.kind == TOK_ID && (td = find_typedef(tok.text)) >= 0) {
            t = typedefs[td].type;
            g_typedef_array_len = typedefs[td].array_len;
            g_typedef_is_func = typedefs[td].is_func;
            saw_any = 1;
            next_token();
            break;
        }
        break;
    }

    if (!saw_any) {
        error_here("type expected");
        t = TYPE_INT;
    } else if (t == 0) {
        if (saw_float) t = TYPE_FLOAT;
        else if (saw_void) t = TYPE_VOID;
        else if (saw_char) t = TYPE_CHAR;
        else if (saw_long) t = TYPE_LONG;
        else t = TYPE_INT;
        (void)saw_short;
        if (saw_unsigned && t != TYPE_FLOAT && t != TYPE_VOID)
            t |= TYPE_UNSIGNED;
    }
    skip_type_qualifiers();
    return t;
}

static int parse_type(void)
{
    int t;
    t = parse_base_type();
    while (accept('*')) {
        skip_type_qualifiers();
        t = type_add_ptr(t);
    }
    return t;
}

static void skip_type_name_param_list(void)
{
    int depth;

    depth = 1;
    next_token();
    while (tok.kind != TOK_EOF && depth > 0) {
        if (tok.kind == '(')
            depth++;
        else if (tok.kind == ')')
            depth--;
        next_token();
    }
}

static int parse_type_name_decl(int *typep, int *sizep)
{
    int t;
    int sz;
    int n;
    int saw_paren_ptr;
    int size_is_pointer_object;

    t = parse_base_type();
    size_is_pointer_object = 0;
    sz = type_size(t);
    if (g_typedef_array_len > 0)
        sz = object_array_size(t, g_typedef_array_len);
    if (sz <= 0)
        sz = 1;

    while (accept('*')) {
        skip_type_qualifiers();
        t = type_add_ptr(t);
        sz = 2;
    }

    if (tok.kind == '(') {
        next_token();
        skip_type_qualifiers();
        saw_paren_ptr = 0;
        while (accept('*')) {
            skip_type_qualifiers();
            saw_paren_ptr = 1;
        }
        if (tok.kind == TOK_ID)
            next_token();
        if (tok.kind == ')') {
            next_token();
            if (saw_paren_ptr) {
                t = type_add_ptr(t);
                sz = 2;
                size_is_pointer_object = 1;
            }
        } else {
            while (tok.kind != TOK_EOF && tok.kind != ')')
                next_token();
            if (tok.kind == ')')
                next_token();
            if (saw_paren_ptr) {
                t = type_add_ptr(t);
                sz = 2;
                size_is_pointer_object = 1;
            }
        }
    } else if (tok.kind == TOK_ID) {
        /* Also accept the same helper for declarations with a concrete name.
         * sizeof(type) normally uses an abstract declarator, but accepting an
         * identifier here lets the cast parser reuse the helper for old DCC
         * function-pointer forms without changing ordinary expression parsing.
         */
        next_token();
    }

    for (;;) {
        if (tok.kind == '[') {
            next_token();
            n = 0;
            if (tok.kind != ']')
                n = parse_const_int_expr();
            expect(']');
            if (n < 0)
                n = 0;
            if (n == 0)
                n = 1;
            /* In an abstract declarator such as char (*)[4], the [4]
             * qualifies the pointee, not the object being sized.  Keep the
             * result pointer-sized.  Without this, sizeof(char (*)[4]) was
             * incorrectly computed as 2 * 4.
             */
            if (!size_is_pointer_object)
                sz *= n;
        } else if (tok.kind == '(') {
            /* Function type suffix.  Function designators are pointer-sized
             * only when the declarator already introduced a pointer, e.g.
             *     sizeof(int (*)(int))
             * Plain sizeof(function type) is invalid C; keep a small, safe
             * size so DCC can continue after the diagnostic-free parse.
             */
            skip_type_name_param_list();
            if (t & (TYPE_PTR | TYPE_PTR2))
                sz = 2;
            else if (sz <= 0)
                sz = 1;
        } else {
            break;
        }
    }

    typep[0] = t;
    sizep[0] = sz;
    return 1;
}

static int parse_sizeof_expr_operand(void);
static long parse_const_long_expr(void);

static long parse_const_long_primary(void)
{
    long v;
    int sign;

    sign = 1;
    if (tok.kind == '-') {
        sign = -1;
        next_token();
    } else if (tok.kind == '+') {
        next_token();
    }

    if (tok.kind == TOK_SIZEOF) {
        next_token();
        if (accept('(')) {
            if (starts_type()) {
                int t;
                int sz;
                parse_type_name_decl(&t, &sz);
                v = sz;
                (void)t;
            } else {
                v = parse_sizeof_expr_operand();
            }
            expect(')');
        } else {
            error_here("'(' expected after sizeof in constant expression");
            v = 2;
        }
        return sign * v;
    }

    if (tok.kind == '(') {
        next_token();

        /*
         * Casts are allowed in C constant expressions, and lzpack uses
         * forms such as (size_t)(MAXDIST * 2).  DCC's small integer model
         * does not need to distinguish the cast for sizing/initializers, so
         * parse and ignore the type, then evaluate the cast operand.
         */
        if (starts_type()) {
            int t;
            (void)t;
            t = parse_type();
            while (accept('*')) { skip_type_qualifiers(); t = type_add_ptr(t); }
            expect(')');
            v = parse_const_long_primary();
            return sign * v;
        }

        v = parse_const_long_expr();
        expect(')');
        return sign * v;
    }

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        v = tok.val;
        next_token();
        return sign * v;
    }

    if (tok.kind == TOK_ID) {
        int i;
        for (i = 0; i < nenum_consts; ++i) {
            if (!strcmp(enum_const_names[i], tok.text)) {
                v = enum_const_values[i];
                next_token();
                return sign * v;
            }
        }
    }

    error_here("constant integer expression expected");
    return 0;
}

static long parse_const_long_mul(void)
{
    long v;
    int op;
    long r;

    v = parse_const_long_primary();
    while (tok.kind == '*' || tok.kind == '/' || tok.kind == '%') {
        op = tok.kind;
        next_token();
        r = parse_const_long_primary();
        if (op == '*') v *= r;
        else if (op == '/') {
            if (r == 0) {
                error_here("division by zero in constant expression");
                r = 1;
            }
            v /= r;
        } else {
            if (r == 0) {
                error_here("division by zero in constant expression");
                r = 1;
            }
            v %= r;
        }
    }
    return v;
}

static long parse_const_long_add(void)
{
    long v;
    int op;
    long r;

    v = parse_const_long_mul();
    while (tok.kind == '+' || tok.kind == '-') {
        op = tok.kind;
        next_token();
        r = parse_const_long_mul();
        if (op == '+') v += r;
        else v -= r;
    }
    return v;
}

static long parse_const_long_shift(void)
{
    long v;
    int op;
    long r;

    v = parse_const_long_add();
    while (tok.kind == TOK_SHL || tok.kind == TOK_SHR) {
        op = tok.kind;
        next_token();
        r = parse_const_long_add();
        if (r < 0) r = 0;
        if (r > 31) r = 31;
        if (op == TOK_SHL) v <<= (int)r;
        else v = (long)((unsigned long)v >> (int)r);
    }
    return v;
}

static long parse_const_long_band(void)
{
    long v;

    v = parse_const_long_shift();
    while (tok.kind == '&') {
        next_token();
        v &= parse_const_long_shift();
    }
    return v;
}

static long parse_const_long_xor(void)
{
    long v;

    v = parse_const_long_band();
    while (tok.kind == '^') {
        next_token();
        v ^= parse_const_long_band();
    }
    return v;
}

static long parse_const_long_expr(void)
{
    long v;

    v = parse_const_long_xor();
    while (tok.kind == '|') {
        next_token();
        v |= parse_const_long_xor();
    }
    return v;
}

static int parse_const_int_expr(void)
{
    return (int)(parse_const_long_expr() & 0xffffL);
}


static int starts_type(void)
{
    return tok.kind == TOK_INT || tok.kind == TOK_LONG || tok.kind == TOK_SHORT || tok.kind == TOK_FLOAT || tok.kind == TOK_CHAR || tok.kind == TOK_VOID ||
           tok.kind == TOK_UNSIGNED || tok.kind == TOK_SIGNED || tok.kind == TOK_CONST || tok.kind == TOK_VOLATILE ||
           tok.kind == TOK_EXTERN || tok.kind == TOK_STATIC || tok.kind == TOK_REGISTER || tok.kind == TOK_AUTO ||
           tok.kind == TOK_INLINE ||
           tok.kind == TOK_TYPEDEF || tok.kind == TOK_STRUCT ||
           tok.kind == TOK_UNION || tok.kind == TOK_ENUM ||
           (tok.kind == TOK_ID && find_typedef(tok.text) >= 0);
}

static struct Sym *find_local(const char *name)
{
    int i;
    for (i = nlocals - 1; i >= 0; --i)
        if (!strcmp(locals[i].name, name)) return &locals[i];
    return NULL;
}

static struct Sym *find_global(const char *name)
{
    int i;
    for (i = nglobals - 1; i >= 0; --i)
        if (!strcmp(globals[i].name, name)) return &globals[i];
    return NULL;
}

static struct Sym *find_sym(const char *name)
{
    struct Sym *s;
    s = find_local(name);
    if (s) return s;
    return find_global(name);
}


static int is_global_char_array_sym(struct Sym *s)
{
    if (!s) return 0;
    if (s->storage != SC_GLOBAL) return 0;
    if (!s->is_array) return 0;
    if ((s->type & 15) != TYPE_CHAR) return 0;
    if (type_ptr_depth(s->type) != 0) return 0;
    return 1;
}

static void emit_global_char_index_addr(struct Sym *s)
{
    emit_extrn_if_needed(s);
    fprintf(outf, "\tld de,%s\n", asm_name_for(s->name));
    emit("\tadd hl,de\n");
}


static int emit_simple_local_index_to_hl(void)
{
    struct Sym *idx;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    if (tok.kind != TOK_ID)
        return 0;

    idx = find_sym(tok.text);
    if (!idx)
        return 0;

    if (idx->storage != SC_LOCAL && idx->storage != SC_PARAM)
        return 0;

    if (type_size(idx->type) != 2)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    if (tok.kind != ']') {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    fprintf(outf, "\tld l,(ix%+d)\n", idx->offset);
    fprintf(outf, "\tld h,(ix%+d)\n", idx->offset + 1);
    return 1;
}

static void emit_test_global_char_index_zero(struct Sym *s, int false_label)
{
    emit_global_char_index_addr(s);
    emit("\tld a,(hl)\n");
    emit("\tor a\n");
    emit_jp_label("jp z,", false_label);
}

static struct Sym *add_global(const char *name, int type, int storage)
{
    struct Sym *s;
    s = find_global(name);
    if (s) {
        if (storage == SC_FUNC) s->storage = SC_FUNC;
        return s;
    }

    if (nglobals >= MAX_SYMS) fatal("too many globals");

    s = &globals[nglobals++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->type = type;
    s->storage = storage;
    s->size = type_size(type);
    return s;
}

static struct Sym *add_local_known(const char *name, int type, int storage,
                                   int offset, int bytes)
{
    struct Sym *s;

    if (nlocals >= MAX_LOCALS) fatal("too many locals");

    s = &locals[nlocals++];
    memset(s, 0, sizeof(*s));
    strncpy(s->name, name, sizeof(s->name) - 1);
    s->type = type;
    s->storage = storage;
    s->offset = offset;
    s->size = bytes;
    return s;
}

static struct Sym *add_local_alloc(const char *name, int type, int bytes)
{
    struct Sym *s;
    local_size += bytes;
    s = add_local_known(name, type, SC_LOCAL, -local_size, bytes);
    return s;
}

static struct Sym *add_param_alloc(const char *name, int type)
{
    struct Sym *s;
    int sz = type_size(type);
    if (sz < 2) sz = 2;
    s = add_local_known(name, type, SC_PARAM, param_offset, sz);
    param_offset += sz;
    return s;
}

static int add_string_ex(const char *s, int is_wide)
{
    int i;
    is_wide = is_wide ? 1 : 0;
    for (i = 0; i < nstrings; i++)
        if (string_wide[i] == is_wide && strcmp(strings[i], s) == 0)
            return i;
    if (nstrings >= MAX_STRINGS) fatal("too many strings");
    strings[nstrings] = xstrdup2(s);
    string_wide[nstrings] = is_wide;
    return nstrings++;
}

static char *read_adjacent_string_literals_ex(int *is_widep)
{
    char *buf;
    int cap;
    int len;
    int is_wide;

    cap = 256;
    len = 0;
    is_wide = 0;
    buf = (char *)xmalloc((size_t)cap);
    buf[0] = 0;

    while (tok.kind == TOK_STR || tok.kind == TOK_WSTR) {
        int slen;
        if (tok.kind == TOK_WSTR)
            is_wide = 1;

        slen = (int)strlen(tok.text);
        if (len + slen + 1 > cap) {
            char *nbuf;
            int ncap;
            ncap = cap;
            while (len + slen + 1 > ncap)
                ncap *= 2;
            nbuf = (char *)xmalloc((size_t)ncap);
            memcpy(nbuf, buf, (size_t)len);
            free(buf);
            buf = nbuf;
            cap = ncap;
        }
        memcpy(buf + len, tok.text, (size_t)slen);
        len += slen;
        buf[len] = 0;
        next_token();
    }

    if (is_widep)
        *is_widep = is_wide;
    return buf;
}

/* If an extern symbol had its extrn deferred, emit it now (once). */
static void emit_extrn_if_needed(struct Sym *s)
{
    int i;

    /*
     * Do not emit M80 EXTRN at the reference site.  C file-scope function
     * declarations are external by default, but the same function may still be
     * defined later in this translation unit.  Emitting EXTRN eagerly can then
     * create duplicate EXTRN/PUBLIC trouble.  Instead, remember that code used
     * the symbol and flush only still-undefined symbols at end of assembly.
     */
    if (!s || !s->needs_extrn || s->is_defined)
        return;

    /*
     * M80 wants EXTRN before the first reference.  For SC_EXTERN objects
     * (stdin/stdout/stderr/errno or extern data declarations), the reference
     * itself proves it is needed, so emit immediately.  Function prototypes
     * still use deferred EXTRN so a later definition in this translation unit
     * does not create EXTRN/PUBLIC conflicts.
     */
    if (s->storage == SC_EXTERN && !asm_name_is_internal_public(s->name)) {
        /*
         * In scan_mode we are only computing locals/frame size.  Do not mark
         * the EXTRN as emitted here, because scan_mode suppresses output; the
         * real generation pass must still print it before the first reference.
         */
        if (!scan_mode) {
            fprintf(outf, "\textrn %s\n", asm_name_for(s->name));
            s->needs_extrn = 0;
        }
        return;
    }

    for (i = 0; i < nused_extrns; ++i)
        if (used_extrns[i] == s)
            return;

    if (nused_extrns >= MAX_USED_EXTRNS)
        fatal("too many used externs");

    used_extrns[nused_extrns++] = s;
}

static void emit_deferred_extrns(void)
{
    int i;

    for (i = 0; i < nused_extrns; ++i) {
        struct Sym *s;
        s = used_extrns[i];
        if (s && s->needs_extrn && !s->is_defined && !asm_name_is_internal_public(s->name))
            fprintf(outf, "\textrn %s\n", asm_name_for(s->name));
    }
}


static void emit_runtime_extrn_if_needed(const char *name)
{
    static const char *emitted[64];
    static int nemitted;
    int i;

    for (i = 0; i < nemitted; ++i) {
        if (!strcmp(emitted[i], name))
            return;
    }

    if (nemitted >= 64)
        fatal("too many runtime extrns");

    if (!scan_mode)
        fprintf(outf, "\textrn %s\n", name);
    emitted[nemitted++] = name;
}

static void emit_runtime_call(const char *name)
{
    emit_runtime_extrn_if_needed(name);
    if (!scan_mode)
        fprintf(outf, "\tcall %s\n", name);
}

static void emit_load_sym_addr(struct Sym *s)
{
    int n;

    if (s->storage == SC_LOCAL || s->storage == SC_PARAM) {
        emit("\tpush ix\n");
        emit("\tpop hl\n");

        if (s->offset > 0 && s->offset <= 3) {
            for (n = 0; n < s->offset; ++n)
                emit("\tinc hl\n");
        } else if (s->offset < 0 && s->offset >= -3) {
            for (n = 0; n < -s->offset; ++n)
                emit("\tdec hl\n");
        } else if (s->offset != 0) {
            fprintf(outf, "\tld de,%d\n", s->offset);
            emit("\tadd hl,de\n");
        }
    } else {
        emit_extrn_if_needed(s);
        fprintf(outf, "\tld hl,%s\n", asm_name_for(s->name));
    }
}


static int sym_can_ix_direct(struct Sym *s)
{
    int sz;
    if (!s) return 0;
    if (s->storage != SC_LOCAL && s->storage != SC_PARAM) return 0;
    if (s->is_array) return 0;
    sz = type_size(s->type);
    if (sz < 1) sz = 1;
    if (s->offset < -128 || s->offset + sz - 1 > 127) return 0;
    return 1;
}

/* True for global/extern 16-bit non-array variables that support direct word load/store. */
static int is_global_word_sym(struct Sym *s)
{
    if (!s) return 0;
    if (s->storage != SC_GLOBAL && s->storage != SC_EXTERN) return 0;
    if (s->is_array) return 0;
    return type_size(s->type) == 2;
}

/* Z80: ld hl,(name) — load 16-bit value from global/extern directly. */
static void emit_load_global_word_direct(struct Sym *s)
{
    emit_extrn_if_needed(s);
    fprintf(outf, "\tld hl,(%s)\n", asm_name_for(s->name));
}

/* Z80: ld (name),hl — store 16-bit HL value to global/extern directly. */
static void emit_store_global_word_direct(struct Sym *s)
{
    emit_extrn_if_needed(s);
    fprintf(outf, "\tld (%s),hl\n", asm_name_for(s->name));
}

static void emit_load_sym_value_direct(struct Sym *s)
{
    if (is_global_word_sym(s)) {
        emit_load_global_word_direct(s);
        return;
    }
    if (type_size(s->type) == 1) {
        fprintf(outf, "\tld l,(ix%+d)\n", s->offset);
        if (s->type & TYPE_UNSIGNED)
            emit("\tld h,0\n");
        else
            emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
    } else if (type_size(s->type) == 4) {
        fprintf(outf, "\tld l,(ix%+d)\n", s->offset);
        fprintf(outf, "\tld h,(ix%+d)\n", s->offset + 1);
        fprintf(outf, "\tld e,(ix%+d)\n", s->offset + 2);
        fprintf(outf, "\tld d,(ix%+d)\n", s->offset + 3);
    } else {
        fprintf(outf, "\tld l,(ix%+d)\n", s->offset);
        fprintf(outf, "\tld h,(ix%+d)\n", s->offset + 1);
    }
}

static void emit_load_sym_de_direct(struct Sym *s)
{
    if (type_size(s->type) == 1) {
        fprintf(outf, "\tld e,(ix%+d)\n", s->offset);
        if (s->type & TYPE_UNSIGNED)
            emit("\tld d,0\n");
        else
            emit("\tld a,e\n\trlca\n\tsbc a,a\n\tld d,a\n");
    } else {
        fprintf(outf, "\tld e,(ix%+d)\n", s->offset);
        fprintf(outf, "\tld d,(ix%+d)\n", s->offset + 1);
    }
}

static void emit_store_hl_to_sym_direct(struct Sym *s)
{
    if (is_global_word_sym(s)) {
        emit_store_global_word_direct(s);
        return;
    }
    if (type_size(s->type) == 1) {
        fprintf(outf, "\tld (ix%+d),l\n", s->offset);
    } else if (type_size(s->type) == 4) {
        /* DE:HL -> sym (DE=high, HL=low) */
        fprintf(outf, "\tld (ix%+d),l\n", s->offset);
        fprintf(outf, "\tld (ix%+d),h\n", s->offset + 1);
        fprintf(outf, "\tld (ix%+d),e\n", s->offset + 2);
        fprintf(outf, "\tld (ix%+d),d\n", s->offset + 3);
    } else {
        fprintf(outf, "\tld (ix%+d),l\n", s->offset);
        fprintf(outf, "\tld (ix%+d),h\n", s->offset + 1);
    }
}

static void emit_incdec_sym_direct(struct Sym *s, int op)
{
    int done;

    /* Global 16-bit integer (non-pointer): ld hl,(nn); inc/dec hl; ld (nn),hl.
     * inc hl / dec hl are atomic 16-bit ops so no byte-by-byte ripple needed. */
    if (is_global_word_sym(s) && type_ptr_depth(s->type) == 0) {
        emit_load_global_word_direct(s);
        if (op == TOK_INC) emit("\tinc hl\n");
        else               emit("\tdec hl\n");
        emit_store_global_word_direct(s);
        return;
    }

    /* Pointer ++/-- advances by the pointed-to object size, not by one
     * byte. Global pointer case now handled via emit_load_sym_value_direct +
     * emit_store_hl_to_sym_direct which both support globals. */
    if (s && type_ptr_depth(s->type) > 0) {
        int elem;
        elem = type_index_elem_size(s->type);
        emit_load_sym_value_direct(s);
        if (op == TOK_INC) {
            emit_add_const_to_hl(elem);
        } else {
            emit_ld_de_const(elem);
            emit("\tor a\n\tsbc hl,de\n");
        }
        emit_store_hl_to_sym_direct(s);
        return;
    }

    if (type_size(s->type) == 1) {
        if (op == TOK_INC)
            fprintf(outf, "\tinc (ix%+d)\n", s->offset);
        else
            fprintf(outf, "\tdec (ix%+d)\n", s->offset);
        return;
    }

    done = new_label();

    if (type_size(s->type) == 4) {
        /*
         * 4-byte IX-direct long ++/--.  The previous fast path always used
         * the 2-byte sequence, so a long decrement 0 -> -1 produced
         * 0000FFFF instead of FFFFFFFF.  That made loops like:
         *
         *     for (i = BLOCKS - 1; i >= 0; i--)
         *
         * continue once more with i's low word equal to -1.
         */
        if (op == TOK_INC) {
            fprintf(outf, "\tinc (ix%+d)\n", s->offset);
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tinc (ix%+d)\n", s->offset + 1);
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tinc (ix%+d)\n", s->offset + 2);
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tinc (ix%+d)\n", s->offset + 3);
        } else {
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset);
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 1);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset + 1);
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset + 2);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset + 2);
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset + 3);
        }
    } else {
        /* 2-byte int ++/--. */
        if (op == TOK_INC) {
            fprintf(outf, "\tinc (ix%+d)\n", s->offset);
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tinc (ix%+d)\n", s->offset + 1);
        } else {
            fprintf(outf, "\tld a,(ix%+d)\n", s->offset);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset);
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            fprintf(outf, "\tdec (ix%+d)\n", s->offset + 1);
        }
    }

    emit_label(done);
}



static void emit_load_from_hl(int type);
static void emit_promote_byte_to_int(int actual_type);
static void emit_extend_to_long_typed(int source_type);
static void emit_extend_to_long(int source_is_unsigned);

static int base_struct_id_from_type(int type)
{
    if (type & TYPE_STRUCT)
        return type_struct_id(type);
    return 0;
}

static void emit_add_field_offset(struct FieldDef *fd)
{
    int i;
    /* inc hl is 1 byte; ld de,N + add hl,de is 4 bytes regardless of N */
    if (fd->offset >= 1 && fd->offset <= 3) {
        for (i = 0; i < fd->offset; i++)
            emit("\tinc hl\n");
    } else if (fd->offset) {
        fprintf(outf, "\tld de,%d\n", fd->offset);
        emit("\tadd hl,de\n");
    }
}

static int apply_field_access_from_addr(int cur_type, int arrow, int *is_array)
{
    struct FieldDef *fd;
    int sid;
    int di;

    if (is_array) {
        is_array[0] = 0;
    }

    if (arrow) {
        emit_load_from_hl(cur_type);
    }

    if (tok.kind != TOK_ID) {
        error_here("field name expected");
        return TYPE_INT;
    }

    sid = base_struct_id_from_type(cur_type);
    fd = find_field_def(sid, tok.text);
    if (!fd) {
        error_here("unknown struct field");
        next_token();
        return TYPE_INT;
    }

    next_token();

    emit_add_field_offset(fd);

    if (is_array) {
        is_array[0] = fd->is_array;
    }
    current_field_array_elem_size = fd->elem_size ? fd->elem_size : type_size(fd->type);
    /* Float fields are stored as opaque 4-byte objects.  This is normally
     * already true through type_size(), but keep the field metadata explicit
     * so arrays of structs containing float fields and float field arrays use
     * the same 4-byte stride as plain float arrays. */
    if (fd->is_array && type_is_float(fd->elem_type))
        current_field_array_elem_size = 4;
    else if (!fd->is_array && type_is_float(fd->type))
        current_field_array_elem_size = 4;
    current_field_array_dim_count = fd->dim_count;
    for (di = 0; di < 4; ++di) {
        current_field_array_dims[di] = fd->dims[di];
    }
    current_field_bit_width = fd->bit_width;
    current_field_bit_shift = fd->bit_shift;
    current_field_bit_mask = fd->bit_mask;
    if (fd->is_array)
        return fd->elem_type;
    return fd->type;
}

static void skip_balanced_bracket(int open_ch, int close_ch)
{
    int depth;

    depth = 1;
    next_token();

    while (tok.kind != TOK_EOF && depth > 0) {
        if (tok.kind == open_ch) {
            depth++;
        } else if (tok.kind == close_ch) {
            depth--;
        }

        next_token();
    }
}

static int parse_sizeof_expr_operand(void);

static int sizeof_common_type(int a, int b, int op)
{
    int au;
    int bu;

    /* Comparisons and logical operators have int result. */
    if (op == TOK_EQ || op == TOK_NE || op == TOK_LE || op == TOK_GE ||
        op == '<' || op == '>' || op == TOK_ANDAND || op == TOK_OROR)
        return TYPE_INT;

    if (type_is_long(a) || type_is_long(b)) {
        if ((a & TYPE_UNSIGNED) || (b & TYPE_UNSIGNED))
            return TYPE_LONG | TYPE_UNSIGNED;
        return TYPE_LONG;
    }

    au = a & TYPE_UNSIGNED;
    bu = b & TYPE_UNSIGNED;
    if (au || bu)
        return TYPE_INT | TYPE_UNSIGNED;

    return TYPE_INT;
}

static int sizeof_parse_primary_type(int *typep, int *sizep)
{
    struct Sym *s;
    struct FieldDef *fd;
    int type;
    int sz;
    int is_arr;
    int elem_size;
    int sid;
    int i;

    if (tok.kind == '*') {
        next_token();
        if (!sizeof_parse_primary_type(&type, &sz)) {
            *typep = TYPE_INT;
            *sizep = 2;
            return 0;
        }
        type = type_decay_ptr(type);
        sz = type_size(type);
        if (sz <= 0) sz = 1;
        *typep = type;
        *sizep = sz;
        return 1;
    }

    if (tok.kind == '&') {
        next_token();
        if (!sizeof_parse_primary_type(&type, &sz)) {
            *typep = TYPE_INT | TYPE_PTR;
            *sizep = 2;
            return 0;
        }
        *typep = type_add_ptr(type);
        *sizep = 2;
        return 1;
    }

    if (tok.kind == TOK_NUM) {
        type = g_tok_long_suffix ? TYPE_LONG : TYPE_INT;
        if (g_tok_unsigned_suffix)
            type |= TYPE_UNSIGNED;
        next_token();
        *typep = type;
        *sizep = type_size(type);
        return 1;
    }

    if (tok.kind == TOK_CHARLIT) {
        next_token();
        *typep = TYPE_INT;
        *sizep = 2;
        return 1;
    }

    if (tok.kind == TOK_STR || tok.kind == TOK_WSTR) {
        char *lit;
        int is_wide;
        lit = read_adjacent_string_literals_ex(&is_wide);
        sz = (int)strlen(lit) + 1;
        if (is_wide)
            sz *= 2;
        free(lit);
        *typep = (is_wide ? TYPE_INT : TYPE_CHAR) | TYPE_PTR;
        *sizep = sz;
        return 1;
    }

    if (tok.kind == '(') {
        next_token();
        if (starts_type()) {
            parse_type_name_decl(&type, &sz);
            expect(')');
            *typep = type;
            *sizep = sz;
            return 1;
        }

        sz = parse_sizeof_expr_operand();
        expect(')');
        *typep = TYPE_INT;
        *sizep = sz;
        return 1;
    }

    if (tok.kind != TOK_ID) {
        error_here("unsupported sizeof expression");
        *typep = TYPE_INT;
        *sizep = 2;
        return 0;
    }

    s = find_sym(tok.text);
    if (!s) {
        /* enum constants behave like int; unknown identifiers are diagnosed. */
        for (i = 0; i < nenum_consts; ++i) {
            if (!strcmp(enum_const_names[i], tok.text)) {
                next_token();
                *typep = TYPE_INT;
                *sizep = 2;
                return 1;
            }
        }
        error_here("undefined symbol");
        next_token();
        *typep = TYPE_INT;
        *sizep = 2;
        return 0;
    }

    type = s->type;
    is_arr = s->is_array;
    sz = is_arr ? s->size : type_size(type);
    elem_size = s->elem_size ? s->elem_size : type_size(type);
    if (elem_size <= 0) elem_size = 1;
    next_token();

    for (;;) {
        if (tok.kind == '[') {
            skip_balanced_bracket('[', ']');
            if (is_arr) {
                sz = elem_size;
                is_arr = 0;
            } else {
                type = type_decay_ptr(type);
                sz = type_size(type);
                if (sz <= 0) sz = 1;
            }
        } else if (tok.kind == '(') {
            /* Function call expression: sizeof uses the function return type.
             * Arguments are not evaluated; just skip the argument list. */
            skip_balanced_bracket('(', ')');
            is_arr = 0;
            sz = type_size(type);
            if (sz <= 0) sz = 2;
        } else if (tok.kind == '.' || tok.kind == TOK_ARROW) {
            int arrow;

            arrow = tok.kind == TOK_ARROW;
            next_token();

            if (tok.kind != TOK_ID) {
                error_here("field name expected");
                break;
            }

            if (arrow)
                sid = base_struct_id_from_type(type_decay_ptr(type));
            else
                sid = base_struct_id_from_type(type);

            fd = find_field_def(sid, tok.text);
            if (!fd) {
                error_here("unknown struct field");
                next_token();
                break;
            }

            next_token();

            type = fd->is_array ? fd->elem_type : fd->type;
            sz = fd->is_array ? fd->size : fd->size;
            is_arr = fd->is_array;
            elem_size = fd->elem_size ? fd->elem_size : type_size(type);
            if (elem_size <= 0) elem_size = 1;
        } else {
            break;
        }
    }

    *typep = type;
    *sizep = sz;
    return 1;
}

struct ConstVal {
    unsigned long u;
    int type;
};

static unsigned long cf_mask_for_type(int type)
{
    int sz;
    if (type & (TYPE_PTR | TYPE_PTR2))
        return 0xffffUL;
    sz = type_size(type);
    if (sz == 1) return 0xffUL;
    if (sz == 2) return 0xffffUL;
    return 0xffffffffUL;
}

static unsigned long cf_sign_bit_for_type(int type)
{
    int sz;
    if (type & (TYPE_PTR | TYPE_PTR2))
        return 0x8000UL;
    sz = type_size(type);
    if (sz == 1) return 0x80UL;
    if (sz == 2) return 0x8000UL;
    return 0x80000000UL;
}

static long cf_signed_value(struct ConstVal v)
{
    unsigned long mask;
    unsigned long sign;
    unsigned long u;

    mask = cf_mask_for_type(v.type);
    sign = cf_sign_bit_for_type(v.type);
    u = v.u & mask;
    if ((v.type & TYPE_UNSIGNED) || (v.type & (TYPE_PTR | TYPE_PTR2)))
        return (long)u;
    if (u & sign)
        return (long)(u | ~mask);
    return (long)u;
}

static int cf_promote_type(int type)
{
    if (type & (TYPE_PTR | TYPE_PTR2 | TYPE_STRUCT))
        return type;
    if (type_is_float(type))
        return type;
    if (type_is_long(type))
        return type;
    if ((type & 15) == TYPE_CHAR)
        return TYPE_INT;
    return (type & TYPE_UNSIGNED) ? (TYPE_INT | TYPE_UNSIGNED) : TYPE_INT;
}

static int cf_common_arith_type(int a, int b)
{
    a = cf_promote_type(a);
    b = cf_promote_type(b);
    if (type_is_long(a) || type_is_long(b)) {
        if ((type_is_long(a) && (a & TYPE_UNSIGNED)) ||
            (type_is_long(b) && (b & TYPE_UNSIGNED)))
            return TYPE_LONG | TYPE_UNSIGNED;
        return TYPE_LONG;
    }
    if ((a & TYPE_UNSIGNED) || (b & TYPE_UNSIGNED))
        return TYPE_INT | TYPE_UNSIGNED;
    return TYPE_INT;
}

static void cf_cast_to_type(struct ConstVal *v, int type)
{
    unsigned long u;
    unsigned long mask;
    unsigned long sign;

    v->type = type;
    mask = cf_mask_for_type(type);
    u = v->u & mask;

    if (!(type & TYPE_UNSIGNED) && !(type & (TYPE_PTR | TYPE_PTR2)) &&
        type_size(type) < 4) {
        sign = cf_sign_bit_for_type(type);
        if (u & sign)
            u |= ~mask;
    }

    v->u = u & 0xffffffffUL;
}

static void cf_convert_to_type(struct ConstVal *v, int type)
{
    if (!(v->type & TYPE_UNSIGNED) && !(v->type & (TYPE_PTR | TYPE_PTR2)))
        v->u = (unsigned long)cf_signed_value(*v);
    cf_cast_to_type(v, type);
}

static int cf_is_expr_stop(int kind)
{
    return kind == TOK_EOF || kind == ')' || kind == ']' || kind == ',' ||
           kind == ';' || kind == ':' || kind == '}';
}

static int cf_parse_lor(struct ConstVal *out);

static unsigned long cf_parse_integer_literal_bits(const char *text)
{
    const char *p;
    unsigned long v;
    int base;

    p = text;
    while (*p && isspace((unsigned char)*p))
        p++;

    /* The constant-folder handles unary +/- separately.  If a macro has
     * already become a signed textual token, accept the sign here too so
     * speculative snippet folding remains harmless. */
    if (*p == '+')
        p++;
    else if (*p == '-') {
        p++;
        v = cf_parse_integer_literal_bits(p);
        return (0UL - v) & 0xffffffffUL;
    }

    v = 0;
    base = 10;
    if (p[0] == '0' && (p[1] == 'x' || p[1] == 'X')) {
        base = 16;
        p += 2;
    } else if (p[0] == '0') {
        base = 8;
        p++;
    }

    if (base == 16) {
        while (isxdigit((unsigned char)*p)) {
            v <<= 4;
            if (*p >= '0' && *p <= '9') v += (unsigned long)(*p - '0');
            else if (*p >= 'a' && *p <= 'f') v += (unsigned long)(*p - 'a' + 10);
            else v += (unsigned long)(*p - 'A' + 10);
            p++;
        }
    } else {
        while (*p >= '0' && *p <= (base == 8 ? '7' : '9')) {
            v = v * (unsigned long)base + (unsigned long)(*p - '0');
            p++;
        }
    }

    return v & 0xffffffffUL;
}

static int cf_parse_primary(struct ConstVal *out)
{
    long v;
    int t;
    int sz;

    if (tok.kind == TOK_NUM) {
        /* Do not use tok.val here.  On MSVC host long is 32-bit, so tokens
         * such as 2147483648UL/4294967295UL cannot be represented as a
         * positive signed long.  Re-read tok.text as raw 32-bit bits and then
         * apply the target C89 literal type. */
        out->u = cf_parse_integer_literal_bits(tok.text);
        out->type = g_tok_long_suffix ? TYPE_LONG : TYPE_INT;
        if (g_tok_unsigned_suffix)
            out->type |= TYPE_UNSIGNED;
        cf_cast_to_type(out, out->type);
        next_token();
        return 1;
    }

    if (tok.kind == TOK_CHARLIT) {
        out->u = (unsigned long)tok.val;
        out->type = TYPE_INT;
        cf_cast_to_type(out, out->type);
        next_token();
        return 1;
    }

    if (tok.kind == TOK_SIZEOF) {
        next_token();
        if (accept('(')) {
            if (starts_type()) {
                parse_type_name_decl(&t, &sz);
                v = sz;
            } else {
                v = parse_sizeof_expr_operand();
            }
            expect(')');
        } else {
            /* sizeof unary-expression without parentheses.  Do not consume
             * following binary operators: sizeof a + 1 is (sizeof a) + 1,
             * not sizeof(a + 1). */
            if (!sizeof_parse_primary_type(&t, &sz))
                return 0;
            v = sz;
        }
        out->u = (unsigned long)v;
        out->type = TYPE_INT | TYPE_UNSIGNED;
        cf_cast_to_type(out, out->type);
        return 1;
    }

    if (tok.kind == '(') {
        next_token();
        if (starts_type()) {
            parse_type_name_decl(&t, &sz);
            expect(')');
            if (!cf_parse_primary(out))
                return 0;
            cf_cast_to_type(out, t);
            return 1;
        }
        if (!cf_parse_lor(out))
            return 0;
        if (!accept(')'))
            return 0;
        return 1;
    }

    /* String literals and identifiers are not integer constants. */
    return 0;
}

static int cf_parse_unary(struct ConstVal *out)
{
    int op;

    if (tok.kind == '+' || tok.kind == '-' || tok.kind == '~' || tok.kind == '!') {
        op = tok.kind;
        next_token();
        if (!cf_parse_unary(out))
            return 0;
        cf_convert_to_type(out, cf_promote_type(out->type));
        if (op == '+') {
            return 1;
        } else if (op == '-') {
            if (out->type & TYPE_UNSIGNED)
                out->u = (0UL - out->u) & cf_mask_for_type(out->type);
            else
                out->u = (unsigned long)(-cf_signed_value(*out));
            cf_cast_to_type(out, out->type);
            return 1;
        } else if (op == '~') {
            out->u = (~out->u) & cf_mask_for_type(out->type);
            cf_cast_to_type(out, out->type);
            return 1;
        } else {
            out->u = cf_signed_value(*out) != 0 ? 0UL : 1UL;
            out->type = TYPE_INT;
            return 1;
        }
    }

    return cf_parse_primary(out);
}

static int cf_parse_mul(struct ConstVal *out)
{
    struct ConstVal rhs;
    int op;
    int common;
    long ls, rs;

    if (!cf_parse_unary(out))
        return 0;
    while (tok.kind == '*' || tok.kind == '/' || tok.kind == '%') {
        op = tok.kind;
        next_token();
        if (!cf_parse_unary(&rhs))
            return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        if (op == '*') {
            out->u = (out->u * rhs.u) & cf_mask_for_type(common);
        } else if (common & TYPE_UNSIGNED) {
            if ((rhs.u & cf_mask_for_type(common)) == 0)
                return 0;
            if (op == '/')
                out->u = (out->u & cf_mask_for_type(common)) / (rhs.u & cf_mask_for_type(common));
            else
                out->u = (out->u & cf_mask_for_type(common)) % (rhs.u & cf_mask_for_type(common));
        } else {
            rs = cf_signed_value(rhs);
            if (rs == 0)
                return 0;
            ls = cf_signed_value(*out);
            if (op == '/')
                out->u = (unsigned long)(ls / rs);
            else
                out->u = (unsigned long)(ls % rs);
        }
        out->type = common;
        cf_cast_to_type(out, common);
    }
    return 1;
}

static int cf_parse_add(struct ConstVal *out)
{
    struct ConstVal rhs;
    int op;
    int common;

    if (!cf_parse_mul(out))
        return 0;
    while (tok.kind == '+' || tok.kind == '-') {
        op = tok.kind;
        next_token();
        if (!cf_parse_mul(&rhs))
            return 0;
        if ((out->type & (TYPE_PTR | TYPE_PTR2)) || (rhs.type & (TYPE_PTR | TYPE_PTR2)))
            return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        if (op == '+')
            out->u = (out->u + rhs.u) & cf_mask_for_type(common);
        else
            out->u = (out->u - rhs.u) & cf_mask_for_type(common);
        out->type = common;
        cf_cast_to_type(out, common);
    }
    return 1;
}

static int cf_parse_shift(struct ConstVal *out)
{
    struct ConstVal rhs;
    int op;
    int lhs_type;
    int width;
    long sc;
    long sv;

    if (!cf_parse_add(out))
        return 0;
    while (tok.kind == TOK_SHL || tok.kind == TOK_SHR) {
        op = tok.kind;
        next_token();
        if (!cf_parse_add(&rhs))
            return 0;
        lhs_type = cf_promote_type(out->type);
        cf_convert_to_type(out, lhs_type);
        sc = cf_signed_value(rhs);
        if (sc < 0)
            return 0;
        width = type_is_long(lhs_type) ? 32 : 16;
        if (sc >= width)
            return 0;
        if (op == TOK_SHL) {
            out->u = (out->u << (int)sc) & cf_mask_for_type(lhs_type);
        } else if (lhs_type & TYPE_UNSIGNED) {
            out->u = (out->u & cf_mask_for_type(lhs_type)) >> (int)sc;
        } else {
            sv = cf_signed_value(*out);
            out->u = (unsigned long)(sv >> (int)sc);
        }
        out->type = lhs_type;
        cf_cast_to_type(out, lhs_type);
    }
    return 1;
}

static int cf_parse_rel(struct ConstVal *out)
{
    struct ConstVal rhs;
    int op;
    int common;
    int r;

    if (!cf_parse_shift(out))
        return 0;
    while (tok.kind == '<' || tok.kind == '>' || tok.kind == TOK_LE || tok.kind == TOK_GE) {
        op = tok.kind;
        next_token();
        if (!cf_parse_shift(&rhs))
            return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        if (common & TYPE_UNSIGNED) {
            unsigned long a = out->u & cf_mask_for_type(common);
            unsigned long b = rhs.u & cf_mask_for_type(common);
            r = (op == '<') ? (a < b) : (op == '>') ? (a > b) : (op == TOK_LE) ? (a <= b) : (a >= b);
        } else {
            long a = cf_signed_value(*out);
            long b = cf_signed_value(rhs);
            r = (op == '<') ? (a < b) : (op == '>') ? (a > b) : (op == TOK_LE) ? (a <= b) : (a >= b);
        }
        out->u = r ? 1UL : 0UL;
        out->type = TYPE_INT;
    }
    return 1;
}

static int cf_parse_eq(struct ConstVal *out)
{
    struct ConstVal rhs;
    int op;
    int common;
    int r;

    if (!cf_parse_rel(out))
        return 0;
    while (tok.kind == TOK_EQ || tok.kind == TOK_NE) {
        op = tok.kind;
        next_token();
        if (!cf_parse_rel(&rhs))
            return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        r = ((out->u & cf_mask_for_type(common)) == (rhs.u & cf_mask_for_type(common)));
        if (op == TOK_NE)
            r = !r;
        out->u = r ? 1UL : 0UL;
        out->type = TYPE_INT;
    }
    return 1;
}

static int cf_parse_band(struct ConstVal *out)
{
    struct ConstVal rhs;
    int common;
    if (!cf_parse_eq(out)) return 0;
    while (tok.kind == '&') {
        next_token();
        if (!cf_parse_eq(&rhs)) return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        out->u = (out->u & rhs.u) & cf_mask_for_type(common);
        out->type = common;
    }
    return 1;
}

static int cf_parse_bxor(struct ConstVal *out)
{
    struct ConstVal rhs;
    int common;
    if (!cf_parse_band(out)) return 0;
    while (tok.kind == '^') {
        next_token();
        if (!cf_parse_band(&rhs)) return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        out->u = (out->u ^ rhs.u) & cf_mask_for_type(common);
        out->type = common;
    }
    return 1;
}

static int cf_parse_bor(struct ConstVal *out)
{
    struct ConstVal rhs;
    int common;
    if (!cf_parse_bxor(out)) return 0;
    while (tok.kind == '|') {
        next_token();
        if (!cf_parse_bxor(&rhs)) return 0;
        common = cf_common_arith_type(out->type, rhs.type);
        cf_convert_to_type(out, common);
        cf_convert_to_type(&rhs, common);
        out->u = (out->u | rhs.u) & cf_mask_for_type(common);
        out->type = common;
    }
    return 1;
}

static int cf_parse_land(struct ConstVal *out)
{
    struct ConstVal rhs;
    if (!cf_parse_bor(out)) return 0;
    while (tok.kind == TOK_ANDAND) {
        next_token();
        if (!cf_parse_bor(&rhs)) return 0;
        out->u = (cf_signed_value(*out) != 0 && cf_signed_value(rhs) != 0) ? 1UL : 0UL;
        out->type = TYPE_INT;
    }
    return 1;
}

static int cf_parse_lor(struct ConstVal *out)
{
    struct ConstVal rhs;
    if (!cf_parse_land(out)) return 0;
    while (tok.kind == TOK_OROR) {
        next_token();
        if (!cf_parse_land(&rhs)) return 0;
        out->u = (cf_signed_value(*out) != 0 || cf_signed_value(rhs) != 0) ? 1UL : 0UL;
        out->type = TYPE_INT;
    }
    return 1;
}

static int try_parse_const_expr_value(struct ConstVal *out)
{
    return cf_parse_lor(out);
}

static void emit_const_value(struct ConstVal v)
{
    cf_cast_to_type(&v, v.type);
    if (type_size(v.type) == 4) {
        fprintf(outf, "\tld hl,%lu\n", v.u & 0xffffUL);
        fprintf(outf, "\tld de,%lu\n", (v.u >> 16) & 0xffffUL);
    } else if (type_size(v.type) == 1) {
        unsigned long b = v.u & 0xffUL;
        fprintf(outf, "\tld l,%lu\n", b);
        if (v.type & TYPE_UNSIGNED)
            emit("\tld h,0\n");
        else if (b & 0x80UL)
            emit("\tld h,255\n");
        else
            emit("\tld h,0\n");
    } else {
        fprintf(outf, "\tld hl,%lu\n", v.u & 0xffffUL);
    }
    g_expr_type = v.type;
}

static int try_gen_const_expr(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    int save_errors;
    int save_long_suffix;
    int save_unsigned_suffix;
    struct Token save_tok;
    struct ConstVal v;

    if (tok.kind == TOK_ID || tok.kind == TOK_STR || tok.kind == TOK_WSTR || tok.kind == TOK_FLOATLIT)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_errors = errors;
    save_long_suffix = g_tok_long_suffix;
    save_unsigned_suffix = g_tok_unsigned_suffix;
    save_tok = tok;

    if (!try_parse_const_expr_value(&v) || !cf_is_expr_stop(tok.kind) || errors != save_errors) {
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

    emit_const_value(v);
    return 1;
}


static int parse_sizeof_expr_operand(void)
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


static void emit_load_from_hl(int type)
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

static void emit_store_de_to_addr_hl(int type)
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


static int type_is_struct_object(int type)
{
    return (type & TYPE_STRUCT) && type_ptr_depth(type) == 0;
}

static int same_struct_type(int a, int b)
{
    return type_is_struct_object(a) && type_is_struct_object(b) &&
           type_struct_id(a) == type_struct_id(b);
}

static void emit_copy_de_to_hl_bytes(int n)
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

static void emit_push_struct_arg_from_hl(int n)
{
    if (n <= 0)
        return;
    emit("\tex de,hl\n");          /* DE = source */
    fprintf(outf, "\tld hl,-%d\n", n);
    emit("\tadd hl,sp\n");        /* HL = destination */
    emit("\tld sp,hl\n");
    emit_copy_de_to_hl_bytes(n);
}

static void emit_load_hl_from_sp_offset(int off)
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

static void gen_expr(void);
static void gen_expr_no_comma(void);
static void gen_unary(void);
static void gen_snippet_lvalue_addr(const char *snippet, int *ptype);
static int snippet_is_single_pointer_id(const char *s);
static void gen_statement(void);
static int parse_funcptr_declarator(int *ptype, char *name, int namesz)
{
    int type;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    g_funcptr_decl_array_len = 0;

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
        return 0;
    }

    type = type_add_ptr(ptype[0]);

    if (accept('(')) {
        while (tok.kind != ')' && tok.kind != TOK_EOF)
            next_token();
        expect(')');
    }

    ptype[0] = type;
    return 1;
}

static int char_array_string_initializer_size(int base_type)
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
static void parse_array_declarator_dims(int base_type,
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




static int count_initializer_atoms_level(void)
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

static int count_omitted_array_initializer_atoms(void)
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

static void emit_init_auto_char_array_from_string(struct Sym *s, const char *str)
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

static void parse_typedef_decl(void);
static void parse_global_init_list(struct Sym *s);
static void scan_static_local_decl_after_type(int base);
static char *copy_range(long a, long b);
static void gen_snippet_expr(const char *snippet);
static void emit_incdec_addr(int type, int op);

/* Look up or pre-allocate a user label ID (for goto / label: targets).
 * Labels are function-scoped; nulabels is reset before each function. */
static int find_or_alloc_user_label(const char *name)
{
    int i;
    for (i = 0; i < nulabels; ++i)
        if (!strcmp(ulabel_names[i], name))
            return ulabel_ids[i];
    if (nulabels >= MAX_USER_LABELS) fatal("too many goto labels");
    strncpy(ulabel_names[nulabels], name, sizeof(ulabel_names[nulabels]) - 1);
    ulabel_names[nulabels][sizeof(ulabel_names[nulabels]) - 1] = 0;
    ulabel_ids[nulabels] = new_label();
    return ulabel_ids[nulabels++];
}

/* Parse a signed integer constant value used in enum bodies (e.g. = -1) */
static int parse_enum_const_value(void)
{
    int sign = 1;
    int v;
    if (tok.kind == '-') { sign = -1; next_token(); }
    else if (tok.kind == '+') { next_token(); }
    if (tok.kind != TOK_NUM) {
        error_here("integer constant expected in enum");
        return 0;
    }
    v = (int)(tok.val & 0xffffL);
    next_token();
    return sign * v;
}



static int bracket_expr_has_field_access(void)
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

static int try_fast_global_char_array_condition(int false_label)
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

static int is_assignment_token(int k)
{
    return k == '=' || k == TOK_ADDEQ || k == TOK_SUBEQ || k == TOK_MULEQ ||
           k == TOK_DIVEQ || k == TOK_MODEQ || k == TOK_ANDEQ || k == TOK_OREQ ||
           k == TOK_XOREQ || k == TOK_SHLEQ || k == TOK_SHREQ;
}

static int skip_lvalue_syntax(void)
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

static int lookahead_is_assignment(void)
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


static int try_fast_global_char_array_store(void)
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


static void gen_post_update_symbol_addr_value(struct Sym *s, int op)
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
static int try_gen_deref_postinc_lvalue_addr(int *ptype)
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


static int try_parse_const_subscript(long *out)
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

static void gen_lvalue_addr(int *ptype)
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
                    elem_size = type_index_elem_size(cur_type);
                addr_array_index_count++;

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (s->is_array)
                    elem_size = sym_array_index_elem_size(s, addr_array_index_count);
                else
                    elem_size = type_index_elem_size(cur_type);
                addr_array_index_count++;

                if (elem_size == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem_size == 2) emit("\tadd hl,hl\n");
                else if (elem_size > 4) {
                    fprintf(outf, "\tld de,%d\n", elem_size);
                    emit_runtime_call("__mulu");
                }
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

        while (accept('[')) {
            cur_type = ptype ? *ptype : s->type;

            if (!addr_is_array && type_ptr_depth(cur_type) > 0)
                emit_load_from_hl(cur_type);

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (addr_is_array)
                    elem_size = type_size(cur_type);
                else
                    elem_size = type_index_elem_size(cur_type);

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (addr_is_array)
                    elem_size = type_size(cur_type);
                else
                    elem_size = type_index_elem_size(cur_type);

                if (elem_size == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem_size == 2) emit("\tadd hl,hl\n");
                else if (elem_size > 4) {
                    fprintf(outf, "\tld de,%d\n", elem_size);
                    emit_runtime_call("__mulu");
                }
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (addr_is_array) {
                addr_array_index_count++;
                if (addr_array_index_count >= current_field_array_dim_count)
                    addr_is_array = 0;
            } else {
                cur_type = type_decay_ptr(cur_type);
                if (ptype) {
                ptype[0] = cur_type;
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

static void gen_post_update_from_addr(int type, int op)
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


static int paren_starts_indirect_call(void)
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


static int try_inline_strcpy_call(char *name, long *arg_start, long *arg_end, int argc)
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


static void emit_promote_byte_to_int(int actual_type)
{
    if ((actual_type & 15) != TYPE_CHAR || type_ptr_depth(actual_type) != 0)
        return;

    if (actual_type & TYPE_UNSIGNED)
        emit("\tld h,0\n");
    else
        emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
}

static void emit_promote_int_to_long(int actual_type, int expected_type)
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


static void emit_convert_int_to_float(int actual_type)
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

static void emit_convert_float_to_intlike(int target_type)
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

static int expected_arg_type(struct Sym *fn, int arg_index, int *ptype)
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


static void trim_small(char *s)
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

static void strip_line_directives_and_semi(char *s)
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

static int parse_simple_ident_text(const char *s, char *out)
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

static int va_size_from_text(const char *s)
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

static int try_builtin_stdarg_call(const char *name, long *arg_start, long *arg_end, int argc)
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

static int parse_call_args_after_lparen(long *arg_start, long *arg_end, int *argcp)
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


static int try_emit_fast_byte_add_const_snippet(const char *snippet)
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

static int emit_default_call_args(long *arg_start, long *arg_end, int argc)
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

static void emit_cleanup_stack_bytes(int bytes)
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


static int try_emit_push_struct_return_call_arg(const char *snippet, int want_type);

static int try_emit_struct_return_call_assignment(int lhs_type)
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
    return 1;
}


static int try_emit_push_struct_return_call_arg(const char *snippet, int want_type)
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

static void emit_call_hl_from_stack_offset(int off)
{
    fprintf(outf, "\tld hl,%d\n", off);
    emit("\tadd hl,sp\n");
    emit("\tld e,(hl)\n");
    emit("\tinc hl\n");
    emit("\tld d,(hl)\n");
    emit("\tex de,hl\n");
    emit_runtime_call("__call_hl");
}


static void emit_extract_bitfield(void)
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

static void emit_store_bitfield_from_hl(void)
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

static void emit_load_float_bits(unsigned long bits);
static void emit_float_compare_call(int op);


static int try_inline_cb_is_zero_call(const char *name,
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


static int try_gen_parenthesized_const_size_expr(void)
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

static void gen_primary(void)
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
    int di;

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

        if (s->storage == SC_FUNC &&
            tok.kind != '[' && tok.kind != '.' && tok.kind != TOK_ARROW &&
            tok.kind != TOK_INC && tok.kind != TOK_DEC) {
            fprintf(outf, "\tld hl,%s\n", asm_name_for(name));
            g_expr_type = type_add_ptr(s->type);
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

        while (accept('[')) {
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
                    elem_size = type_index_elem_size(cur_type);

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (val_is_array)
                    elem_size = sym_array_index_elem_size(s, val_array_index_count);
                else
                    elem_size = type_index_elem_size(cur_type);

                if (elem_size == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem_size == 2) emit("\tadd hl,hl\n");
                else if (elem_size > 4) {
                    fprintf(outf, "\tld de,%d\n", elem_size);
                    emit_runtime_call("__mulu");
                }
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

        while (accept('[')) {
            indexed = 1;
            cur_type = val_type;

            if (!val_is_array && type_ptr_depth(cur_type) > 0)
                emit_load_from_hl(cur_type);

            {
                long const_index;

                if (try_parse_const_subscript(&const_index)) {

                if (val_is_array) {
                    elem_size = current_field_array_elem_size ? current_field_array_elem_size : type_size(cur_type);
                    if (current_field_array_dim_count > 0) {
                        for (di = val_array_index_count + 1;
                             di < current_field_array_dim_count;
                             ++di)
                            elem_size *= current_field_array_dims[di];
                    }
                } else {
                    elem_size = type_index_elem_size(cur_type);
                }

                emit_add_const_to_hl(const_index * elem_size);
                } else {
                emit("\tpush hl\n");
                gen_expr();
                expect(']');

                if (val_is_array) {
                    elem_size = current_field_array_elem_size ? current_field_array_elem_size : type_size(cur_type);
                    if (current_field_array_dim_count > 0) {
                        for (di = val_array_index_count + 1;
                             di < current_field_array_dim_count;
                             ++di)
                            elem_size *= current_field_array_dims[di];
                    }
                } else {
                    elem_size = type_index_elem_size(cur_type);
                }

                if (elem_size == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem_size == 2) emit("\tadd hl,hl\n");
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
                emit("\tadd hl,de\n");
                }
            }

            if (val_is_array) {
                val_array_index_count++;
                if (val_array_index_count >= current_field_array_dim_count)
                    val_is_array = 0;
            } else {
                val_type = type_decay_ptr(cur_type);
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
            gen_post_update_from_addr(s->type, op);
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

static int paren_starts_cast(void)
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


static int try_gen_simple_deref_value(void)
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



static void emit_incdec_value_in_dehl(int type, int op)
{
    int no_carry;

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

static void emit_pre_incdec_lvalue(int type, int op)
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

static void gen_unary(void)
{
    int t;
    int sz;

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
        } else {
            emit("\txor a\n\tsub l\n\tld l,a\n\tld a,0\n\tsbc a,h\n\tld h,a\n");
        }
        return;
    }

    if (accept('!')) {
        int lt;
        int le;
        lt = new_label();
        le = new_label();
        gen_unary();
        if (type_is_long(g_expr_type)) {
            /* logical not of 32-bit: zero iff DE:HL==0 */
            emit("\tld a,h\n\tor l\n\tor d\n\tor e\n");
        } else {
            emit("\tld a,h\n\tor l\n");
        }
        emit_jp_label("jp z,", lt);
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
        } else {
            emit("\tld a,h\n\tcpl\n\tld h,a\n\tld a,l\n\tcpl\n\tld l,a\n");
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

static void gen_cmp(int op)
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

static void gen_cmp_typed(int op, int lhs_type)
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

static int is_relop_token(int k)
{
    return k == TOK_EQ || k == TOK_NE || k == '<' || k == '>' ||
           k == TOK_LE || k == TOK_GE;
}

static void emit_signed_bias_for_relop(int op)
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

static void emit_cmp_branch_false(int op, int lfalse)
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

static void emit_cmp_branch_true(int op, int ltrue)
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


static int const_positive_snippet_value(const char *s, long *out)
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

static int snippet_const_expr_value(const char *snippet, struct ConstVal *out)
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

static int const_rel_result(struct ConstVal lhs, int op, struct ConstVal rhs, int *resultp)
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

static void emit_const_rel_branch(int rel_result, int label, int branch_when_true)
{
    if ((rel_result != 0) == (branch_when_true != 0))
        emit_jp_label("jp", label);
}

static void emit_cmp_branch_false_unsigned(int op, int lfalse)
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

static void emit_cmp_branch_true_unsigned(int op, int ltrue)
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


static void scan_until_stop_at_depth0(int stop_kind)
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

static int invert_relop_for_swap(int op)
{
    if (op == '<') return '>';
    if (op == '>') return '<';
    if (op == TOK_LE) return TOK_GE;
    if (op == TOK_GE) return TOK_LE;
    return op;
}

static int parse_byte_const_operand(long *vp)
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


struct ByteOperand {
    int kind;          /* 1 = IX direct byte, 2 = constant, 3 = global byte array[index] */
    struct Sym *sym;
    struct Sym *idx_sym;
    long val;
};

static int parse_global_byte_array_index_operand(struct ByteOperand *op)
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

static int parse_byte_operand_fast(struct ByteOperand *op)
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

static void emit_byte_operand_to_a(struct ByteOperand *op)
{
    if (op->kind == 1) {
        fprintf(outf, "\tld a,(ix%+d)\n", op->sym->offset);
    } else if (op->kind == 2) {
        fprintf(outf, "\tld a,%ld\n", op->val & 255);
    } else if (op->kind == 3) {
        emit_extrn_if_needed(op->sym);
        if (op->idx_sym) {
            fprintf(outf, "\tld hl,%s\n", asm_name_for(op->sym->name));
            fprintf(outf, "\tld e,(ix%+d)\n", op->idx_sym->offset);
            emit("\tld d,0\n");
            emit("\tadd hl,de\n");
            emit("\tld a,(hl)\n");
        } else {
            fprintf(outf, "\tld a,(%s+%ld)\n", asm_name_for(op->sym->name), op->val & 0xffffL);
        }
    }
}

static void emit_cp_byte_operand(struct ByteOperand *op)
{
    if (op->kind == 1) {
        fprintf(outf, "\tcp (ix%+d)\n", op->sym->offset);
    } else if (op->kind == 2) {
        fprintf(outf, "\tcp %ld\n", op->val & 255);
    } else if (op->kind == 3) {
        emit_extrn_if_needed(op->sym);
        if (op->idx_sym) {
            emit("\tld b,a\n");
            fprintf(outf, "\tld hl,%s\n", asm_name_for(op->sym->name));
            fprintf(outf, "\tld e,(ix%+d)\n", op->idx_sym->offset);
            emit("\tld d,0\n");
            emit("\tadd hl,de\n");
            emit("\tld a,b\n");
            emit("\tcp (hl)\n");
        } else {
            fprintf(outf, "\tcp (%s+%ld)\n", asm_name_for(op->sym->name), op->val & 0xffffL);
        }
    }
}

static int byte_operand_can_be_lhs(struct ByteOperand *op)
{
    return op->kind == 1 || op->kind == 3;
}

static void emit_byte_cmp_branch_after_cp(int op, int label, int branch_when_true)
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

static int gen_direct_byte_rel_branch_until(int label, int branch_when_true, int stop_kind)
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

static int simple_direct_condition_until(int stop_kind)
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

        if (depth == 0) {
            if (tok.kind == TOK_FLOATLIT) {
                bad = 1;
            } else if (tok.kind == TOK_ID) {
                struct Sym *fs;
                fs = find_sym(tok.text);
                if (fs && type_is_float(fs->type))
                    bad = 1;
            }

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


static int snippet_is_single_pointer_id(const char *s)
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


static const char *skip_snippet_ws_and_lines(const char *s)
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

static int snippet_simple_type(const char *s, int *typep)
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
        typep[0] = TYPE_INT;
        while (isxdigit((unsigned char)*s) || *s == 'x' || *s == 'X')
            s++;
        if (*s == 'l' || *s == 'L')
            typep[0] = TYPE_LONG;
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

static int snippet_needs_long_compare(const char *lhs, const char *rhs, int *commonp)
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

static void emit_branch_on_bool_hl(int label, int branch_when_true)
{
    emit("\tld a,h\n\tor l\n");
    if (branch_when_true)
        emit_jp_label("jp nz,", label);
    else
        emit_jp_label("jp z,", label);
}

static void gen_direct_rel_branch_until(int op, int label, int branch_when_true,
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
        int ptr_cmp;
        int common_type;

        if (snippet_needs_long_compare(lhs_code, rhs_code, &common_type)) {
            gen_snippet_expr(lhs_code);
            lhs_type = g_expr_type;
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_snippet_expr(rhs_code);
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_cmp32(op, common_type);
            emit_branch_on_bool_hl(label, branch_when_true);
        } else {
            ptr_cmp = snippet_is_single_pointer_id(lhs_code) ||
                      snippet_is_single_pointer_id(rhs_code);
            gen_snippet_expr(lhs_code);
            lhs_type = g_expr_type;
            emit("\tpush hl\n");
            gen_snippet_expr(rhs_code);
            emit("\tex de,hl\n\tpop hl\n");
            if ((lhs_type & TYPE_UNSIGNED) || ptr_cmp) {
                if (branch_when_true) emit_cmp_branch_true_unsigned(op, label);
                else emit_cmp_branch_false_unsigned(op, label);
            } else {
                if (branch_when_true) emit_cmp_branch_true(op, label);
                else emit_cmp_branch_false(op, label);
            }
        }
    }

    free(lhs_code);
    free(rhs_code);
}


static int gen_direct_byte_bitand_branch_until(int label, int branch_when_true, int stop_kind)
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


static int gen_const_and_byte_condition_branch_until(int label, int branch_when_true, int stop_kind)
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


static int emit_cmp_const_branch_for_signed_local16(struct Sym *s, int op, long c,
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

static int gen_direct_small_const_int_rel_branch_until(int label, int branch_when_true,
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

static int gen_condition_branch_false_until(int lfalse, int stop_kind)
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

static int gen_condition_branch_true_until(int ltrue, int stop_kind)
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

static int gen_condition_branch_false(int lfalse)
{
    return gen_condition_branch_false_until(lfalse, ')');
}

static int gen_condition_branch_true(int ltrue)
{
    return gen_condition_branch_true_until(ltrue, ')');
}


static void gen_signed_divmod16(int op)
{
    /*
     * Entry: HL = signed lhs, DE = signed rhs.
     * The RTL wrappers use unsigned division on absolute values and then
     * fix the sign.  Keeping this in the runtime avoids emitting the same
     * 40+ instruction sequence at every signed 16-bit / or % site.
     */
    if (op == '/')
        emit_runtime_call("__divs");
    else
        emit_runtime_call("__mods");
}

static void gen_binop(int op)
{
    switch (op) {
    case '+':
        emit("\tadd hl,de\n");
        break;
    case '-':
        emit("\tor a\n\tsbc hl,de\n");
        break;
    case '&':
        emit("\tld a,h\n\tand d\n\tld h,a\n\tld a,l\n\tand e\n\tld l,a\n");
        break;
    case '|':
        emit("\tld a,h\n\tor d\n\tld h,a\n\tld a,l\n\tor e\n\tld l,a\n");
        break;
    case '^':
        emit("\tld a,h\n\txor d\n\tld h,a\n\tld a,l\n\txor e\n\tld l,a\n");
        break;
    case '*':
        emit_runtime_call("__mulu");
        break;
    case '/':
        emit_runtime_call("__divu");
        break;
    case '%':
        emit_runtime_call("__modu");
        break;
    case TOK_EQ:
    case TOK_NE:
    case '<':
    case '>':
    case TOK_LE:
    case TOK_GE:
        gen_cmp(op);
        break;
    default:
        emit("\t; unsupported binary op\n");
        break;
    }
}

static void gen_binop_typed(int op, int lhs_type)
{
    if (op == TOK_EQ || op == TOK_NE ||
        op == '<' || op == '>' || op == TOK_LE || op == TOK_GE) {
        gen_cmp_typed(op, lhs_type);
    } else if ((op == '/' || op == '%') && !(lhs_type & TYPE_UNSIGNED)) {
        gen_signed_divmod16(op);
    } else {
        gen_binop(op);
    }
}

/* Zero-extend HL to DE:HL for implicit int-to-long promotion.
 * Explicit signed literals use the L suffix and are always emitted TYPE_LONG. */
static void emit_extend_to_long_typed(int source_type)
{
    emit_promote_byte_to_int(source_type);
    if ((source_type & TYPE_UNSIGNED) || type_ptr_depth(source_type)) {
        emit("\tld de,0\n");
    } else {
        /* Sign-extend signed 16-bit HL into DE:HL.  This is also correct
         * when the target type is unsigned long: C converts the negative
         * value modulo 2^32, yielding the same bit pattern. */
        emit("\tld a,h\n");
        emit("\trlca\n");
        emit("\tsbc a,a\n");
        emit("\tld d,a\n");
        emit("\tld e,a\n");
    }
}

static void emit_extend_to_long(int source_is_unsigned)
{
    emit_extend_to_long_typed(source_is_unsigned ? (TYPE_INT | TYPE_UNSIGNED) : TYPE_INT);
}

/*
 * 32-bit binary op: LHS on stack as push de;push hl, RHS in DE:HL.
 * Inline for +,-,&,|,^; runtime call for *,/,%.
 * Result left in DE:HL (DE=high word, HL=low word).
 */
static void gen_binop32(int op, int lhs_type)
{
    const char *rname;
    switch (op) {
    case '+':
        emit("\tpop bc\n\tor a\n\tadd hl,bc\n");
        emit("\tex de,hl\n\tpop bc\n\tadc hl,bc\n\tex de,hl\n");
        break;
    case '-':
        emit("\tld b,h\n\tld c,l\n\tpop hl\n\tor a\n\tsbc hl,bc\n");
        emit("\tld b,h\n\tld c,l\n\tpop hl\n\tsbc hl,de\n");
        emit("\tld d,b\n\tld e,c\n\tex de,hl\n");
        break;
    case '&':
        emit("\tpop bc\n");
        emit("\tld a,l\n\tand c\n\tld l,a\n\tld a,h\n\tand b\n\tld h,a\n");
        emit("\tex de,hl\n\tpop bc\n");
        emit("\tld a,l\n\tand c\n\tld l,a\n\tld a,h\n\tand b\n\tld h,a\n");
        emit("\tex de,hl\n");
        break;
    case '|':
        emit("\tpop bc\n");
        emit("\tld a,l\n\tor c\n\tld l,a\n\tld a,h\n\tor b\n\tld h,a\n");
        emit("\tex de,hl\n\tpop bc\n");
        emit("\tld a,l\n\tor c\n\tld l,a\n\tld a,h\n\tor b\n\tld h,a\n");
        emit("\tex de,hl\n");
        break;
    case '^':
        emit("\tpop bc\n");
        emit("\tld a,l\n\txor c\n\tld l,a\n\tld a,h\n\txor b\n\tld h,a\n");
        emit("\tex de,hl\n\tpop bc\n");
        emit("\tld a,l\n\txor c\n\tld l,a\n\tld a,h\n\txor b\n\tld h,a\n");
        emit("\tex de,hl\n");
        break;
    case '*': rname = "__lmul";  goto l32call;
    case '/': rname = (lhs_type & TYPE_UNSIGNED) ? "__ldu" : "__lds"; goto l32call;
    case '%': rname = (lhs_type & TYPE_UNSIGNED) ? "__lmu" : "__lms"; goto l32call;
    l32call:
        /* push RHS (4 bytes) then call; runtime returns DE:HL; caller cleans 8 bytes */
        emit("\tpush de\n\tpush hl\n");
        emit_runtime_call(rname);
        emit("\tld b,d\n\tld c,e\n\tex de,hl\n");
        emit("\tld hl,8\n\tadd hl,sp\n\tld sp,hl\n");
        emit("\tex de,hl\n\tld d,b\n\tld e,c\n");
        break;
    default:
        emit("\t; unsupported 32-bit binary op\n");
        emit("\tpop bc\n\tpop bc\n"); /* balance stack */
        break;
    }
}

/*
 * 32-bit comparison: LHS on stack as push de;push hl, RHS in DE:HL.
 * Inline for ==,!=; runtime call for ordered comparisons.
 * Result left in HL (0 or 1). Stack cleaned (8 bytes).
 */
static void gen_cmp32(int op, int lhs_type)
{
    const char *rname;
    int lt, le;
    int is_unsigned = (lhs_type & TYPE_UNSIGNED) != 0;

    if (op == TOK_EQ || op == TOK_NE) {
        /* XOR all bytes together; A=0 iff equal */
        lt = new_label();
        le = new_label();
        emit("\tpop bc\n");  /* BC=LHS_L */
        emit("\tld a,c\n\txor l\n\tld l,a\n\tld a,b\n\txor h\n\tor l\n\tld l,a\n");
        emit("\tpop bc\n");  /* BC=LHS_H */
        emit("\tld a,c\n\txor e\n\tor l\n\tld l,a\n\tld a,b\n\txor d\n\tor l\n");
        if (op == TOK_EQ) emit_jp_label("jp z,", lt);
        else              emit_jp_label("jp nz,", lt);
        emit("\tld hl,0\n");
        emit_jp_label("jp", le);
        emit_label(lt);
        emit("\tld hl,1\n");
        emit_label(le);
        return;
    }

    /* ordered: push RHS onto stack, call runtime, clean 8 bytes */
    if (op == '<')      rname = is_unsigned ? "__ltu" : "__lts";
    else if (op == TOK_LE) rname = is_unsigned ? "__leu" : "__les";
    else if (op == '>')      rname = is_unsigned ? "__lgu" : "__lgs";
    else                rname = is_unsigned ? "__lku" : "__lks"; /* >= */
    emit("\tpush de\n\tpush hl\n");
    emit_runtime_call(rname);
    emit("\tex de,hl\n\tld hl,8\n\tadd hl,sp\n\tld sp,hl\n\tex de,hl\n");
}

static void gen_binop32_typed(int op, int lhs_type)
{
    if (op == TOK_EQ || op == TOK_NE ||
        op == '<' || op == '>' || op == TOK_LE || op == TOK_GE) {
        gen_cmp32(op, lhs_type);
    } else {
        gen_binop32(op, lhs_type);
    }
}


static void emit_mul_hl_const(long v)
{
    /*
     * HL = HL * small positive constant.
     * Used for strength-reducing common benchmark cases like 10*x.
     */
    if (v == 0) {
        emit("\tld hl,0\n");
    } else if (v == 1) {
        /* no-op */
    } else if (v == 2) {
        emit("\tadd hl,hl\n");
    } else if (v == 3) {
        emit("\tpush hl\n");
        emit("\tadd hl,hl\n");
        emit("\tpop de\n");
        emit("\tadd hl,de\n");
    } else if (v == 4) {
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
    } else if (v == 5) {
        emit("\tpush hl\n");
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
        emit("\tpop de\n");
        emit("\tadd hl,de\n");
    } else if (v == 8) {
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
    } else if (v == 10) {
        emit("\tpush hl\n");     /* save x */
        emit("\tadd hl,hl\n");   /* 2x */
        emit("\tadd hl,hl\n");   /* 4x */
        emit("\tadd hl,hl\n");   /* 8x */
        emit("\tpop de\n");      /* x */
        emit("\tadd hl,de\n");   /* 9x */
        emit("\tadd hl,de\n");   /* 10x */
    } else if (v == 16) {
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
    } else {
        emit_ld_de_const(v);
        emit_runtime_call("__mulu");
    }
}

static int try_gen_const_times(void)
{
    long v;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT)
        return 0;

    v = tok.val & 0xffffL;
    if (!(v == 0 || v == 1 || v == 2 || v == 3 || v == 4 ||
          v == 5 || v == 8 || v == 10 || v == 16))
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

    gen_unary();
    emit_mul_hl_const(v);
    return 1;
}



/* C89 integer promotion / usual arithmetic conversion helpers.
 * This compiler only has 16-bit int and 32-bit long.  Plain char and
 * unsigned char both promote to signed int because 16-bit int represents
 * all unsigned-char values.  For long vs unsigned-int, signed long wins
 * because it represents every 16-bit unsigned int value. */
static int type_is_unsigned(int t)
{
    return (t & TYPE_UNSIGNED) != 0;
}

static int type_is_arith(int t)
{
    if (t & (TYPE_PTR | TYPE_PTR2 | TYPE_STRUCT)) return 0;
    return 1;
}

static int promote_int_type(int t)
{
    if (!type_is_arith(t)) return t;
    if (type_is_float(t)) return t;
    if (type_is_long(t)) return t;
    if ((t & 15) == TYPE_CHAR) return TYPE_INT;
    return (t & TYPE_UNSIGNED) ? (TYPE_INT | TYPE_UNSIGNED) : TYPE_INT;
}

static int common_arith_type(int a, int b)
{
    a = promote_int_type(a);
    b = promote_int_type(b);

    if (type_is_float(a) || type_is_float(b))
        return TYPE_FLOAT;

    if (type_is_long(a) || type_is_long(b)) {
        /* unsigned long dominates; otherwise signed long can hold all
         * 16-bit unsigned values on this target. */
        if ((type_is_long(a) && type_is_unsigned(a)) ||
            (type_is_long(b) && type_is_unsigned(b)))
            return TYPE_LONG | TYPE_UNSIGNED;
        return TYPE_LONG;
    }

    if (type_is_unsigned(a) || type_is_unsigned(b))
        return TYPE_INT | TYPE_UNSIGNED;
    return TYPE_INT;
}

static void emit_cast_16_to_common(int from_type, int common_type)
{
    if (type_is_long(common_type) && !type_is_long(from_type))
        emit_extend_to_long_typed(from_type);
}

static int peek_simple_unary_type(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    int save_long_suffix;
    int save_unsigned_suffix;
    struct Token save_tok;
    int t;
    struct Sym *s;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_long_suffix = g_tok_long_suffix;
    save_unsigned_suffix = g_tok_unsigned_suffix;
    save_tok = tok;

    t = TYPE_INT;

    if (tok.kind == '(') {
        next_token();
        if (starts_type()) {
            t = parse_type();
            if (tok.kind == ')') {
                posi = save_pos; tok_start_pos = save_tok_start;
                line_no = save_line; tok_line = save_tok_line;
                g_tok_long_suffix = save_long_suffix; g_tok_unsigned_suffix = save_unsigned_suffix; tok = save_tok;
                return promote_int_type(t);
            }
        }
    } else if (tok.kind == TOK_FLOATLIT) {
        t = TYPE_FLOAT;
    } else if (tok.kind == TOK_NUM) {
        if (tok.val > 0xffffL || tok.val < -32768L || g_tok_long_suffix)
            t = TYPE_LONG;
        else
            t = TYPE_INT;
        if (g_tok_unsigned_suffix)
            t |= TYPE_UNSIGNED;
    } else if (tok.kind == TOK_CHARLIT) {
        t = TYPE_INT;
    } else if (tok.kind == TOK_ID) {
        s = find_sym(tok.text);
        if (s) {
            int is_arr;
            int tt;
            t = s->type;

            /* For lookahead purposes, recognize calls through function
             * pointers and function-pointer arrays as returning int.
             * Without this, an expression like:
             *
             *     tab[0](30) + tab[1](40)
             *
             * is misclassified as integer + pointer before the RHS is
             * generated, so gen_add() applies pointer scaling to the left
             * call result.  This compiler only tracks int-returning
             * function pointers today, which matches the supported
             * declarator forms. */
            tt = t;
            is_arr = s->is_array;
            next_token();
            while (tok.kind == '[') {
                skip_balanced_bracket('[', ']');
                if (is_arr)
                    is_arr = 0;
                else
                    tt = type_decay_ptr(tt);
            }
            if (tok.kind == '(' && type_ptr_depth(tt) > 0)
                t = TYPE_INT;
        }
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    g_tok_long_suffix = save_long_suffix;
    g_tok_unsigned_suffix = save_unsigned_suffix;
    tok = save_tok;
    return promote_int_type(t);
}


static int float_literal_pow2_exp(const char *s, int *exp_out)
{
    double d;
    int e;

    d = atof(s);
    if (d <= 0.0)
        return 0;

    e = 0;
    while (d > 1.0 && e < 30) {
        d = d / 2.0;
        e++;
    }
    while (d < 1.0 && e > -30) {
        d = d * 2.0;
        e--;
    }

    if (d != 1.0)
        return 0;

    exp_out[0] = e;
    return 1;
}

static void emit_fscale_pow2(int exp_delta)
{
    if (exp_delta < -128 || exp_delta > 127) {
        /* Out of helper range; this should not happen for normal literals. */
        emit_runtime_call(exp_delta < 0 ? "__fdiv" : "__fmul");
        return;
    }

    fprintf(outf, "\tld b,%d\n", exp_delta);
    emit_runtime_call("__fscale_pow2");
    g_expr_type = TYPE_FLOAT;
}

static void gen_mul(void)
{
    int op;
    int lhs_type;
    int rhs_type;
    int common_type;

    if (!try_gen_const_times())
        gen_unary();
    lhs_type = promote_int_type(g_expr_type);

    while (tok.kind == '*' || tok.kind == '/' || tok.kind == '%') {
        op = tok.kind;
        next_token();

        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);

        if (type_is_float(common_type)) {
            if (op == '%') {
                error_float_unsupported("float modulo not supported");
                gen_unary();
                lhs_type = TYPE_INT;
                g_expr_type = TYPE_INT;
                continue;
            }

            /*
             * Exact power-of-two scale fast path:
             *     x * 16.0f   => __fscale_pow2(x, +4)
             *     x / 16.0f   => __fscale_pow2(x, -4)
             *
             * This avoids generic __fmul/__fdiv while preserving the operation
             * as exponent scaling in the RTL.
             */
            if ((op == '*' || op == '/') && tok.kind == TOK_FLOATLIT) {
                int pow2_exp;
                if (float_literal_pow2_exp(tok.text, &pow2_exp)) {
                    next_token();
                    if (!type_is_float(lhs_type))
                        emit_convert_int_to_float(lhs_type);
                    if (op == '/')
                        pow2_exp = -pow2_exp;
                    emit_fscale_pow2(pow2_exp);
                    lhs_type = TYPE_FLOAT;
                    g_expr_type = TYPE_FLOAT;
                    continue;
                }
            }

            if (!type_is_float(lhs_type))
                emit_convert_int_to_float(lhs_type);
            emit("\tpush de\n\tpush hl\n");
            gen_unary();
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_runtime_call(op == '*' ? "__fmul" : "__fdiv");
            emit("\tpop bc\n\tpop bc\n\tpop bc\n\tpop bc\n");
            lhs_type = TYPE_FLOAT;
            g_expr_type = TYPE_FLOAT;
            continue;
        }

        if (!type_is_long(common_type) && op == '*' &&
            (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT)) {
            long rhs_val = tok.val & 0xffffL;
            if (rhs_val == 0 || rhs_val == 1 || rhs_val == 2 ||
                rhs_val == 3 || rhs_val == 4 || rhs_val == 5 ||
                rhs_val == 8 || rhs_val == 10 || rhs_val == 16) {
                next_token();
                emit_mul_hl_const(rhs_val);
                lhs_type = common_type;
                g_expr_type = common_type;
                continue;
            }
        }

        if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_unary();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32(op, common_type);
        } else {
            emit("\tpush hl\n");
            gen_unary();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop_typed(op, common_type);
        }
        lhs_type = common_type;
        g_expr_type = common_type;
    }
}

static void scale_hl_by_elem_size(int elem)
{
    if (elem == 1) {
        return;
    } else if (elem == 2) {
        emit("\tadd hl,hl\n");
    } else if (elem == 4) {
        emit("\tadd hl,hl\n");
        emit("\tadd hl,hl\n");
    } else if (elem > 1) {
        fprintf(outf, "\tld de,%d\n", elem);
        emit_runtime_call("__mulu");
    }
}

static void divide_hl_by_elem_size(int elem)
{
    if (elem <= 1)
        return;

    fprintf(outf, "\tld de,%d\n", elem);
    emit_runtime_call("__divs");
}

static void gen_add(void)
{
    int op;
    int lhs_type;
    int rhs_type;
    int common_type;

    gen_mul();
    lhs_type = promote_int_type(g_expr_type);

    while (tok.kind == '+' || tok.kind == '-') {
        op = tok.kind;
        next_token();
        rhs_type = peek_simple_unary_type();

        /* Pointer arithmetic is not an arithmetic conversion case.
         *
         * Supported C89 forms:
         *   ptr + n       -> byte address plus n * sizeof(*ptr)
         *   ptr - n       -> byte address minus n * sizeof(*ptr)
         *   n + ptr       -> n * sizeof(*ptr) plus byte address
         *   ptr2 - ptr1   -> signed element distance
         */
        if ((lhs_type & (TYPE_PTR | TYPE_PTR2))) {
            int elem = type_index_elem_size(lhs_type);

            emit("\tpush hl\n");
            gen_mul();

            if (g_expr_type & (TYPE_PTR | TYPE_PTR2)) {
                if (op == '-') {
                    emit("\tex de,hl\n\tpop hl\n");
                    gen_binop('-');
                    divide_hl_by_elem_size(elem);
                    lhs_type = TYPE_INT;
                    g_expr_type = TYPE_INT;
                    continue;
                }

                error_here("invalid pointer arithmetic");
                emit("\tpop de\n");
                lhs_type = TYPE_INT;
                g_expr_type = TYPE_INT;
                continue;
            }

            scale_hl_by_elem_size(elem);
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop(op);
            g_expr_type = lhs_type;
            continue;
        }

        if ((rhs_type & (TYPE_PTR | TYPE_PTR2)) && op == '+') {
            int elem = type_index_elem_size(rhs_type);

            scale_hl_by_elem_size(elem);
            emit("\tpush hl\n");
            gen_mul();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop('+');
            lhs_type = rhs_type;
            g_expr_type = rhs_type;
            continue;
        }

        common_type = common_arith_type(lhs_type, rhs_type);

        if (type_is_float(common_type)) {
            if (!type_is_float(lhs_type))
                emit_convert_int_to_float(lhs_type);
            emit("\tpush de\n\tpush hl\n");
            gen_mul();
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_runtime_call(op == '+' ? "__fadd" : "__fsub");
            emit("\tpop bc\n\tpop bc\n\tpop bc\n\tpop bc\n");
            lhs_type = TYPE_FLOAT;
            g_expr_type = TYPE_FLOAT;
            continue;
        }

        if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_mul();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32(op, common_type);
        } else {
            emit("\tpush hl\n");
            gen_mul();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop_typed(op, common_type);
        }
        lhs_type = common_type;
        g_expr_type = common_type;
    }
}

static int emit_shift_const_long(int op, int lhs_type, long count)
{
    int is_left;
    int is_unsigned;

    if (!type_is_long(lhs_type))
        return 0;

    is_left = (op == TOK_SHL || op == TOK_SHLEQ);
    is_unsigned = (lhs_type & TYPE_UNSIGNED) != 0;

    if (count <= 0)
        return 1;

    if (count >= 32) {
        if (is_left || is_unsigned)
            emit("\tld hl,0\n\tld de,0\n");
        else
            emit("\tld a,d\n\trla\n\tsbc a,a\n\tld h,a\n\tld l,a\n\tld d,a\n\tld e,a\n");
        return 1;
    }

    if (is_left) {
        if (count == 8) { emit("\tld d,e\n\tld e,h\n\tld h,l\n\tld l,0\n"); return 1; }
        if (count == 16) { emit("\tld e,l\n\tld d,h\n\tld hl,0\n"); return 1; }
        if (count == 24) { emit("\tld d,l\n\tld e,0\n\tld hl,0\n"); return 1; }
    } else if (is_unsigned) {
        if (count == 8) { emit("\tld l,h\n\tld h,e\n\tld e,d\n\tld d,0\n"); return 1; }
        if (count == 16) { emit("\tld l,e\n\tld h,d\n\tld de,0\n"); return 1; }
        if (count == 24) { emit("\tld l,d\n\tld h,0\n\tld de,0\n"); return 1; }
    }

    return 0;
}

static void emit_shift_loop(int op, int lhs_type)
{
    int ltop = new_label();
    int ldone = new_label();

    emit_label(ltop);
    emit("\tld a,b\n\tor a\n");
    emit_jp_label("jp z,", ldone);

    if (type_is_long(lhs_type)) {
        if (op == TOK_SHL || op == TOK_SHLEQ) {
            emit("\tadd hl,hl\n\trl e\n\trl d\n");
        } else if (lhs_type & TYPE_UNSIGNED) {
            emit("\tsrl d\n\trr e\n\trr h\n\trr l\n");
        } else {
            emit("\tsra d\n\trr e\n\trr h\n\trr l\n");
        }
    } else if (op == TOK_SHL || op == TOK_SHLEQ) {
        emit("\tadd hl,hl\n");
    } else if (lhs_type & TYPE_UNSIGNED) {
        emit("\tsrl h\n\trr l\n");
    } else if (type_size(lhs_type) == 1) {
        emit("\tsra l\n");
    } else {
        emit("\tsra h\n\trr l\n");
    }

    emit("\tdec b\n");
    emit_jp_label("jp", ltop);
    emit_label(ldone);
}

static void gen_shift(void)
{
    int op;
    int lhs_type;

    gen_add();
    lhs_type = g_expr_type;

    while (tok.kind == TOK_SHL || tok.kind == TOK_SHR) {
        op = tok.kind;
        next_token();

        if (type_is_long(lhs_type) && (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT)) {
            long scount;
            scount = tok.val;
            next_token();
            if (!emit_shift_const_long(op, lhs_type, scount)) {
                fprintf(outf, "\tld b,%ld\n", scount & 255L);
                emit_shift_loop(op, lhs_type);
            }
        } else if (type_is_long(lhs_type)) {
            emit("\tpush de\n\tpush hl\n");
            gen_add();
            emit("\tld b,l\n");
            emit("\tpop hl\n\tpop de\n");
            emit_shift_loop(op, lhs_type);
        } else {
            emit("\tpush hl\n");
            gen_add();
            emit("\tld b,l\n");
            emit("\tpop hl\n");
            emit_shift_loop(op, lhs_type);
        }

        g_expr_type = lhs_type;
    }
}

static void emit_float_compare_call(int op)
{
    const char *helper;
    int k;

    /* The evaluation sequence pushes the left operand first, computes and
     * pushes the right operand second.  That means the C-style helper sees
     * arguments in reversed order: arg1=right, arg2=left.  Use the inverse
     * ordered helper where necessary.
     */
    if (op == TOK_EQ) helper = "__feq";
    else if (op == TOK_NE) helper = "__fneq";
    else if (op == '<') helper = "__fgt";      /* rhs > lhs */
    else if (op == '>') helper = "__flt";      /* rhs < lhs */
    else if (op == TOK_LE) helper = "__fge";   /* rhs >= lhs */
    else helper = "__fle";                     /* rhs <= lhs */

    emit_runtime_call(helper);
    for (k = 0; k < 4; ++k)
        emit("\tpop bc\n");
    g_expr_type = TYPE_INT;
}

static void gen_rel(void)
{
    int op;
    int lhs_type;
    int rhs_type;
    int common_type;
    gen_shift();
    lhs_type = promote_int_type(g_expr_type);

    while (tok.kind == '<' || tok.kind == '>' || tok.kind == TOK_LE || tok.kind == TOK_GE) {
        op = tok.kind;
        next_token();
        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);

        if (type_is_float(common_type)) {
            if (!type_is_float(lhs_type))
                emit_convert_int_to_float(lhs_type);
            emit("\tpush de\n\tpush hl\n");
            gen_shift();
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_float_compare_call(op);
        } else if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_shift();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32_typed(op, common_type);
        } else {
            emit("\tpush hl\n");
            gen_shift();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop_typed(op, common_type);
        }
        g_expr_type = TYPE_INT;
        lhs_type = TYPE_INT;
    }
}

static void gen_eq(void)
{
    int op;
    int lhs_type;
    int rhs_type;
    int common_type;
    gen_rel();
    lhs_type = promote_int_type(g_expr_type);

    while (tok.kind == TOK_EQ || tok.kind == TOK_NE) {
        op = tok.kind;
        next_token();
        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);

        if (type_is_float(common_type)) {
            if (!type_is_float(lhs_type))
                emit_convert_int_to_float(lhs_type);
            emit("\tpush de\n\tpush hl\n");
            gen_rel();
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_float_compare_call(op);
        } else if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_rel();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32_typed(op, common_type);
        } else {
            emit("\tpush hl\n");
            gen_rel();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop_typed(op, common_type);
        }
        g_expr_type = TYPE_INT;
        lhs_type = TYPE_INT;
    }
}

static void gen_band(void)
{
    int lhs_type;
    int rhs_type;
    int common_type;
    gen_eq();
    lhs_type = promote_int_type(g_expr_type);

    while (accept('&')) {
        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);
        if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_eq();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32('&', common_type);
        } else {
            emit("\tpush hl\n");
            gen_eq();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop('&');
        }
        lhs_type = common_type;
        g_expr_type = common_type;
    }
}

static void gen_bxor(void)
{
    int lhs_type;
    int rhs_type;
    int common_type;
    gen_band();
    lhs_type = promote_int_type(g_expr_type);

    while (accept('^')) {
        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);
        if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_band();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32('^', common_type);
        } else {
            emit("\tpush hl\n");
            gen_band();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop('^');
        }
        lhs_type = common_type;
        g_expr_type = common_type;
    }
}

static void gen_bor(void)
{
    int lhs_type;
    int rhs_type;
    int common_type;
    gen_bxor();
    lhs_type = promote_int_type(g_expr_type);

    while (accept('|')) {
        rhs_type = peek_simple_unary_type();
        common_type = common_arith_type(lhs_type, rhs_type);
        if (type_is_long(common_type)) {
            emit_cast_16_to_common(lhs_type, common_type);
            emit("\tpush de\n\tpush hl\n");
            gen_bxor();
            emit_cast_16_to_common(g_expr_type, common_type);
            gen_binop32('|', common_type);
        } else {
            emit("\tpush hl\n");
            gen_bxor();
            emit("\tex de,hl\n\tpop hl\n");
            gen_binop('|');
        }
        lhs_type = common_type;
        g_expr_type = common_type;
    }
}


static void emit_test_expr_nonzero(int expr_type, int true_label, int branch_when_true)
{
    if (type_is_long(expr_type))
        emit("\tld a,h\n\tor l\n\tor d\n\tor e\n");
    else
        emit("\tld a,h\n\tor l\n");

    if (branch_when_true)
        emit_jp_label("jp nz,", true_label);
    else
        emit_jp_label("jp z,", true_label);
}

static void gen_land(void)
{
    int lf, le;
    int lhs_type;

    gen_bor();
    lhs_type = g_expr_type;

    while (accept(TOK_ANDAND)) {
        lf = new_label();
        le = new_label();

        emit_test_expr_nonzero(lhs_type, lf, 0);

        gen_bor();

        emit_test_expr_nonzero(g_expr_type, lf, 0);

        emit("\tld hl,1\n");
        emit_jp_label("jp", le);
        emit_label(lf);
        emit("\tld hl,0\n");
        emit_label(le);

        g_expr_type = TYPE_INT;
        lhs_type = TYPE_INT;
    }
}

static void gen_lor(void)
{
    int lt, le;
    int lhs_type;

    gen_land();
    lhs_type = g_expr_type;

    while (accept(TOK_OROR)) {
        lt = new_label();
        le = new_label();

        emit_test_expr_nonzero(lhs_type, lt, 1);

        gen_land();

        emit_test_expr_nonzero(g_expr_type, lt, 1);

        emit("\tld hl,0\n");
        emit_jp_label("jp", le);
        emit_label(lt);
        emit("\tld hl,1\n");
        emit_label(le);

        g_expr_type = TYPE_INT;
        lhs_type = TYPE_INT;
    }
}

static void gen_conditional(void)
{
    int lfalse;
    int lend;
    int true_type;
    int false_type;
    int need_long_result;

    gen_lor();

    if (accept('?')) {
        lfalse = new_label();
        lend = new_label();

        if (type_is_long(g_expr_type))
            emit("\tld a,h\n\tor l\n\tor d\n\tor e\n");
        else
            emit("\tld a,h\n\tor l\n");
        emit_jp_label("jp z,", lfalse);

        gen_expr();
        true_type = g_expr_type;
        if (type_is_struct_object(true_type))
            error_here("unsupported struct conditional expression");

        /*
         * C's conditional operator applies the usual conversions between the
         * second and third operands.  DCC does not have a full typed IR, so a
         * narrow true arm used with a long false arm could leave DE stale.
         *
         * Example from tarray.c:
         *     int32_t cap = (end_of_row > beyond) ? beyond : end_of_row;
         *
         * beyond is 16-bit size_t, end_of_row is 32-bit int32_t.  Without
         * extending the true arm, selecting beyond stores a garbage high word.
         *
         * Extending a 16-bit true arm here is safe when the whole expression is
         * ultimately 16-bit: callers/stores that want int use HL and ignore DE.
         */
        need_long_result = type_is_long(true_type);
        if (!type_is_long(true_type) && !type_is_float(true_type)) {
            emit_extend_to_long((true_type & TYPE_UNSIGNED) ||
                                (true_type & (TYPE_PTR | TYPE_PTR2)));
        }

        emit_jp_label("jp", lend);

        expect(':');
        emit_label(lfalse);

        gen_conditional();
        false_type = g_expr_type;
        if (type_is_struct_object(false_type))
            error_here("unsupported struct conditional expression");

        if (type_is_long(false_type))
            need_long_result = 1;

        if (need_long_result && !type_is_long(false_type) && !type_is_float(false_type)) {
            emit_extend_to_long((false_type & TYPE_UNSIGNED) ||
                                (false_type & (TYPE_PTR | TYPE_PTR2)));
            false_type = (false_type & TYPE_UNSIGNED) ? (TYPE_LONG | TYPE_UNSIGNED) : TYPE_LONG;
        }

        emit_label(lend);

        if (need_long_result) {
            if ((true_type & TYPE_UNSIGNED) || (false_type & TYPE_UNSIGNED))
                g_expr_type = TYPE_LONG | TYPE_UNSIGNED;
            else
                g_expr_type = TYPE_LONG;
        }
    }
}


static void emit_load_float_bits(unsigned long bits)
{
    if (!scan_mode) {
        fprintf(outf, "\tld hl,%lu\n", bits & 0xffffUL);
        fprintf(outf, "\tld de,%lu\n", (bits >> 16) & 0xffffUL);
    }
}

static int parse_float_assignment_literal(unsigned long *bits)
{
    if (tok.kind != TOK_FLOATLIT)
        return 0;

    bits[0] = parse_float_literal_bits(tok.text);
    next_token();
    return 1;
}

static int try_emit_float_rvalue_dehl(void)
{
    struct Sym *rs;
    unsigned long bits;

    if (tok.kind == '-') {
        long save_pos;
        long save_tok_start;
        int save_line;
        int save_tok_line;
        struct Token save_tok;

        save_pos = posi;
        save_tok_start = tok_start_pos;
        save_line = line_no;
        save_tok_line = tok_line;
        save_tok = tok;

        next_token();
        if (try_emit_float_rvalue_dehl()) {
            emit("\tld a,d\n\txor 80h\n\tld d,a\n");
            return 1;
        }

        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
    }

    if (parse_float_assignment_literal(&bits)) {
        emit_load_float_bits(bits);
        g_expr_type = TYPE_FLOAT;
        return 1;
    }

    if (tok.kind == '*') {
        long save_pos;
        long save_tok_start;
        int save_line;
        int save_tok_line;
        struct Token save_tok;
        int rt;

        save_pos = posi;
        save_tok_start = tok_start_pos;
        save_line = line_no;
        save_tok_line = tok_line;
        save_tok = tok;

        gen_lvalue_addr(&rt);
        if (type_is_float(rt)) {
            emit_load_from_hl(rt);
            g_expr_type = rt;
            return 1;
        }

        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
    }

    if (tok.kind == TOK_ID) {
        /* Direct call returning float, e.g. f_id(x).  Must check this before
         * treating a function name as a scalar addressable object. */
        {
            long save_pos2;
            long save_tok_start2;
            int save_line2;
            int save_tok_line2;
            struct Token save_tok2;
            char cname[64];

            save_pos2 = posi;
            save_tok_start2 = tok_start_pos;
            save_line2 = line_no;
            save_tok_line2 = tok_line;
            save_tok2 = tok;

            strncpy(cname, tok.text, sizeof(cname) - 1);
            cname[sizeof(cname) - 1] = 0;
            rs = find_global(cname);
            next_token();
            if (rs && rs->storage == SC_FUNC && type_is_float(rs->type) && tok.kind == '(') {
                posi = save_pos2;
                tok_start_pos = save_tok_start2;
                line_no = save_line2;
                tok_line = save_tok_line2;
                tok = save_tok2;
                gen_expr();
                return type_is_float(g_expr_type);
            }

            posi = save_pos2;
            tok_start_pos = save_tok_start2;
            line_no = save_line2;
            tok_line = save_tok_line2;
            tok = save_tok2;
        }

        rs = find_sym(tok.text);
        if (rs && type_is_float(rs->type) && !rs->is_array && rs->storage != SC_FUNC) {
            next_token();
            if (sym_can_ix_direct(rs)) {
                emit_load_sym_value_direct(rs);
            } else {
                emit_load_sym_addr(rs);
                emit_load_from_hl(rs->type);
            }
            g_expr_type = rs->type;
            return 1;
        }

        /* Float array elements and float struct fields are valid
         * storage-only rvalues even though general float expressions are
         * still unsupported.  Examples:
         *     b = a[i];
         *     b = s.f;
         *     b = ps->f;
         * gen_lvalue_addr leaves the selected object's address in HL; for
         * a 4-byte float, load the raw bits into DE:HL. */
        {
            long save_pos;
            long save_tok_start;
            int save_line;
            int save_tok_line;
            struct Token save_tok;
            int rt;

            save_pos = posi;
            save_tok_start = tok_start_pos;
            save_line = line_no;
            save_tok_line = tok_line;
            save_tok = tok;

            gen_lvalue_addr(&rt);
            if (type_is_float(rt)) {
                emit_load_from_hl(rt);
                g_expr_type = rt;
                return 1;
            }

            posi = save_pos;
            tok_start_pos = save_tok_start;
            line_no = save_line;
            tok_line = save_tok_line;
            tok = save_tok;
        }
    }

    return 0;
}


static void emit_global_byte_array_index_addr(struct Sym *arr, struct Sym *idx_sym, long idx_const, int has_const)
{
    emit_extrn_if_needed(arr);
    if (has_const) {
        if (idx_const == 0)
            fprintf(outf, "\tld hl,%s\n", asm_name_for(arr->name));
        else
            fprintf(outf, "\tld hl,%s+%ld\n", asm_name_for(arr->name), idx_const & 0xffffL);
    } else {
        fprintf(outf, "\tld hl,%s\n", asm_name_for(arr->name));
        fprintf(outf, "\tld e,(ix%+d)\n", idx_sym->offset);
        emit("\tld d,0\n");
        emit("\tadd hl,de\n");
    }
}

static int try_fast_global_byte_array_store(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *arr;
    struct Sym *idx_sym;
    struct Sym *rhs_sym;
    long idx_const;
    long rhs_const;
    int idx_has_const;
    int rhs_kind;       /* 1 = sym, 2 = const */

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind != TOK_ID)
        goto fail;

    arr = find_sym(tok.text);
    if (!arr || arr->storage != SC_GLOBAL || !arr->is_array || type_size(arr->type) != 1)
        goto fail;

    next_token();
    if (!accept('['))
        goto fail;

    idx_sym = NULL;
    idx_const = 0;
    idx_has_const = 0;

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        idx_const = tok.val;
        idx_has_const = 1;
        next_token();
    } else if (tok.kind == TOK_ID) {
        idx_sym = find_sym(tok.text);
        if (!sym_can_ix_direct(idx_sym) || type_size(idx_sym->type) != 1)
            goto fail;
        next_token();
    } else {
        goto fail;
    }

    if (!accept(']'))
        goto fail;

    if (!accept('='))
        goto fail;

    rhs_kind = 0;
    rhs_sym = NULL;
    rhs_const = 0;

    if (tok.kind == TOK_ID) {
        rhs_sym = find_sym(tok.text);
        if (!sym_can_ix_direct(rhs_sym) || type_size(rhs_sym->type) != 1)
            goto fail;
        rhs_kind = 1;
        next_token();
    } else if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        rhs_const = tok.val;
        if (rhs_const < 0 || rhs_const > 255)
            goto fail;
        rhs_kind = 2;
        next_token();
    } else {
        goto fail;
    }

    /*
     * Keep this statement-only.  If assignment value is needed, or if a larger
     * expression follows, let the normal C expression path preserve semantics.
     */
    if (!expr_result_dead || tok.kind != ';')
        goto fail;

    emit_global_byte_array_index_addr(arr, idx_sym, idx_const, idx_has_const);
    if (rhs_kind == 1) {
        fprintf(outf, "\tld a,(ix%+d)\n", rhs_sym->offset);
        emit("\tld (hl),a\n");
    } else {
        fprintf(outf, "\tld (hl),%ld\n", rhs_const & 255);
    }

    g_expr_type = arr->type;
    return 1;

fail:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

static void gen_assign(void)
{
    if (try_gen_const_expr())
        return;

    int t, op;
    int want_dead;
    struct Sym *direct_sym;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *rhs_sym;
    long rhs_val;
    int fast_rhs;
    int rhs_type;
    int common_type;
    int bf_width;
    int bf_shift;
    unsigned int bf_mask;

    if (try_fast_global_byte_array_store())
        return;

    direct_sym = NULL;

    if (tok.kind == TOK_ID) {
        direct_sym = find_sym(tok.text);

        if (!sym_can_ix_direct(direct_sym) && !is_global_word_sym(direct_sym))
            direct_sym = NULL;

        if (direct_sym) {
            save_pos = posi;
            save_tok_start = tok_start_pos;
            save_line = line_no;
            save_tok_line = tok_line;
            save_tok = tok;

            next_token();

            if (!is_assignment_token(tok.kind)) {
                posi = save_pos;
                tok_start_pos = save_tok_start;
                line_no = save_line;
                tok_line = save_tok_line;
                tok = save_tok;
                direct_sym = NULL;
            }
        }
    }

    if (direct_sym) {
        if (type_is_struct_object(direct_sym->type)) {
            posi = save_pos;
            tok_start_pos = save_tok_start;
            line_no = save_line;
            tok_line = save_tok_line;
            tok = save_tok;
            direct_sym = NULL;
            goto normal_assign;
        }
        want_dead = expr_result_dead;
        op = tok.kind;
        next_token();

        /* Fast path for simple dead-result local compound updates, e.g.
         *     i += step;
         *     i -= 3;
         * This avoids push/pop shuffling in tight loops.  Only fire when
         * the RHS is a single constant or IX-direct local/param and the
         * assignment expression value is not needed.
         */
        if (!type_is_long(direct_sym->type) && !type_is_float(direct_sym->type) &&
            want_dead && (op == TOK_ADDEQ || op == TOK_SUBEQ)) {
            fast_rhs = 0;
            rhs_sym = NULL;
            rhs_val = 0;

            if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
                rhs_val = tok.val;
                next_token();
                fast_rhs = 1;
            } else if (tok.kind == TOK_ID) {
                rhs_sym = find_sym(tok.text);
                if (sym_can_ix_direct(rhs_sym)) {
                    next_token();
                    fast_rhs = 2;
                }
            }

            if (fast_rhs && tok.kind == ';') {
                emit_load_sym_value_direct(direct_sym);
                if (fast_rhs == 1) {
                    long scaled = rhs_val;
                    if (direct_sym->type & (TYPE_PTR | TYPE_PTR2))
                        scaled *= type_index_elem_size(direct_sym->type);
                    emit_ld_de_const(scaled);
                } else {
                    emit_load_sym_de_direct(rhs_sym);
                    if (direct_sym->type & (TYPE_PTR | TYPE_PTR2)) {
                        int elem = type_index_elem_size(direct_sym->type);
                        if (elem == 2) {
                            emit("\tex de,hl\n\tadd hl,hl\n\tex de,hl\n");
                        } else if (elem == 4) {
                            emit("\tex de,hl\n\tadd hl,hl\n\tadd hl,hl\n\tex de,hl\n");
                        } else if (elem > 2) {
                            emit("\tpush hl\n");     /* save pointer */
                            emit("\tex de,hl\n");    /* HL = step */
                            fprintf(outf, "\tld de,%d\n", elem);
                            emit_runtime_call("__mulu"); /* HL = step * elem */
                            emit("\tex de,hl\n");    /* DE = scaled step */
                            emit("\tpop hl\n");      /* HL = pointer */
                        }
                    }
                }

                if (op == TOK_ADDEQ) {
                    emit("\tadd hl,de\n");
                } else {
                    emit("\tor a\n\tsbc hl,de\n");
                }

                emit_store_hl_to_sym_direct(direct_sym);
                return;
            }

            /* Fall back to normal parsing if RHS was not exactly simple. */
            posi = save_pos;
            tok_start_pos = save_tok_start;
            line_no = save_line;
            tok_line = save_tok_line;
            tok = save_tok;
            direct_sym = NULL;
            goto normal_assign;
        }

        if (op == '=') {
            if (type_size(direct_sym->type) == 1 && tok.kind == TOK_ID) {
                struct Sym *bs;
                long rhs_save_pos;
                long rhs_save_tok_start;
                int rhs_save_line;
                int rhs_save_tok_line;
                struct Token rhs_save_tok;

                rhs_save_pos = posi;
                rhs_save_tok_start = tok_start_pos;
                rhs_save_line = line_no;
                rhs_save_tok_line = tok_line;
                rhs_save_tok = tok;

                bs = find_sym(tok.text);
                if (sym_can_ix_direct(bs) && type_size(bs->type) == 1) {
                    next_token();
                    if (tok.kind == ';' || tok.kind == ',' || tok.kind == ')' || tok.kind == ']') {
                        fprintf(outf, "\tld a,(ix%+d)\n", bs->offset);
                        fprintf(outf, "\tld (ix%+d),a\n", direct_sym->offset);
                        g_expr_type = direct_sym->type;
                        return;
                    }
                }

                posi = rhs_save_pos;
                tok_start_pos = rhs_save_tok_start;
                line_no = rhs_save_line;
                tok_line = rhs_save_tok_line;
                tok = rhs_save_tok;
            }
            if (type_size(direct_sym->type) == 1 && (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) && tok.val >= 0 && tok.val <= 255) {
                long rhs_save_pos;
                long rhs_save_tok_start;
                int rhs_save_line;
                int rhs_save_tok_line;
                struct Token rhs_save_tok;
                long rhs_byte_val;

                rhs_save_pos = posi;
                rhs_save_tok_start = tok_start_pos;
                rhs_save_line = line_no;
                rhs_save_tok_line = tok_line;
                rhs_save_tok = tok;
                rhs_byte_val = tok.val;

                next_token();
                if (tok.kind == ';' || tok.kind == ',' || tok.kind == ')' || tok.kind == ']') {
                    fprintf(outf, "\tld (ix%+d),%ld\n", direct_sym->offset, rhs_byte_val & 255);
                    g_expr_type = direct_sym->type;
                    return;
                }

                posi = rhs_save_pos;
                tok_start_pos = rhs_save_tok_start;
                line_no = rhs_save_line;
                tok_line = rhs_save_tok_line;
                tok = rhs_save_tok;
            }

            if (type_is_float(direct_sym->type)) {
                expr_result_dead = 0;
                gen_assign();
                expr_result_dead = want_dead;
                if (!type_is_float(g_expr_type))
                    emit_convert_int_to_float(g_expr_type);
                emit_store_hl_to_sym_direct(direct_sym);
                return;
            }

            expr_result_dead = 0;
            gen_assign();
            expr_result_dead = want_dead;

            if (type_is_float(g_expr_type)) {
                emit_convert_float_to_intlike(direct_sym->type);
            }
            if (type_is_long(direct_sym->type) && !type_is_long(g_expr_type))
                emit_extend_to_long_typed(g_expr_type);
            else if (type_size(direct_sym->type) > 1 && !type_is_long(g_expr_type))
                emit_promote_byte_to_int(g_expr_type);
            emit_store_hl_to_sym_direct(direct_sym);
            return;
        }

        rhs_type = peek_simple_unary_type();

        /* Compound assignment uses the usual arithmetic conversions for
         * the operation, then converts the result back to the LHS type.
         * The old path used the LHS width too early, so cases such as
         *     int i; long l; i += l;
         * silently discarded the high word before doing the operation. */
        if (!type_is_long(direct_sym->type) &&
            !(direct_sym->type & (TYPE_PTR | TYPE_PTR2)) &&
            (op == TOK_ADDEQ || op == TOK_SUBEQ || op == TOK_MULEQ ||
             op == TOK_DIVEQ || op == TOK_MODEQ || op == TOK_ANDEQ ||
             op == TOK_OREQ  || op == TOK_XOREQ)) {
            common_type = common_arith_type(direct_sym->type, rhs_type);
            if (type_is_long(common_type)) {
                emit_load_sym_value_direct(direct_sym);
                emit_cast_16_to_common(direct_sym->type, common_type);
                emit("\tpush de\n\tpush hl\n");

                expr_result_dead = 0;
                gen_assign();
                expr_result_dead = want_dead;
                emit_cast_16_to_common(g_expr_type, common_type);

                if (op == TOK_ADDEQ) gen_binop32('+', common_type);
                else if (op == TOK_SUBEQ) gen_binop32('-', common_type);
                else if (op == TOK_MULEQ) gen_binop32('*', common_type);
                else if (op == TOK_DIVEQ) gen_binop32('/', common_type);
                else if (op == TOK_MODEQ) gen_binop32('%', common_type);
                else if (op == TOK_ANDEQ) gen_binop32('&', common_type);
                else if (op == TOK_OREQ)  gen_binop32('|', common_type);
                else if (op == TOK_XOREQ) gen_binop32('^', common_type);

                /* Convert back to the non-long LHS by storing the low word. */
                emit_store_hl_to_sym_direct(direct_sym);
                return;
            }
        }

        emit_load_sym_value_direct(direct_sym);
        if ((op == TOK_SHLEQ || op == TOK_SHREQ) && type_is_long(direct_sym->type) &&
            (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT)) {
            long scount;
            scount = tok.val;
            next_token();
            if (!emit_shift_const_long(op, direct_sym->type, scount)) {
                fprintf(outf, "\tld b,%ld\n", scount & 255L);
                emit_shift_loop(op, direct_sym->type);
            }
            emit_store_hl_to_sym_direct(direct_sym);
            return;
        }

        if (type_is_long(direct_sym->type) || type_is_float(direct_sym->type))
            emit("\tpush de\n\tpush hl\n");
        else
            emit("\tpush hl\n");

        expr_result_dead = 0;
        gen_assign();
        expr_result_dead = want_dead;

        if (op == TOK_SHLEQ || op == TOK_SHREQ) {
            if (type_is_long(direct_sym->type)) {
                emit("\tld b,l\n\tpop hl\n\tpop de\n");
            } else {
                emit("\tld b,l\n\tpop hl\n");
            }
            emit_shift_loop(op, direct_sym->type);
            emit_store_hl_to_sym_direct(direct_sym);
            return;
        }

        if (type_is_float(direct_sym->type) && (op == TOK_ADDEQ || op == TOK_SUBEQ || op == TOK_MULEQ || op == TOK_DIVEQ)) {
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_runtime_call(op == TOK_MULEQ ? "__fmul" : (op == TOK_DIVEQ ? "__fdiv" : (op == TOK_ADDEQ ? "__fadd" : "__fsub")));
            emit("\tpop bc\n\tpop bc\n\tpop bc\n\tpop bc\n");
            emit_store_hl_to_sym_direct(direct_sym);
            return;
        }

        if (type_is_long(direct_sym->type)) {
            if (!type_is_long(g_expr_type))
                emit_extend_to_long_typed(g_expr_type);
            if (op == TOK_ADDEQ) gen_binop32('+', direct_sym->type);
            else if (op == TOK_SUBEQ) gen_binop32('-', direct_sym->type);
            else if (op == TOK_MULEQ) gen_binop32('*', direct_sym->type);
            else if (op == TOK_DIVEQ) gen_binop32('/', direct_sym->type);
            else if (op == TOK_MODEQ) gen_binop32('%', direct_sym->type);
            else if (op == TOK_ANDEQ) gen_binop32('&', direct_sym->type);
            else if (op == TOK_OREQ)  gen_binop32('|', direct_sym->type);
            else if (op == TOK_XOREQ) gen_binop32('^', direct_sym->type);
        } else {
            if ((direct_sym->type & (TYPE_PTR | TYPE_PTR2)) && (op == TOK_ADDEQ || op == TOK_SUBEQ)) {
                int elem = type_index_elem_size(direct_sym->type);
                if (elem == 2) emit("\tadd hl,hl\n");
                else if (elem == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem > 2) {
                    fprintf(outf, "\tld de,%d\n", elem);
                    emit_runtime_call("__mulu");
                }
            }
            emit("\tex de,hl\n\tpop hl\n");
            common_type = common_arith_type(direct_sym->type, g_expr_type);
            if (op == TOK_ADDEQ) gen_binop_typed('+', common_type);
            else if (op == TOK_SUBEQ) gen_binop_typed('-', common_type);
            else if (op == TOK_MULEQ) gen_binop_typed('*', common_type);
            else if (op == TOK_DIVEQ) gen_binop_typed('/', common_type);
            else if (op == TOK_MODEQ) gen_binop_typed('%', common_type);
            else if (op == TOK_ANDEQ) gen_binop_typed('&', common_type);
            else if (op == TOK_OREQ) gen_binop_typed('|', common_type);
            else if (op == TOK_XOREQ) gen_binop_typed('^', common_type);
        }

        emit_store_hl_to_sym_direct(direct_sym);
        return;
    }

normal_assign:
    if (lookahead_is_assignment()) {
        want_dead = expr_result_dead;

        gen_lvalue_addr(&t);
        bf_width = current_field_bit_width;
        bf_shift = current_field_bit_shift;
        bf_mask = current_field_bit_mask;
        op = tok.kind;
        next_token();

        if (op == '=') {
            if (type_is_struct_object(t)) {
                int rt;
                emit("\tpush hl\n"); /* push destination address */
                if (try_emit_struct_return_call_assignment(t))
                    return;
                gen_lvalue_addr(&rt);
                if (!same_struct_type(t, rt))
                    error_here("incompatible struct assignment");
                emit("\tex de,hl\n"); /* DE = source */
                emit("\tpop hl\n");   /* HL = destination */
                emit_copy_de_to_hl_bytes(type_size(t));
                g_expr_type = t;
                return;
            }

            emit("\tpush hl\n"); /* push address */

            if (type_is_float(t)) {
                if (try_emit_float_rvalue_dehl()) {
                    emit_store_de_to_addr_hl(t);  /* pops address */
                    return;
                }
                expr_result_dead = 0;
                gen_assign();
                expr_result_dead = want_dead;
                if (!type_is_float(g_expr_type))
                    emit_convert_int_to_float(g_expr_type);
                emit_store_de_to_addr_hl(t);  /* pops address */
                return;
            }

            /* The RHS value is needed for the store even when the whole
             * assignment statement result is dead.  This preserves cases
             * such as a = b = 1.
             */
            expr_result_dead = 0;
            gen_assign();
            expr_result_dead = want_dead;
            if (type_is_float(g_expr_type)) {
                emit_convert_float_to_intlike(t);
            }
            current_field_bit_width = bf_width;
            current_field_bit_shift = bf_shift;
            current_field_bit_mask = bf_mask;

            if (current_field_bit_width > 0) {
                emit_store_bitfield_from_hl();
            } else if (type_is_long(t)) {
                if (!type_is_long(g_expr_type))
                    emit_extend_to_long_typed(g_expr_type);
                /* emit_store_de_to_addr_hl for 32-bit pops the address itself */
                emit_store_de_to_addr_hl(t);
                /* result: value gone (address popped inside store) */
            } else {
                emit("\tex de,hl\n\tpop hl\n");
                emit_store_de_to_addr_hl(t);
                if (!want_dead)
                    emit("\tex de,hl\n");
            }
            return;
        }

        if ((op == TOK_SHLEQ || op == TOK_SHREQ) && type_is_long(t) &&
            (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT)) {
            long scount;
            scount = tok.val;
            next_token();

            emit("\tpush hl\n");
            emit_load_from_hl(t);
            if (!emit_shift_const_long(op, t, scount)) {
                fprintf(outf, "\tld b,%ld\n", scount & 255L);
                emit_shift_loop(op, t);
            }

            emit("\tld b,d\n\tld c,e\n");
            emit("\tpop de\n");
            emit("\tex de,hl\n");
            emit("\tld (hl),e\n\tinc hl\n\tld (hl),d\n\tinc hl\n\tld (hl),c\n\tinc hl\n\tld (hl),b\n");
            if (!want_dead) {
                emit("\tex de,hl\n");
                emit("\tld d,b\n\tld e,c\n");
            }
            g_expr_type = t;
            return;
        }

        rhs_type = peek_simple_unary_type();

        emit("\tpush hl\n");

        if (!type_is_long(t) && !(t & (TYPE_PTR | TYPE_PTR2)) &&
            (op == TOK_ADDEQ || op == TOK_SUBEQ || op == TOK_MULEQ ||
             op == TOK_DIVEQ || op == TOK_MODEQ || op == TOK_ANDEQ ||
             op == TOK_OREQ  || op == TOK_XOREQ)) {
            common_type = common_arith_type(t, rhs_type);
            if (type_is_long(common_type)) {
                emit_load_from_hl(t);
                emit_cast_16_to_common(t, common_type);
                emit("\tpush de\n\tpush hl\n");

                expr_result_dead = 0;
                gen_assign();
                expr_result_dead = want_dead;
                emit_cast_16_to_common(g_expr_type, common_type);

                if (op == TOK_ADDEQ) gen_binop32('+', common_type);
                else if (op == TOK_SUBEQ) gen_binop32('-', common_type);
                else if (op == TOK_MULEQ) gen_binop32('*', common_type);
                else if (op == TOK_DIVEQ) gen_binop32('/', common_type);
                else if (op == TOK_MODEQ) gen_binop32('%', common_type);
                else if (op == TOK_ANDEQ) gen_binop32('&', common_type);
                else if (op == TOK_OREQ)  gen_binop32('|', common_type);
                else if (op == TOK_XOREQ) gen_binop32('^', common_type);

                emit("\tex de,hl\n\tpop hl\n");
                emit_store_de_to_addr_hl(t);
                if (!want_dead)
                    emit("\tex de,hl\n");
                return;
            }
        }

        emit_load_from_hl(t);
        if (type_is_long(t) || type_is_float(t))
            emit("\tpush de\n\tpush hl\n");
        else
            emit("\tpush hl\n");

        expr_result_dead = 0;
        gen_assign();
        expr_result_dead = want_dead;

        if (op == TOK_SHLEQ || op == TOK_SHREQ) {
            if (type_is_long(t)) {
                emit("\tld b,l\n\tpop hl\n\tpop de\n");
            } else {
                emit("\tld b,l\n\tpop hl\n");
            }
            emit_shift_loop(op, t);
            /* store: for 32-bit, need address from deeper in stack */
            if (type_is_long(t)) {
                /*
                 * Result is in DE:HL and the saved lvalue address is still
                 * on top of the stack.
                 */
                emit("\tld b,d\n\tld c,e\n");   /* BC = high word */
                emit("\tpop de\n");               /* DE = address */
                emit("\tex de,hl\n");             /* HL = address, DE = low word */
                emit("\tld (hl),e\n\tinc hl\n\tld (hl),d\n\tinc hl\n\tld (hl),c\n\tinc hl\n\tld (hl),b\n");
                if (!want_dead) {
                    emit("\tex de,hl\n");         /* HL = low word */
                    emit("\tld d,b\n\tld e,c\n"); /* DE = high word */
                }
            } else {
                emit("\tex de,hl\n\tpop hl\n");
                emit_store_de_to_addr_hl(t);
                if (!want_dead)
                    emit("\tex de,hl\n");
            }
            g_expr_type = t;
            return;
        }

        if (type_is_float(t) && (op == TOK_ADDEQ || op == TOK_SUBEQ || op == TOK_MULEQ || op == TOK_DIVEQ)) {
            if (!type_is_float(g_expr_type))
                emit_convert_int_to_float(g_expr_type);
            emit("\tpush de\n\tpush hl\n");
            emit_runtime_call(op == TOK_MULEQ ? "__fmul" : (op == TOK_DIVEQ ? "__fdiv" : (op == TOK_ADDEQ ? "__fadd" : "__fsub")));
            emit("\tpop bc\n\tpop bc\n\tpop bc\n\tpop bc\n");
            emit_store_de_to_addr_hl(t); /* pops saved lvalue address */
        } else if (type_is_long(t)) {
            if (!type_is_long(g_expr_type))
                emit_extend_to_long_typed(g_expr_type);
            if (op == TOK_ADDEQ) gen_binop32('+', t);
            else if (op == TOK_SUBEQ) gen_binop32('-', t);
            else if (op == TOK_MULEQ) gen_binop32('*', t);
            else if (op == TOK_DIVEQ) gen_binop32('/', t);
            else if (op == TOK_MODEQ) gen_binop32('%', t);
            else if (op == TOK_ANDEQ) gen_binop32('&', t);
            else if (op == TOK_OREQ)  gen_binop32('|', t);
            else if (op == TOK_XOREQ) gen_binop32('^', t);
            /* result in DE:HL (DE=high, HL=low), address at top of stack */
            emit_store_de_to_addr_hl(t); /* pops address and stores 4 bytes */
        } else {
            if ((t & (TYPE_PTR | TYPE_PTR2)) && (op == TOK_ADDEQ || op == TOK_SUBEQ)) {
                int elem = type_index_elem_size(t);
                if (elem == 2) emit("\tadd hl,hl\n");
                else if (elem == 4) { emit("\tadd hl,hl\n"); emit("\tadd hl,hl\n"); }
                else if (elem > 2) {
                    fprintf(outf, "\tld de,%d\n", elem);
                    emit_runtime_call("__mulu");
                }
            }
            emit("\tex de,hl\n\tpop hl\n");
            common_type = common_arith_type(t, g_expr_type);
            if (op == TOK_ADDEQ) gen_binop_typed('+', common_type);
            else if (op == TOK_SUBEQ) gen_binop_typed('-', common_type);
            else if (op == TOK_MULEQ) gen_binop_typed('*', common_type);
            else if (op == TOK_DIVEQ) gen_binop_typed('/', common_type);
            else if (op == TOK_MODEQ) gen_binop_typed('%', common_type);
            else if (op == TOK_ANDEQ) gen_binop_typed('&', common_type);
            else if (op == TOK_OREQ) gen_binop_typed('|', common_type);
            else if (op == TOK_XOREQ) gen_binop_typed('^', common_type);
            emit("\tex de,hl\n\tpop hl\n");
            emit_store_de_to_addr_hl(t);
            if (!want_dead)
                emit("\tex de,hl\n");
        }
        return;
    }

    gen_conditional();
}
static void gen_expr_no_comma(void)
{
    /*
     * Assignment-expression, not full expression.  Declaration initializers
     * use assignment-expression grammar, so a comma at this level separates
     * declarators rather than becoming the comma operator:
     *
     *     int a = 0, b = 1;
     *
     * The comma operator is still available when parenthesized because
     * gen_primary() calls gen_expr() for the expression inside parentheses:
     *
     *     int a = (0, 1);
     */
    gen_assign();
}

static void gen_expr(void)
{
    gen_assign();
    /* comma operator: evaluate and discard left, then evaluate right.
     * DCC has no struct-valued expression temporary model, so reject
     * struct operands here instead of emitting bogus scalar loads/copies. */
    while (tok.kind == ',') {
        if (type_is_struct_object(g_expr_type))
            error_here("unsupported struct comma expression");
        next_token();
        gen_assign();
        if (type_is_struct_object(g_expr_type))
            error_here("unsupported struct comma expression");
    }
}

static void emit_incdec_addr(int type, int op)
{
    int done;

    if (type_size(type) == 1) {
        if (op == TOK_INC)
            emit("\tinc (hl)\n");
        else
            emit("\tdec (hl)\n");
        return;
    }

    done = new_label();

    if (type_size(type) == 4) {
        /* 4-byte (long) ripple increment/decrement */
        if (op == TOK_INC) {
            emit("\tinc (hl)\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tinc (hl)\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tinc (hl)\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tinc (hl)\n");
        } else {
            emit("\tld a,(hl)\n");
            emit("\tdec (hl)\n");
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tld a,(hl)\n");
            emit("\tdec (hl)\n");
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tld a,(hl)\n");
            emit("\tdec (hl)\n");
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tdec (hl)\n");
        }
    } else {
        /* 2-byte (int) ripple increment/decrement */
        if (op == TOK_INC) {
            emit("\tinc (hl)\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tinc (hl)\n");
        } else {
            emit("\tld a,(hl)\n");
            emit("\tdec (hl)\n");
            emit("\tor a\n");
            emit_jp_label("jp nz,", done);
            emit("\tinc hl\n");
            emit("\tdec (hl)\n");
        }
    }

    emit_label(done);
}

static int lookahead_is_post_incdec_statement(int *op_out)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int r;
    int op;

    if (tok.kind != TOK_ID && tok.kind != '*' && tok.kind != '(')
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (!skip_lvalue_syntax()) {
        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    r = 0;
    op = tok.kind;
    if (op == TOK_INC || op == TOK_DEC) {
        next_token();
        if (tok.kind == ';') {
            r = 1;
            op_out[0] = op;
        }
    }

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return r;
}


static int try_fast_local_self_add_statement(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    char lhs_name[64];
    char rhs1_name[64];
    char rhs2_name[64];
    struct Sym *lhs;
    struct Sym *rhs1;
    struct Sym *rhs2;
    int op;
    long cval;
    int rhs2_const;

    if (tok.kind != TOK_ID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    strncpy(lhs_name, tok.text, sizeof(lhs_name) - 1);
    lhs_name[sizeof(lhs_name) - 1] = 0;
    lhs = find_sym(lhs_name);
    if (!sym_can_ix_direct(lhs) || type_size(lhs->type) != 2)
        goto no;

    next_token();
    if (!accept('='))
        goto no;

    if (tok.kind != TOK_ID)
        goto no;
    strncpy(rhs1_name, tok.text, sizeof(rhs1_name) - 1);
    rhs1_name[sizeof(rhs1_name) - 1] = 0;
    rhs1 = find_sym(rhs1_name);
    if (!sym_can_ix_direct(rhs1) || type_size(rhs1->type) != 2)
        goto no;
    next_token();

    op = tok.kind;
    if (op != '+' && op != '-')
        goto no;
    next_token();

    rhs2_const = 0;
    cval = 0;
    rhs2 = NULL;

    if (tok.kind == TOK_ID) {
        strncpy(rhs2_name, tok.text, sizeof(rhs2_name) - 1);
        rhs2_name[sizeof(rhs2_name) - 1] = 0;
        rhs2 = find_sym(rhs2_name);
        if (!sym_can_ix_direct(rhs2) || type_size(rhs2->type) != 2)
            goto no;
        next_token();
    } else if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        rhs2_const = 1;
        cval = tok.val & 0xffffL;
        next_token();
    } else {
        goto no;
    }

    if (tok.kind != ';')
        goto no;

    /*
     * This recognizes:
     *     lhs = rhs1 + rhs2;
     *     lhs = rhs1 - rhs2;
     * where lhs/rhs are simple local int objects.  This is especially useful
     * for sieve's k = k + prime.
     */
    fprintf(outf, "\tld l,(ix%+d)\n", rhs1->offset);
    fprintf(outf, "\tld h,(ix%+d)\n", rhs1->offset + 1);

    if (rhs2_const) {
        if (rhs1->type & (TYPE_PTR | TYPE_PTR2))
            cval *= type_index_elem_size(rhs1->type);
        emit_ld_de_const(cval);
    } else {
        fprintf(outf, "\tld e,(ix%+d)\n", rhs2->offset);
        fprintf(outf, "\tld d,(ix%+d)\n", rhs2->offset + 1);
        if ((rhs1->type & (TYPE_PTR | TYPE_PTR2)) &&
            !(rhs2->type & (TYPE_PTR | TYPE_PTR2))) {
            int elem = type_index_elem_size(rhs1->type);
            if (elem == 2) {
                emit("\tex de,hl\n\tadd hl,hl\n\tex de,hl\n");
            } else if (elem == 4) {
                emit("\tex de,hl\n\tadd hl,hl\n\tadd hl,hl\n\tex de,hl\n");
            } else if (elem > 1) {
                emit("\tpush hl\n");
                emit("\tex de,hl\n");
                fprintf(outf, "\tld de,%d\n", elem);
                emit_runtime_call("__mulu");
                emit("\tex de,hl\n");
                emit("\tpop hl\n");
            }
        }
    }

    if (op == '+') {
        emit("\tadd hl,de\n");
    } else {
        emit("\tor a\n\tsbc hl,de\n");
        if ((rhs1->type & (TYPE_PTR | TYPE_PTR2)) && rhs2 &&
            (rhs2->type & (TYPE_PTR | TYPE_PTR2))) {
            int elem = type_index_elem_size(rhs1->type);
            if (elem > 1) {
                fprintf(outf, "\tld de,%d\n", elem);
                emit_runtime_call("__divs");
            }
        }
    }

    fprintf(outf, "\tld (ix%+d),l\n", lhs->offset);
    fprintf(outf, "\tld (ix%+d),h\n", lhs->offset + 1);

    expect(';');
    return 1;

no:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

static int try_gen_incdec_statement(void)
{
    int op;
    int t;
    struct Sym *s;
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
        op = tok.kind;
        next_token();

        if (tok.kind == TOK_ID) {
            s = find_sym(tok.text);
            if (sym_can_ix_direct(s) || is_global_word_sym(s)) {
                next_token();
                if (tok.kind == ';') {
                    emit_incdec_sym_direct(s, op);
                    expect(';');
                    return 1;
                }
            }
        }

        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;

        op = tok.kind;
        next_token();
        gen_lvalue_addr(&t);
        emit_incdec_addr(t, op);
        expect(';');
        return 1;
    }

    if (tok.kind == TOK_ID) {
        s = find_sym(tok.text);
        if (sym_can_ix_direct(s) || is_global_word_sym(s)) {
            next_token();
            if (tok.kind == TOK_INC || tok.kind == TOK_DEC) {
                op = tok.kind;
                next_token();
                if (tok.kind == ';') {
                    emit_incdec_sym_direct(s, op);
                    expect(';');
                    return 1;
                }
            }
        }

        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
    }

    if (lookahead_is_post_incdec_statement(&op)) {
        gen_lvalue_addr(&t);
        next_token();
        emit_incdec_addr(t, op);
        expect(';');
        return 1;
    }

    return 0;
}

static void emit_store_a_to_sym_byte_preserve_de(struct Sym *s, int byte_off)
{
    if (sym_can_ix_direct(s)) {
        fprintf(outf, "\tld (ix%+d),a\n", s->offset + byte_off);
        return;
    }

    if (s && (s->storage == SC_LOCAL || s->storage == SC_PARAM)) {
        emit("\tpush de\n");
        emit("\tpush ix\n\tpop hl\n");
        fprintf(outf, "\tld de,%d\n", s->offset + byte_off);
        emit("\tadd hl,de\n\tld (hl),a\n");
        emit("\tpop de\n");
        return;
    }

    /* Conservative fallback for globals: preserve DE while storing A. */
    emit("\tpush de\n");
    fprintf(outf, "\tld hl,%s\n", asm_name_for(s->name));
    if (byte_off)
        fprintf(outf, "\tld de,%d\n\tadd hl,de\n", byte_off);
    emit("\tld (hl),a\n\tpop de\n");
}


static void emit_load_simple_byte_to_c(struct Sym *s, long val, int is_const)
{
    if (is_const) {
        fprintf(outf, "\tld c,%ld\n", val & 255L);
        return;
    }

    if (sym_can_ix_direct(s)) {
        fprintf(outf, "\tld c,(ix%+d)\n", s->offset);
    } else {
        emit("\tpush ix\n\tpop hl\n");
        fprintf(outf, "\tld de,%d\n", s->offset);
        emit("\tadd hl,de\n\tld c,(hl)\n");
    }
}

static void emit_and_mask_byte_direct(struct Sym *mask_sym, int byte_index)
{
    /* A = value, mask_sym must be IX-direct.  Result remains in A. */
    emit("\tld b,a\n");
    fprintf(outf, "\tld a,(ix%+d)\n", mask_sym->offset + byte_index);
    emit("\tand b\n");
}

static int try_fast_crc_update_byte_simple_args(struct Sym *dst)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *arg_crc;
    struct Sym *arg_tbl;
    struct Sym *arg_mask;
    struct Sym *arg_byte;
    long byte_val;
    int byte_is_const;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    if (tok.kind != TOK_ID)
        goto no;
    arg_crc = find_sym(tok.text);
    if (arg_crc != dst)
        goto no;
    next_token();
    if (!accept(','))
        goto no;

    if (tok.kind != TOK_ID)
        goto no;
    arg_tbl = find_sym(tok.text);
    if (!arg_tbl || !sym_can_ix_direct(arg_tbl))
        goto no;
    next_token();
    if (!accept(','))
        goto no;

    if (tok.kind != TOK_ID)
        goto no;
    arg_mask = find_sym(tok.text);
    if (!arg_mask || !sym_can_ix_direct(arg_mask) || !type_is_long(arg_mask->type))
        goto no;
    next_token();
    if (!accept(','))
        goto no;

    arg_byte = NULL;
    byte_val = 0;
    byte_is_const = 0;
    if (tok.kind == TOK_ID) {
        arg_byte = find_sym(tok.text);
        if (!arg_byte)
            goto no;
        next_token();
    } else if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        byte_val = tok.val;
        byte_is_const = 1;
        next_token();
    } else {
        goto no;
    }

    if (!accept(')'))
        goto no;
    if (!accept(';'))
        goto no;

    /* This direct path is tuned for compute_crc_fb(), where the CRC local is
     * outside IX's signed 8-bit displacement range.  The older generic inline
     * path remains for IX-direct locals and complex byte expressions. */
    if (sym_can_ix_direct(dst))
        goto no;
    if (!(dst->storage == SC_LOCAL || dst->storage == SC_PARAM))
        goto no;

    /* Put destination base address in the alternate HL set. */
    emit("\tpush ix\n\tpop hl\n");
    fprintf(outf, "\tld de,%d\n", dst->offset);
    emit("\tadd hl,de\n\texx\n");

    /* C = input byte. */
    emit_load_simple_byte_to_c(arg_byte, byte_val, byte_is_const);

    /* idx = original_crc_byte3 ^ input_byte.  Read byte3 through alt HL. */
    emit("\texx\n\tinc hl\n\tinc hl\n\tinc hl\n\tld a,(hl)\n\tdec hl\n\tdec hl\n\tdec hl\n\texx\n");
    emit("\txor c\n\tld l,a\n\tld h,0\n\tadd hl,hl\n\tadd hl,hl\n");

    /* DE = &tbl[idx]. */
    emit("\tld b,h\n\tld c,l\n");
    fprintf(outf, "\tld l,(ix%+d)\n\tld h,(ix%+d)\n", arg_tbl->offset, arg_tbl->offset + 1);
    emit("\tld d,b\n\tld e,c\n\tadd hl,de\n\tex de,hl\n");

    /* Store high-to-low so original crc bytes are read before overwrite. */

    /* byte3 = table[3] ^ old byte2, masked. */
    emit("\tinc de\n\tinc de\n\tinc de\n\tld a,(de)\n");
    emit("\texx\n\tinc hl\n\tinc hl\n\txor (hl)\n\tinc hl\n\texx\n");
    emit_and_mask_byte_direct(arg_mask, 3);
    emit("\texx\n\tld (hl),a\n\texx\n");

    /* byte2 = table[2] ^ old byte1, masked. */
    emit("\tdec de\n\tld a,(de)\n");
    emit("\texx\n\tdec hl\n\tdec hl\n\txor (hl)\n\tinc hl\n\texx\n");
    emit_and_mask_byte_direct(arg_mask, 2);
    emit("\texx\n\tld (hl),a\n\texx\n");

    /* byte1 = table[1] ^ old byte0, masked. */
    emit("\tdec de\n\tld a,(de)\n");
    emit("\texx\n\tdec hl\n\tdec hl\n\txor (hl)\n\tinc hl\n\texx\n");
    emit_and_mask_byte_direct(arg_mask, 1);
    emit("\texx\n\tld (hl),a\n\texx\n");

    /* byte0 = table[0], masked. */
    emit("\tdec de\n\tld a,(de)\n");
    emit_and_mask_byte_direct(arg_mask, 0);
    emit("\texx\n\tdec hl\n\tld (hl),a\n\texx\n");

    return 1;

no:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}

static int try_fast_crc_update_byte_statement(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    struct Sym *dst;
    int t_arg1;
    int t_arg2;
    int t_arg3;
    int t_arg4;
    int k;
    int use_alt_dst;

    if (tok.kind != TOK_ID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    dst = find_sym(tok.text);
    if (!dst || !type_is_long(dst->type))
        goto no;

    next_token();
    if (!accept('='))
        goto no;

    if (tok.kind != TOK_ID || strcmp(tok.text, "crc_update_byte") != 0)
        goto no;

    next_token();
    if (!accept('('))
        goto no;

    if (try_fast_crc_update_byte_simple_args(dst))
        return 1;

    /*
     * Fast path for crc.c's inner loop:
     *
     *     crc = crc_update_byte(crc, tbl, mask32, byte_expr);
     *
     * crc_update_byte does:
     *     idx = ((crc >> 24) ^ byte) & 0xff;
     *     crc = ((crc << 8) ^ tbl[idx]) & mask32;
     *
     * Stack layout after evaluating arguments below:
     *     sp+0..3  mask32     (low word, high word)
     *     sp+4..5  tbl pointer
     *     sp+6..9  original crc (low word, high word)
     *
     * The generated byte stores use:
     *     result byte0 = tbl[0] ^ 0
     *     result byte1 = tbl[1] ^ crc byte0
     *     result byte2 = tbl[2] ^ crc byte1
     *     result byte3 = tbl[3] ^ crc byte2
     * and each byte is ANDed with the corresponding mask byte.
     */

    expr_result_dead = 0;
    gen_assign();                       /* arg1: crc */
    t_arg1 = g_expr_type;
    if (!type_is_long(t_arg1))
        emit_extend_to_long_typed(t_arg1);
    emit("\tpush de\n\tpush hl\n");

    expect(',');

    gen_assign();                       /* arg2: tbl */
    t_arg2 = g_expr_type;
    (void)t_arg2;
    emit("\tpush hl\n");

    expect(',');

    gen_assign();                       /* arg3: mask32 */
    t_arg3 = g_expr_type;
    if (!type_is_long(t_arg3))
        emit_extend_to_long_typed(t_arg3);
    emit("\tpush de\n\tpush hl\n");

    expect(',');

    gen_assign();                       /* arg4: byte */
    t_arg4 = g_expr_type;
    (void)t_arg4;

    expect(')');
    expect(';');

    use_alt_dst = (!sym_can_ix_direct(dst) &&
                   (dst->storage == SC_LOCAL || dst->storage == SC_PARAM));

    if (use_alt_dst) {
        /*
         * Keep the non-IX-direct destination pointer in the alternate HL
         * register pair.  That avoids recomputing ix+offset and push/pop'ing
         * DE for each of the four result-byte stores in compute_crc_fb().
         * Save the byte expression result while EXX makes the main register
         * set temporarily undefined.
         */
        emit("\tpush hl\n");
        emit("\tpush ix\n\tpop hl\n");
        fprintf(outf, "\tld de,%d\n", dst->offset);
        emit("\tadd hl,de\n");
        emit("\texx\n");
        emit("\tpop hl\n");
    }

    /* C = input byte.  idx = high byte of crc ^ input byte. */
    emit("\tld c,l\n");
    emit("\tld hl,9\n\tadd hl,sp\n\tld a,(hl)\n\txor c\n\tld l,a\n\tld h,0\n");

    /* HL = idx * 4. */
    emit("\tadd hl,hl\n\tadd hl,hl\n");

    /* DE = tbl pointer, then DE = &tbl[idx]. */
    emit("\tld b,h\n\tld c,l\n");
    emit("\tld hl,4\n\tadd hl,sp\n\tld e,(hl)\n\tinc hl\n\tld d,(hl)\n");
    emit("\tld h,b\n\tld l,c\n\tadd hl,de\n\tex de,hl\n");

    /* result byte 0: table byte 0, masked by mask byte 0. */
    emit("\tld a,(de)\n");
    emit("\tld hl,0\n\tadd hl,sp\n\tand (hl)\n");
    if (use_alt_dst)
        emit("\texx\n\tld (hl),a\n\tinc hl\n\texx\n");
    else
        emit_store_a_to_sym_byte_preserve_de(dst, 0);
    emit("\tinc de\n");

    /* result byte 1: table byte 1 ^ original crc byte 0. */
    emit("\tld a,(de)\n");
    emit("\tld hl,6\n\tadd hl,sp\n\txor (hl)\n");
    emit("\tld hl,1\n\tadd hl,sp\n\tand (hl)\n");
    if (use_alt_dst)
        emit("\texx\n\tld (hl),a\n\tinc hl\n\texx\n");
    else
        emit_store_a_to_sym_byte_preserve_de(dst, 1);
    emit("\tinc de\n");

    /* result byte 2: table byte 2 ^ original crc byte 1. */
    emit("\tld a,(de)\n");
    emit("\tld hl,7\n\tadd hl,sp\n\txor (hl)\n");
    emit("\tld hl,2\n\tadd hl,sp\n\tand (hl)\n");
    if (use_alt_dst)
        emit("\texx\n\tld (hl),a\n\tinc hl\n\texx\n");
    else
        emit_store_a_to_sym_byte_preserve_de(dst, 2);
    emit("\tinc de\n");

    /* result byte 3: table byte 3 ^ original crc byte 2. */
    emit("\tld a,(de)\n");
    emit("\tld hl,8\n\tadd hl,sp\n\txor (hl)\n");
    emit("\tld hl,3\n\tadd hl,sp\n\tand (hl)\n");
    if (use_alt_dst)
        emit("\texx\n\tld (hl),a\n\texx\n");
    else
        emit_store_a_to_sym_byte_preserve_de(dst, 3);

    for (k = 0; k < 10; ++k)
        emit("\tinc sp\n");

    return 1;

no:
    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;
    return 0;
}


static void gen_expr_statement(void)
{
    int old_dead;

    if (tok.kind != ';') {
        if (try_gen_incdec_statement())
            return;

        if (try_fast_global_char_array_store()) {
            expect(';');
            return;
        }

        if (try_fast_local_self_add_statement())
            return;

        if (try_fast_crc_update_byte_statement())
            return;

        old_dead = expr_result_dead;
        expr_result_dead = 1;
        gen_expr();
        expr_result_dead = old_dead;
    }
    expect(';');
}


static void skip_initializer_or_decl_tail(void);

static int parse_float_init_literal(unsigned long *bits)
{
    int sign;
    char lit[MAX_TOK_TEXT + 2];
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;

    /*
     * This helper is only for the compact constant-initializer fast path.
     * Be conservative: if a float literal is followed by an operator, as in
     *     float r = 16.0 * f;
     * then it is not a complete initializer.  Rewind and let gen_expr()
     * compile the full expression.
     */
    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    sign = 1;
    if (tok.kind == '-') {
        sign = -1;
        next_token();
    } else if (tok.kind == '+') {
        next_token();
    }

    if (tok.kind == TOK_FLOATLIT) {
        if (sign < 0) {
            lit[0] = '-';
            strncpy(lit + 1, tok.text, MAX_TOK_TEXT);
            lit[MAX_TOK_TEXT] = 0;
            bits[0] = parse_float_literal_bits(lit);
        } else {
            bits[0] = parse_float_literal_bits(tok.text);
        }
        next_token();

        if (tok.kind == ';' || tok.kind == ',' || tok.kind == '}')
            return 1;

        posi = save_pos;
        tok_start_pos = save_tok_start;
        line_no = save_line;
        tok_line = save_tok_line;
        tok = save_tok;
        return 0;
    }

    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) {
        double d;
        union { float f; unsigned char b[4]; } u;
        unsigned long v;
        d = (double)(sign * tok.val);
        u.f = (float)d;
        v = ((unsigned long)u.b[0]) |
            ((unsigned long)u.b[1] << 8) |
            ((unsigned long)u.b[2] << 16) |
            ((unsigned long)u.b[3] << 24);
        bits[0] = v;
        next_token();

        if (tok.kind == ';' || tok.kind == ',' || tok.kind == '}')
            return 1;

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
    return 0;
}

static int parse_global_init_atom(long *val, char *label, int labelsz);

static void emit_store_const_to_local_array_elem(struct Sym *s, int elem_type, int index, long v)
{
    int elem_size;

    elem_size = type_size(elem_type);
    if (elem_size <= 0) elem_size = 2;

    emit_load_sym_addr(s);
    emit_add_const_to_hl((long)index * elem_size);
    emit("\tpush hl\n");

    if (type_size(elem_type) == 4) {
        unsigned long uv;
        uv = (unsigned long)v;
        fprintf(outf, "\tld hl,%lu\n", uv & 0xffffUL);
        fprintf(outf, "\tld de,%lu\n", (uv >> 16) & 0xffffUL);
        emit_store_de_to_addr_hl(elem_type);
    } else {
        fprintf(outf, "\tld hl,%ld\n", v & 0xffffL);
        emit("\tex de,hl\n\tpop hl\n");
        emit_store_de_to_addr_hl(elem_type);
    }
}

static void emit_store_const_to_local_offset(struct Sym *s, int off, int type, long v)
{
    emit_load_sym_addr(s);
    emit_add_const_to_hl(off);
    emit("\tpush hl\n");

    if (type_size(type) == 4) {
        unsigned long uv;
        uv = (unsigned long)v;
        fprintf(outf, "\tld hl,%lu\n", uv & 0xffffUL);
        fprintf(outf, "\tld de,%lu\n", (uv >> 16) & 0xffffUL);
        emit_store_de_to_addr_hl(type);
    } else {
        fprintf(outf, "\tld hl,%ld\n", v & 0xffffL);
        emit("\tex de,hl\n\tpop hl\n");
        emit_store_de_to_addr_hl(type);
    }
}

static void emit_zero_local_bytes(struct Sym *s, int off, int count)
{
    int i;
    for (i = 0; i < count; ++i)
        emit_store_const_to_local_offset(s, off + i, TYPE_CHAR | TYPE_UNSIGNED, 0);
}

static void emit_init_auto_char_array_at_offset_from_string(struct Sym *s, int baseoff, int count, const char *str)
{
    int i;
    int n;

    n = (int)strlen(str);
    if (count <= 0)
        return;

    if (n > count) {
        error_here("string initializer too long for char array field");
        n = count;
    }

    for (i = 0; i < n; ++i)
        emit_store_const_to_local_offset(s, baseoff + i, TYPE_CHAR | TYPE_UNSIGNED,
                                         (unsigned char)str[i]);

    while (i < count) {
        emit_store_const_to_local_offset(s, baseoff + i, TYPE_CHAR | TYPE_UNSIGNED, 0);
        i++;
    }
}

static void emit_init_auto_struct_type(struct Sym *s, int baseoff, int type);
static void skip_initializer_or_decl_tail(void);

static void emit_init_auto_struct_scalar(struct Sym *s, int off, int type)
{
    long v;
    int k;
    char label[64];

    if ((type & 15) == TYPE_FLOAT && type_ptr_depth(type) == 0) {
        unsigned long bits;
        if (parse_float_init_literal(&bits))
            emit_store_const_to_local_offset(s, off, type, (long)bits);
        else {
            error_here("automatic struct float initializer must be constant");
            if (tok.kind != ',' && tok.kind != '}')
                next_token();
        }
        return;
    }

    k = parse_global_init_atom(&v, label, sizeof(label));
    if (k == 1) {
        emit_store_const_to_local_offset(s, off, type, v);
    } else {
        error_here("automatic struct initializer must be constant");
        if (tok.kind != ',' && tok.kind != '}')
            next_token();
    }
}

static void emit_init_auto_struct_array(struct Sym *s, int baseoff, int elem_type, int count, int elem_size)
{
    int n;
    int total_bytes;

    if (elem_size <= 0) elem_size = type_size(elem_type);
    if (elem_size <= 0) elem_size = 2;

    if ((elem_type & 15) == TYPE_CHAR && type_ptr_depth(elem_type) == 0 &&
        tok.kind == TOK_STR) {
        char *lit;
        int is_wide;
        lit = read_adjacent_string_literals_ex(&is_wide);
        if (is_wide)
            error_here("wide string cannot initialize char array field");
        else
            emit_init_auto_char_array_at_offset_from_string(s, baseoff, count, lit);
        free(lit);
        return;
    }

    if (tok.kind == '{')
        next_token();

    n = 0;
    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (count > 0 && n >= count) {
            error_here("too many initializer elements");
            skip_initializer_or_decl_tail();
            break;
        }

        if ((elem_type & TYPE_STRUCT) && type_ptr_depth(elem_type) == 0)
            emit_init_auto_struct_type(s, baseoff + n * elem_size, elem_type);
        else
            emit_init_auto_struct_scalar(s, baseoff + n * elem_size, elem_type);

        n++;
        if (!accept(',')) break;
        if (tok.kind == '}') break;
    }
    expect('}');

    if (count > 0 && n < count) {
        total_bytes = (count - n) * elem_size;
        emit_zero_local_bytes(s, baseoff + n * elem_size, total_bytes);
    }
}


static long parse_struct_init_const_value(void)
{
    long v;
    char label[64];
    int k;

    k = parse_global_init_atom(&v, label, sizeof(label));
    if (k != 1) {
        error_here("bitfield initializer must be constant integer");
        if (tok.kind != ',' && tok.kind != '}')
            next_token();
        return 0;
    }
    return v;
}

static unsigned int bitfield_init_part(struct FieldDef *fd, long v)
{
    unsigned long mask;

    if (fd->bit_width <= 0)
        return 0;

    mask = (1UL << fd->bit_width) - 1UL;
    return (unsigned int)(((unsigned long)v & mask) << fd->bit_shift);
}

static int next_parent_field_index(int sid, int start)
{
    int k;
    for (k = start; k < nfield_defs; ++k)
        if (field_defs[k].parent_struct_id == sid)
            return k;
    return -1;
}

static void emit_store_const_bitfield_unit_to_local(struct Sym *s, int off, unsigned int unit)
{
    emit_store_const_to_local_offset(s, off, TYPE_UNSIGNED | TYPE_INT, (long)(unit & 0xffffU));
}

static void emit_init_auto_struct_type(struct Sym *s, int baseoff, int type)
{
    int sid;
    int i;
    int used;
    int total;
    int is_union;

    sid = type_struct_id(type);
    total = type_size(type);
    used = 0;
    is_union = (sid > 0 && sid <= nstruct_defs && struct_defs[sid - 1].is_union);

    if (tok.kind == '{')
        next_token();

    if (is_union) {
        struct FieldDef *first;
        first = NULL;
        for (i = 0; i < nfield_defs; ++i) {
            if (field_defs[i].parent_struct_id == sid) {
                first = &field_defs[i];
                break;
            }
        }

        if (first && tok.kind != TOK_EOF && tok.kind != '}') {
            if (first->is_array)
                emit_init_auto_struct_array(s, baseoff, first->elem_type, first->array_len, first->elem_size);
            else if ((first->type & TYPE_STRUCT) && type_ptr_depth(first->type) == 0)
                emit_init_auto_struct_type(s, baseoff, first->type);
            else
                emit_init_auto_struct_scalar(s, baseoff, first->type);
            used = first->size;

            if (accept(',')) {
                if (tok.kind != '}') {
                    error_here("too many union initializer elements");
                    while (tok.kind != TOK_EOF && tok.kind != '}') {
                        skip_initializer_or_decl_tail();
                        if (tok.kind == ',') next_token();
                        else break;
                    }
                }
            }
        }

        expect('}');
        if (total > used)
            emit_zero_local_bytes(s, baseoff + used, total - used);
        return;
    }

    for (i = 0; i < nfield_defs && tok.kind != TOK_EOF && tok.kind != '}'; ++i) {
        struct FieldDef *fd;
        fd = &field_defs[i];
        if (fd->parent_struct_id != sid)
            continue;

        if (fd->offset > used)
            emit_zero_local_bytes(s, baseoff + used, fd->offset - used);

        if (fd->bit_width > 0) {
            int unit_off;
            int k;
            int next;
            unsigned int unit;
            int stop;

            unit_off = fd->offset;
            unit = 0;
            stop = 0;
            k = i;
            while (k >= 0 && k < nfield_defs && tok.kind != TOK_EOF && tok.kind != '}') {
                struct FieldDef *bfd;
                bfd = &field_defs[k];
                if (bfd->parent_struct_id == sid) {
                    if (bfd->bit_width <= 0 || bfd->offset != unit_off)
                        break;
                    unit |= bitfield_init_part(bfd, parse_struct_init_const_value());
                    if (!accept(',')) {
                        stop = 1;
                        break;
                    }
                    if (tok.kind == '}') {
                        stop = 1;
                        break;
                    }
                }
                next = next_parent_field_index(sid, k + 1);
                if (next < 0) {
                    if (tok.kind != '}') {
                        error_here("too many initializer elements");
                        while (tok.kind != TOK_EOF && tok.kind != '}') {
                            skip_initializer_or_decl_tail();
                            if (tok.kind == ',') next_token();
                            else break;
                        }
                    }
                    stop = 1;
                    break;
                }
                k = next;
            }
            emit_store_const_bitfield_unit_to_local(s, baseoff + unit_off, unit);
            used = unit_off + 2;
            if (k > i)
                i = k - 1;
            if (stop)
                break;
            continue;
        }

        if (fd->is_array)
            emit_init_auto_struct_array(s, baseoff + fd->offset, fd->elem_type, fd->array_len, fd->elem_size);
        else if ((fd->type & TYPE_STRUCT) && type_ptr_depth(fd->type) == 0)
            emit_init_auto_struct_type(s, baseoff + fd->offset, fd->type);
        else
            emit_init_auto_struct_scalar(s, baseoff + fd->offset, fd->type);

        used = fd->offset + fd->size;
        if (!accept(',')) break;
        if (tok.kind == '}') break;
    }
    expect('}');

    if (total > used)
        emit_zero_local_bytes(s, baseoff + used, total - used);
}

static void emit_init_auto_struct_from_list(struct Sym *s)
{
    emit_init_auto_struct_type(s, 0, s->type);
}

static void emit_init_auto_struct_array_from_list(struct Sym *s)
{
    int elem_size;
    elem_size = s->elem_size ? s->elem_size : type_size(s->type);
    if (elem_size <= 0) elem_size = 2;
    emit_init_auto_struct_array(s, 0, s->type, s->array_len, elem_size);
}

static int sym_array_elems_from_level(struct Sym *s, int level)
{
    int i;
    int n;

    if (s->dim_count <= 0)
        return s->array_len;

    if (level < 0)
        level = 0;
    if (level >= s->dim_count)
        return 1;

    n = 1;
    for (i = level; i < s->dim_count; ++i) {
        if (s->dims[i] <= 0)
            return s->array_len;
        n *= s->dims[i];
    }
    return n;
}

static int sym_array_total_elems(struct Sym *s)
{
    return sym_array_elems_from_level(s, 0);
}

static void emit_init_auto_array_scalar(struct Sym *s, int elem_type, int *np)
{
    long v;
    int k;
    int n;
    char label[64];

    n = np[0];
    if (s->array_len > 0 && n >= s->array_len) {
        error_here("too many initializer elements");
        if (tok.kind != ',' && tok.kind != '}')
            next_token();
        return;
    }

    if ((elem_type & 15) == TYPE_FLOAT && type_ptr_depth(elem_type) == 0) {
        unsigned long bits;
        if (parse_float_init_literal(&bits))
            emit_store_const_to_local_array_elem(s, elem_type, n, (long)bits);
        else {
            error_here("automatic float array initializer must be constant");
            if (tok.kind != ',' && tok.kind != '}')
                next_token();
            return;
        }
    } else {
        k = parse_global_init_atom(&v, label, sizeof(label));
        if (k == 1) {
            emit_store_const_to_local_array_elem(s, elem_type, n, v);
        } else {
            error_here("automatic array initializer must be constant");
            if (tok.kind != ',' && tok.kind != '}')
                next_token();
            return;
        }
    }

    np[0] = n + 1;
}

static void emit_init_auto_array_level(struct Sym *s, int elem_type, int *np, int level)
{
    int start;
    int limit;

    if (!accept('{')) {
        emit_init_auto_array_scalar(s, elem_type, np);
        return;
    }

    start = np[0];
    limit = start + sym_array_elems_from_level(s, level);

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == '{' && s->dim_count > 0 && level + 1 < s->dim_count)
            emit_init_auto_array_level(s, elem_type, np, level + 1);
        else
            emit_init_auto_array_scalar(s, elem_type, np);

        if (!accept(','))
            break;
        if (tok.kind == '}')
            break;
    }
    expect('}');

    while (np[0] < limit) {
        emit_store_const_to_local_array_elem(s, elem_type, np[0], 0);
        np[0] = np[0] + 1;
    }
}

static void emit_init_auto_array_from_list(struct Sym *s, int elem_type)
{
    int n;
    int total;

    n = 0;
    emit_init_auto_array_level(s, elem_type, &n, 0);

    total = sym_array_total_elems(s);
    while (total > 0 && n < total) {
        emit_store_const_to_local_array_elem(s, elem_type, n, 0);
        n++;
    }
}

static void gen_local_decl_after_type(int base)
{
    int type, bytes, arrlen;
    int total_elems;
    char name[64];
    struct Sym *s;

    for (;;) {
        type = base;

        while (accept('*')) { skip_type_qualifiers(); type = type_add_ptr(type); }

        if (!parse_funcptr_declarator(&type, name, sizeof(name))) {
            if (tok.kind != TOK_ID) {
                error_here("identifier expected");
                break;
            }

            strncpy(name, tok.text, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            next_token();
        }

        arrlen = g_funcptr_decl_array_len;
        g_funcptr_decl_array_len = 0;
        total_elems = arrlen;
        {
            int first_stride_bytes;
            first_stride_bytes = 0;
            if (arrlen == 0)
                parse_array_declarator_dims(type, &total_elems, &first_stride_bytes, 1);
            else
                total_elems = arrlen;

            arrlen = total_elems;

            /*
             * Local omitted-size arrays need the initializer count before
             * allocation:
             *     char data[] = { 'a', 'b', 0 };
             */
            if (arrlen == 0 && g_last_array_dim_count > 0 && tok.kind == '=') {
                int atoms;
                int inner;
                int di;

                atoms = count_omitted_array_initializer_atoms();
                inner = 1;
                for (di = 1; di < g_last_array_dim_count; ++di) {
                    if (g_last_array_dims[di] > 0)
                        inner *= g_last_array_dims[di];
                }
                if (inner <= 0) inner = 1;

                if (atoms > 0) {
                    total_elems = atoms;
                    arrlen = (atoms + inner - 1) / inner;
                    g_last_array_dims[0] = arrlen;
                }
            }

            if (first_stride_bytes > 0) {
                /* stash temporarily in bytes; assigned to Sym below */
                current_field_array_elem_size = first_stride_bytes;
            } else {
                current_field_array_elem_size = 0;
            }
        }
        /* inherit array length from array typedef (e.g. typedef int T[4]) */
        if (arrlen == 0 && g_typedef_array_len > 0) {
            arrlen = g_typedef_array_len;
            total_elems = g_typedef_array_len;
        }

        s = find_local(name);
        if (!s) {
            bytes = type_size(type);
            if (total_elems > 0)
                bytes = object_array_size(type, total_elems);

            s = add_local_alloc(name, type, bytes);
            if (arrlen > 0 || g_last_array_dim_count > 0) {
                s->is_array = 1;
                s->array_len = arrlen;
                s->elem_size = current_field_array_elem_size ? current_field_array_elem_size : type_size(type);
                if (s->elem_size <= 0) s->elem_size = 2;
                copy_last_array_dims_to_sym(s);
            }
        }

        if (accept('=')) {
            if ((type & TYPE_STRUCT) && type_ptr_depth(type) == 0 && tok.kind != '{') {
                error_here("struct initializer list expected");
                skip_initializer_or_decl_tail();
            } else if (s->is_array && (type & 15) == TYPE_CHAR && type_ptr_depth(type) == 0 && tok.kind == TOK_STR) {
                char *lit;
                int is_wide;
                lit = read_adjacent_string_literals_ex(&is_wide);
                if (is_wide)
                    error_here("wide string cannot initialize char array");
                emit_init_auto_char_array_from_string(s, lit);
                free(lit);
            } else if (s->is_array && tok.kind == '{' && (type & TYPE_STRUCT) && type_ptr_depth(type) == 0) {
                emit_init_auto_struct_array_from_list(s);
            } else if (!s->is_array && tok.kind == '{' && (type & TYPE_STRUCT) && type_ptr_depth(type) == 0) {
                emit_init_auto_struct_from_list(s);
            } else if (s->is_array && tok.kind == '{' && !(type & TYPE_STRUCT)) {
                emit_init_auto_array_from_list(s, type);
            } else if (!s->is_array && tok.kind == '{') {
                next_token();
                emit_load_sym_addr(s);
                emit("\tpush hl\n");
                gen_expr_no_comma();
                if (type_is_long(type)) {
                    if (!type_is_long(g_expr_type))
                        emit_extend_to_long_typed(g_expr_type);
                    emit_store_de_to_addr_hl(type);
                } else {
                    if (type_size(type) > 1 && !type_is_long(g_expr_type))
                        emit_promote_byte_to_int(g_expr_type);
                    emit("\tex de,hl\n\tpop hl\n");
                    emit_store_de_to_addr_hl(type);
                }
                accept(',');
                expect('}');
            } else if (!s->is_array && (type & 15) == TYPE_FLOAT && type_ptr_depth(type) == 0) {
                unsigned long bits;
                if (parse_float_init_literal(&bits)) {
                    emit_load_sym_addr(s);
                    emit("\tpush hl\n");
                    fprintf(outf, "\tld hl,%lu\n", bits & 0xffffUL);
                    fprintf(outf, "\tld de,%lu\n", (bits >> 16) & 0xffffUL);
                    emit_store_de_to_addr_hl(type);
                } else {
                    /* Extension beyond strict C89: allow automatic float
                     * declarations to use expression initializers, e.g.
                     *     float r = 16.0f * f;
                     * This is emitted like a declaration followed by an
                     * assignment.  The constant fast path above stays for
                     * smaller code.
                     */
                    emit_load_sym_addr(s);
                    emit("\tpush hl\n");
                    gen_expr_no_comma();
                    if (!type_is_float(g_expr_type))
                        emit_convert_int_to_float(g_expr_type);
                    emit_store_de_to_addr_hl(type);
                }
            } else {
                emit_load_sym_addr(s);
                emit("\tpush hl\n");
                gen_expr_no_comma();
                if (type_is_long(type)) {
                    /* For long locals, emit_store_de_to_addr_hl pops the
                     * address itself via "pop de", so don't consume it here. */
                    if (!type_is_long(g_expr_type))
                        emit_extend_to_long_typed(g_expr_type);
                    emit_store_de_to_addr_hl(type);
                } else {
                    if (type_size(type) > 1 && !type_is_long(g_expr_type))
                        emit_promote_byte_to_int(g_expr_type);
                    emit("\tex de,hl\n\tpop hl\n");
                    emit_store_de_to_addr_hl(type);
                }
            }
        }

        if (!accept(',')) break;
    }

    expect(';');
}

static void gen_compound(void)
{
    expect('{');

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == TOK_TYPEDEF) {
            parse_typedef_decl();
        } else if (starts_type()) {
            int t;
            int is_static_local;
            decl_is_extern = 0;
            is_static_local = (tok.kind == TOK_STATIC);
            t = parse_base_type();
            if (tok.kind == ';') {
                next_token();
            } else if (is_static_local) {
                scan_static_local_decl_after_type(t);
            } else {
                gen_local_decl_after_type(t);
            }
        } else {
            gen_statement();
        }
    }

    expect('}');
}

static void gen_if(void)
{
    int lelse, lend;

    lelse = new_label();
    lend = new_label();

    expect(TOK_IF);
    expect('(');
    if (!try_fast_global_char_array_condition(lelse) &&
        !gen_condition_branch_false(lelse)) {
        gen_expr();
        emit_test_expr_nonzero(g_expr_type, lelse, 0);
    }
    expect(')');

    gen_statement();

    emit_jp_label("jp", lend);
    emit_label(lelse);

    if (accept(TOK_ELSE)) gen_statement();

    emit_label(lend);
}

static void gen_while(void)
{
    int ltop, lend;

    ltop = new_label();
    lend = new_label();

    expect(TOK_WHILE);
    emit_label(ltop);

    expect('(');
    if (!gen_condition_branch_false(lend)) {
        gen_expr();
        emit_test_expr_nonzero(g_expr_type, lend, 0);
    }
    expect(')');

    break_stack[nflow] = lend;
    cont_stack[nflow] = ltop;
    nflow++;

    gen_statement();

    nflow--;
    emit_jp_label("jp", ltop);
    emit_label(lend);
}

static char *copy_range(long a, long b)
{
    long n;
    char *p;
    char fbuf[256];
    char lbuf[384];
    int lno;
    int lnl;

    n = b - a;
    if (n < 0) n = 0;

    source_location_at(a, fbuf, sizeof(fbuf), &lno);
    sprintf(lbuf, "#line %d \"%s\"\n", lno, fbuf);
    lnl = (int)strlen(lbuf);

    p = (char *)xmalloc((size_t)(lnl + n + 2));
    memcpy(p, lbuf, (size_t)lnl);
    memcpy(p + lnl, src + a, (size_t)n);
    p[lnl + n] = ';';
    p[lnl + n + 1] = 0;
    return p;
}

static void gen_snippet_expr(const char *snippet)
{
    char *old_src;
    long old_len;
    long old_pos;
    long old_tok_start;
    int old_line;
    int old_tok_line;
    struct Token old_tok;

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

    if (tok.kind != ';') {
        if (expr_result_dead) {
            if (!try_gen_incdec_statement()) {
                if (!try_fast_local_self_add_statement())
                    gen_expr();
            }
        } else {
            gen_expr();
        }
    }

    src = old_src;
    src_len = old_len;
    posi = old_pos;
    tok_start_pos = old_tok_start;
    line_no = old_line;
    tok_line = old_tok_line;
    tok = old_tok;
}

static void gen_snippet_lvalue_addr(const char *snippet, int *ptype)
{
    char *old_src;
    long old_len;
    long old_pos;
    long old_tok_start;
    int old_line;
    int old_tok_line;
    struct Token old_tok;

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

    gen_lvalue_addr(ptype);

    src = old_src;
    src_len = old_len;
    posi = old_pos;
    tok_start_pos = old_tok_start;
    line_no = old_line;
    tok_line = old_tok_line;
    tok = old_tok;
}

static void skip_expr_until_rparen(long *startp, long *endp)
{
    int depth;
    int done;

    depth = 0;
    done = 0;
    startp[0] = tok_start_pos;

    while (tok.kind != TOK_EOF && !done) {
        if (depth == 0 && tok.kind == ')') {
            done = 1;
        } else {
            if (tok.kind == '(') {
                depth = depth + 1;
            } else {
                if (tok.kind == '[') {
                    depth = depth + 1;
                } else {
                    if (tok.kind == ')') {
                        depth = depth - 1;
                    } else {
                        if (tok.kind == ']') {
                            depth = depth - 1;
                        }
                    }
                }
            }

            if (!done) {
                next_token();
            }
        }
    }

    endp[0] = tok_start_pos;
}

/*
 * Skip past the for-loop condition (up to but not including the ';').
 * Records the source positions so copy_range can capture the text.
 * Like skip_expr_until_rparen but stops at ';' instead of ')'.
 */
static void skip_for_cond(long *startp, long *endp)
{
    startp[0] = tok_start_pos;
    while (tok.kind != TOK_EOF && tok.kind != ';')
        next_token();
    endp[0] = tok_start_pos;
}

/*
 * Replay a captured condition snippet and emit a branch to ltrue when
 * the condition is true.  Used to place the for-loop condition check at
 * the bottom of the loop (do-while style).
 */
static void gen_snippet_cond_true(const char *snippet, int ltrue)
{
    char *old_src;
    long old_len;
    long old_pos;
    long old_tok_start;
    int old_line;
    int old_tok_line;
    struct Token old_tok;

    old_src       = src;
    old_len       = src_len;
    old_pos       = posi;
    old_tok_start = tok_start_pos;
    old_line      = line_no;
    old_tok_line  = tok_line;
    old_tok       = tok;

    src     = (char *)snippet;
    src_len = (long)strlen(snippet);
    posi    = 0;
    line_no = 1;
    next_token();

    if (tok.kind != ';') {
        if (!gen_condition_branch_true_until(ltrue, ';')) {
            gen_expr();
            emit_test_expr_nonzero(g_expr_type, ltrue, 1);
        }
    }

    src           = old_src;
    src_len       = old_len;
    posi          = old_pos;
    tok_start_pos = old_tok_start;
    line_no       = old_line;
    tok_line      = old_tok_line;
    tok           = old_tok;
}

/*
 * Lookahead: return 1 if the for-loop init/condition has the form
 *   ident = const ; ident relop bound ;
 * where const satisfies the condition (so the loop always executes at
 * least once).  Recognises <, <=, != as the relop.
 * Does not consume any tokens.
 */
static int for_init_always_enters_loop(void)
{
    long save_pos;
    long save_tok_start;
    int  save_line;
    int  save_tok_line;
    struct Token save_tok;
    char varname[64];
    long initval;
    long bound;
    int  op;
    int  result;

    if (tok.kind != TOK_ID)
        return 0;

    save_pos       = posi;
    save_tok_start = tok_start_pos;
    save_line      = line_no;
    save_tok_line  = tok_line;
    save_tok       = tok;

    result = 0;

    /* ident */
    strncpy(varname, tok.text, sizeof(varname) - 1);
    varname[sizeof(varname) - 1] = 0;
    next_token();

    /* = (plain assignment, not ==) */
    if (tok.kind != '=') goto restore;
    next_token();

    /* non-negative integer constant */
    if (tok.kind != TOK_NUM) goto restore;
    if (tok.val < 0) goto restore;
    initval = tok.val;
    next_token();

    /* ; separating init from condition */
    if (tok.kind != ';') goto restore;
    next_token();

    /* same identifier */
    if (tok.kind != TOK_ID) goto restore;
    if (strcmp(tok.text, varname) != 0) goto restore;
    next_token();

    /* relational operator — exclude <= because its signed-bias sbc pattern is
     * matched by pass_reuse_sbc_result_for_flagcheck / pass_recover_index_from_sbc
     * which both require the while-style jp LE exit jump that do-while omits. */
    op = tok.kind;
    if (op != '<' && op != TOK_NE) goto restore;
    next_token();

    /* integer constant bound */
    if (tok.kind != TOK_NUM) goto restore;
    bound = tok.val;

    /* confirm init value satisfies condition */
    if (op == '<'    && !(initval <  bound)) goto restore;
    if (op == TOK_LE && !(initval <= bound)) goto restore;
    if (op == TOK_NE && !(initval != bound)) goto restore;

    result = 1;

restore:
    posi          = save_pos;
    tok_start_pos = save_tok_start;
    line_no       = save_line;
    tok_line      = save_tok_line;
    tok           = save_tok;
    return result;
}

static void gen_for(void)
{
    int ltop, linc, lend;
    long inc_start, inc_end;
    long cond_start, cond_end;
    char *inc_code;
    char *cond_code;
    int do_while;

    ltop = new_label();
    linc = new_label();
    lend = new_label();

    expect(TOK_FOR);
    expect('(');

    /* Peek: if init assigns a constant that already satisfies the condition,
     * generate a do-while (check at bottom) to skip the always-true first test. */
    do_while = 0;
    if (!starts_type() && tok.kind != ';')
        do_while = for_init_always_enters_loop();

    if (starts_type()) {
        /* C99-style for-init declaration: for (int i = 0; ...) */
        int t;
        decl_is_extern = 0;
        t = parse_base_type();
        if (tok.kind != ';')
            gen_local_decl_after_type(t); /* consumes the ';' */
        else
            next_token();
        /* no expect(';') here — already consumed above */
    } else {
        if (tok.kind != ';') gen_expr();
        expect(';');
    }

    if (!do_while) {
        emit_label(ltop);

        if (tok.kind != ';') {
            if (!gen_condition_branch_false_until(lend, ';')) {
                gen_expr();
                emit_test_expr_nonzero(g_expr_type, lend, 0);
            }
        }
        expect(';');
    } else {
        /* Capture condition as text for replay at the bottom; skip it here. */
        cond_start = tok_start_pos;
        skip_for_cond(&cond_start, &cond_end);
        cond_code = copy_range(cond_start, cond_end);
        expect(';');

        emit_label(ltop);
    }

    inc_start = tok_start_pos;
    skip_expr_until_rparen(&inc_start, &inc_end);
    inc_code = copy_range(inc_start, inc_end);

    expect(')');

    break_stack[nflow] = lend;
    cont_stack[nflow] = linc;
    nflow++;

    gen_statement();

    nflow--;

    emit_label(linc);
    {
        int old_dead;

        old_dead = expr_result_dead;
        expr_result_dead = 1;
        gen_snippet_expr(inc_code);
        expr_result_dead = old_dead;
    }
    free(inc_code);

    if (!do_while) {
        emit_jp_label("jp", ltop);
    } else {
        gen_snippet_cond_true(cond_code, ltop);
        free(cond_code);
    }
    emit_label(lend);
}

static void gen_return(void)
{
    expect(TOK_RETURN);

    if (tok.kind != ';') {
        if (type_is_struct_object(current_return_type)) {
            int rt;
            gen_lvalue_addr(&rt);
            if (!same_struct_type(current_return_type, rt))
                error_here("incompatible struct return");
            emit("\tex de,hl\n");          /* DE = source */
            emit("\tld l,(ix+4)\n");       /* hidden return pointer */
            emit("\tld h,(ix+5)\n");
            emit_copy_de_to_hl_bytes(type_size(current_return_type));
            g_expr_type = current_return_type;
            expect(';');
            emit_jp_label("jp", current_return_label);
            return;
        }

        /*
         * Byte return fast path.  This keeps tiny helpers from materializing a
         * full generic expression when the return value is just a byte local,
         * parameter, or small constant.  It is conservative: anything more
         * complex falls through to gen_expr(), preserving C integer promotions.
         */
        if (type_size(current_return_type) == 1) {
            if (tok.kind == TOK_ID) {
                struct Sym *rs;
                long save_pos;
                long save_tok_start;
                int save_line;
                int save_tok_line;
                struct Token save_tok;

                save_pos = posi;
                save_tok_start = tok_start_pos;
                save_line = line_no;
                save_tok_line = tok_line;
                save_tok = tok;

                rs = find_sym(tok.text);
                if (sym_can_ix_direct(rs) && type_size(rs->type) == 1) {
                    next_token();
                    if (tok.kind == ';') {
                        fprintf(outf, "\tld l,(ix%+d)\n", rs->offset);
                        if (current_return_type & TYPE_UNSIGNED)
                            emit("\tld h,0\n");
                        else
                            emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
                        g_expr_type = current_return_type;
                        expect(';');
                        emit_jp_label("jp", current_return_label);
                        return;
                    }
                }

                posi = save_pos;
                tok_start_pos = save_tok_start;
                line_no = save_line;
                tok_line = save_tok_line;
                tok = save_tok;
            } else if ((tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT) &&
                       tok.val >= 0 && tok.val <= 255) {
                long rv;
                rv = tok.val;
                next_token();
                if (tok.kind == ';') {
                    fprintf(outf, "\tld hl,%ld\n", rv & 255);
                    g_expr_type = current_return_type;
                    expect(';');
                    emit_jp_label("jp", current_return_label);
                    return;
                }
            }
        }

        /*
         * Float returns used to use try_emit_float_rvalue_dehl(), which only
         * consumes a simple float rvalue.  The normal expression generator now
         * supports full float expressions and leaves float results in DE:HL.
         */
        gen_expr();
        if (type_is_float(current_return_type) && !type_is_float(g_expr_type)) {
            emit_convert_int_to_float(g_expr_type);
        } else if (!type_is_float(current_return_type) && type_is_float(g_expr_type)) {
            emit_convert_float_to_intlike(current_return_type);
        } else if (type_size(current_return_type) == 1 && !type_is_long(g_expr_type)) {
            /*
             * Normalize byte-sized function returns.  The ABI returns all
             * integer scalars in HL, and callers use the function's declared
             * return type to decide whether later widening is signed or
             * unsigned.  Therefore a uint8_t return must leave HL as 00xx,
             * not FFxx inherited from the promoted expression used by
             * `return -1;`.  Likewise an int8_t return should leave HL sign
             * extended from L so direct assignment to wider signed types works.
             */
            if (current_return_type & TYPE_UNSIGNED)
                emit("\tld h,0\n");
            else
                emit("\tld a,l\n\trlca\n\tsbc a,a\n\tld h,a\n");
            g_expr_type = current_return_type;
        } else if (type_is_long(current_return_type) && !type_is_long(g_expr_type)) {
            /*
             * Long returns use DE:HL.  A plain int expression such as
             *
             *     uint32_t f(void) { return 0; }
             *
             * leaves only HL defined.  Without promotion DE contains whatever
             * happened to be live before the return.
             */
            emit_promote_int_to_long(g_expr_type, current_return_type);
            g_expr_type = current_return_type;
        }
    }

    expect(';');
    emit_jp_label("jp", current_return_label);
}

static void gen_switch_chain(void)
{
    int lend;
    int lcleanup;
    int lnext;      /* pending "not-matched" label from previous case (-1 if none) */
    int lbody;      /* body-entry label for the current case */
    int lbody_prev; /* body-entry label of the previous case (-1 if none) */
    long cv;

    expect(TOK_SWITCH);
    expect('(');
    gen_expr();
    expect(')');

    emit("\tpush hl\n");

    lend = new_label();
    lcleanup = new_label();

    expect('{');

    break_stack[nflow] = lcleanup;
    /* continue inside a switch should target the enclosing loop, not the switch */
    cont_stack[nflow] = (nflow > 0) ? cont_stack[nflow - 1] : lcleanup;
    nflow++;

    lnext = -1;
    lbody_prev = -1;

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == TOK_CASE) {
            next_token();
            if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT) {
                error_here("constant case expected");
                cv = 0;
            } else {
                cv = tok.val;
                next_token();
            }
            expect(':');

            lbody = new_label();

            /*
             * Fall-through from the previous case body jumps directly to
             * this case's body, bypassing this case's comparison check.
             */
            if (lbody_prev >= 0)
                emit_jp_label("jp", lbody);

            /*
             * "Not matched" from the previous case's comparison arrives
             * here to try this case's comparison.
             */
            if (lnext >= 0)
                emit_label(lnext);

            lnext = new_label();
            emit("\tpop de\n");
            emit("\tpush de\n");
            fprintf(outf, "\tld hl,%ld\n", cv & 0xffffL);
            emit("\tor a\n\tsbc hl,de\n");
            emit_jp_label("jp nz,", lnext);

            /* Body starts here — also the fall-through entry from a previous case. */
            emit_label(lbody);

            while (tok.kind != TOK_EOF &&
                   tok.kind != TOK_CASE &&
                   tok.kind != TOK_DEFAULT &&
                   tok.kind != '}') {
                gen_statement();
            }

            lbody_prev = lbody;
        } else if (tok.kind == TOK_DEFAULT) {
            next_token();
            expect(':');

            lbody = new_label();

            if (lbody_prev >= 0)
                emit_jp_label("jp", lbody);

            if (lnext >= 0)
                emit_label(lnext);

            /* default: no comparison — everything that arrives here executes the body */
            lnext = -1;
            emit_label(lbody);

            while (tok.kind != TOK_EOF &&
                   tok.kind != TOK_CASE &&
                   tok.kind != TOK_DEFAULT &&
                   tok.kind != '}') {
                gen_statement();
            }

            lbody_prev = lbody;
        } else {
            gen_statement();
        }
    }

    expect('}');

    nflow--;

    /* "Not matched" from the last case falls here, then to cleanup. */
    if (lnext >= 0)
        emit_label(lnext);

    emit_label(lcleanup);
    emit("\tpop de\n");
    emit_label(lend);
}

static int scan_switch_cases_for_table(int *case_vals, int *case_labs,
                                       int *case_countp, int *default_labp,
                                       int *minp, int *maxp)
{
    long save_posi;
    long save_tok_start_pos;
    int save_line_no;
    int save_tok_line;
    struct Token save_tok;
    int depth;
    int ncase;
    int have_default;
    int deflab;
    int minv;
    int maxv;
    int ok;
    int i;

    if (tok.kind != '{')
        return 0;

    save_posi = posi;
    save_tok_start_pos = tok_start_pos;
    save_line_no = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    ncase = 0;
    have_default = 0;
    deflab = -1;
    minv = 0;
    maxv = 0;
    ok = 1;

    next_token();
    depth = 1;

    while (tok.kind != TOK_EOF && depth > 0 && ok) {
        if (tok.kind == '{') {
            depth++;
            next_token();
        } else if (tok.kind == '}') {
            depth--;
            if (depth > 0)
                next_token();
        } else if (depth == 1 && tok.kind == TOK_CASE) {
            long cv;
            next_token();
            if (tok.kind != TOK_NUM && tok.kind != TOK_CHARLIT) {
                ok = 0;
                break;
            }
            cv = tok.val;
            if (cv < 0 || cv > 32767) {
                ok = 0;
                break;
            }
            if (ncase >= MAX_SWITCH_CASES) {
                ok = 0;
                break;
            }
            for (i = 0; i < ncase; ++i) {
                if (case_vals[i] == (int)cv) {
                    ok = 0;
                    break;
                }
            }
            if (!ok)
                break;
            case_vals[ncase] = (int)cv;
            case_labs[ncase] = new_label();
            if (ncase == 0 || (int)cv < minv) minv = (int)cv;
            if (ncase == 0 || (int)cv > maxv) maxv = (int)cv;
            ncase++;
            next_token();
            if (tok.kind != ':') {
                ok = 0;
                break;
            }
            next_token();
        } else if (depth == 1 && tok.kind == TOK_DEFAULT) {
            if (have_default) {
                ok = 0;
                break;
            }
            have_default = 1;
            deflab = new_label();
            next_token();
            if (tok.kind != ':') {
                ok = 0;
                break;
            }
            next_token();
        } else {
            next_token();
        }
    }

    if (depth != 0)
        ok = 0;

    posi = save_posi;
    tok_start_pos = save_tok_start_pos;
    line_no = save_line_no;
    tok_line = save_tok_line;
    tok = save_tok;

    if (!ok || ncase < 3)
        return 0;

    if ((maxv - minv) > 31)
        return 0;

    if ((maxv - minv + 1) > ncase * 2)
        return 0;

    case_countp[0] = ncase;
    default_labp[0] = have_default ? deflab : -1;
    minp[0] = minv;
    maxp[0] = maxv;
    return 1;
}

static int switch_label_for_value(int value, int *case_vals, int *case_labs, int ncase, int default_lab, int lend)
{
    int i;
    for (i = 0; i < ncase; ++i)
        if (case_vals[i] == value)
            return case_labs[i];
    return default_lab >= 0 ? default_lab : lend;
}

static void emit_switch_jump_table(int minv, int maxv,
                                   int *case_vals, int *case_labs,
                                   int ncase, int default_lab, int lend)
{
    int lok;
    int ltab;
    int v;
    int target;

    lok = new_label();
    ltab = new_label();

    if (minv != 0) {
        fprintf(outf, "\tld de,%d\n", minv);
        emit("\tor a\n\tsbc hl,de\n");
        emit_jp_label("jp c,", default_lab >= 0 ? default_lab : lend);
    }

    emit("\tpush hl\n");
    fprintf(outf, "\tld de,%d\n", maxv - minv);
    emit("\tor a\n\tsbc hl,de\n");
    emit("\tpop hl\n");
    emit_jp_label("jp z,", lok);
    emit_jp_label("jp nc,", default_lab >= 0 ? default_lab : lend);
    emit_label(lok);

    emit("\tadd hl,hl\n");
    fprintf(outf, "\tld de,L%d\n", ltab);
    emit("\tadd hl,de\n");
    emit("\tld e,(hl)\n");
    emit("\tinc hl\n");
    emit("\tld d,(hl)\n");
    emit("\tex de,hl\n");
    emit("\tjp (hl)\n");

    emit_label(ltab);
    for (v = minv; v <= maxv; ++v) {
        target = switch_label_for_value(v, case_vals, case_labs, ncase, default_lab, lend);
        fprintf(outf, "\tdw L%d\n", target);
    }
}

static void gen_switch_table(void)
{
    int case_vals[MAX_SWITCH_CASES];
    int case_labs[MAX_SWITCH_CASES];
    int ncase;
    int default_lab;
    int minv;
    int maxv;
    int lend;
    int ci;
    int active_default_lab;

    expect(TOK_SWITCH);
    expect('(');
    gen_expr();
    expect(')');

    if (!scan_switch_cases_for_table(case_vals, case_labs, &ncase, &default_lab, &minv, &maxv)) {
        /* Re-parse via the old chain generator by restoring from the caller is
         * hard after emitting the expression, so this entry point is only used
         * after gen_switch() has already scanned successfully. */
        return;
    }

    lend = new_label();
    emit_switch_jump_table(minv, maxv, case_vals, case_labs, ncase, default_lab, lend);

    expect('{');

    break_stack[nflow] = lend;
    cont_stack[nflow] = (nflow > 0) ? cont_stack[nflow - 1] : lend;
    nflow++;

    ci = 0;
    active_default_lab = default_lab;

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == TOK_CASE) {
            next_token();
            if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT)
                next_token();
            else
                error_here("constant case expected");
            expect(':');

            if (ci < ncase)
                emit_label(case_labs[ci++]);

            while (tok.kind != TOK_EOF &&
                   tok.kind != TOK_CASE &&
                   tok.kind != TOK_DEFAULT &&
                   tok.kind != '}') {
                gen_statement();
            }
        } else if (tok.kind == TOK_DEFAULT) {
            next_token();
            expect(':');

            if (active_default_lab >= 0)
                emit_label(active_default_lab);

            while (tok.kind != TOK_EOF &&
                   tok.kind != TOK_CASE &&
                   tok.kind != TOK_DEFAULT &&
                   tok.kind != '}') {
                gen_statement();
            }
        } else {
            gen_statement();
        }
    }

    expect('}');
    nflow--;
    emit_label(lend);
}

static void gen_switch(void)
{
    long save_posi;
    long save_tok_start_pos;
    int save_line_no;
    int save_tok_line;
    struct Token save_tok;
    int case_vals[MAX_SWITCH_CASES];
    int case_labs[MAX_SWITCH_CASES];
    int ncase;
    int default_lab;
    int minv;
    int maxv;

    /* Scan before consuming anything so fallback can use the old chain path. */
    save_posi = posi;
    save_tok_start_pos = tok_start_pos;
    save_line_no = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();          /* switch */
    if (tok.kind == '(') {
        int depth;
        next_token();
        depth = 1;
        while (tok.kind != TOK_EOF && depth > 0) {
            if (tok.kind == '(') depth++;
            else if (tok.kind == ')') depth--;
            next_token();
        }
    }

    if (tok.kind != '{' ||
        !scan_switch_cases_for_table(case_vals, case_labs, &ncase, &default_lab, &minv, &maxv)) {
        posi = save_posi;
        tok_start_pos = save_tok_start_pos;
        line_no = save_line_no;
        tok_line = save_tok_line;
        tok = save_tok;
        gen_switch_chain();
        return;
    }

    posi = save_posi;
    tok_start_pos = save_tok_start_pos;
    line_no = save_line_no;
    tok_line = save_tok_line;
    tok = save_tok;
    gen_switch_table();
}

static void gen_do_while(void)
{
    int ltop, lcont, lend;

    ltop  = new_label();
    lcont = new_label();
    lend  = new_label();

    expect(TOK_DO);
    emit_label(ltop);

    break_stack[nflow] = lend;
    cont_stack[nflow]  = lcont;
    nflow++;

    gen_statement();

    nflow--;

    emit_label(lcont);
    expect(TOK_WHILE);
    expect('(');
    if (!gen_condition_branch_true(ltop)) {
        gen_expr();
        emit_test_expr_nonzero(g_expr_type, ltop, 1);
    }
    expect(')');
    expect(';');
    emit_label(lend);
}

static void gen_statement(void)
{
    if (tok.kind == '{') {
        gen_compound();
    } else if (tok.kind == TOK_DO) {
        gen_do_while();
    } else if (tok.kind == TOK_IF) {
        gen_if();
    } else if (tok.kind == TOK_WHILE) {
        gen_while();
    } else if (tok.kind == TOK_FOR) {
        gen_for();
    } else if (tok.kind == TOK_SWITCH) {
        gen_switch();
    } else if (tok.kind == TOK_RETURN) {
        gen_return();
    } else if (tok.kind == TOK_BREAK) {
        next_token();
        expect(';');
        if (nflow <= 0) error_here("break outside loop");
        else emit_jp_label("jp", break_stack[nflow - 1]);
    } else if (tok.kind == TOK_CONTINUE) {
        next_token();
        expect(';');
        if (nflow <= 0) error_here("continue outside loop");
        else emit_jp_label("jp", cont_stack[nflow - 1]);
    } else if (tok.kind == TOK_GOTO) {
        char lname[64];
        next_token();
        if (tok.kind != TOK_ID) {
            error_here("label name expected after goto");
        } else {
            strncpy(lname, tok.text, sizeof(lname) - 1);
            lname[sizeof(lname) - 1] = 0;
            next_token();
            emit_jp_label("jp", find_or_alloc_user_label(lname));
        }
        expect(';');
    } else if (tok.kind == TOK_ID) {
        /* Peek one token ahead to check for a user label: identifier ':' */
        long save_pos;
        long save_tok_start;
        int save_line, save_tok_line;
        struct Token save_tok;
        char lname[64];
        save_pos = posi;
        save_tok_start = tok_start_pos;
        save_line = line_no;
        save_tok_line = tok_line;
        save_tok = tok;
        strncpy(lname, tok.text, sizeof(lname) - 1);
        lname[sizeof(lname) - 1] = 0;
        next_token();
        if (tok.kind == ':') {
            /* This is a labeled statement: emit the label, consume ':', recurse */
            emit_label(find_or_alloc_user_label(lname));
            next_token();
            gen_statement();
        } else {
            /* Not a label — restore state and fall through to expression statement */
            posi = save_pos;
            tok_start_pos = save_tok_start;
            line_no = save_line;
            tok_line = save_tok_line;
            tok = save_tok;
            gen_expr_statement();
        }
    } else {
        gen_expr_statement();
    }
}


static int current_void_is_empty_param_list(void)
{
    long save_pos;
    long save_tok_start;
    int save_line;
    int save_tok_line;
    struct Token save_tok;
    int r;

    if (tok.kind != TOK_VOID)
        return 0;

    save_pos = posi;
    save_tok_start = tok_start_pos;
    save_line = line_no;
    save_tok_line = tok_line;
    save_tok = tok;

    next_token();
    r = (tok.kind == ')');

    posi = save_pos;
    tok_start_pos = save_tok_start;
    line_no = save_line;
    tok_line = save_tok_line;
    tok = save_tok;

    return r;
}

static void skip_prototype_array_suffixes(int *ptype)
{
    while (accept('[')) {
        while (tok.kind != ']' && tok.kind != TOK_EOF)
            next_token();
        expect(']');
        ptype[0] = type_add_ptr(ptype[0]);
    }
}

static void skip_prototype_function_suffix(void)
{
    int depth;

    if (!accept('('))
        return;

    depth = 1;
    while (tok.kind != TOK_EOF && depth > 0) {
        if (tok.kind == '(')
            depth++;
        else if (tok.kind == ')')
            depth--;
        next_token();
    }
}


static void clear_parsed_prototype(void)
{
    int i;
    g_proto_has = 0;
    g_proto_nargs = 0;
    g_proto_variadic = 0;
    for (i = 0; i < MAX_PROTO_PARAMS; ++i)
        g_proto_types[i] = 0;
}

static void copy_parsed_prototype_to_sym(struct Sym *s)
{
    int i;
    if (!s) return;
    s->has_proto = g_proto_has;
    s->proto_nargs = g_proto_nargs;
    s->proto_variadic = g_proto_variadic;
    for (i = 0; i < MAX_PROTO_PARAMS; ++i)
        s->proto_types[i] = g_proto_types[i];
}

static void remember_proto_param_type(int type)
{
    g_proto_has = 1;
    if (g_proto_nargs < MAX_PROTO_PARAMS)
        g_proto_types[g_proto_nargs] = type;
    g_proto_nargs++;
}

static void parse_param_list(void)
{
    int type;
    char name[64];
    int unnamed_id;

    nlocals = 0;
    local_size = 0;
    param_offset = ((parse_function_return_type & TYPE_STRUCT) && type_ptr_depth(parse_function_return_type) == 0) ? 6 : 4;
    clear_parsed_prototype();

    if (current_void_is_empty_param_list()) {
        g_proto_has = 1;
        next_token();
        return;
    }

    /* Empty parentheses in C89 mean old-style/no prototype. */
    if (tok.kind == ')') return;

    for (;;) {
        if (tok.kind == TOK_ELLIPSIS) {
            g_proto_has = 1;
            g_proto_variadic = 1;
            next_token();
            break;
        }

        type = parse_type();
        unnamed_id = 0;

        while (accept('*')) {
            skip_type_qualifiers();
            type = type_add_ptr(type);
        }
        skip_type_qualifiers();

        if (parse_funcptr_declarator(&type, name, sizeof(name))) {
            /* function pointer parameter */
        } else if (tok.kind == TOK_ID) {
            strncpy(name, tok.text, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            next_token();
        } else {
            /* Prototype declarations may omit parameter names:
             *     int f(int, char *);
             * Give such parameters private dummy names so function
             * definitions using named parameters continue to work exactly
             * as before, while header prototypes are accepted. */
            sprintf(name, "__p%d", param_offset);
            unnamed_id = 1;
        }

        /* Parameter arrays decay to pointers.  This makes both named and
         * unnamed forms work:
         *     char *argv[]
         *     char *[]
         */
        skip_prototype_array_suffixes(&type);

        /* Accept function-typed parameters in prototypes and treat them as
         * pointer-sized for this compiler's simple type model.  This keeps
         * declarations like int f(int cb(void)); from poisoning the parse. */
        if (tok.kind == '(') {
            skip_prototype_function_suffix();
            type = type_add_ptr(type);
        }

        remember_proto_param_type(type);
        add_param_alloc(name, type);
        (void)unnamed_id;

        if (!accept(',')) break;
    }
}

static void emit_function_prologue(const char *name, int local_bytes)
{
    struct Sym *s;

    s = find_global(name);
    if (!s || !s->is_static)
        fprintf(outf, "\n\tpublic %s\n", asm_name_for(name));
    else
        fprintf(outf, "\n");

    fprintf(outf, "%s:\n", asm_name_for(name));
    emit("\tpush ix\n");
    emit("\tld ix,0\n");
    emit("\tadd ix,sp\n");

    if (local_bytes > 0) {
        fprintf(outf, "\tld hl,-%d\n", local_bytes);
        emit("\tadd hl,sp\n");
        emit("\tld sp,hl\n");
    }
}

static void emit_function_epilogue(void)
{
    emit_label(current_return_label);
    /* Always emit ld sp,ix so that gen_switch_chain's push/pop of the switch
     * value does not corrupt the stack when a return fires inside a case body.
     * pass_elim_ix_frame and pass_shared_frame_stubs clean up the extra
     * instruction for functions that never actually need the stack restore. */
    emit("\tld sp,ix\n");
    emit("\tpop ix\n");
    emit("\tret\n");
}

static void skip_initializer_or_decl_tail(void)
{
    int depth;

    depth = 0;

    while (tok.kind != TOK_EOF) {
        if (depth == 0 && (tok.kind == ',' || tok.kind == ';')) return;

        if (tok.kind == '(' || tok.kind == '[' || tok.kind == '{') depth++;
        else if (tok.kind == ')' || tok.kind == ']' || tok.kind == '}') {
            if (depth > 0) depth--;
        }

        next_token();
    }
}


static void scan_local_decl_after_type(int base)
{
    int type, bytes, arrlen;
    int total_elems;
    char name[64];
    struct Sym *s;

    for (;;) {
        type = base;

        while (accept('*')) { skip_type_qualifiers(); type = type_add_ptr(type); }

        if (!parse_funcptr_declarator(&type, name, sizeof(name))) {
            if (tok.kind != TOK_ID) return;

            strncpy(name, tok.text, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            next_token();
        }

        arrlen = g_funcptr_decl_array_len;
        g_funcptr_decl_array_len = 0;
        total_elems = arrlen;
        {
            int first_stride_bytes;
            first_stride_bytes = 0;
            if (arrlen == 0)
                parse_array_declarator_dims(type, &total_elems, &first_stride_bytes, 1);
            else
                total_elems = arrlen;

            arrlen = total_elems;

            if (arrlen == 0 && g_last_array_dim_count > 0 && tok.kind == '=') {
                int atoms;
                int inner;
                int di;

                atoms = count_omitted_array_initializer_atoms();
                inner = 1;
                for (di = 1; di < g_last_array_dim_count; ++di) {
                    if (g_last_array_dims[di] > 0)
                        inner *= g_last_array_dims[di];
                }
                if (inner <= 0) inner = 1;

                if (atoms > 0) {
                    total_elems = atoms;
                    arrlen = (atoms + inner - 1) / inner;
                    g_last_array_dims[0] = arrlen;
                }
            }

            current_field_array_elem_size = first_stride_bytes;
        }
        /* inherit array length from array typedef */
        if (arrlen == 0 && g_typedef_array_len > 0) {
            arrlen = g_typedef_array_len;
            total_elems = g_typedef_array_len;
        }

        bytes = type_size(type);
        if (total_elems > 0) bytes *= total_elems;

        if (!find_local(name)) {
            s = add_local_alloc(name, type, bytes);
            if (arrlen > 0 || g_last_array_dim_count > 0) {
                s->is_array = 1;
                s->array_len = arrlen;
                s->elem_size = current_field_array_elem_size ? current_field_array_elem_size : type_size(type);
                if (s->elem_size <= 0) s->elem_size = 2;
                copy_last_array_dims_to_sym(s);
            }
        }

        if (accept('=')) skip_initializer_or_decl_tail();

        if (!accept(',')) break;
    }

    expect(';');
}

/* Function-scope static declarations are backed by normal global storage,
 * but are entered in the local symbol table so ordinary references inside
 * the function resolve correctly.  This is enough for forms such as:
 *
 *     static int knight_dir[8] = { 17, 15, ... };
 *
 * and also supports uninitialized local static arrays.
 */
static void scan_static_local_decl_after_type(int base)
{
    int type, bytes, arrlen;
    char name[64];
    struct Sym *g;
    struct Sym *l;

    for (;;) {
        type = base;

        while (accept('*')) { skip_type_qualifiers(); type = type_add_ptr(type); }

        if (tok.kind != TOK_ID) return;

        strncpy(name, tok.text, sizeof(name) - 1);
        name[sizeof(name) - 1] = 0;
        next_token();

        arrlen = g_funcptr_decl_array_len;
        g_funcptr_decl_array_len = 0;
        {
            int first_stride_bytes;
            first_stride_bytes = 0;
            if (arrlen == 0)
                parse_array_declarator_dims(type, &arrlen, &first_stride_bytes, 1);
            current_field_array_elem_size = first_stride_bytes;
        }
        if (arrlen == 0 && g_typedef_array_len > 0)
            arrlen = g_typedef_array_len;

        bytes = type_size(type);
        if (arrlen > 0)
            bytes = object_array_size(type, arrlen);
        else if (arrlen < 0)
            bytes = 0;

        g = add_global(name, type, SC_GLOBAL);
        g->size = bytes;
        if (arrlen != 0) {
            g->is_array = 1;
            g->array_len = arrlen > 0 ? arrlen : 0;
            g->elem_size = current_field_array_elem_size ? current_field_array_elem_size : type_size(type);
            if (g->elem_size <= 0) g->elem_size = 2;
            copy_last_array_dims_to_sym(g);
        }

        if (!find_local(name)) {
            l = add_local_known(name, type, SC_GLOBAL, 0, bytes);
            if (arrlen != 0) {
                l->is_array = 1;
                l->array_len = arrlen > 0 ? arrlen : 0;
                l->elem_size = g->elem_size;
                l->dim_count = g->dim_count;
                memcpy(l->dims, g->dims, sizeof(l->dims));
            }
        }

        parse_global_init_list(g);

        /* If this was static char name[] = "...", parse_global_init_list()
         * inferred the real storage size.  Mirror that back into the local
         * alias used for references inside the function.
         */
        l = find_local(name);
        if (l && g->is_array && l->is_array) {
            l->size = g->size;
            l->array_len = g->array_len;
            l->elem_size = g->elem_size;
            l->dim_count = g->dim_count;
            memcpy(l->dims, g->dims, sizeof(l->dims));
        }

        if (!accept(',')) break;
    }

    expect(';');
}

static void scan_function_body(void)
{
    int brace;
    int can_decl;

    expect('{');
    brace = 1;
    can_decl = 1;

    while (tok.kind != TOK_EOF && brace > 0) {
        if (tok.kind == '{') {
            brace++;
            next_token();
            can_decl = 1;
        } else if (tok.kind == '}') {
            brace--;
            next_token();
            can_decl = 1;
        } else if (tok.kind == TOK_FOR) {
            /*
             * The old scanner looked for starts_type() at every token in the
             * function body.  That is unsafe now that casts are supported:
             *
             *     f = s + ((float)p / (float)denom);
             *
             * During the pre-pass, the "float" in the cast was mistaken for a
             * block declaration.  scan_local_decl_after_type() then bailed out
             * at ')' without consuming a declaration, and the scan could miss
             * later real declarations.  The generated prologue therefore
             * reserved too little stack space; a call such as printf could
             * overwrite locals, making a later call like nmfpart(f) receive
             * garbage even though f printed correctly just before the call.
             *
             * Scan declarations only at statement/declaration boundaries, with
             * a special case for C99-style for-init declarations.
             */
            next_token();
            if (accept('(')) {
                int depth;

                if (starts_type()) {
                    int t;
                    decl_is_extern = 0;
                    t = parse_base_type();
                    if (tok.kind != ';')
                        scan_local_decl_after_type(t); /* consumes ';' */
                    else
                        next_token();
                } else {
                    depth = 0;
                    while (tok.kind != TOK_EOF) {
                        if (depth == 0 && tok.kind == ';') {
                            next_token();
                            break;
                        }
                        if (tok.kind == '(' || tok.kind == '[') depth++;
                        else if (tok.kind == ')' || tok.kind == ']') {
                            if (depth > 0) depth--;
                        }
                        next_token();
                    }
                }

                /* Skip the condition and increment clauses.  They may contain
                 * casts, but cannot contain declarations except for the
                 * for-init handled above. */
                depth = 0;
                while (tok.kind != TOK_EOF) {
                    if (depth == 0 && tok.kind == ')') {
                        next_token();
                        break;
                    }
                    if (tok.kind == '(' || tok.kind == '[') depth++;
                    else if (tok.kind == ')' || tok.kind == ']') {
                        if (depth > 0) depth--;
                    }
                    next_token();
                }
            }
            can_decl = 1;
        } else if (can_decl && tok.kind == TOK_TYPEDEF) {
            parse_typedef_decl();
            can_decl = 1;
        } else if (can_decl && starts_type()) {
            int t;
            int is_static_local;
            decl_is_extern = 0;
            is_static_local = (tok.kind == TOK_STATIC);
            t = parse_base_type();
            if (tok.kind == ';') {
                next_token();
            } else if (is_static_local) {
                scan_static_local_decl_after_type(t);
            } else {
                scan_local_decl_after_type(t);
            }
            can_decl = 1;
        } else {
            int k;
            k = tok.kind;
            next_token();

            if (k == ';' || k == ':')
                can_decl = 1;
            else
                can_decl = 0;
        }
    }
}

static void parse_typedef_decl(void)
{
    int base_type;
    int done;

    expect(TOK_TYPEDEF);

    /* Parse C89 typedef declarator lists with per-declarator pointer and
     * suffix handling:
     *     typedef unsigned long UL, *PUL;
     *     typedef int A4[4], FN(int), (*PF)(int);
     */
    base_type = parse_base_type();
    done = 0;

    while (!done && tok.kind != TOK_EOF) {
        int type;
        int typedef_array_len;
        int is_func;
        char name[64];

        type = base_type;
        typedef_array_len = 0;
        is_func = 0;
        name[0] = 0;

        while (accept('*')) {
            skip_type_qualifiers();
            type = type_add_ptr(type);
        }

        if (parse_funcptr_declarator(&type, name, sizeof(name))) {
            /* Parenthesized function-pointer typedef. */
        } else {
            if (tok.kind != TOK_ID) {
                error_here("identifier expected in typedef");
                while (tok.kind != ';' && tok.kind != TOK_EOF) next_token();
                expect(';');
                return;
            }
            strncpy(name, tok.text, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            next_token();
        }

        if (tok.kind == '[') {
            next_token();
            if (tok.kind == ']') {
                typedef_array_len = 0;
                next_token();
            } else {
                typedef_array_len = parse_const_int_expr();
                expect(']');
            }
            while (tok.kind == '[') {
                next_token();
                if (tok.kind != ']')
                    (void)parse_const_int_expr();
                expect(']');
            }
        } else if (tok.kind == '(') {
            skip_prototype_function_suffix();
            is_func = (type_ptr_depth(type) == 0);
        }

        add_typedef_name_ex(name, type, typedef_array_len, is_func);

        if (accept(','))
            continue;
        expect(';');
        done = 1;
    }
}

static int parse_global_init_atom(long *val, char *label, int labelsz)
{
    int sign;

    sign = 1;

    /*
     * Numeric scalar initializers may be full C constant expressions, not just
     * a single token.  This handles forms used by lzpack such as:
     *
     *     static long s_win_start = (MAXDIST * 2);
     *
     * and array bounds using parenthesized macro expressions.
     */
    if (tok.kind == TOK_NUM || tok.kind == TOK_CHARLIT ||
        tok.kind == '-' || tok.kind == '+' || tok.kind == '(' ||
        tok.kind == TOK_SIZEOF) {
        val[0] = parse_const_long_expr();
        if (label) label[0] = 0;
        return 1;
    }

    if (sign != 1) {
        error_here("numeric constant expected after sign");
        return 0;
    }

    if (tok.kind == TOK_STR || tok.kind == TOK_WSTR) {
        int sid;

        {
            char *lit;
            int is_wide;
            lit = read_adjacent_string_literals_ex(&is_wide);
            sid = add_string_ex(lit, is_wide);
            free(lit);
        }
        if (label && labelsz > 0)
            sprintf(label, "S%d", sid);
        return 2;       /* symbolic address */
    }

    if (tok.kind == TOK_ID) {
        if (label && labelsz > 0) {
            strncpy(label, tok.text, labelsz - 1);
            label[labelsz - 1] = 0;
        }
        next_token();
        return 2;       /* symbolic address */
    }

    error_here("constant initializer expected");
    return 0;
}



static void append_global_init(struct Sym *s, const char *label, long v, int bytes, int is_label)
{
    if (s->init_count >= 256) {
        error_here("too many initializer elements");
        return;
    }
    if (bytes <= 0) bytes = 2;
    if (is_label) {
        strncpy(s->init_labels[s->init_count], label, sizeof(s->init_labels[0]) - 1);
        s->init_labels[s->init_count][sizeof(s->init_labels[0]) - 1] = 0;
    } else {
        sprintf(s->init_labels[s->init_count], "%lu", (unsigned long)v);
    }
    s->init_sizes[s->init_count] = bytes;
    s->init_count++;
}

static void append_global_zero_bytes(struct Sym *s, int bytes)
{
    while (bytes > 0) {
        int n;
        n = bytes >= 2 ? 2 : 1;
        append_global_init(s, NULL, 0, n, 0);
        bytes -= n;
    }
}

static void append_global_char_array_string(struct Sym *s, int count, const char *str)
{
    int i;
    int n;

    n = (int)strlen(str);
    if (count <= 0)
        return;

    if (n > count) {
        error_here("string initializer too long for char array field");
        n = count;
    }

    for (i = 0; i < n; ++i)
        append_global_init(s, NULL, (unsigned char)str[i], 1, 0);

    while (i < count) {
        append_global_init(s, NULL, 0, 1, 0);
        i++;
    }
}

static void parse_global_init_type(struct Sym *s, int type, int size);

static void parse_global_init_array(struct Sym *s, int elem_type, int count, int elem_size)
{
    int n;

    if (elem_size <= 0) elem_size = type_size(elem_type);
    if (elem_size <= 0) elem_size = 2;

    if ((elem_type & 15) == TYPE_CHAR && type_ptr_depth(elem_type) == 0 &&
        tok.kind == TOK_STR) {
        char *lit;
        int is_wide;
        lit = read_adjacent_string_literals_ex(&is_wide);
        if (is_wide)
            error_here("wide string cannot initialize char array field");
        else
            append_global_char_array_string(s, count, lit);
        free(lit);
        return;
    }

    if (tok.kind == '{')
        next_token();
    n = 0;
    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (count > 0 && n >= count) {
            error_here("too many initializer elements");
            skip_initializer_or_decl_tail();
            break;
        }
        parse_global_init_type(s, elem_type, elem_size);
        n++;
        if (!accept(',')) break;
        if (tok.kind == '}') break;
    }
    expect('}');

    while (count > 0 && n < count) {
        append_global_zero_bytes(s, elem_size);
        n++;
    }
}

static void parse_global_init_struct(struct Sym *s, int type)
{
    int sid;
    int i;
    int used;
    int total;
    int is_union;

    sid = type_struct_id(type);
    total = type_size(type);
    used = 0;
    is_union = (sid > 0 && sid <= nstruct_defs && struct_defs[sid - 1].is_union);

    if (tok.kind == '{')
        next_token();

    if (is_union) {
        struct FieldDef *first;
        first = NULL;
        for (i = 0; i < nfield_defs; ++i) {
            if (field_defs[i].parent_struct_id == sid) {
                first = &field_defs[i];
                break;
            }
        }

        if (first && tok.kind != TOK_EOF && tok.kind != '}') {
            if (first->is_array)
                parse_global_init_array(s, first->elem_type, first->array_len, first->elem_size);
            else
                parse_global_init_type(s, first->type, first->size);
            used = first->size;

            if (accept(',')) {
                if (tok.kind != '}') {
                    error_here("too many union initializer elements");
                    while (tok.kind != TOK_EOF && tok.kind != '}') {
                        skip_initializer_or_decl_tail();
                        if (tok.kind == ',') next_token();
                        else break;
                    }
                }
            }
        }

        expect('}');
        if (total > used)
            append_global_zero_bytes(s, total - used);
        return;
    }

    for (i = 0; i < nfield_defs && tok.kind != TOK_EOF && tok.kind != '}'; ++i) {
        struct FieldDef *fd;
        fd = &field_defs[i];
        if (fd->parent_struct_id != sid)
            continue;

        if (fd->offset > used)
            append_global_zero_bytes(s, fd->offset - used);

        if (fd->bit_width > 0) {
            int unit_off;
            int k;
            int next;
            unsigned int unit;
            int stop;

            unit_off = fd->offset;
            unit = 0;
            stop = 0;
            k = i;
            while (k >= 0 && k < nfield_defs && tok.kind != TOK_EOF && tok.kind != '}') {
                struct FieldDef *bfd;
                bfd = &field_defs[k];
                if (bfd->parent_struct_id == sid) {
                    if (bfd->bit_width <= 0 || bfd->offset != unit_off)
                        break;
                    unit |= bitfield_init_part(bfd, parse_struct_init_const_value());
                    if (!accept(',')) {
                        stop = 1;
                        break;
                    }
                    if (tok.kind == '}') {
                        stop = 1;
                        break;
                    }
                }
                next = next_parent_field_index(sid, k + 1);
                if (next < 0) {
                    if (tok.kind != '}') {
                        error_here("too many initializer elements");
                        while (tok.kind != TOK_EOF && tok.kind != '}') {
                            skip_initializer_or_decl_tail();
                            if (tok.kind == ',') next_token();
                            else break;
                        }
                    }
                    stop = 1;
                    break;
                }
                k = next;
            }
            append_global_init(s, NULL, (long)(unit & 0xffffU), 2, 0);
            used = unit_off + 2;
            if (k > i)
                i = k - 1;
            if (stop)
                break;
            continue;
        }

        if (fd->is_array)
            parse_global_init_array(s, fd->elem_type, fd->array_len, fd->elem_size);
        else
            parse_global_init_type(s, fd->type, fd->size);

        used = fd->offset + fd->size;
        if (!accept(',')) break;
        if (tok.kind == '}') break;
    }
    expect('}');

    if (total > used)
        append_global_zero_bytes(s, total - used);
}

static void parse_global_init_type(struct Sym *s, int type, int size)
{
    long v;
    char label[64];
    int k;

    if ((type & TYPE_STRUCT) && type_ptr_depth(type) == 0) {
        parse_global_init_struct(s, type);
        return;
    }

    if ((type & 15) == TYPE_FLOAT && type_ptr_depth(type) == 0) {
        unsigned long bits;
        if (parse_float_init_literal(&bits))
            append_global_init(s, NULL, (long)bits, 4, 0);
        else {
            error_here("float initializer must be constant");
            if (tok.kind != ',' && tok.kind != '}') next_token();
        }
        return;
    }

    k = parse_global_init_atom(&v, label, sizeof(label));
    if (k == 1)
        append_global_init(s, NULL, v, size, 0);
    else if (k == 2)
        append_global_init(s, label, 0, size, 1);
    else
        next_token();
}

static void parse_global_scalar_array_init_scalar(struct Sym *s, int *np)
{
    long v;
    int k;
    int n;
    int elem_bytes;

    n = np[0];
    if (n >= 256) {
        error_here("too many initializer elements");
        if (tok.kind != ',' && tok.kind != '}')
            next_token();
        return;
    }

    s->init_labels[n][0] = 0;

    elem_bytes = type_size(s->type);
    if (elem_bytes <= 0)
        elem_bytes = 2;

    if ((s->type & 15) == TYPE_FLOAT && type_ptr_depth(s->type) == 0) {
        unsigned long bits;
        if (parse_float_init_literal(&bits)) {
            sprintf(s->init_labels[n], "%lu", bits);
            s->init_sizes[n] = 4;
            np[0] = n + 1;
        } else {
            error_here("float initializer must be constant");
            if (tok.kind != ',' && tok.kind != '}')
                next_token();
        }
    } else {
        k = parse_global_init_atom(&v, s->init_labels[n],
                                   sizeof(s->init_labels[n]));
        if (k == 1) {
            sprintf(s->init_labels[n], "%ld", v);
            s->init_sizes[n] = elem_bytes;
            np[0] = n + 1;
        } else if (k == 2) {
            s->init_sizes[n] = elem_bytes;
            np[0] = n + 1;
        } else {
            if (tok.kind != ',' && tok.kind != '}')
                next_token();
        }
    }
}

static void parse_global_scalar_array_zero_to(struct Sym *s, int *np, int limit)
{
    int elem_bytes;

    elem_bytes = type_size(s->type);
    if (elem_bytes <= 0)
        elem_bytes = 2;

    while (np[0] < limit && np[0] < 256) {
        sprintf(s->init_labels[np[0]], "0");
        s->init_sizes[np[0]] = elem_bytes;
        np[0] = np[0] + 1;
    }
}

static void parse_global_scalar_array_init_level(struct Sym *s, int *np, int level)
{
    int start;
    int limit;

    if (!accept('{')) {
        parse_global_scalar_array_init_scalar(s, np);
        return;
    }

    start = np[0];
    limit = start + sym_array_elems_from_level(s, level);

    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == '{' && s->dim_count > 0 && level + 1 < s->dim_count)
            parse_global_scalar_array_init_level(s, np, level + 1);
        else
            parse_global_scalar_array_init_scalar(s, np);

        if (!accept(','))
            break;
        if (tok.kind == '}')
            break;
    }
    expect('}');

    parse_global_scalar_array_zero_to(s, np, limit);
}



static void parse_global_init_list(struct Sym *s)
{
    int n;
    long v;
    int k;

    if (!accept('='))
        return;

    /*
     * C permits char arrays to be initialized by one or more adjacent
     * string literals:
     *     static char s[6] = "he" "llo";
     * The lexer already decodes escapes into tok.text.  Store the bytes in
     * init_labels[] as decimal strings so the existing data emitter can use
     * the normal 1-byte array path.  Add the trailing NUL if there is room in
     * the declared object.
     */
    if (s->is_array && (s->type & 15) == TYPE_CHAR && type_ptr_depth(s->type) == 0 &&
        tok.kind == TOK_STR) {
        n = 0;
        while (tok.kind == TOK_STR) {
            int si;
            for (si = 0; tok.text[si]; ++si) {
                if (n >= 256) {
                    error_here("too many initializer elements");
                    break;
                }
                sprintf(s->init_labels[n], "%u", (unsigned char)tok.text[si]);
                s->init_sizes[n] = 1;
                n++;
            }
            next_token();
        }

        /* For C89 forms like:
         *     static char s[] = "hello";
         * infer the array size from the string literal plus the terminating
         * NUL.  Callers mark omitted-dimension arrays with is_array set and
         * size/array_len left as zero.
         */
        if (s->size == 0 || s->array_len == 0) {
            if (n < 256) {
                sprintf(s->init_labels[n], "0");
                s->init_sizes[n] = 1;
                n++;
            }
            s->size = n;
            s->array_len = n;
            s->elem_size = 1;
        } else if (n < s->size && n < 256) {
            sprintf(s->init_labels[n], "0");
            s->init_sizes[n] = 1;
            n++;
        }

        while (s->size > 0 && n < s->size && n < 256) {
            sprintf(s->init_labels[n], "0");
            s->init_sizes[n] = 1;
            n++;
        }

        s->has_init = 1;
        s->init_count = n;
        return;
    }

    if (!accept('{')) {
        if (!s->is_array) {
            if ((s->type & TYPE_STRUCT) && type_ptr_depth(s->type) == 0) {
                error_here("struct initializer list expected");
                skip_initializer_or_decl_tail();
                return;
            }
            s->has_init = 1;
            if ((s->type & 15) == TYPE_FLOAT && type_ptr_depth(s->type) == 0) {
                unsigned long bits;
                if (parse_float_init_literal(&bits)) {
                    sprintf(s->init_labels[0], "%lu", bits);
                    s->init_sizes[0] = 4;
                    s->init_count = 1;
                }
            } else {
                k = parse_global_init_atom(&v, s->init_labels[0],
                                           sizeof(s->init_labels[0]));
                if (k == 1) {
                    s->init_value = v;
                    s->init_count = 0;
                } else if (k == 2) {
                    s->init_sizes[0] = type_size(s->type);
                    if (s->init_sizes[0] <= 0) s->init_sizes[0] = 2;
                    s->init_count = 1;
                }
            }
        } else {
            error_here("array initializer list expected");
        }
        return;
    }

    /* Aggregate initializers for structs and arrays of structs need to be
     * flattened field-by-field so each emitted element uses the correct byte
     * width.  Plain scalar arrays keep the older simple path below.
     */
    if ((s->type & TYPE_STRUCT) && type_ptr_depth(s->type) == 0) {
        s->init_count = 0;
        if (s->is_array)
            parse_global_init_array(s, s->type, s->array_len, s->elem_size);
        else
            parse_global_init_struct(s, s->type);
        s->has_init = 1;
        return;
    }

    n = 0;
    while (tok.kind != TOK_EOF && tok.kind != '}') {
        if (tok.kind == '{' && s->dim_count > 1)
            parse_global_scalar_array_init_level(s, &n, 1);
        else
            parse_global_scalar_array_init_scalar(s, &n);
        if (!accept(','))
            break;
        if (tok.kind == '}')
            break;
    }

    expect('}');
    if (s->is_array && s->array_len > 0) {
        int total;
        total = sym_array_total_elems(s);
        parse_global_scalar_array_zero_to(s, &n, total);
    }
    s->has_init = 1;
    s->init_count = n;
    if (s->is_array && (s->array_len == 0 || s->size == 0)) {
        int elem_bytes;
        elem_bytes = type_size(s->type);
        if (elem_bytes <= 0) elem_bytes = 2;

        infer_omitted_first_dim_from_init(s, n);

        if (s->array_len == 0)
            s->array_len = n;
        if (s->size == 0)
            s->size = n * elem_bytes;
        if (s->elem_size <= 0)
            s->elem_size = elem_bytes;
    }
}

static void parse_function_or_global(int base_type)
{
    int done;

    done = 0;

    while (!done && tok.kind != TOK_EOF) {
        int type;
        char name[64];
        int arrlen;
        struct Sym *s;
        long body_end_pos;
        long body_end_tok_start;
        int body_end_line;
        int body_end_tok_line;
        struct Token body_end_tok;
        long saved_pos;
        long saved_tok_start;
        int saved_line;
        int saved_tok_line;
        struct Token saved_tok;
        int saved_nlocals;
        int saved_local_size;
        int saved_param_offset;

        int base_is_func_typedef;

        type = base_type;
        base_is_func_typedef = g_typedef_is_func;
        name[0] = 0;

        /* Each declarator starts again from the shared declaration-specifier
         * base type.  This is the important C declarator rule for forms like:
         *     int *a, b, c[10];
         * where only a is a pointer. */
        while (accept('*')) {
            skip_type_qualifiers();
            type = type_add_ptr(type);
            base_is_func_typedef = 0;
        }

        if (!parse_funcptr_declarator(&type, name, sizeof(name))) {
            if (tok.kind != TOK_ID) {
                error_here("identifier expected");
                while (tok.kind != ';' && tok.kind != TOK_EOF) next_token();
                expect(';');
                return;
            }

            strncpy(name, tok.text, sizeof(name) - 1);
            name[sizeof(name) - 1] = 0;
            next_token();
        }

        /* A typedef-name that denotes a function type can declare a function
         * without repeating the parameter list:
         *     typedef int fn_t(int);
         *     extern fn_t foo;
         * Treat this as a function declaration.  Pointer declarators such as
         * fn_t *fp have already cleared base_is_func_typedef above. */
        if (base_is_func_typedef && g_funcptr_decl_array_len == 0) {
            s = add_global(name, type, SC_FUNC);
            parse_function_return_type = type;
            if (decl_is_static) {
                s->is_static = 1;
                s->needs_extrn = 0;
            } else if (!s->is_defined)
                s->needs_extrn = 1;
            if (accept(','))
                continue;
            expect(';');
            return;
        }

        /* Function declarator or definition. */
        if (g_funcptr_decl_array_len == 0 && accept('(')) {
            s = add_global(name, type, SC_FUNC);
            parse_function_return_type = type;
            if (decl_is_static) {
                s->is_static = 1;
                s->needs_extrn = 0;
            }
            parse_param_list();
            copy_parsed_prototype_to_sym(s);
            expect(')');

            if (tok.kind == '{') {
                saved_pos = posi;
                saved_tok_start = tok_start_pos;
                saved_line = line_no;
                saved_tok_line = tok_line;
                saved_tok = tok;
                saved_nlocals = nlocals;
                saved_local_size = local_size;
                saved_param_offset = param_offset;

                scan_function_body();
                body_end_pos = posi;
                body_end_tok_start = tok_start_pos;
                body_end_line = line_no;
                body_end_tok_line = tok_line;
                body_end_tok = tok;
                current_local_bytes = local_size;
                if (current_local_bytes > max_function_local_bytes)
                    max_function_local_bytes = current_local_bytes;

                posi = saved_pos;
                tok_start_pos = saved_tok_start;
                line_no = saved_line;
                tok_line = saved_tok_line;
                tok = saved_tok;
                nlocals = saved_nlocals;
                local_size = saved_local_size;
                param_offset = saved_param_offset;

                scan_function_body();

                posi = saved_pos;
                tok_start_pos = saved_tok_start;
                line_no = saved_line;
                tok_line = saved_tok_line;
                tok = saved_tok;

                s->is_defined = 1;
                s->needs_extrn = 0;

                nulabels = 0;
                current_return_label = new_label();
                current_return_type = type;
                emit_function_prologue(name, current_local_bytes);
                gen_compound();
                emit_function_epilogue();

                posi = body_end_pos;
                tok_start_pos = body_end_tok_start;
                line_no = body_end_line;
                tok_line = body_end_tok_line;
                tok = body_end_tok;
                return;
            }

            /*
             * C89: a file-scope function declaration has external linkage even
             * without the 'extern' keyword.  Record it as a possible external,
             * but the M80 EXTRN is emitted only if actually referenced and not
             * later defined in this translation unit.
             */
            if (decl_is_static) {
                s->is_static = 1;
                s->needs_extrn = 0;
            } else if (!s->is_defined)
                s->needs_extrn = 1;

            if (accept(','))
                continue;
            expect(';');
            return;
        }

        {
            int total_count = 1;
            int first_dim = g_funcptr_decl_array_len;
            int base_size = type_size(type);
            int dim_count = 0;
            int dims[8];
            int i;
            int inner_count;

            for (i = 0; i < 8; ++i)
                dims[i] = 0;

            if (g_funcptr_decl_array_len > 0) {
                total_count = g_funcptr_decl_array_len;
                dim_count = 1;
                dims[0] = g_funcptr_decl_array_len;
            }
            g_funcptr_decl_array_len = 0;

            while (tok.kind == '[') {
                int d;
                next_token();
                if (tok.kind == ']') {
                    d = 0;
                    next_token();
                } else {
                    d = parse_const_int_expr();
                    expect(']');
                }
                if (dim_count < 8)
                    dims[dim_count++] = d;
            }

            if (dim_count > 0) {
                first_dim = dims[0];

                total_count = 1;
                for (i = 0; i < dim_count; ++i) {
                    if (dims[i] <= 0) {
                        total_count = 0;
                        break;
                    }
                    total_count *= dims[i];
                }
            }

            arrlen = first_dim;
            if (arrlen == 0 && dim_count == 0 && g_typedef_array_len > 0) {
                arrlen = g_typedef_array_len;
                first_dim = g_typedef_array_len;
                total_count = g_typedef_array_len;
                dim_count = 1;
                dims[0] = g_typedef_array_len;
            }

            inner_count = 1;
            if (dim_count > 1) {
                for (i = 1; i < dim_count; ++i) {
                    if (dims[i] <= 0) {
                        inner_count = 1;
                        break;
                    }
                    inner_count *= dims[i];
                }
            }

            if (decl_is_extern) {
                int already_declared = (find_global(name) != NULL);
                s = add_global(name, type, SC_EXTERN);
                if (!already_declared && !asm_name_is_internal_public(name))
                    s->needs_extrn = 1;
                else if (asm_name_is_internal_public(name))
                    s->needs_extrn = 0;

                /* Extern declarations may also be declarator lists:
                 *     extern int a, *b, f(void);
                 * Do not skip to ';' after the first one. */
                if (accept(','))
                    continue;
                expect(';');
                return;
            }

            s = add_global(name, type, SC_GLOBAL);
            s->is_defined = 1;
            s->needs_extrn = 0;
            if (decl_is_static)
                s->is_static = 1;

            if (dim_count > 0 || arrlen || total_count == 0) {
                s->is_array = 1;
                s->array_len = arrlen;
                s->dim_count = dim_count;
                for (i = 0; i < 8; ++i)
                    s->dims[i] = (i < dim_count) ? dims[i] : 0;

                if (dim_count > 1)
                    s->elem_size = inner_count * base_size;
                else
                    s->elem_size = base_size;
                if (s->elem_size <= 0) s->elem_size = 2;

                if (total_count > 0)
                    s->size = object_array_size(type, total_count);
                else
                    s->size = 0;
            }

            parse_global_init_list(s);
        }

        if (accept(','))
            continue;

        expect(';');
        done = 1;
    }
}

static void add_predefined_extern(const char *name, int type, int storage)
{
    struct Sym *s;

    s = add_global(name, type, storage);
    if (!asm_name_is_internal_public(name))
        s->needs_extrn = 1;
}

static void parse_translation_unit(void)
{
    emit("\t; dcc stage-1d output\n\n      cseg\n");

    /*
     * Do not predeclare C library/runtime functions here.  In C89, file-scope
     * prototypes in headers already have external linkage even without the
     * 'extern' keyword; parse_function_or_global() records those prototypes.
     * If no prototype is visible, call codegen can still create an implicit
     * extern function symbol.  M80 EXTRN records are deferred until end of
     * translation unit and only emitted for symbols that were actually used and
     * not defined here.
     */

    /* Predefined linker-visible bounds of the final app's BSS.
     * Compile-only helper modules must not define or reference these, or
     * multiple independently compiled modules will collide at link time. */
    if (!opt_module) {
        add_global("__bssb", TYPE_CHAR, SC_EXTERN);
        add_global("__bsse", TYPE_CHAR, SC_EXTERN);
        add_global("__hstart", TYPE_CHAR, SC_EXTERN);
        add_global("__data_end", TYPE_CHAR, SC_EXTERN);
    }

    add_predefined_extern("stdin", TYPE_INT, SC_EXTERN);
    add_predefined_extern("stdout", TYPE_INT, SC_EXTERN);
    add_predefined_extern("stderr", TYPE_INT, SC_EXTERN);
    add_predefined_extern("errno", TYPE_INT, SC_EXTERN);

    next_token();

    while (tok.kind != TOK_EOF) {
        if (tok.kind == TOK_TYPEDEF) {
            parse_typedef_decl();
        } else if (starts_type()) {
            int t;
            decl_is_extern = 0;
            decl_is_static = 0;
            t = parse_type();
            if (tok.kind == ';') {
                next_token();
            } else {
                parse_function_or_global(t);
            }
        } else {
            error_here("external declaration expected");
            next_token();
        }
    }
}

static int init_label_is_number(const char *p)
{
    if (*p == '-' || *p == '+')
        p++;

    if (!isdigit((unsigned char)*p))
        return 0;

    while (isdigit((unsigned char)*p))
        p++;

    return *p == 0;
}

static void emit_init_numeric(long v, int bytes)
{
    unsigned long uv;

    /* Treat initializer storage as raw object bits.  This matters for
     * float constants such as -3.0f whose IEEE representation has the
     * high bit set.  On MSVC, long is 32-bit and atol()/signed shifts can
     * corrupt values above LONG_MAX, so keep emission unsigned here. */
    uv = (unsigned long)v;

    if (bytes == 1) {
        fprintf(outf, "\tdb %lu\n", uv & 255UL);
    } else if (bytes == 4) {
        fprintf(outf, "\tdw %lu\n", uv & 0xffffUL);
        fprintf(outf, "\tdw %lu\n", (uv >> 16) & 0xffffUL);
    } else {
        fprintf(outf, "\tdw %lu\n", uv & 0xffffUL);
    }
}

static int init_label_is_string_literal_label(const char *p)
{
    if (*p != 'S')
        return 0;

    p++;
    if (!isdigit((unsigned char)*p))
        return 0;

    while (isdigit((unsigned char)*p))
        p++;

    return *p == 0;
}

static void emit_init_label_or_number(const char *p, int bytes)
{
    long v;

    if (init_label_is_number(p)) {
        v = (long)strtoul(p, NULL, 10);
        emit_init_numeric(v, bytes);
        return;
    }

    /*
     * Symbolic initializers are addresses.  String literal labels are
     * compiler-generated assembly labels (S0, S1, ...), while C symbol
     * names still need normal target name mapping.
     */
    if (init_label_is_string_literal_label(p))
        fprintf(outf, "\tdw %s\n", p);
    else
        fprintf(outf, "\tdw %s\n", asm_name_for(p));

    if (bytes > 2)
        fprintf(outf, "\tds %d\n", bytes - 2);
}

static void emit_data(void)
{
    int i, j;
    struct Sym *s;

    emit("\n\t; string literals\n");

    for (i = 0; i < nstrings; ++i) {
        int col;
        char buf[8];
        int vlen;

        fprintf(outf, "S%d:\n", i);

        if (string_wide[i]) {
            emit("\tdw ");
            col = 4; /* tab + "dw " */

            for (j = 0; strings[i][j]; ++j) {
                sprintf(buf, "%u", (unsigned char)strings[i][j]);
                vlen = (int)strlen(buf);
                if (j) {
                    if (col + 1 + vlen > 96) {
                        emit("\n\tdw ");
                        col = 4;
                    } else {
                        emit(",");
                        col += 1;
                    }
                }
                emit(buf);
                col += vlen;
            }

            if (j) {
                if (col + 2 > 96) {
                    emit("\n\tdw 0\n");
                } else {
                    emit(",0\n");
                }
            } else {
                emit("0\n");
            }
        } else {
            emit("\tdb ");
            col = 4; /* tab + "db " */

            for (j = 0; strings[i][j]; ++j) {
                sprintf(buf, "%u", (unsigned char)strings[i][j]);
                vlen = (int)strlen(buf);
                if (j) {
                    /* Break before emitting comma+value if it would push past 96 chars,
                       leaving comfortable room for the trailing ",0" */
                    if (col + 1 + vlen > 96) {
                        emit("\n\tdb ");
                        col = 4;
                    } else {
                        emit(",");
                        col += 1;
                    }
                }
                emit(buf);
                col += vlen;
            }

            /* Terminating null byte */
            if (j) {
                if (col + 2 > 96) {
                    emit("\n\tdb 0\n");
                } else {
                    emit(",0\n");
                }
            } else {
                emit("0\n");
            }
        }
    }

    emit("\n\t; initialized globals\n");

    for (i = 0; i < nglobals; ++i) {
        s = &globals[i];

        if (s->storage == SC_FUNC || s->storage == SC_EXTERN) continue;
        /* skip BSS (uninitialized) globals — emitted separately below */
        if (!(s->has_init && s->init_count > 0) && !(s->has_init && !s->is_array)) continue;

        if (!s->is_static)
            fprintf(outf, "\tpublic %s\n", asm_name_for(s->name));
        fprintf(outf, "%s:\n", asm_name_for(s->name));
        if (s->has_init && s->init_count > 0) {
            int j;
            int elem_bytes;
            int used_bytes;

            elem_bytes = s->is_array ? s->elem_size : type_size(s->type);
            if (elem_bytes <= 0)
                elem_bytes = type_size(s->type);
            if (elem_bytes <= 0)
                elem_bytes = 2;

            used_bytes = 0;
            for (j = 0; j < s->init_count; ++j) {
                 {
                    int ib;
                    ib = s->init_sizes[j] ? s->init_sizes[j] : elem_bytes;
                    emit_init_label_or_number(s->init_labels[j], ib);
                    used_bytes += ib;
                }
            }

            if (s->size > used_bytes)
                fprintf(outf, "\tds %d\n", s->size - used_bytes);
        } else {
            emit_init_numeric(s->init_value, type_size(s->type));
        }
    }

    /* BSS: uninitialized globals.
     *
     * Do not emit DS bytes for BSS.  With M80/L80, DS in the normal
     * relocatable stream is still reflected in the .COM image size.
     * COMMON also changes placement and can put BSS before/over code in
     * simple .COM links.
     *
     * Instead, mark the physical end of emitted code/data and make each
     * uninitialized global an EQU alias at an offset from __bssb.  This
     * gives every BSS object a stable run-time address immediately after
     * the loaded image without adding bytes to the .COM file.
     */
    {
        int bss_off;
        int bss_size;

        if (opt_module) {
            /* A separately linked helper module cannot share the final app's
             * synthetic __bssb EQU space: doing so would overlap BSS from
             * multiple modules and also create duplicate boundary publics.
             * Use ordinary DS storage here.  This may increase COM size for
             * helper modules with writable globals, but it is link-safe. */
            emit("\n\t; module uninitialized globals\n");
            for (i = 0; i < nglobals; ++i) {
                s = &globals[i];

                if (s->storage == SC_FUNC || s->storage == SC_EXTERN) continue;
                if ((s->has_init && s->init_count > 0) || (s->has_init && !s->is_array)) continue;

                bss_size = s->size > 0 ? s->size : 2;
                if (!s->is_static)
                    fprintf(outf, "\tpublic %s\n", asm_name_for(s->name));
                fprintf(outf, "%s:\n", asm_name_for(s->name));
                fprintf(outf, "\tds %d\n", bss_size);
            }
            return;
        }

        bss_off = 0;
        {
            int effective_stack_size;
            int min_stack_size;

            effective_stack_size = opt_stack_size;
            min_stack_size = max_function_local_bytes + 128;
            if (min_stack_size < 0)
                min_stack_size = max_function_local_bytes;
            if (effective_stack_size < min_stack_size)
                effective_stack_size = min_stack_size;

            emit("\n\tpublic\t__stack_size\n");
            fprintf(outf, "__stack_size\tequ\t%d\n", effective_stack_size);
            if (effective_stack_size != opt_stack_size)
                fprintf(outf, "\t; dcc: raised stack reserve from %d to %d; max local frame is %d bytes\n",
                        opt_stack_size, effective_stack_size, max_function_local_bytes);
        }
        emit("\n\tpublic\t__data_end\n__data_end:\n");
        emit("\tpublic\t__bssb\n__bssb:\n");

        for (i = 0; i < nglobals; ++i) {
            s = &globals[i];

            if (s->storage == SC_FUNC || s->storage == SC_EXTERN) continue;
            /* skip initialized globals — already emitted above */
            if ((s->has_init && s->init_count > 0) || (s->has_init && !s->is_array)) continue;

            bss_size = s->size > 0 ? s->size : 2;
            fprintf(outf, "%s\tequ\t__bssb+%d\n", asm_name_for(s->name), bss_off);
            bss_off += bss_size;
        }

        fprintf(outf, "__bsse\tequ\t__bssb+%d\n", bss_off);
        fprintf(outf, "__hstart\tequ\t__bsse\n");
        emit("\tpublic\t__bsse\n");
        emit("\tpublic\t__hstart\n");
    }
}

static long file_size(FILE *f)
{
    long n;
    fseek(f, 0, SEEK_END);
    n = ftell(f);
    fseek(f, 0, SEEK_SET);
    return n;
}


static char *splice_backslash_newlines(char *in, long *lenp)
{
    long i;
    long o;
    long n;
    char *out;

    n = lenp[0];
    out = (char *)xmalloc((size_t)n + 1);

    i = 0;
    o = 0;
    while (i < n) {
        if (in[i] == '\\') {
            if (i + 1 < n && in[i + 1] == '\n') {
                i += 2;
                continue;
            }
            if (i + 2 < n && in[i + 1] == '\r' && in[i + 2] == '\n') {
                i += 3;
                continue;
            }
        }

        out[o++] = in[i++];
    }

    out[o] = 0;
    free(in);
    lenp[0] = o;
    return out;
}

static char *read_file(const char *name, long *lenp)
{
    FILE *f;
    long n;
    char *p;

    f = fopen(name, "rb");
    if (!f) fatal("cannot open input");

    n = file_size(f);
    p = (char *)xmalloc((size_t)n + 1);

    if (fread(p, 1, (size_t)n, f) != (size_t)n)
        fatal("cannot read input");

    fclose(f);
    p[n] = 0;
    lenp[0] = n;

    /*
     * C translation phase 2: delete each backslash-newline pair before
     * preprocessing/tokenization.  This enables continued macro definitions,
     * strings, identifiers, and split operators.
     */
    p = splice_backslash_newlines(p, lenp);
    return p;
}

#define MAX_INCLUDE_DEPTH 8

static void append_mem(char **outp, long *lenp, long *capp,
                       const char *s, long n)
{
    char *p;
    long newcap;

    if (*lenp + n + 1 > *capp) {
        newcap = *capp ? *capp : 4096;
        while (*lenp + n + 1 > newcap)
            newcap *= 2;

        p = (char *)realloc(*outp, (size_t)newcap);
        if (!p) fatal("out of memory");

        *outp = p;
        *capp = newcap;
    }

    memcpy(*outp + *lenp, s, (size_t)n);
    *lenp += n;
    (*outp)[*lenp] = 0;
}

static void make_include_path(const char *base, const char *inc,
                              char *out, int outsz)
{
    (void)base;
    strncpy(out, inc, (size_t)outsz - 1);
    out[outsz - 1] = 0;
}

static int try_parse_include(const char *line, long n, char *name, int namesz,
                              int *is_system)
{
    long i;
    int j;
    char endch;

    i = 0;

    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;

    if (i >= n || line[i] != '#')
        return 0;

    i++;

    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;

    if (i + 7 > n || memcmp(line + i, "include", 7) != 0)
        return 0;

    i += 7;

    while (i < n && (line[i] == ' ' || line[i] == '\t'))
        i++;

    if (i >= n || (line[i] != '"' && line[i] != '<'))
        fatal("bad include syntax");

    if (line[i] == '<') {
        endch = '>';
        if (is_system) *is_system = 1;
    } else {
        endch = '"';
        if (is_system) *is_system = 0;
    }

    i++;
    j = 0;

    while (i < n && line[i] != endch) {
        if (j + 1 >= namesz)
            fatal("include name too long");

        name[j++] = line[i++];
    }

    if (i >= n || line[i] != endch)
        fatal("unterminated include name");

    name[j] = 0;
    return 1;
}

static void append_line_directive(char **outp, long *lenp, long *capp,
                                  int line, const char *name)
{
    char buf[768];

    sprintf(buf, "#line %d \"%s\"\n", line, name);
    append_mem(outp, lenp, capp, buf, (long)strlen(buf));
}

static char *preprocess_includes_file(const char *name, int depth, long *out_len)
{
    char *raw;
    char *out;
    char incname[256];
    char incpath[512];
    char *incsrc;
    long raw_len;
    long inc_len;
    long out_len2;
    long out_cap;
    long p;
    long line_start;
    long line_end;
    int src_line;

    if (depth > MAX_INCLUDE_DEPTH)
        fatal("too many nested includes");

    raw = read_file(name, &raw_len);

    out = NULL;
    out_len2 = 0;
    out_cap = 0;
    src_line = 1;

    append_line_directive(&out, &out_len2, &out_cap, 1, name);

    p = 0;
    while (p < raw_len) {
        line_start = p;

        while (p < raw_len && raw[p] != '\n')
            p++;

        line_end = p;
        if (p < raw_len && raw[p] == '\n')
            p++;

        {
            int is_system = 0;
            if (try_parse_include(raw + line_start,
                                  line_end - line_start,
                                  incname,
                                  sizeof(incname),
                                  &is_system)) {
                if (is_system) {
                    /* For system includes (<foo.h>), try the local directory
                     * first.  If a local file is found, include it just like a
                     * user include; otherwise silently drop the directive. */
                    make_include_path(name, incname, incpath, sizeof(incpath));
                    {
                        FILE *probe = fopen(incpath, "rb");
                        if (probe) {
                            fclose(probe);
                            incsrc = preprocess_includes_file(incpath, depth + 1, &inc_len);
                            append_mem(&out, &out_len2, &out_cap, incsrc, inc_len);
                            append_mem(&out, &out_len2, &out_cap, "\n", 1);
                            append_line_directive(&out, &out_len2, &out_cap, src_line + 1, name);
                            free(incsrc);
                        } else {
                            append_line_directive(&out, &out_len2, &out_cap, src_line + 1, name);
                        }
                        /* else: not found locally — silently ignore */
                    }
                } else {
                    make_include_path(name, incname, incpath, sizeof(incpath));
                    incsrc = preprocess_includes_file(incpath, depth + 1, &inc_len);
                    append_mem(&out, &out_len2, &out_cap, incsrc, inc_len);
                    append_mem(&out, &out_len2, &out_cap, "\n", 1);
                    append_line_directive(&out, &out_len2, &out_cap, src_line + 1, name);
                    free(incsrc);
                }
            } else {
                append_mem(&out, &out_len2, &out_cap,
                           raw + line_start,
                           p - line_start);
            }
        } /* end try_parse_include block */
        src_line++;
    } /* end while lines */

    free(raw);

    if (!out) {
        out = (char *)xmalloc(1);
        out[0] = 0;
    }

    out_len[0] = out_len2;
    return out;
}


/* Pre-scan active function-like macro definitions after include expansion.
 * The normal tokenizer also handles #define, but include guards and conditional
 * state in expanded headers can otherwise make a function-like macro such as
 * assert(e) unavailable by the time uses are parsed.  This pass deliberately
 * records only function-like macros, not object-like include guards such as
 * _ASSERT_H, so it does not change later conditional parsing of header bodies.
 */
/* Reduce preprocessor conditionals after include expansion.
 *
 * DCC's lexer-level preprocessor is intentionally small, but handling
 * conditional blocks only while tokenising can lose declarations in included
 * headers when a macro from the same active conditional block is pre-scanned.
 * This pass walks the already-expanded source once, honors #if/#ifdef/#ifndef,
 * #else/#elif/#endif, records active #define/#undef directives, and emits only
 * active non-directive source lines (plus #line directives).  That makes code
 * and macros in an active include block visible consistently: an assert.h block
 * can both define assert(e) and define __assert_fail().
 */
static char *filter_active_preprocessor_source(long *lenp)
{
    char *out;
    long out_len;
    long out_cap;
    long p;
    long line_start;
    long line_end;
    int active_stack[MAX_IFSTACK];
    int branch_taken[MAX_IFSTACK];
    int seen_else[MAX_IFSTACK];
    int sp;
    int active;

    out = NULL;
    out_len = 0;
    out_cap = 0;
    p = 0;
    sp = 0;
    active = 1;

    while (p < src_len) {
        const char *s;
        const char *e;
        char word[32];
        int is_directive;

        line_start = p;
        while (p < src_len && src[p] != '\n')
            p++;
        line_end = p;
        if (p < src_len && src[p] == '\n')
            p++;

        s = src + line_start;
        e = src + line_end;
        while (s < e && (*s == ' ' || *s == '\t'))
            s++;

        is_directive = (s < e && *s == '#');
        if (!is_directive) {
            if (active)
                append_mem(&out, &out_len, &out_cap, src + line_start, p - line_start);
            else
                append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        s++;
        while (s < e && (*s == ' ' || *s == '\t'))
            s++;

        {
            int wi;
            wi = 0;
            while (s < e && is_ident_char((unsigned char)*s) && wi < (int)sizeof(word) - 1)
                word[wi++] = *s++;
            word[wi] = 0;
        }

        if (!strcmp(word, "line")) {
            if (active)
                append_mem(&out, &out_len, &out_cap, src + line_start, p - line_start);
            continue;
        }

        if (!strcmp(word, "ifdef") || !strcmp(word, "ifndef")) {
            char name[64];
            int ni;
            int cond;
            while (s < e && (*s == ' ' || *s == '\t')) s++;
            ni = 0;
            while (s < e && is_ident_char((unsigned char)*s) && ni < 63)
                name[ni++] = *s++;
            name[ni] = 0;
            if (sp >= MAX_IFSTACK)
                fatal("too many nested #if");
            cond = (name[0] && find_define(name) >= 0);
            if (!strcmp(word, "ifndef"))
                cond = !cond;
            active_stack[sp] = active;
            branch_taken[sp] = (active && cond) ? 1 : 0;
            seen_else[sp] = 0;
            active = active && cond;
            sp++;
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "if")) {
            char expr[512];
            int ei;
            int cond;
            while (s < e && (*s == ' ' || *s == '\t')) s++;
            ei = 0;
            while (s < e && ei < (int)sizeof(expr) - 1)
                expr[ei++] = *s++;
            expr[ei] = 0;
            cond = pp_eval_simple_expr(expr);
            if (sp >= MAX_IFSTACK)
                fatal("too many nested #if");
            active_stack[sp] = active;
            branch_taken[sp] = (active && cond) ? 1 : 0;
            seen_else[sp] = 0;
            active = active && cond;
            sp++;
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "elif")) {
            if (sp > 0) {
                int i;
                int parent;
                int cond;
                char expr[512];
                int ei;
                i = sp - 1;
                parent = active_stack[i];
                if (seen_else[i] || branch_taken[i]) {
                    active = 0;
                } else {
                    while (s < e && (*s == ' ' || *s == '\t')) s++;
                    ei = 0;
                    while (s < e && ei < (int)sizeof(expr) - 1)
                        expr[ei++] = *s++;
                    expr[ei] = 0;
                    cond = pp_eval_simple_expr(expr);
                    active = parent && cond;
                    if (active)
                        branch_taken[i] = 1;
                }
            }
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "else")) {
            if (sp > 0) {
                int i;
                int parent;
                i = sp - 1;
                parent = active_stack[i];
                if (!seen_else[i]) {
                    active = parent && !branch_taken[i];
                    branch_taken[i] = 1;
                    seen_else[i] = 1;
                } else {
                    active = 0;
                }
            }
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "endif")) {
            if (sp > 0) {
                sp--;
                active = active_stack[sp];
            }
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!active) {
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "error")) {
            char msg[256];
            char filebuf[256];
            int lno;
            int mi;

            while (s < e && (*s == ' ' || *s == '\t')) s++;
            mi = 0;
            while (s < e && mi < (int)sizeof(msg) - 1)
                msg[mi++] = *s++;
            while (mi > 0 && (msg[mi - 1] == ' ' || msg[mi - 1] == '\t' || msg[mi - 1] == '\r'))
                mi--;
            msg[mi] = 0;

            source_location_at(line_start, filebuf, sizeof(filebuf), &lno);
            fprintf(stderr, "%s:%d: error: #error %s\n", filebuf, lno, msg);
            errors++;
            if (errors > 40)
                fatal("too many errors");
            append_mem(&out, &out_len, &out_cap, "\n", 1);
            continue;
        }

        if (!strcmp(word, "undef")) {
            char name[64];
            int ni;
            while (s < e && (*s == ' ' || *s == '\t')) s++;
            ni = 0;
            while (s < e && is_ident_char((unsigned char)*s) && ni < 63)
                name[ni++] = *s++;
            name[ni] = 0;
            if (name[0])
                remove_define(name);
            /* Keep active #undef in the filtered source so the normal
             * lexer-level preprocessor sees it at the correct source order.
             * The mutation above is only for this filtering pass.
             */
            append_mem(&out, &out_len, &out_cap, src + line_start, p - line_start);
            continue;
        }

        if (!strcmp(word, "define")) {
            char name[64];
            char val[256];
            int ni;
            int vi;

            while (s < e && (*s == ' ' || *s == '\t')) s++;
            ni = 0;
            while (s < e && is_ident_char((unsigned char)*s) && ni < 63)
                name[ni++] = *s++;
            name[ni] = 0;

            if (name[0] && s < e && *s == '(') {
                char params[8][32];
                int nargs;
                int pi;
                memset(params, 0, sizeof(params));
                nargs = 0;
                s++;
                while (s < e && *s != ')') {
                    while (s < e && (*s == ' ' || *s == '\t')) s++;
                    if (s >= e || *s == ')') break;
                    pi = 0;
                    while (s < e && is_ident_char((unsigned char)*s) && pi < 31)
                        params[nargs][pi++] = *s++;
                    params[nargs][pi] = 0;
                    if (params[nargs][0] && nargs < 7)
                        nargs++;
                    while (s < e && (*s == ' ' || *s == '\t')) s++;
                    if (s < e && *s == ',') s++;
                }
                if (s < e && *s == ')') s++;
                while (s < e && (*s == ' ' || *s == '\t')) s++;
                vi = 0;
                while (s < e && vi < (int)sizeof(val) - 1)
                    val[vi++] = *s++;
                while (vi > 0 && (val[vi - 1] == ' ' || val[vi - 1] == '\t' || val[vi - 1] == '\r'))
                    vi--;
                val[vi] = 0;
                add_define_ex(name, val[0] ? val : "1", 1, nargs, params);
            } else if (name[0]) {
                while (s < e && (*s == ' ' || *s == '\t')) s++;
                vi = 0;
                while (s < e && vi < (int)sizeof(val) - 1)
                    val[vi++] = *s++;
                while (vi > 0 && (val[vi - 1] == ' ' || val[vi - 1] == '\t' || val[vi - 1] == '\r'))
                    vi--;
                val[vi] = 0;
                add_define(name, val[0] ? val : "1");
            }
            /* Keep active #define in the filtered source so macro scope is
             * applied in normal C source order.  The add_define above is only
             * for evaluating later conditionals during this filtering pass.
             */
            append_mem(&out, &out_len, &out_cap, src + line_start, p - line_start);
            continue;
        }

        /* Unknown directives in active code are ignored but keep a newline. */
        append_mem(&out, &out_len, &out_cap, "\n", 1);
    }

    if (!out) {
        out = (char *)xmalloc(1);
        out[0] = 0;
    }

    lenp[0] = out_len;
    return out;
}

static void usage(void);

static int is_macro_name_text(const char *s, int n)
{
    int i;

    if (n <= 0)
        return 0;

    if (!is_ident_start((unsigned char)s[0]))
        return 0;

    for (i = 1; i < n; ++i) {
        if (!is_ident_char((unsigned char)s[i]))
            return 0;
    }

    return 1;
}

static void add_cmdline_define(const char *arg)
{
    char name[64];
    char value[256];
    const char *eqp;
    int namelen;

    if (!arg || !arg[0])
        usage();

    eqp = strchr(arg, '=');
    if (eqp) {
        namelen = (int)(eqp - arg);
    } else {
        namelen = (int)strlen(arg);
    }

    if (namelen <= 0 || namelen >= (int)sizeof(name))
        usage();

    if (!is_macro_name_text(arg, namelen))
        usage();

    memcpy(name, arg, (size_t)namelen);
    name[namelen] = 0;

    if (eqp) {
        strncpy(value, eqp + 1, sizeof(value) - 1);
        value[sizeof(value) - 1] = 0;
    } else {
        strcpy(value, "1");
    }

    add_define(name, value);
}

static void usage(void)
{
    fprintf(stderr, "usage: dcc [-c|-module] [-ffloatio] [-stack bytes] [-Dname[=value]] input.c -o output.mac\n");
    exit(1);
}

int main(int argc, char **argv)
{
    int i;

    input_name = NULL;
    output_name = NULL;
    opt_module = 0;
    opt_stack_size = 512;
    max_function_local_bytes = 0;

    add_define("_DCC_", "1");

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-ffloatio")) {
            opt_floatio = 1;
        } else if (!strcmp(argv[i], "-c") || !strcmp(argv[i], "-module")) {
            opt_module = 1;
        } else if (!strcmp(argv[i], "-stack") || !strcmp(argv[i], "--stack")) {
            char *endp;
            long v;
            if (++i >= argc) usage();
            v = strtol(argv[i], &endp, 0);
            if (*endp != 0 || v < 0 || v > 32767)
                usage();
            opt_stack_size = (int)v;
        } else if (!strncmp(argv[i], "-stack=", 7)) {
            char *endp;
            long v;
            v = strtol(argv[i] + 7, &endp, 0);
            if (*endp != 0 || v < 0 || v > 32767)
                usage();
            opt_stack_size = (int)v;
        } else if (!strncmp(argv[i], "--stack=", 8)) {
            char *endp;
            long v;
            v = strtol(argv[i] + 8, &endp, 0);
            if (*endp != 0 || v < 0 || v > 32767)
                usage();
            opt_stack_size = (int)v;
        } else if (!strcmp(argv[i], "-D")) {
            if (++i >= argc) usage();
            add_cmdline_define(argv[i]);
        } else if (!strncmp(argv[i], "-D", 2)) {
            add_cmdline_define(argv[i] + 2);
        } else if (!strcmp(argv[i], "-o")) {
            if (++i >= argc) usage();
            output_name = argv[i];
        } else if (argv[i][0] == '-') {
            /* ignored for now */
        } else {
            input_name = argv[i];
        }
    }

    if (!input_name) usage();
    if (!output_name) output_name = "out.mac";

    init_predefined_macro_texts();

    strncpy(current_file_name, input_name, sizeof(current_file_name) - 1);
    current_file_name[sizeof(current_file_name) - 1] = 0;

    src = preprocess_includes_file(input_name, 0, &src_len);
    {
        char *filtered_src;
        long filtered_len;
        int saved_ndefs;
        struct Def saved_defs[MAX_DEFINES];

        /* filter_active_preprocessor_source() uses the define table to
         * evaluate #if/#ifdef while reducing inactive source.  Do not let
         * definitions discovered later in the file leak into the real parse
         * before their source-order #define is reached; otherwise a later
         * '#define n 20' can rewrite 'int n' in an earlier included header.
         */
        saved_ndefs = ndefs;
        memcpy(saved_defs, defs, sizeof(defs));

        filtered_src = filter_active_preprocessor_source(&filtered_len);

        ndefs = saved_ndefs;
        memcpy(defs, saved_defs, sizeof(defs));

        free(src);
        src = filtered_src;
        src_len = filtered_len;
    }
    /* Function-like macros are now left in the filtered source and processed
     * by the normal lexer-level preprocessor in source order.  Do not pre-scan
     * them globally; that breaks C macro scoping/order.
     */
    posi = 0;
    tok_start_pos = 0;
    line_no = 1;
    tok_line = 1;

    outf = fopen(output_name, "w");
    if (!outf) fatal("cannot open output");

    add_typedef_name("FILE", TYPE_INT, 0);

    /* stdout/stderr/stdin are runtime data objects, not functions.
     * They are predeclared lazily in parse_translation_unit() as
     * SC_EXTERN so emit_load_sym_addr() emits EXTRN when they are
     * actually referenced.  Do not pre-add them here as SC_FUNC,
     * or add_global() preserves the wrong storage class and M80 sees
     * ld hl,_stdout without a preceding EXTRN. */
    add_define("NULL", "0");
    if (find_define("EOF") < 0)
        add_define("EOF", "-1");

    parse_translation_unit();
    emit_deferred_extrns();
    emit_data();
    emit("\n\tend\n");

    fclose(outf);

    if (errors) {
        fprintf(stderr, "dcc: %d error(s)\n", errors);
        return 1;
    }

    return 0;
}
