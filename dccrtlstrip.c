/*
 * dccrtlstrip.c - conservative pre-link reducer for dccrtl.mac
 *
 * Usage:
 *   dccrtlstrip [-k symbol ...] -r dccrtl.mac -o rtlmin.mac app.mac [app2.mac ...]
 *
 * This version treats PUBLIC directives as runtime block boundaries, not
 * arbitrary labels.  That is important for routines like _printf whose body
 * contains private labels/data such as __pf_run, pf_sink, etc.  Splitting on
 * every label can keep only the entry stub and strip the real body.
 */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES   60000
#define MAX_BLOCKS   4096
#define MAX_SYMS    20000
#define MAX_ROOTS   20000
#define MAX_LINE      512

struct Line {
    char *s;
};

struct Block {
    int start;
    int end;
    int keep;
    char name[128];
};

struct SymMap {
    char name[128];
    int block;
};

static struct Line lines[MAX_LINES];
static int nlines;

static struct Block blocks[MAX_BLOCKS];
static int nblocks;

static struct SymMap syms[MAX_SYMS];
static int nsyms;

static char roots[MAX_ROOTS][128];
static int nroots;

static char *xstrdup2(const char *s)
{
    char *p = (char *)malloc(strlen(s) + 1);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    strcpy(p, s);
    return p;
}

static void rtrim(char *s)
{
    int n = (int)strlen(s);
    while (n > 0 && (s[n-1] == '\n' || s[n-1] == '\r' ||
                     s[n-1] == ' ' || s[n-1] == '\t'))
        s[--n] = 0;
}

static const char *skipws(const char *p)
{
    while (*p == ' ' || *p == '\t') p++;
    return p;
}

static void strip_comment_copy(const char *in, char *out, int outsz)
{
    int i = 0;
    while (*in && *in != ';' && i < outsz - 1)
        out[i++] = *in++;
    out[i] = 0;
    rtrim(out);
}

static int is_ident_start(int c)
{
    return isalpha((unsigned char)c) || c == '_' || c == '?' || c == '.';
}

static int is_ident_char2(int c)
{
    return isalnum((unsigned char)c) || c == '_' || c == '?' || c == '.' || c == '$';
}

static int parse_ident_token(const char **pp, char *out)
{
    const char *p = skipws(*pp);
    int i = 0;

    if (!is_ident_start((unsigned char)*p))
        return 0;

    while (is_ident_char2((unsigned char)*p) && i < 127)
        out[i++] = *p++;
    out[i] = 0;
    *pp = p;
    return i > 0;
}

static int is_number_token(const char *s)
{
    if (*s == 0) return 0;
    if (*s == '-' || *s == '+') s++;
    if (*s == 0) return 0;
    if (*s == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        if (!isxdigit((unsigned char)*s)) return 0;
        while (isxdigit((unsigned char)*s)) s++;
    } else {
        if (!isdigit((unsigned char)*s)) return 0;
        while (isdigit((unsigned char)*s)) s++;
    }
    return *s == 0 || *s == 'h' || *s == 'H';
}

static int is_register_name(const char *s)
{
    static const char *regs[] = {
        "a","b","c","d","e","h","l","af","bc","de","hl","sp","ix","iy",
        "nz","z","nc","pe","po","p","m","c", NULL
    };
    int i;
    for (i = 0; regs[i]; ++i)
        if (!strcmp(s, regs[i]))
            return 1;
    return 0;
}

static int ci_strncmp(const char *a, const char *b, int n)
{
    int ca, cb;
    while (n-- > 0) {
        ca = tolower((unsigned char)*a++);
        cb = tolower((unsigned char)*b++);
        if (ca != cb) return ca - cb;
        if (ca == 0) return 0;
    }
    return 0;
}

static int starts_with_word(const char *s, const char *w)
{
    int n = (int)strlen(w);
    s = skipws(s);
    if (strncmp(s, w, n) != 0)
        return 0;
    return s[n] == 0 || s[n] == ' ' || s[n] == '\t';
}

static void add_root(const char *name)
{
    int i;
    if (!name || !name[0]) return;
    if (is_number_token(name) || is_register_name(name)) return;

    for (i = 0; i < nroots; ++i)
        if (!strcmp(roots[i], name))
            return;
    if (nroots >= MAX_ROOTS) {
        fprintf(stderr, "too many roots\n");
        exit(1);
    }
    strncpy(roots[nroots], name, sizeof(roots[nroots]) - 1);
    roots[nroots][sizeof(roots[nroots]) - 1] = 0;
    nroots++;
}

