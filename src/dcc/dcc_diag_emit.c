/*
 * dcc_diag_emit.c - diagnostics, allocation, and low-level emit primitives.
 *
 * The compiler's "plumbing": fatal()/error_here() error reporting,
 * source_location_at() for #line-aware positions, xmalloc/xstrdup2, label
 * allocation, the emit()/emit_label()/emit_jp_label() assembly-output
 * primitives, and the raw source character readers (peekc/getc_src).
 *
 * MODULE: compiled as its own translation unit; shared declarations are in dcc.h.
 * Source provenance: monolith src/ddc.c lines 495-691.
 */

#include "dcc.h"
void fatal(const char *msg)
{
    fprintf(stderr, "dcc: fatal: %s\n", msg);
    exit(1);
}

void init_predefined_macro_texts(void)
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

void source_location_at(long ofs, char *filebuf, int filebufsz, int *linep)
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

void error_here(const char *msg)
{
    const char *fn;

    fn = tok.file[0] ? tok.file : (input_name ? input_name : "<input>");
    fprintf(stderr, "%s:%d: error: %s near '%s'\n",
            fn, tok_line, msg, tok.text);
    errors++;
    if (errors > 40) fatal("too many errors");
}

void *xmalloc(size_t n)
{
    void *p;
    p = malloc(n);
    if (!p) fatal("out of memory");
    return p;
}

char *xstrdup2(const char *s)
{
    char *p;
    p = (char *)xmalloc(strlen(s) + 1);
    strcpy(p, s);
    return p;
}

int new_label(void)
{
    return ++label_id;
}

void emit(const char *s);

void emit_ld_de_const(long v)
{
    if (!scan_mode)
        fprintf(outf, "\tld de,%ld\n", v & 0xffffL);
}

void emit_add_const_to_hl(long v)
{
    v &= 0xffffL;
    if (v == 0)
        return;
    emit_ld_de_const(v);
    emit("\tadd hl,de\n");
}

void emit(const char *s)
{
    if (!scan_mode)
        fputs(s, outf);
}

void emit_label(int n)
{
    if (!scan_mode)
        fprintf(outf, "L%d:\n", n);
}

void emit_jp_label(const char *op, int n)
{
    if (!scan_mode)
        fprintf(outf, "\t%s L%d\n", op, n);
}

int is_ident_start(int c)
{
    return isalpha((unsigned char)c) || c == '_';
}

int is_ident_char(int c)
{
    return isalnum((unsigned char)c) || c == '_';
}

int peekc(void)
{
    if (posi >= src_len) return 0;
    return (unsigned char)src[posi];
}

int getc_src(void)
{
    int c;
    if (posi >= src_len) return 0;
    c = (unsigned char)src[posi++];
    if (c == '\n') line_no++;
    return c;
}

int define_number_value(const char *name, long *out, int depth);
void strip_macro_replacement_comments(char *s);