static void add_sym(const char *name, int block)
{
    int i;
    if (!name || !name[0]) return;
    for (i = 0; i < nsyms; ++i) {
        if (!strcmp(syms[i].name, name)) {
            /* Prefer the earliest block that actually owns the PUBLIC. */
            if (syms[i].block < 0)
                syms[i].block = block;
            return;
        }
    }
    if (nsyms >= MAX_SYMS) {
        fprintf(stderr, "too many symbols\n");
        exit(1);
    }
    strncpy(syms[nsyms].name, name, sizeof(syms[nsyms].name) - 1);
    syms[nsyms].name[sizeof(syms[nsyms].name) - 1] = 0;
    syms[nsyms].block = block;
    nsyms++;
}

static int parse_label(const char *line, char *lab);

static int str_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        if (tolower((unsigned char)*a) != tolower((unsigned char)*b))
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

static int line_public_mentions(const char *line, const char *name)
{
    char clean[MAX_LINE];
    const char *p;
    char sym[128];

    strip_comment_copy(line, clean, sizeof(clean));
    p = skipws(clean);
    if (ci_strncmp(p, "public", 6) != 0 ||
        !(p[6] == ' ' || p[6] == '\t'))
        return 0;
    p += 6;

    for (;;) {
        p = skipws(p);
        if (!parse_ident_token(&p, sym))
            break;
        if (str_ieq(sym, name))
            return 1;
        p = skipws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        break;
    }
    return 0;
}

static int line_label_is(const char *line, const char *name)
{
    char lab[128];
    return parse_label(line, lab) && str_ieq(lab, name);
}

static int find_symbol_block_by_scan(const char *name)
{
    int b, i;

    /* Last-resort block ownership check.  This is deliberately independent
     * of the symbol table built by build_blocks(), so an app EXTRN can still
     * keep a runtime block even if PUBLIC parsing missed that symbol or case
     * differs after M80/L80 folding. */
    for (b = 0; b < nblocks; ++b) {
        for (i = blocks[b].start; i < blocks[b].end; ++i) {
            if (line_public_mentions(lines[i].s, name) ||
                line_label_is(lines[i].s, name))
                return b;
        }
    }

    return -1;
}

static int find_sym_block(const char *name)
{
    int i;
    for (i = 0; i < nsyms; ++i)
        if (!strcmp(syms[i].name, name) || str_ieq(syms[i].name, name))
            return syms[i].block;
    return find_symbol_block_by_scan(name);
}

static int parse_label(const char *line, char *lab)
{
    const char *p;
    int i;

    p = skipws(line);
    if (!is_ident_start((unsigned char)*p))
        return 0;

    i = 0;
    while (is_ident_char2((unsigned char)*p) && i < 127)
        lab[i++] = *p++;
    lab[i] = 0;

    p = skipws(p);
    return *p == ':';
}

static void collect_publics_from_line(const char *line, int block)
{
    char clean[MAX_LINE];
    const char *p;
    char sym[128];

    strip_comment_copy(line, clean, sizeof(clean));
    p = skipws(clean);
    if (ci_strncmp(p, "public", 6) != 0 ||
        !(p[6] == ' ' || p[6] == '\t'))
        return;
    p += 6;

    for (;;) {
        p = skipws(p);
        if (!parse_ident_token(&p, sym))
            break;
        add_sym(sym, block);
        p = skipws(p);
        if (*p == ',') {
            p++;
            continue;
        }
        break;
    }
}

static int is_public_line(const char *line)
{
    char clean[MAX_LINE];
    const char *p;
    strip_comment_copy(line, clean, sizeof(clean));
    p = skipws(clean);
    return ci_strncmp(p, "public", 6) == 0 && (p[6] == ' ' || p[6] == '\t');
}

static void read_runtime(const char *fn)
{
    FILE *f;
    char buf[MAX_LINE];

    f = fopen(fn, "r");
    if (!f) {
        perror(fn);
        exit(1);
    }

    while (fgets(buf, sizeof(buf), f)) {
        if (nlines >= MAX_LINES) {
            fprintf(stderr, "too many runtime lines\n");
            exit(1);
        }
        rtrim(buf);
        lines[nlines++].s = xstrdup2(buf);
    }
    fclose(f);
}

/* Build blocks using PUBLIC directives as boundaries.  Lines before the
 * first PUBLIC are kept as a preamble.  Each PUBLIC block includes its PUBLIC
 * directive and all following private labels/data until the next PUBLIC.
 */
static int prev_nonblank_is_public(int i)
{
    int j;
    for (j = i - 1; j >= 0; --j) {
        const char *p = skipws(lines[j].s);
        if (*p == 0 || *p == ';')
            continue;
        return is_public_line(lines[j].s);
    }
    return 0;
}

static void start_new_block_at(int i, int *curp)
{
    int cur;

    if (*curp >= 0)
        blocks[*curp].end = i;

    if (nblocks >= MAX_BLOCKS) {
        fprintf(stderr, "too many blocks\n");
        exit(1);
    }

    cur = nblocks++;
    memset(&blocks[cur], 0, sizeof(blocks[cur]));
    blocks[cur].start = i;
    blocks[cur].end = nlines;
    sprintf(blocks[cur].name, "block%d", cur);
    *curp = cur;
}

static void build_blocks(void)
{
    int i;
    int cur;
    char lab[128];

    nblocks = 0;

    cur = -1;
    for (i = 0; i < nlines; ++i) {
        if (is_public_line(lines[i].s)) {
            /* Consecutive PUBLIC directives usually introduce one shared
             * implementation cluster, e.g.:
             *
             *     public _open
             *     public _read
             *     public _write
             *     FIRSTFD equ 3
             *     _open:
             *        ...
             *     _read:
             *        ...
             *
             * Splitting at each PUBLIC makes orphan public declarations and
             * loses shared EQU constants.  Start a new block only for the
             * first PUBLIC in such a run. */
            if (cur < 0 || !prev_nonblank_is_public(i))
                start_new_block_at(i, &cur);
        }

        if (cur >= 0) {
            collect_publics_from_line(lines[i].s, cur);

            if (parse_label(lines[i].s, lab)) {
                /* Private/internal labels within a kept public block should
                 * also resolve dependencies to that same block.  This keeps
                 * helpers like __pf_run when _printf calls them, and prevents
                 * splitting at data labels such as pf_sink.
                 */
                add_sym(lab, cur);
            }
        }
    }

    if (cur >= 0)
        blocks[cur].end = nlines;
}

static void add_refs_from_line(const char *line)
{
    char clean[MAX_LINE];
    char op[64], sym[128];
    const char *p;
    int i;

    strip_comment_copy(line, clean, sizeof(clean));
    p = skipws(clean);
    if (*p == 0)
        return;

    /* Skip labels at start. */
    if (parse_label(p, sym)) {
        char *colon = strchr(clean, ':');
        if (!colon) return;
        p = skipws(colon + 1);
        if (*p == 0)
            return;
    }

    if (!parse_ident_token(&p, op))
        return;

    for (i = 0; op[i]; ++i)
        op[i] = (char)tolower((unsigned char)op[i]);

    if (!strcmp(op, "extrn") || !strcmp(op, "public")) {
        for (;;) {
            p = skipws(p);
            if (!parse_ident_token(&p, sym))
                break;
            if (!strcmp(op, "extrn"))
                add_root(sym);
            p = skipws(p);
            if (*p == ',') {
                p++;
                continue;
            }
            break;
        }
        return;
    }

    if (!strcmp(op, "call") || !strcmp(op, "jp") || !strcmp(op, "jr")) {
        /* jp z,label has condition first. */
        p = skipws(p);
        if (parse_ident_token(&p, sym)) {
            p = skipws(p);
            if (*p == ',') {
                p++;
                if (parse_ident_token(&p, sym))
                    add_root(sym);
            } else {
                add_root(sym);
            }
        }
        return;
    }

    if (!strcmp(op, "dw")) {
        for (;;) {
            p = skipws(p);
            if (parse_ident_token(&p, sym))
                add_root(sym);
            else {
                while (*p && *p != ',') p++;
            }
            p = skipws(p);
            if (*p == ',') {
                p++;
                continue;
            }
            break;
        }
        return;
    }

    if (!strcmp(op, "ld")) {
        /* catch ld hl,sym / ld de,sym / ld (addr),hl-ish symbol uses */
        const char *q = strchr(p, ',');
        if (q) {
            q++;
            q = skipws(q);
            if (parse_ident_token(&q, sym))
                add_root(sym);
        }
        /* Also catch ld (sym),hl */
        p = skipws(p);
        if (*p == '(') {
            p++;
            if (parse_ident_token(&p, sym))
                add_root(sym);
        }
        return;
    }
}


static int symbol_mentioned_in_line(const char *line, const char *sym)
{
    char clean[MAX_LINE];
    const char *p;
    int n;

    strip_comment_copy(line, clean, sizeof(clean));
    n = (int)strlen(sym);
    if (n <= 0)
        return 0;

    p = clean;
    while ((p = strstr(p, sym)) != NULL) {
        int before_ok;
        int after_ok;
        before_ok = (p == clean) || !is_ident_char2((unsigned char)p[-1]);
        after_ok = !is_ident_char2((unsigned char)p[n]);
        if (before_ok && after_ok)
            return 1;
        p++;
    }
    return 0;
}

static void add_known_runtime_refs_from_line(const char *line)
{
    int i;

    /*
     * Fallback root scan: after the runtime has been parsed, every PUBLIC and
     * internal label is known.  Some compiler output uses symbols in forms not
     * covered by the small opcode parser above, or may contain extra spacing /
     * macro-like constructs.  If an app line mentions an exact runtime symbol,
     * keep that symbol's owning block.  This is intentionally conservative and
     * fixes cases such as the float-printf entry _pffio being emitted by dcc but
     * omitted from rtlmin.mac.
     */
    for (i = 0; i < nsyms; ++i) {
        if (symbol_mentioned_in_line(line, syms[i].name))
            add_root(syms[i].name);
    }
}

static void scan_app(const char *fn)
{
    FILE *f;
    char buf[MAX_LINE];

    f = fopen(fn, "r");
    if (!f) {
        perror(fn);
        exit(1);
    }
    while (fgets(buf, sizeof(buf), f)) {
        rtrim(buf);
        add_refs_from_line(buf);
        add_known_runtime_refs_from_line(buf);
    }
    fclose(f);
}

static int keep_block_for_symbol(const char *name)
{
    int b = find_sym_block(name);
    if (b >= 0 && !blocks[b].keep) {
        blocks[b].keep = 1;
        return 1;
    }
    return 0;
}

static void mark_reachable(void)
{
    int changed;
    int i, b, pass_start, pass_end;

    add_root("start");

    do {
        changed = 0;

        for (i = 0; i < nroots; ++i)
            if (keep_block_for_symbol(roots[i]))
                changed = 1;

        for (b = 0; b < nblocks; ++b) {
            if (!blocks[b].keep)
                continue;

            pass_start = blocks[b].start;
            pass_end = blocks[b].end;
            for (i = pass_start; i < pass_end; ++i) {
                int before = nroots;
                add_refs_from_line(lines[i].s);
                if (nroots != before)
                    changed = 1;
            }
        }
    } while (changed);
}

static void write_output(const char *fn)
{
    FILE *f;
    int i, b;
    int first_block_start;

    f = fopen(fn, "w");
    if (!f) {
        perror(fn);
        exit(1);
    }

    fprintf(f, "; generated by dccrtlstrip - conservative reduced runtime\n");

    first_block_start = nlines;
    for (b = 0; b < nblocks; ++b) {
        if (blocks[b].start < first_block_start)
            first_block_start = blocks[b].start;
    }

    /* Preamble before first PUBLIC: org/equ/comments/etc. */
    for (i = 0; i < first_block_start; ++i)
        fprintf(f, "%s\n", lines[i].s);

    for (b = 0; b < nblocks; ++b) {
        if (!blocks[b].keep)
            continue;
        for (i = blocks[b].start; i < blocks[b].end; ++i)
            fprintf(f, "%s\n", lines[i].s);
    }

    /* If the original file had a standalone END outside any kept block,
     * preserve it.  If not, M80 can still assemble but warns; better to add
     * END START for CP/M runtime modules.
     */
    fprintf(f, "\n\tend\tstart\n");
    fclose(f);
}

static void usage(void)
{
    fprintf(stderr, "usage: dccrtlstrip [-k symbol ...] -r dccrtl.mac -o rtlmin.mac app.mac [app2.mac ...]\n");
    exit(1);
}

int main(int argc, char **argv)
{
    const char *rt = NULL;
    const char *out = NULL;
    int i;
    int saw_app = 0;

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-r") && i + 1 < argc) {
            rt = argv[++i];
        } else if (!strcmp(argv[i], "-o") && i + 1 < argc) {
            out = argv[++i];
        } else if (!strcmp(argv[i], "-k") && i + 1 < argc) {
            /* explicit root; added after runtime symbol table is built */
            i++;
        } else if (!strcmp(argv[i], "-root") && i + 1 < argc) {
            /* alias for -k */
            i++;
        } else if (argv[i][0] == '-') {
            usage();
        } else {
            /* apps scanned after runtime symbols are known below */
        }
    }

    if (!rt || !out)
        usage();

    read_runtime(rt);
    build_blocks();

    for (i = 1; i < argc; ++i) {
        if (!strcmp(argv[i], "-r") || !strcmp(argv[i], "-o")) {
            i++;
            continue;
        }
        if ((!strcmp(argv[i], "-k") || !strcmp(argv[i], "-root")) && i + 1 < argc) {
            add_root(argv[++i]);
            continue;
        }
        if (argv[i][0] == '-')
            continue;
        scan_app(argv[i]);
        saw_app = 1;
    }

    if (!saw_app)
        usage();

    mark_reachable();
    write_output(out);
    return 0;
}
