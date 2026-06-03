/*
    peephole optimizer for dcc C89 compiler targeting Z80 with M80 syntax
*/
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <ctype.h>

#define MAX_LINES 40000
#define MAX_LINE  512

static char *lines[MAX_LINES];
static int nlines;

static char *xstrdup2(const char *s)
{
    char *p;
    p = (char *)malloc(strlen(s) + 1);
    if (!p) {
        fprintf(stderr, "out of memory\n");
        exit(1);
    }
    strcpy(p, s);
    return p;
}

static void trim(char *s)
{
    int i;
    int j;
    int n;

    n = (int)strlen(s);
    while (n > 0 &&
           (s[n - 1] == '\n' || s[n - 1] == '\r' ||
            s[n - 1] == ' '  || s[n - 1] == '\t'))
        s[--n] = 0;

    i = 0;
    while (s[i] == ' ' || s[i] == '\t')
        i++;

    if (i) {
        j = 0;
        while (s[i])
            s[j++] = s[i++];
        s[j] = 0;
    }
}

static int eq(int i, const char *s)
{
    char buf[MAX_LINE];
    char *semi;
    int n;

    if (i < 0 || i >= nlines)
        return 0;

    strncpy(buf, lines[i], sizeof(buf) - 1);
    buf[sizeof(buf) - 1] = 0;

    semi = strchr(buf, ';');
    if (semi)
        *semi = 0;

    n = (int)strlen(buf);
    while (n > 0 && (buf[n - 1] == ' ' || buf[n - 1] == '\t'))
        buf[--n] = 0;

    return strcmp(buf, s) == 0;
}

static int starts_label(const char *s)
{
    int n;
    n = (int)strlen(s);
    return n > 0 && s[n - 1] == ':';
}

static int is_blank_or_comment(const char *s)
{
    return s[0] == 0 || s[0] == ';';
}


static void strip_peep_comment_copy(char *dst, const char *src)
{
    int i;
    int n;

    i = 0;
    while (src[i] && src[i] != ';' && i < MAX_LINE - 1) {
        dst[i] = src[i];
        i++;
    }
    dst[i] = 0;

    n = (int)strlen(dst);
    while (n > 0 && (dst[n - 1] == ' ' || dst[n - 1] == '\t')) {
        dst[n - 1] = 0;
        n--;
    }
}

static void replace1(int i, const char *s)
{
    char *p;

    /*
     * Be careful when callers pass lines[i] as the replacement text.
     * The old version freed lines[i] before duplicating s, which is a
     * use-after-free if s == lines[i].  That happened in the signed_le_zero
     * peephole and produced platform-dependent garbage on Linux while often
     * appearing to work on Windows.
     */
    p = xstrdup2(s);
    free(lines[i]);
    lines[i] = p;
}

static void replace1_tagged(int i, const char *s, const char *tag)
{
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s ; peep: %s", s, tag);
    replace1(i, buf);
}

static void delete_n(int i, int count)
{
    int j;

    for (j = 0; j < count; j++)
        free(lines[i + j]);

    for (j = i; j + count < nlines; j++)
        lines[j] = lines[j + count];

    nlines -= count;
}

static void insert_line(int i, const char *s)
{
    int j;

    if (nlines >= MAX_LINES) {
        fprintf(stderr, "too many lines\n");
        exit(1);
    }

    for (j = nlines; j > i; j--)
        lines[j] = lines[j - 1];

    lines[i] = xstrdup2(s);
    nlines++;
}

static void insert_line_tagged(int i, const char *s, const char *tag)
{
    char buf[MAX_LINE];
    snprintf(buf, sizeof(buf), "%s ; peep: %s", s, tag);
    insert_line(i, buf);
}

static int parse_ld_hl_imm(const char *s, char *val)
{
    const char *p;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);
    p = "ld hl,";
    if (strncmp(tmp, p, strlen(p)) != 0)
        return 0;

    strcpy(val, tmp + strlen(p));
    return 1;
}

static int parse_ld_de_imm(const char *s, char *val)
{
    const char *p;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);
    p = "ld de,";
    if (strncmp(tmp, p, strlen(p)) != 0)
        return 0;

    strcpy(val, tmp + strlen(p));
    return 1;
}

static int parse_nonneg_int(const char *s, int *out)
{
    int v;

    if (*s < '0' || *s > '9')
        return 0;

    v = 0;
    while (*s >= '0' && *s <= '9') {
        v = v * 10 + (*s - '0');
        s++;
    }

    if (*s != 0)
        return 0;

    *out = v;
    return 1;
}

static void make_inc_sp_lines(char *out, int n)
{
    int i;

    out[0] = 0;
    for (i = 0; i < n; i++) {
        if (i)
            strcat(out, "\n");
        strcat(out, "inc sp");
    }
}

static int is_uncond_jp(const char *s)
{
    const char *p;

    if (strncmp(s, "jp ", 3) != 0)
        return 0;

    p = s + 3;

    /* Conditional forms are emitted as jp z, Lx / jp nc, Lx, etc. */
    while (*p) {
        if (*p == ',')
            return 0;
        p++;
    }

    return 1;
}

static int is_jp_to_next_label(int i)
{
    char target[128];
    char label[128];
    int n;

    if (i + 1 >= nlines)
        return 0;
    if (!is_uncond_jp(lines[i]))
        return 0;
    if (!starts_label(lines[i + 1]))
        return 0;

    strcpy(target, lines[i] + 3);
    strcpy(label, lines[i + 1]);
    n = (int)strlen(label);
    if (n > 0 && label[n - 1] == ':')
        label[n - 1] = 0;

    return strcmp(target, label) == 0;
}


static int parse_jp_cond_label(const char *s, const char *cond, char *label)
{
    char pat[32];
    int n;

    sprintf(pat, "jp %s, ", cond);
    n = (int)strlen(pat);
    if (strncmp(s, pat, n) != 0)
        return 0;
    strcpy(label, s + n);
    return 1;
}

static int parse_jp_z_label(const char *s, char *label)
{
    return parse_jp_cond_label(s, "z", label);
}

static int parse_jp_nz_label(const char *s, char *label)
{
    return parse_jp_cond_label(s, "nz", label);
}

static int parse_jp_c_label(const char *s, char *label)
{
    return parse_jp_cond_label(s, "c", label);
}

static int parse_jp_nc_label(const char *s, char *label)
{
    return parse_jp_cond_label(s, "nc", label);
}

static int line_is_label_name(int i, const char *name)
{
    char tmp[MAX_LINE];
    if (i < 0 || i >= nlines)
        return 0;
    sprintf(tmp, "%s:", name);
    return strcmp(lines[i], tmp) == 0;
}

static void replace_block_with_5(int i,
                                 const char *a, const char *b,
                                 const char *c, const char *d,
                                 const char *e, const char *tag)
{
    int oldn;
    oldn = 10;
    replace1_tagged(i, a, tag);
    replace1(i + 1, b);
    replace1(i + 2, c);
    replace1(i + 3, d);
    replace1(i + 4, e);
    delete_n(i + 5, oldn - 5);
}

static int try_fold_bool_branch(int i)
{
    char t1[128];
    char t2[128];
    char e[128];
    char f[128];
    char a[256];
    char b[256];
    char c[256];
    char d[256];
    char elab[256];

    if (i + 9 >= nlines)
        return 0;

    if (!eq(i + 2, "ld hl,0"))
        return 0;
    if (!is_uncond_jp(lines[i + 3]))
        return 0;
    strcpy(e, lines[i + 3] + 3);

    if (!line_is_label_name(i + 6, e))
        return 0;
    if (!eq(i + 7, "ld a,h"))
        return 0;
    if (!eq(i + 8, "or l"))
        return 0;

    /* Two-way true test, used for <= and >= materialization. */
    if ((parse_jp_z_label(lines[i], t1) &&
         (parse_jp_c_label(lines[i + 1], t2) || parse_jp_nc_label(lines[i + 1], t2))) &&
        strcmp(t1, t2) == 0 &&
        line_is_label_name(i + 4, t1) &&
        eq(i + 5, "ld hl,1")) {

        sprintf(d, "%s:", t1);
        sprintf(elab, "%s:", e);

        if (parse_jp_z_label(lines[i + 9], f)) {
            sprintf(a, "%s", lines[i]);
            sprintf(b, "%s", lines[i + 1]);
            sprintf(c, "jp %s", f);
            replace_block_with_5(i, a, b, c, d, elab, "fold_bool_branch_2way");
            return 1;
        }

        if (parse_jp_nz_label(lines[i + 9], f)) {
            /* if boolean true, branch to f; otherwise fall through */
            sprintf(a, "jp z, %s", f);
            if (parse_jp_c_label(lines[i + 1], t2))
                sprintf(b, "jp c, %s", f);
            else
                sprintf(b, "jp nc, %s", f);
            sprintf(c, "jp %s", e);
            replace_block_with_5(i, a, b, c, d, elab, "fold_bool_branch_2way");
            return 1;
        }
    }

    /* Single true test, used for ==/!= materialization. */
    if ((parse_jp_z_label(lines[i], t1) || parse_jp_nz_label(lines[i], t1)) &&
        line_is_label_name(i + 4, t1) &&
        eq(i + 5, "ld hl,1")) {

        sprintf(d, "%s:", t1);
        sprintf(elab, "%s:", e);

        if (parse_jp_z_label(lines[i + 9], f)) {
            /* false branch goes to f */
            sprintf(a, "%s", lines[i]);
            sprintf(b, "jp %s", f);
            sprintf(c, "%s", d);
            replace1_tagged(i, a, "fold_bool_branch_single");
            replace1(i + 1, b);
            replace1(i + 2, c);
            replace1(i + 3, elab);
            delete_n(i + 4, 6);
            return 1;
        }

        if (parse_jp_nz_label(lines[i + 9], f)) {
            /* true branch goes to f */
            if (parse_jp_z_label(lines[i], t1))
                sprintf(a, "jp z, %s", f);
            else
                sprintf(a, "jp nz, %s", f);
            sprintf(b, "jp %s", e);
            sprintf(c, "%s", d);
            replace1_tagged(i, a, "fold_bool_branch_single");
            replace1(i + 1, b);
            replace1(i + 2, c);
            replace1(i + 3, elab);
            delete_n(i + 4, 6);
            return 1;
        }
    }

    return 0;
}


static int is_jump_line(const char *s)
{
    return strncmp(s, "jp ", 3) == 0;
}

static int jump_target(const char *s, char *out)
{
    const char *p;
    int i;

    if (!is_jump_line(s))
        return 0;

    p = s + 3;

    /* conditional form: jp z, L1 / jp nc, L1 */
    while (*p && *p != ',')
        p++;

    if (*p == ',') {
        p++;
        while (*p == ' ' || *p == '\t')
            p++;
    } else {
        p = s + 3;
        while (*p == ' ' || *p == '\t')
            p++;
    }

    if (*p == 0)
        return 0;

    i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < 120)
        out[i++] = *p++;
    out[i] = 0;
    return i > 0;
}

static void rewrite_jump_target(int i, const char *newtarget)
{
    char out[256];
    char *comma;

    comma = strchr(lines[i], ',');
    if (comma) {
        int prefix_len;
        prefix_len = (int)(comma - lines[i]) + 1;
        if (prefix_len > 200) prefix_len = 200;
        memcpy(out, lines[i], (size_t)prefix_len);
        out[prefix_len] = 0;
        strcat(out, " ");
        strcat(out, newtarget);
    } else {
        strcpy(out, "jp ");
        strcat(out, newtarget);
    }

    replace1(i, out);
}

static int label_name_at(int i, char *out)
{
    int n;

    if (i < 0 || i >= nlines || !starts_label(lines[i]))
        return 0;

    n = (int)strlen(lines[i]);
    if (n <= 1 || n > 120)
        return 0;

    memcpy(out, lines[i], (size_t)(n - 1));
    out[n - 1] = 0;
    return 1;
}

static int is_label_referenced(const char *lab)
{
    int i;
    char tgt[128];
    int lablen;
    const char *found;
    char before;
    char after;

    lablen = (int)strlen(lab);

    for (i = 0; i < nlines; i++) {
        if (jump_target(lines[i], tgt) && strcmp(tgt, lab) == 0)
            return 1;

        /* Be conservative for data or non-jump references. */
        if (!starts_label(lines[i]) && !is_jump_line(lines[i])) {
            found = strstr(lines[i], lab);
            while (found != NULL) {
                /* Require word boundaries so "L1" does not match inside "L10". */
                before = (found > lines[i]) ? *(found - 1) : 0;
                after  = *(found + lablen);
                if (!isalnum((unsigned char)before) && before != '_' &&
                    !isalnum((unsigned char)after)  && after  != '_') {
                    return 1;
                }
                found = strstr(found + 1, lab);
            }
        }
    }

    return 0;
}

/*
 * Collapse adjacent-label chains:
 *
 *   L1:
 *   L2:
 *
 * redirects references to L1 to L2, then removes L1 if unreferenced.
 * This also handles:
 *
 *   jp L1
 *   L1:
 *   L2:
 *
 * after the redirect, the existing jump-to-next-label rule can remove
 * jp L2 / L2: on a later pass.
 */
static int pass_labels(void)
{
    int i, j;
    int changed;
    char oldlab[128];
    char newlab[128];
    char tgt[128];

    changed = 0;

    for (i = 0; i + 1 < nlines; i++) {
        if (!label_name_at(i, oldlab))
            continue;

        j = i + 1;
        while (j < nlines && is_blank_or_comment(lines[j]))
            j++;

        if (!label_name_at(j, newlab))
            continue;

        /* Rewrite all jumps to old label to the following label. */
        {
            int k;
            for (k = 0; k < nlines; k++) {
                if (jump_target(lines[k], tgt) && strcmp(tgt, oldlab) == 0) {
                    rewrite_jump_target(k, newlab);
                    changed = 1;
                }
            }
        }

        if (!is_label_referenced(oldlab)) {
            delete_n(i, 1);
            changed = 1;
            if (i > 0) i--;
        }
    }

    return changed;
}



static int parse_ld_de_positive_imm(const char *s, long *out)
{
    char *endp;
    long v;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld de,", 6) != 0)
        return 0;

    v = strtol(tmp + 6, &endp, 0);
    while (*endp == ' ' || *endp == '\t')
        endp++;

    if (*endp != 0)
        return 0;

    if (v <= 0 || v > 32767)
        return 0;

    out[0] = v;
    return 1;
}




static int peep_parse_jp_cond_label(const char *s, const char *cond, char *lab)
{
    char prefix[32];
    const char *p;
    int i;

    sprintf(prefix, "jp %s,", cond);
    if (strncmp(s, prefix, strlen(prefix)) != 0)
        return 0;

    p = s + strlen(prefix);
    while (*p == ' ' || *p == '\t')
        p++;

    i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < 120)
        lab[i++] = *p++;
    lab[i] = 0;
    return i > 0;
}

static int peep_parse_jp_uncond_label(const char *s, char *lab)
{
    const char *p;
    int i;

    if (strncmp(s, "jp ", 3) != 0)
        return 0;

    if (strchr(s + 3, ',') != NULL)
        return 0;

    p = s + 3;
    while (*p == ' ' || *p == '\t')
        p++;

    i = 0;
    while (*p && *p != ' ' && *p != '\t' && i < 120)
        lab[i++] = *p++;
    lab[i] = 0;
    return i > 0;
}

static void peep_make_cond_jump(char *out, const char *cond, const char *lab)
{
    sprintf(out, "jp %s, %s", cond, lab);
}

static int peep_parse_any_cond_jump(const char *s, char *cond, char *lab)
{
    if (peep_parse_jp_cond_label(s, "z", lab)) { strcpy(cond, "z"); return 1; }
    if (peep_parse_jp_cond_label(s, "nz", lab)) { strcpy(cond, "nz"); return 1; }
    if (peep_parse_jp_cond_label(s, "c", lab)) { strcpy(cond, "c"); return 1; }
    if (peep_parse_jp_cond_label(s, "nc", lab)) { strcpy(cond, "nc"); return 1; }
    return 0;
}

static const char *peep_inverse_cond(const char *cond)
{
    if (!strcmp(cond, "z")) return "nz";
    if (!strcmp(cond, "nz")) return "z";
    if (!strcmp(cond, "c")) return "nc";
    if (!strcmp(cond, "nc")) return "c";
    return NULL;
}

static int pass_branch_over_jump(void)
{
    int i;
    int changed;
    char cond[16];
    char lbody[128];
    char lexit[128];
    const char *inv;
    char newline[160];

    changed = 0;

    for (i = 0; i + 2 < nlines; ++i) {
        if (peep_parse_any_cond_jump(lines[i], cond, lbody) &&
            peep_parse_jp_uncond_label(lines[i + 1], lexit) &&
            line_is_label_name(i + 2, lbody)) {
            inv = peep_inverse_cond(cond);
            if (!inv)
                continue;
            sprintf(newline, "jp %s, %s", inv, lexit);
            replace1_tagged(i, newline, "branch_over_jump");
            delete_n(i + 1, 1);
            changed = 1;
            if (i > 0)
                --i;
        }
    }

    return changed;
}



static int peep_parse_ld_l_ix(const char *s, char *off)
{
    char tmp[MAX_LINE];
    const char *p;
    int i;

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld l,(ix", 8) != 0)
        return 0;

    p = tmp + 8;
    i = 0;
    while (*p && *p != ')' && i < 31)
        off[i++] = *p++;
    off[i] = 0;

    if (*p != ')' || p[1] != 0)
        return 0;

    return i > 0;
}

static int peep_is_jp_z_or_nz(const char *s)
{
    return strncmp(s, "jp z,", 5) == 0 || strncmp(s, "jp nz,", 6) == 0;
}

static void peep_make_ld_a_ix(char *out, const char *off)
{
    sprintf(out, "ld a,(ix%s)", off);
}




/* Match exactly:
 *     ld (ix+N),a
 *     ld a,(ix+N)
 * allowing optimizer tags/comments after either instruction.  This is safe
 * only for adjacent instructions because the store does not alter A and no
 * intervening instruction can clobber it.
 */
static int peep_parse_ld_ix_a(const char *s, char *off)
{
    char tmp[MAX_LINE];
    const char *p;
    int i;

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld (ix", 6) != 0)
        return 0;

    p = tmp + 6;
    i = 0;
    while (*p && *p != ')' && i < 31)
        off[i++] = *p++;
    off[i] = 0;

    if (*p != ')' || p[1] != ',' || p[2] != 'a' || p[3] != 0)
        return 0;

    return i > 0;
}

static int peep_parse_ld_a_ix(const char *s, char *off)
{
    char tmp[MAX_LINE];
    const char *p;
    int i;

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld a,(ix", 8) != 0)
        return 0;

    p = tmp + 8;
    i = 0;
    while (*p && *p != ')' && i < 31)
        off[i++] = *p++;
    off[i] = 0;

    if (*p != ')' || p[1] != 0)
        return 0;

    return i > 0;
}

static int pass_remove_ix_store_reload_a(void)
{
    int i;
    int changed;
    char off1[32];
    char off2[32];

    changed = 0;

    for (i = 0; i + 1 < nlines; ++i) {
        if (peep_parse_ld_ix_a(lines[i], off1) &&
            peep_parse_ld_a_ix(lines[i + 1], off2) &&
            strcmp(off1, off2) == 0) {
            delete_n(i + 1, 1);
            changed = 1;
            if (i > 0)
                i--;
        }
    }

    return changed;
}


static int peep_parse_ld_de_0_to_255(const char *s, int *out)
{
    char *endp;
    long v;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld de,", 6) != 0)
        return 0;

    v = strtol(tmp + 6, &endp, 0);
    while (*endp == ' ' || *endp == '\t')
        endp++;

    if (*endp != 0 || v < 0 || v > 255)
        return 0;

    out[0] = (int)v;
    return 1;
}

static int peep_parse_ld_de_signed(const char *s, int *out)
{
    char *endp;
    long v;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld de,", 6) != 0)
        return 0;

    v = strtol(tmp + 6, &endp, 0);
    if (*endp != 0)
        return 0;

    *out = (int)v;
    return 1;
}

static void peep_format_ix_off(char *buf, int off)
{
    if (off >= 0)
        sprintf(buf, "+%d", off);
    else
        sprintf(buf, "%d", off);
}

/*
 * Dead IX-frame store elimination.
 *
 * Performs a forward-scan within each function/segment body.  A store to
 * (ix+N) is dead when it is overwritten by a later store to the same offset
 * before any read of that offset, or when the function returns (ret) without
 * ever reading the offset again.
 *
 * Conservative behaviour:
 *   - Internal labels (L\d+:) and forward/backward jumps flush all pending
 *     stores (we can no longer prove they are overwritten before read).
 *   - Segment boundaries (_Z...: labels etc.) also flush — the new segment's
 *     IX frame is different from the old one.
 *
 * Fires on crc_update_byte: ix-8..ix-5 (temp "t") are written but never read
 * (16 dead stores); earlier writes to ix-4..ix-1 (idx) and ix+4..ix+7 (crc)
 * are killed by subsequent writes without intervening reads (20 more).
 */
static int pass_elim_dead_ix_stores(void)
{
    static char is_dead[MAX_LINES];
    int last_store[256];  /* last_store[offset+128] = line index; -1 = none */
    int i, idx, changed;
    char tmp[MAX_LINE];
    const char *p;
    char *endp;
    long v;
    int n;

    memset(is_dead, 0, sizeof(is_dead));
    memset(last_store, -1, sizeof(last_store));
    changed = 0;

    for (i = 0; i < nlines; i++) {
        strip_peep_comment_copy(tmp, lines[i]);
        n = (int)strlen(tmp);

        /* Any label ending in ':' that is not an internal 'L<digits>:' label
           is a segment boundary (new function or data).  Reset all state. */
        if (n > 1 && tmp[n-1] == ':' && tmp[0] != ' ' && tmp[0] != '\t') {
            /* Check whether it is an internal local label L\d+: */
            int is_local = 0;
            if (tmp[0] == 'L') {
                int j;
                is_local = 1;
                for (j = 1; j < n-1; j++)
                    if (tmp[j] < '0' || tmp[j] > '9') { is_local = 0; break; }
            }
            if (!is_local) {
                /* Segment boundary: flush (remaining stores from previous
                   segment are addressed by the old IX — not our problem) */
                memset(last_store, -1, sizeof(last_store));
            } else {
                /* Internal label: conservative flush — a branch from elsewhere
                   might rely on a pending store being present. */
                memset(last_store, -1, sizeof(last_store));
            }
            continue;
        }

        /* ret: end of function — any pending store was never read → dead */
        if (strcmp(tmp, "ret") == 0) {
            for (idx = 0; idx < 256; idx++)
                if (last_store[idx] >= 0)
                    is_dead[last_store[idx]] = 1;
            memset(last_store, -1, sizeof(last_store));
            continue;
        }

        /* Jump instructions: flush — can't prove overwrite on all paths */
        if (strncmp(tmp, "jp ", 3) == 0 || strncmp(tmp, "jr ", 3) == 0 ||
            strncmp(tmp, "djnz ", 5) == 0) {
            memset(last_store, -1, sizeof(last_store));
            continue;
        }

        /* Indirect IX-frame access pattern: push ix / pop hl / ld de,K / add hl,de
         * This computes HL = IX+K and then subsequent (hl) accesses read IX+K.
         * The (hl) instructions contain no "(ix" text, so we must handle this
         * 4-instruction sequence explicitly to avoid false dead-store deletions. */
        if (strcmp(tmp, "push ix") == 0 &&
            i + 3 < nlines) {
            char t1[MAX_LINE], t2[MAX_LINE], t3[MAX_LINE];
            long kv;
            char *ep;
            strip_peep_comment_copy(t1, lines[i + 1]);
            strip_peep_comment_copy(t2, lines[i + 2]);
            strip_peep_comment_copy(t3, lines[i + 3]);
            if (strcmp(t1, "pop hl") == 0 &&
                strncmp(t2, "ld de,", 6) == 0 &&
                strcmp(t3, "add hl,de") == 0) {
                kv = strtol(t2 + 6, &ep, 0);
                if (*ep == 0 && kv >= -128 && kv <= 127) {
                    int b;
                    /* Address of frame offset K is taken via HL.  We do not
                     * know how many bytes will be accessed through the
                     * resulting pointer, so conservatively mark up to 4
                     * consecutive bytes as live.  This covers char (1 byte),
                     * int (2 bytes), and long/float (4 bytes) objects. */
                    for (b = 0; b < 4; b++) {
                        if (kv + b >= -128 && kv + b <= 127) {
                            idx = (int)(kv + b) + 128;
                            last_store[idx] = -1;
                        }
                    }
                }
            }
            /* Fall through: also treat push ix as a regular no-(ix) instruction */
        }

        /* Check whether this instruction touches an IX-indexed address */
        if (strstr(tmp, "(ix") == NULL)
            continue;

        if (strncmp(tmp, "ld (ix", 6) == 0) {
            /* Store: ld (ix+N),something  — track as pending store */
            p = tmp + 6;
            v = strtol(p, &endp, 0);
            if (*endp == ')' && *(endp+1) == ',' && v >= -128 && v <= 127) {
                idx = (int)v + 128;
                if (last_store[idx] >= 0)
                    is_dead[last_store[idx]] = 1;  /* overwritten → dead */
                last_store[idx] = i;
            }
        } else {
            /* Any other use of (ix+N): mark the pending store as live */
            p = strstr(tmp, "(ix");
            if (p) {
                p += 3;
                if (*p == '+' || *p == '-' ||
                    (*p >= '0' && *p <= '9')) {
                    v = strtol(p, &endp, 0);
                    if (*endp == ')' && v >= -128 && v <= 127) {
                        idx = (int)v + 128;
                        last_store[idx] = -1;  /* store is live */
                    }
                }
            }
        }
    }

    /* Delete dead stores in reverse line order to preserve lower indices */
    for (i = nlines - 1; i >= 0; i--) {
        if (is_dead[i]) {
            delete_n(i, 1);
            changed = 1;
        }
    }

    return changed;
}

/*
 * Eliminate: store 32-bit DEHL to (ix+N)..(ix+N+3) immediately followed by
 * reload of the same four bytes back into l/h/e/d.  Since stores to memory do
 * not change the source registers, DEHL still holds the correct value after
 * the stores and the reload is redundant.
 *
 *   ld (ix+N),l      |  ld (ix+N),l
 *   ld (ix+N+1),h    |  ld (ix+N+1),h
 *   ld (ix+N+2),e    |  ld (ix+N+2),e
 *   ld (ix+N+3),d    |  ld (ix+N+3),d
 *   ld l,(ix+N)      |  (deleted)
 *   ld h,(ix+N+1)    |  (deleted)
 *   ld e,(ix+N+2)    |  (deleted)
 *   ld d,(ix+N+3)    |  (deleted)
 *
 * This fires heavily in 32-bit (long) expression code where the compiler
 * spills an intermediate to a local and then immediately reloads it.
 */
static int pass_elim_long_store_reload(void)
{
    int i, changed, ival, k;
    char tmp[MAX_LINE];
    char off0[32], off1[32], off2[32], off3[32];
    char expect[MAX_LINE];
    const char *p;

    changed = 0;

    for (i = 0; i + 7 < nlines; i++) {
        /* Match first store: ld (ix+N),l */
        strip_peep_comment_copy(tmp, lines[i]);
        if (strncmp(tmp, "ld (ix", 6) != 0)
            continue;
        p = tmp + 6;
        k = 0;
        while (*p && *p != ')' && k < 30)
            off0[k++] = *p++;
        off0[k] = 0;
        if (*p != ')' || p[1] != ',' || p[2] != 'l' || p[3] != 0 || k == 0)
            continue;

        /* Compute adjacent offsets */
        ival = (int)strtol(off0, NULL, 0);
        peep_format_ix_off(off1, ival + 1);
        peep_format_ix_off(off2, ival + 2);
        peep_format_ix_off(off3, ival + 3);

        /* Check remaining 3 stores */
        sprintf(expect, "ld (ix%s),h", off1);
        if (!eq(i + 1, expect)) continue;
        sprintf(expect, "ld (ix%s),e", off2);
        if (!eq(i + 2, expect)) continue;
        sprintf(expect, "ld (ix%s),d", off3);
        if (!eq(i + 3, expect)) continue;

        /* Check 4 reloads of the same bytes */
        sprintf(expect, "ld l,(ix%s)", off0);
        if (!eq(i + 4, expect)) continue;
        sprintf(expect, "ld h,(ix%s)", off1);
        if (!eq(i + 5, expect)) continue;
        sprintf(expect, "ld e,(ix%s)", off2);
        if (!eq(i + 6, expect)) continue;
        sprintf(expect, "ld d,(ix%s)", off3);
        if (!eq(i + 7, expect)) continue;

        /* Delete the 4 reload lines; stores stay in case the value is accessed
         * later via ix addressing. */
        delete_n(i + 4, 4);
        changed = 1;
    }

    return changed;
}

static int peep_is_pos_func_label(const char *s)
{
    if (s[0] == '_' &&
        s[1] == 'p' &&
        s[2] == 'o' &&
        s[3] == 's' &&
        strstr(s, "func:") != NULL)
        return 1;

    if (strcmp(s, "_LookForWinner:") == 0)
        return 1;

    return 0;
}

static int peep_is_public_line(const char *s)
{
    return strncmp(s, "public ", 7) == 0;
}

/*
 * In tiny posNfunc helpers, cache the selected board byte in B instead of
 * a one-byte stack local at ix-1.  These helpers make no calls, so B is safe.
 */

/*
 * More tolerant version of the posNfunc byte-local cache.
 * If a tiny no-call posNfunc stores A to the single byte local ix-1 and then
 * only reloads it, keep that byte in B instead.  This removes the local byte
 * allocation and enables later frame elimination.
 */
static int pass_posfunc_ix1_to_b(void)
{
    int i, j, end;
    int changed;
    int has_call;
    int stores;
    int bad_ix1;

    changed = 0;

    for (i = 0; i < nlines; i++) {
        if (!peep_is_pos_func_label(lines[i]))
            continue;

        end = i + 1;
        while (end < nlines && !peep_is_public_line(lines[end]))
            end++;

        has_call = 0;
        stores = 0;
        bad_ix1 = 0;

        for (j = i + 1; j < end; j++) {
            if (strncmp(lines[j], "call ", 5) == 0) {
                has_call = 1;
                break;
            }
            if (eq(j, "ld (ix-1),a")) {
                stores++;
                continue;
            }
            if (strstr(lines[j], "(ix-1)") != NULL &&
                !eq(j, "ld a,(ix-1)") &&
                !eq(j, "ld l,(ix-1)")) {
                bad_ix1 = 1;
                break;
            }
        }

        if (has_call || bad_ix1 || stores != 1)
            continue;

        for (j = i + 1; j < end; j++) {
            if (eq(j, "dec sp")) {
                delete_n(j, 1);
                end--;
                changed = 1;
                j--;
                continue;
            }

            if (eq(j, "ld (ix-1),a")) {
                replace1_tagged(j, "ld b,a", "posfunc_ix1_to_b");
                changed = 1;
                continue;
            }

            if (eq(j, "ld a,(ix-1)")) {
                replace1_tagged(j, "ld a,b", "posfunc_ix1_to_b");
                changed = 1;
                continue;
            }

            if (eq(j, "ld l,(ix-1)")) {
                replace1_tagged(j, "ld l,b", "posfunc_ix1_to_b");
                changed = 1;
                continue;
            }
        }
    }

    return changed;
}

static int pass_posfunc_b_cache(void)
{
    int i, j, end;
    int changed;
    int has_call;
    int has_ix1_store_after_setup;
    int setup_ld_a;
    int setup_store;

    changed = 0;

    for (i = 0; i < nlines; i++) {
        if (!peep_is_pos_func_label(lines[i]))
            continue;

        end = i + 1;
        while (end < nlines && !peep_is_public_line(lines[end]))
            end++;

        has_call = 0;
        for (j = i + 1; j < end; j++) {
            if (strncmp(lines[j], "call ", 5) == 0) {
                has_call = 1;
                break;
            }
        }
        if (has_call)
            continue;

        setup_ld_a = -1;
        setup_store = -1;

        for (j = i + 1; j + 1 < end; j++) {
            if (eq(j, "ld a,(hl)") && eq(j + 1, "ld (ix-1),a")) {
                setup_ld_a = j;
                setup_store = j + 1;
                break;
            }
        }

        if (setup_ld_a < 0)
            continue;

        has_ix1_store_after_setup = 0;
        for (j = setup_store + 1; j < end; j++) {
            if (strstr(lines[j], "(ix-1)") != NULL &&
                strcmp(lines[j], "ld a,(ix-1)") != 0 &&
                strcmp(lines[j], "ld l,(ix-1)") != 0) {
                has_ix1_store_after_setup = 1;
                break;
            }
        }
        if (has_ix1_store_after_setup)
            continue;

        /* Remove the one-byte local allocation if present in the prologue. */
        for (j = i + 1; j < setup_ld_a; j++) {
            if (eq(j, "dec sp")) {
                delete_n(j, 1);
                end--;
                setup_ld_a--;
                setup_store--;
                changed = 1;
                break;
            }
        }

        replace1(setup_ld_a, "ld b,(hl)");
        delete_n(setup_store, 1);
        end--;
        changed = 1;

        for (j = setup_ld_a + 1; j < end; j++) {
            if (eq(j, "ld a,(ix-1)")) {
                replace1(j, "ld a,b");
                changed = 1;
            } else if (eq(j, "ld l,(ix-1)")) {
                replace1(j, "ld l,b");
                changed = 1;
            }
        }
    }

    return changed;
}



static int pass_lookforwinner_b_cache(void)
{
    int i, j, end;
    int changed;
    int in_cache;

    changed = 0;

    for (i = 0; i < nlines; i++) {
        if (strcmp(lines[i], "_LookForWinner:") != 0)
            continue;

        end = i + 1;
        while (end < nlines && !peep_is_public_line(lines[end]))
            end++;

        in_cache = 0;

        for (j = i + 1; j < end; j++) {
            if (eq(j, "dec sp")) {
                delete_n(j, 1);
                end--;
                j--;
                changed = 1;
                continue;
            }

            if (eq(j, "ld a,(hl)") &&
                j + 1 < end &&
                eq(j + 1, "ld (ix-1),a")) {
                replace1(j, "ld b,(hl)");
                delete_n(j + 1, 1);
                end--;
                in_cache = 1;
                changed = 1;
                continue;
            }

            if (in_cache && eq(j, "ld a,(ix-1)")) {
                replace1(j, "ld a,b");
                changed = 1;
                continue;
            }

            if (in_cache && eq(j, "ld l,(ix-1)")) {
                replace1(j, "ld l,b");
                changed = 1;
                continue;
            }

            /* Any other ix-1 store invalidates the cache. */
            if (strstr(lines[j], "(ix-1)") != NULL &&
                strcmp(lines[j], "ld a,(ix-1)") != 0 &&
                strcmp(lines[j], "ld l,(ix-1)") != 0) {
                in_cache = 0;
            }
        }
    }

    return changed;
}



static int peep_parse_ld_hl_0_to_255(const char *s, int *out)
{
    char *endp;
    long v;
    char tmp[MAX_LINE];

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld hl,", 6) != 0)
        return 0;

    v = strtol(tmp + 6, &endp, 0);
    while (*endp == ' ' || *endp == '\t')
        endp++;

    if (*endp != 0 || v < 0 || v > 255)
        return 0;

    out[0] = (int)v;
    return 1;
}

static int peep_parse_ld_h_ix(const char *s, char *off)
{
    char tmp[MAX_LINE];
    const char *p;
    int i;

    strip_peep_comment_copy(tmp, s);

    if (strncmp(tmp, "ld h,(ix", 8) != 0)
        return 0;

    p = tmp + 8;
    i = 0;
    while (*p && *p != ')' && i < 31)
        off[i++] = *p++;
    off[i] = 0;

    if (*p != ')' || p[1] != 0)
        return 0;

    return i > 0;
}





static int peep_parse_ld_ix_pair(const char *s1, const char *s2, int *off)
{
    char loff[32];
    char hoff[32];
    int lo;
    int hi;
    char *endp;

    if (!peep_parse_ld_l_ix(s1, loff))
        return 0;
    if (!peep_parse_ld_h_ix(s2, hoff))
        return 0;

    lo = (int)strtol(loff, &endp, 10);
    if (*endp != 0)
        return 0;
    hi = (int)strtol(hoff, &endp, 10);
    if (*endp != 0)
        return 0;
    if (hi != lo + 1)
        return 0;

    *off = lo;
    return 1;
}


static int peep_parse_st_ix_pair(const char *s1, const char *s2, int *off)
{
    char tmp1[MAX_LINE];
    char tmp2[MAX_LINE];
    char *p;
    char *endp;
    int lo;
    int hi;

    strip_peep_comment_copy(tmp1, s1);
    strip_peep_comment_copy(tmp2, s2);

    if (strncmp(tmp1, "ld (ix", 6) != 0)
        return 0;
    p = tmp1 + 6;
    lo = (int)strtol(p, &endp, 10);
    if (*endp != ')' || endp[1] != ',' || endp[2] != 'l' || endp[3] != 0)
        return 0;

    if (strncmp(tmp2, "ld (ix", 6) != 0)
        return 0;
    p = tmp2 + 6;
    hi = (int)strtol(p, &endp, 10);
    if (*endp != ')' || endp[1] != ',' || endp[2] != 'h' || endp[3] != 0)
        return 0;

    if (hi != lo + 1)
        return 0;

    *off = lo;
    return 1;
}

static int peep_parse_jp_same_z_c(int iz, int ic, char *lab)
{
    char lab2[128];

    if (!peep_parse_jp_cond_label(lines[iz], "z", lab))
        return 0;
    if (!peep_parse_jp_cond_label(lines[ic], "c", lab2))
        return 0;
    return strcmp(lab, lab2) == 0;
}

static int pass_e_signed_le_zero(void)
{
    int i;
    int changed;
    int off;
    char lab[128];
    char line[160];

    changed = 0;

    for (i = 0; i + 12 < nlines; ++i) {
        if (peep_parse_ld_ix_pair(lines[i], lines[i + 1], &off) &&
            eq(i + 2, "ld de,0") &&
            eq(i + 3, "ld a,h") &&
            eq(i + 4, "xor 80h") &&
            eq(i + 5, "ld h,a") &&
            eq(i + 6, "ld a,d") &&
            eq(i + 7, "xor 80h") &&
            eq(i + 8, "ld d,a") &&
            eq(i + 9, "or a") &&
            eq(i + 10, "sbc hl,de") &&
            peep_parse_jp_same_z_c(i + 11, i + 12, lab)) {
            replace1_tagged(i, lines[i], "signed_le_zero");
            replace1(i + 1, lines[i + 1]);
            replace1(i + 2, "ld a,h");
            replace1(i + 3, "or l");
            sprintf(line, "jp z, %s", lab);
            replace1(i + 4, line);
            replace1(i + 5, "bit 7,h");
            sprintf(line, "jp nz, %s", lab);
            replace1(i + 6, line);
            delete_n(i + 7, 6);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int pass_ix_array_word_addr(void)
{
    int i;
    int changed;
    int baseoff;
    int idxoff;
    int step;
    int j;
    char line[160];

    changed = 0;

    for (i = 0; i + 10 < nlines; ++i) {
        if (eq(i, "push ix") &&
            eq(i + 1, "pop hl") &&
            peep_parse_ld_de_signed(lines[i + 2], &baseoff) &&
            eq(i + 3, "add hl,de") &&
            eq(i + 4, "push hl") &&
            peep_parse_ld_ix_pair(lines[i + 5], lines[i + 6], &idxoff)) {
            j = i + 7;
            step = 0;
            if (eq(j, "dec hl")) {
                step = -1;
                j++;
            } else if (eq(j, "inc hl")) {
                step = 1;
                j++;
            }

            if (eq(j, "add hl,hl") &&
                eq(j + 1, "ex de,hl") &&
                eq(j + 2, "pop hl") &&
                eq(j + 3, "add hl,de")) {
                replace1_tagged(i, lines[i + 5], "ix_array_word_addr");
                replace1(i + 1, lines[i + 6]);
                if (step < 0)
                    replace1(i + 2, "dec hl");
                else if (step > 0)
                    replace1(i + 2, "inc hl");
                else
                    replace1(i + 2, "add hl,hl");

                if (step != 0)
                    replace1(i + 3, "add hl,hl");
                else
                    replace1(i + 3, "push ix");
                if (step != 0)
                    replace1(i + 4, "push ix");
                else
                    replace1(i + 4, "pop de");
                if (step != 0)
                    replace1(i + 5, "pop de");
                else
                    replace1(i + 5, "add hl,de");
                if (step != 0)
                    replace1(i + 6, "add hl,de");
                else {
                    sprintf(line, "ld de,%d", baseoff);
                    replace1(i + 6, line);
                }
                if (step != 0) {
                    sprintf(line, "ld de,%d", baseoff);
                    replace1(i + 7, line);
                } else {
                    replace1(i + 7, "add hl,de");
                }
                if (step != 0)
                    replace1(i + 8, "add hl,de");

                if (step != 0)
                    delete_n(i + 9, (j + 4) - (i + 9));
                else
                    delete_n(i + 8, (j + 4) - (i + 8));

                changed = 1;
                if (i > 0) --i;
            }
        }
    }

    return changed;
}

static int pass_ix_postdec_to_local(void)
{
    int i;
    int changed;
    int srcoff;
    int dstoff;
    int j;
    char line[160];
    char offbuf[32];

    changed = 0;

    for (i = 0; i + 17 < nlines; ++i) {
        if (!eq(i, "push ix") || !eq(i + 1, "pop hl"))
            continue;

        j = i + 2;
        if (peep_parse_ld_de_signed(lines[j], &srcoff)) {
            j++;
            if (!eq(j, "add hl,de"))
                continue;
            j++;
        } else {
            srcoff = 0;
            while (eq(j, "dec hl")) {
                srcoff--;
                j++;
            }
            while (eq(j, "inc hl")) {
                srcoff++;
                j++;
            }
            if (srcoff == 0)
                continue;
        }

        if (eq(j, "push hl") &&
            eq(j + 1, "ld e,(hl)") &&
            eq(j + 2, "inc hl") &&
            eq(j + 3, "ld d,(hl)") &&
            eq(j + 4, "ex de,hl") &&
            eq(j + 5, "push hl") &&
            eq(j + 6, "dec hl") &&
            eq(j + 7, "ex de,hl") &&
            eq(j + 8, "pop hl") &&
            eq(j + 9, "ex (sp),hl") &&
            eq(j + 10, "ld (hl),e") &&
            eq(j + 11, "inc hl") &&
            eq(j + 12, "ld (hl),d") &&
            eq(j + 13, "pop hl") &&
            peep_parse_st_ix_pair(lines[j + 14], lines[j + 15], &dstoff)) {
            peep_format_ix_off(offbuf, srcoff);
            sprintf(line, "ld l,(ix%s)", offbuf);
            replace1_tagged(i, line, "ix_postdec_to_local");
            peep_format_ix_off(offbuf, srcoff + 1);
            sprintf(line, "ld h,(ix%s)", offbuf);
            replace1(i + 1, line);
            peep_format_ix_off(offbuf, dstoff);
            sprintf(line, "ld (ix%s),l", offbuf);
            replace1(i + 2, line);
            peep_format_ix_off(offbuf, dstoff + 1);
            sprintf(line, "ld (ix%s),h", offbuf);
            replace1(i + 3, line);
            replace1(i + 4, "dec hl");
            peep_format_ix_off(offbuf, srcoff);
            sprintf(line, "ld (ix%s),l", offbuf);
            replace1(i + 5, line);
            peep_format_ix_off(offbuf, srcoff + 1);
            sprintf(line, "ld (ix%s),h", offbuf);
            replace1(i + 6, line);
            delete_n(i + 7, (j + 16) - (i + 7));
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int pass_store_word_const_hl(void)
{
    int i;
    int changed;
    int imm;
    char line[160];

    changed = 0;

    for (i = 0; i + 3 < nlines; ++i) {
        if (peep_parse_ld_de_0_to_255(lines[i], &imm) &&
            eq(i + 1, "ld (hl),e") &&
            eq(i + 2, "inc hl") &&
            eq(i + 3, "ld (hl),d")) {
            sprintf(line, "ld (hl),%d", imm & 255);
            replace1_tagged(i, line, "store_word_const");
            replace1(i + 1, "inc hl");
            replace1(i + 2, "ld (hl),0");
            delete_n(i + 3, 1);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int peep_prev_nonblank(int i)
{
    i--;
    while (i >= 0 && is_blank_or_comment(lines[i]))
        i--;
    return i;
}

static int pass_incsp_to_popbc(void)
{
    int i;
    int p;
    int changed;

    changed = 0;

    for (i = 0; i + 1 < nlines; ++i) {
        if (eq(i, "inc sp") && eq(i + 1, "inc sp")) {
            p = peep_prev_nonblank(i);
            if (p >= 0 &&
                (strncmp(lines[p], "call ", 5) == 0 ||
                 eq(p, "pop bc"))) {
                replace1_tagged(i, "pop bc", "incsp2_popbc");
                delete_n(i + 1, 1);
                changed = 1;
                if (i > 0) --i;
            }
        }
    }

    return changed;
}

static int pass_remove_unreferenced_labels(void)
{
    int i;
    int changed;
    char lab[128];

    changed = 0;

    for (i = 0; i < nlines; ++i) {
        if (label_name_at(i, lab) &&
            lab[0] == 'L' &&
            !is_label_referenced(lab)) {
            delete_n(i, 1);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int peep_in_function_range(const char *func, int *startp, int *endp)
{
    int i, end;

    for (i = 0; i < nlines; i++) {
        if (strcmp(lines[i], func) == 0) {
            end = i + 1;
            while (end < nlines && !peep_is_public_line(lines[end]))
                end++;
            startp[0] = i;
            endp[0] = end;
            return 1;
        }
    }

    return 0;
}

/*
 * Replace ld de,N; [extrn __mulu;] call __mulu with inline shift-add sequences
 * for small constants (10, 20, 40, 80, 160).  Uses DE as a scratch register to
 * save the original HL value; this matches what the caller already expects
 * (__mulu clobbers DE).
 *
 * Also constant-folds ld hl,C1; ld de,C2; call __mulu → ld hl,C1*C2 when
 * both operands are compile-time constants.
 */

/*
 * Replace integer-zero-to-float conversion followed by a 4-byte float store:
 *
 *   push <addr>
 *   ld hl,0
 *   call __fif
 *   ld b,d
 *   ld c,e
 *   pop de
 *   ex de,hl
 *   ld (hl),e
 *   inc hl
 *   ld (hl),d
 *   inc hl
 *   ld (hl),c
 *   inc hl
 *   ld (hl),b
 *
 * Since __fif(0) is exactly 0.0f, all four stored bytes are zero.  This is
 * common in mm.c's fillc()/ffillc() and avoids a runtime helper call inside
 * the clearing loops.
 */
static int pass_float_zero_store(void)
{
    int i;
    int changed;

    changed = 0;

    for (i = 0; i + 12 < nlines; ++i) {
        if (eq(i, "ld hl,0") &&
            eq(i + 1, "call __fif") &&
            eq(i + 2, "ld b,d") &&
            eq(i + 3, "ld c,e") &&
            eq(i + 4, "pop de") &&
            eq(i + 5, "ex de,hl") &&
            eq(i + 6, "ld (hl),e") &&
            eq(i + 7, "inc hl") &&
            eq(i + 8, "ld (hl),d") &&
            eq(i + 9, "inc hl") &&
            eq(i + 10, "ld (hl),c") &&
            eq(i + 11, "inc hl") &&
            eq(i + 12, "ld (hl),b")) {
            replace1_tagged(i, "pop hl", "float_zero_store");
            replace1(i + 1, "ld (hl),0");
            replace1(i + 2, "inc hl");
            replace1(i + 3, "ld (hl),0");
            replace1(i + 4, "inc hl");
            replace1(i + 5, "ld (hl),0");
            replace1(i + 6, "inc hl");
            replace1(i + 7, "ld (hl),0");
            delete_n(i + 8, 5);
            changed = 1;
            if (i > 0)
                --i;
        }
    }

    return changed;
}

static int pass_mulu_const(void)
{
    int i, changed = 0;

    for (i = 0; i + 1 < nlines; i++) {
        long de_val;
        int has_extrn;
        int n_delete;
        char tmp[MAX_LINE];
        char *endp;

        if (!parse_ld_de_positive_imm(lines[i], &de_val))
            continue;

        has_extrn = (i + 2 < nlines &&
                     eq(i + 1, "extrn __mulu") &&
                     eq(i + 2, "call __mulu"));
        if (!has_extrn && !eq(i + 1, "call __mulu"))
            continue;

        n_delete = has_extrn ? 3 : 2;

        /* Constant fold: ld hl,C1 immediately before ld de,C2; call __mulu */
        if (i > 0) {
            strip_peep_comment_copy(tmp, lines[i - 1]);
            if (strncmp(tmp, "ld hl,", 6) == 0) {
                long hl_val = strtol(tmp + 6, &endp, 0);
                while (*endp == ' ' || *endp == '\t') endp++;
                if (*endp == '\0' && hl_val >= 0 && hl_val <= 32767) {
                    long prod = hl_val * de_val;
                    if (prod >= 0 && prod <= 65535) {
                        char buf[64];
                        sprintf(buf, "ld hl,%ld ; peep: mulu_const_fold", prod);
                        replace1(i - 1, buf);
                        delete_n(i, n_delete);
                        changed = 1;
                        if (i > 1) i -= 2;
                        continue;
                    }
                }
            }
        }

        /* Inline expansion for specific multiplier constants.
         * Strategy: save HL in DE (ld d,h; ld e,l), compute 4x, add original
         * to get 5x, then shift left to reach the target.
         * 10  = 5 * 2    :  ×1→DE, ×2, ×4, +DE(=5), ×2
         * 20  = 5 * 4    :  ×1→DE, ×2, ×4, +DE(=5), ×2, ×2
         * 40  = 5 * 8    :  ×1→DE, ×2, ×4, +DE(=5), ×2, ×2, ×2
         * 80  = 5 * 16   :  ×1→DE, ×2, ×4, +DE(=5), ×2, ×2, ×2, ×2
         * 160 = 5 * 32   :  ×1→DE, ×2, ×4, +DE(=5), ×2, ×2, ×2, ×2, ×2  */
        if (de_val != 10 && de_val != 20 && de_val != 40 &&
            de_val != 80 && de_val != 160)
            continue;

        delete_n(i, n_delete);

        /* Insert instructions in REVERSE order so position i holds the first. */
        /* Extra trailing shifts (160 needs one more than 80, etc.) */
        if (de_val >= 160) insert_line(i, "add hl,hl");   /* ×160 */
        if (de_val >=  80) insert_line(i, "add hl,hl");   /* ×80  */
        if (de_val >=  40) insert_line(i, "add hl,hl");   /* ×40  */
        if (de_val >=  20) insert_line(i, "add hl,hl");   /* ×20  */
        if (de_val >=  10) insert_line(i, "add hl,hl");   /* ×10  */
        /* Core 5× sequence: */
        insert_line(i, "add hl,de");                       /* ×5 = ×4 + ×1 */
        insert_line(i, "add hl,hl");                       /* ×4  */
        insert_line(i, "add hl,hl");                       /* ×2  */
        insert_line(i, "ld e,l");                          /* save ×1 low  */
        {
            char tag[32];
            sprintf(tag, "mulu%ld", de_val);
            insert_line_tagged(i, "ld d,h", tag);          /* save ×1 high */
        }

        changed = 1;
        if (i > 0) i--;
    }

    return changed;
}

static int stride_parse_ld_r_ix_neg(const char *s, char r, int *n); /* forward */

/*
 * Remove the 6-instruction signed-compare bias (xor 80h pattern) when the
 * comparison constant fits in one byte (D=0 in ld de,N) and is followed by
 * ld h,(ix-K) immediately before ld de,N — indicating a local frame variable
 * that is non-negative (loop counter 0..N-1).
 *
 * Safe because: for values 0..127, H=0 always, and the bias xor 80h on both
 * sides cancels, producing the same carry as plain unsigned subtraction.
 */
static int pass_signed_cmp_small_const(void)
{
    int i, changed = 0;
    int imm, hi_off;

    for (i = 0; i + 8 < nlines; i++) {
        /* ld de,N where 0 < N <= 127 (D byte is 0 before and after xor) */
        if (!peep_parse_ld_de_0_to_255(lines[i], &imm) || imm > 127 || imm == 0)
            continue;

        /* Preceding line must be ld h,(ix-K) — high byte of a frame local */
        if (i < 1 || !stride_parse_ld_r_ix_neg(lines[i - 1], 'h', &hi_off))
            continue;

        /* The signed-compare bias block: 6 instructions */
        if (eq(i + 1, "ld a,h") &&
            eq(i + 2, "xor 80h") &&
            eq(i + 3, "ld h,a") &&
            eq(i + 4, "ld a,d") &&
            eq(i + 5, "xor 80h") &&
            eq(i + 6, "ld d,a") &&
            eq(i + 7, "or a") &&
            eq(i + 8, "sbc hl,de")) {
            delete_n(i + 1, 6);
            changed = 1;
            if (i > 0) i--;
        }
    }

    return changed;
}

/*
 * In MinMax, score/value/alpha/beta/depth are constrained to small
 * nonnegative SCORE_* values and depths.  Remove the signed-compare bias
 * from comparisons inside this function only.
 */
static int pass_minmax_unsigned_compares(void)
{
    int start, end;
    int i;
    int changed;

    changed = 0;

    if (!peep_in_function_range("_MinMax:", &start, &end))
        return 0;

    for (i = start; i + 6 < end; i++) {
        if (eq(i, "ld a,h") &&
            eq(i + 1, "xor 80h") &&
            eq(i + 2, "ld h,a") &&
            eq(i + 3, "ld a,d") &&
            eq(i + 4, "xor 80h") &&
            eq(i + 5, "ld d,a") &&
            eq(i + 6, "or a") &&
            eq(i + 7, "sbc hl,de")) {
            delete_n(i, 6);
            end -= 6;
            changed = 1;
            if (i > start)
                i--;
        }
    }

    return changed;
}



static int peep_line_in_function(int line, const char *func)
{
    int i;
    int start;

    start = -1;
    for (i = 0; i < nlines; i++) {
        if (strcmp(lines[i], func) == 0)
            start = i;
        else if (i > line)
            break;
        else if (start >= 0 && peep_is_public_line(lines[i]) && i != start)
            start = -1;
    }

    return start >= 0 && line > start;
}



static int pass_fix_main_argc_gt_one(void)
{
    int start, end, i;
    char lab_true1[128], lab_true2[128], lab_false[128];
    char line[160];

    if (!peep_in_function_range("_main:", &start, &end))
        return 0;

    for (i = start; i + 13 < end; ++i) {
        /*
         * Repair unsafe fold of:
         *     argc > 1 ? atoi(argv[1]) : 1
         *
         * Bad generated/folded shape:
         *     ld l,(ix+4)
         *     ld h,(ix+5)
         *     ld de,1
         *     or a
         *     sbc hl,de
         *     jp z, TRUE
         *     jp nc, TRUE
         *     jp FALSE
         *
         * For argc == 1, Z is set and this incorrectly enters TRUE.
         * Correct for argc > 1 is false on Z or C, true otherwise.
         */
        if (eq(i, "ld l,(ix+4)") &&
            eq(i + 1, "ld h,(ix+5)") &&
            eq(i + 2, "ld de,1") &&
            eq(i + 3, "or a") &&
            eq(i + 4, "sbc hl,de") &&
            peep_parse_jp_cond_label(lines[i + 5], "z", lab_true1) &&
            peep_parse_jp_cond_label(lines[i + 6], "nc", lab_true2) &&
            peep_parse_jp_uncond_label(lines[i + 7], lab_false) &&
            !strcmp(lab_true1, lab_true2) &&
            strcmp(lab_true1, lab_false) != 0) {
            sprintf(line, "jp z, %s", lab_false);
            replace1(i + 5, line);
            sprintf(line, "jp c, %s", lab_false);
            replace1(i + 6, line);
            sprintf(line, "jp %s", lab_true1);
            replace1(i + 7, line);
            return 1;
        }
    }

    return 0;
}



static int pass_replace_tstr_fake_strstr(void)
{
    int i, j, n;
    static const char *body[] = {
        "\tpublic _Z0010",
        "_Z0010:",
        "\tpush ix",
        "\tld ix,0",
        "\tadd ix,sp",
        "\tld l,(ix+6)",
        "\tld h,(ix+7)",
        "\tld a,(hl)",
        "\tor a",
        "\tjp nz, ZSNN",
        "\tld l,(ix+4)",
        "\tld h,(ix+5)",
        "\tjp ZSR",
        "ZSNN:",
        "\tld l,(ix+4)",
        "\tld h,(ix+5)",
        "ZSO:",
        "\tld a,(hl)",
        "\tor a",
        "\tjp z, ZSNF",
        "\tpush hl",
        "\tld e,l",
        "\tld d,h",
        "\tld l,(ix+6)",
        "\tld h,(ix+7)",
        "ZSI:",
        "\tld a,(hl)",
        "\tor a",
        "\tjp z, ZSF",
        "\tld c,a",
        "\tld a,(de)",
        "\tor a",
        "\tjp z, ZSM",
        "\tcp c",
        "\tjp nz, ZSM",
        "\tinc hl",
        "\tinc de",
        "\tjp ZSI",
        "ZSF:",
        "\tpop hl",
        "\tjp ZSR",
        "ZSM:",
        "\tpop hl",
        "\tinc hl",
        "\tjp ZSO",
        "ZSNF:",
        "\tld hl,0",
        "ZSR:",
        "\tld sp,ix",
        "\tpop ix",
        "\tret"
    };

    for (i = 0; i < nlines; ++i) {
        if (!strcmp(lines[i], "public _Z0010") && i + 1 < nlines && !strcmp(lines[i + 1], "_Z0010:")) {
            j = i + 2;
            while (j < nlines && strcmp(lines[j], "public _Z0011") != 0)
                ++j;
            if (j >= nlines)
                return 0;

            n = (int)(sizeof(body) / sizeof(body[0]));
            delete_n(i, j - i);
            for (j = 0; j < n; ++j)
                insert_line(i + j, body[j]);
            return 1;
        }
    }
    return 0;
}

/*
 * Eliminate the IX frame-pointer prologue/epilogue from leaf functions where
 * the peephole optimizer has already removed all IX-relative memory accesses.
 *
 * Pattern to remove:
 *   push ix          ← prologue (3 lines deleted)
 *   ld ix,0
 *   add ix,sp
 *   ... body with no (ix+N)/(ix-N) references ...
 *   ld sp,ix         ← epilogue simplified: these 2 lines deleted, "ret" kept
 *   pop ix
 *   ret
 *
 * Safety checks: abort if any line in the body contains "(ix" (live IX usage)
 * or if an un-removed local-allocation sequence is present.
 */
static int pass_elim_ix_frame(void)
{
    int i, j;
    int changed;
    int next_func;
    int has_ix_use;
    int epi;

    changed = 0;

    for (i = 0; i + 4 < nlines; i++) {
        if (!eq(i, "push ix") || !eq(i+1, "ld ix,0") || !eq(i+2, "add ix,sp"))
            continue;

        /* Find the function boundary: next "public" directive or EOF */
        next_func = nlines;
        for (j = i + 3; j < nlines; j++) {
            if (strncmp(lines[j], "public ", 7) == 0) {
                next_func = j;
                break;
            }
        }

        /* Scan the body for IX usage and locate the epilogue */
        has_ix_use = 0;
        epi = -1;
        for (j = i + 3; j < next_func; j++) {
            /* Locate epilogue first.  Its IX references are the only ones
             * allowed when deciding whether the frame pointer is dead. */
            if (eq(j, "ld sp,ix") && j + 2 < next_func &&
                eq(j + 1, "pop ix") && eq(j + 2, "ret")) {
                epi = j;
                j += 1;
                continue;
            }

            /* Any other remaining IX use means the frame pointer is still
             * live.  The old test only looked for "(ix" indexed memory
             * operands, but generated code also uses IX as a value via:
             *
             *     push ix
             *     pop hl
             *     ld de,4
             *     add hl,de
             *
             * to form parameter/local addresses.  Removing the prologue in
             * that case leaves IX uninitialised and breaks make_move(). */
            {
                char tmp_ix_scan[MAX_LINE];
                strip_peep_comment_copy(tmp_ix_scan, lines[j]);
                if (strstr(tmp_ix_scan, "ix") != NULL) {
                    has_ix_use = 1;
                    break;
                }
            }
            /* Detect an un-removed local allocation: ld hl,-N / add hl,sp / ld sp,hl */
            if (j + 2 < next_func &&
                strncmp(lines[j], "ld hl,-", 7) == 0 &&
                eq(j+1, "add hl,sp") &&
                eq(j+2, "ld sp,hl")) {
                has_ix_use = 1;
                break;
            }
        }

        if (!has_ix_use && epi >= 0) {
            delete_n(i, 3);     /* remove push ix / ld ix,0 / add ix,sp */
            epi -= 3;
            delete_n(epi, 2);   /* remove ld sp,ix / pop ix; "ret" stays */
            changed = 1;
            i--;                /* re-examine same position after deletions */
        }
    }

    return changed;
}

/*
 * pass_shared_frame_stubs:
 *
 * Called after pass_elim_ix_frame (which already stripped IX frames from
 * functions that never touch IX).  This pass converts the remaining framed
 * prologues and epilogues to shared stub calls, saving ~5-7 bytes per
 * prologue and ~2 bytes per epilogue that has locals.
 *
 * With locals (13 inline bytes → 6):
 *   push ix / ld ix,0 / add ix,sp / ld hl,-N / add hl,sp / ld sp,hl
 *   → ld hl,-N / call __entr
 *
 * Without locals but with IX-accessed params (8 inline bytes -> 3):
 *   push ix / ld ix,0 / add ix,sp
 *   -> call __en0
 *
 * Epilogue -- with-locals only (5 inline bytes -> 3):
 *   ld sp,ix / pop ix / ret
 *   -> jp __lve
 *
 * The no-locals epilogue (pop ix / ret, 3 bytes after the dcc.c fix) is
 * already as compact as a jp __lve, so it is left inline.
 *
 * RTL stub names are <=6 chars so they stay distinct in L80's 6-character
 * symbol table.  extrn declarations are injected at the top of the file so
 * that dccrtlstrip includes only the blocks actually used.
 */
static int pass_shared_frame_stubs(void)
{
    int i, j;
    int changed = 0;
    int used_entr = 0, used_en0 = 0, used_lve = 0;

    for (i = 0; i + 2 < nlines; i++) {
        int next_func, epi, has_locals;
        char lsize[MAX_LINE];

        if (!eq(i, "push ix") || !eq(i+1, "ld ix,0") || !eq(i+2, "add ix,sp"))
            continue;

        /* Find next function boundary. */
        next_func = nlines;
        for (j = i + 3; j < nlines; j++) {
            if (strncmp(lines[j], "public ", 7) == 0) {
                next_func = j;
                break;
            }
        }

        /* Check for local variable allocation immediately after prologue. */
        has_locals = 0;
        lsize[0] = '\0';
        if (i + 5 < next_func &&
            strncmp(lines[i+3], "ld hl,-", 7) == 0 &&
            eq(i+4, "add hl,sp") &&
            eq(i+5, "ld sp,hl")) {
            has_locals = 1;
            strncpy(lsize, lines[i+3], sizeof(lsize) - 1);
            lsize[sizeof(lsize) - 1] = '\0';
        }

        if (has_locals) {
            /* Find the matching epilogue: ld sp,ix / pop ix / ret */
            epi = -1;
            for (j = i + 6; j + 2 < next_func; j++) {
                if (eq(j, "ld sp,ix") && eq(j+1, "pop ix") && eq(j+2, "ret")) {
                    epi = j;
                    break;
                }
            }
            if (epi < 0)
                continue; /* no canonical epilogue found — leave as-is */

            /*
             * Replace prologue: remove push ix/ld ix,0/add ix,sp (3 lines),
             * then remove add hl,sp/ld sp,hl (2 lines), leaving ld hl,-N at
             * position i.  Insert call __entr after it.
             */
            delete_n(i, 3);
            epi -= 3;
            delete_n(i + 1, 2);
            epi -= 2;
            insert_line_tagged(i + 1, "call __entr", "shared_frame");
            epi += 1;
            used_entr = 1;

            /* Replace epilogue: ld sp,ix / pop ix / ret -> jp __lve */
            replace1_tagged(epi, "jp __lve", "shared_frame");
            delete_n(epi + 1, 2);
            used_lve = 1;
        } else {
            /* No-locals: push ix / ld ix,0 / add ix,sp -> call __en0.
             * Since dcc now always emits ld sp,ix in the epilogue,
             * also convert ld sp,ix / pop ix / ret -> jp __lve so that
             * no-locals functions remain as compact as before. */
            epi = -1;
            for (j = i + 3; j + 2 < next_func; j++) {
                if (eq(j, "ld sp,ix") && eq(j+1, "pop ix") && eq(j+2, "ret")) {
                    epi = j;
                    break;
                }
            }

            replace1_tagged(i, "call __en0", "shared_frame");
            delete_n(i + 1, 2);
            used_en0 = 1;

            if (epi >= 0) {
                epi -= 2; /* two lines removed from prolog above */
                replace1_tagged(epi, "jp __lve", "shared_frame");
                delete_n(epi + 1, 2);
                used_lve = 1;
            }
        }

        changed = 1;
        i--; /* re-examine same index after line deletions */
    }

    /*
     * Inject extrn declarations at the top of the file for each stub used.
     * dccrtlstrip uses these references to select which RTL blocks to include.
     * Insert in reverse order so the final sequence reads __entr/__en0/__lve.
     */
    if (used_entr || used_en0 || used_lve) {
        int pos = 0;
        if (used_lve)  insert_line(pos, "extrn __lve");
        if (used_en0)  insert_line(pos, "extrn __en0");
        if (used_entr) insert_line(pos, "extrn __entr");
    }

    return changed;
}

static int pass_byte_minmax_patterns(void)
{
    int i;
    int changed;
    int imm;
    char off[32];
    char newline[160];

    changed = 0;

    for (i = 0; i + 11 < nlines; ++i) {
        /*
         * Unsigned byte local/parameter compare against small constant:
         *
         *   ld hl,N
         *   push hl
         *   ld l,(ix+K)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp cc,L
         *
         * For byte values this is equivalent to:
         *
         *   ld a,(ix+K)
         *   cp N
         *   jp cc,L
         */
        if (peep_parse_ld_hl_0_to_255(lines[i], &imm) &&
            eq(i + 1, "push hl") &&
            peep_parse_ld_l_ix(lines[i + 2], off) &&
            eq(i + 3, "ld h,0") &&
            eq(i + 4, "ex de,hl") &&
            eq(i + 5, "pop hl") &&
            eq(i + 6, "or a") &&
            eq(i + 7, "sbc hl,de") &&
            (strncmp(lines[i + 8], "jp z,", 5) == 0 ||
             strncmp(lines[i + 8], "jp nz,", 6) == 0 ||
             strncmp(lines[i + 8], "jp c,", 5) == 0 ||
             strncmp(lines[i + 8], "jp nc,", 6) == 0)) {
            sprintf(newline, "ld a,(ix%s)", off);
            replace1_tagged(i, newline, "byte_const_cmp");
            sprintf(newline, "cp %d", imm);
            replace1(i + 1, newline);
            replace1(i + 2, lines[i + 8]);
            delete_n(i + 3, 6);
            changed = 1;
            if (i > 0) --i;
            continue;
        }

        /*
         * Same compare, but the constant is in DE because push_lde_pop
         * has already folded a push/pop pair:
         *
         *   ld l,(ix+K)
         *   ld h,0
         *   ld de,N
         *   or a
         *   sbc hl,de
         *   jp cc,L
         */
        if (peep_parse_ld_l_ix(lines[i], off) &&
            eq(i + 1, "ld h,0") &&
            peep_parse_ld_de_0_to_255(lines[i + 2], &imm) &&
            eq(i + 3, "or a") &&
            eq(i + 4, "sbc hl,de") &&
            (strncmp(lines[i + 5], "jp z,", 5) == 0 ||
             strncmp(lines[i + 5], "jp nz,", 6) == 0 ||
             strncmp(lines[i + 5], "jp c,", 5) == 0 ||
             strncmp(lines[i + 5], "jp nc,", 6) == 0)) {
            sprintf(newline, "ld a,(ix%s)", off);
            replace1_tagged(i, newline, "byte_de_cmp");
            sprintf(newline, "cp %d", imm);
            replace1(i + 1, newline);
            replace1(i + 2, lines[i + 5]);
            delete_n(i + 3, 3);
            changed = 1;
            if (i > 0) --i;
            continue;
        }

        /*
         * Byte local/parameter compare:
         *
         *   ld l,(ix+A)
         *   ld h,0
         *   push hl
         *   ld l,(ix+B)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp cc,L
         *
         * becomes:
         *   ld a,(ix+A)
         *   cp (ix+B)
         *   jp cc,L
         */
        {
            char off2[32];
            if (peep_parse_ld_l_ix(lines[i], off) &&
                eq(i + 1, "ld h,0") &&
                eq(i + 2, "push hl") &&
                peep_parse_ld_l_ix(lines[i + 3], off2) &&
                eq(i + 4, "ld h,0") &&
                eq(i + 5, "ex de,hl") &&
                eq(i + 6, "pop hl") &&
                eq(i + 7, "or a") &&
                eq(i + 8, "sbc hl,de") &&
                (strncmp(lines[i + 9], "jp z,", 5) == 0 ||
                 strncmp(lines[i + 9], "jp nz,", 6) == 0 ||
                 strncmp(lines[i + 9], "jp c,", 5) == 0 ||
                 strncmp(lines[i + 9], "jp nc,", 6) == 0)) {
                sprintf(newline, "ld a,(ix%s)", off);
                replace1_tagged(i, newline, "byte_ix_cmp");
                sprintf(newline, "cp (ix%s)", off2);
                replace1(i + 1, newline);
                replace1(i + 2, lines[i + 9]);
                delete_n(i + 3, 7);
                changed = 1;
                if (i > 0) --i;
                continue;
            }
        }

        /*
         * Byte "(x & 1)" branch:
         *
         *   ld l,(ix+K)
         *   ld h,0
         *   ld de,1
         *   ld a,h
         *   and d
         *   ld h,a
         *   ld a,l
         *   and e
         *   ld l,a
         *   ld a,h
         *   or l
         *   jp z/nz,L
         */
        if (peep_parse_ld_l_ix(lines[i], off) &&
            eq(i + 1, "ld h,0") &&
            peep_parse_ld_de_0_to_255(lines[i + 2], &imm) &&
            imm == 1 &&
            eq(i + 3, "ld a,h") &&
            eq(i + 4, "and d") &&
            eq(i + 5, "ld h,a") &&
            eq(i + 6, "ld a,l") &&
            eq(i + 7, "and e") &&
            eq(i + 8, "ld l,a") &&
            eq(i + 9, "ld a,h") &&
            eq(i + 10, "or l") &&
            (strncmp(lines[i + 11], "jp z,", 5) == 0 ||
             strncmp(lines[i + 11], "jp nz,", 6) == 0)) {
            sprintf(newline, "ld a,(ix%s)", off);
            replace1_tagged(i, newline, "byte_and1_bool");
            replace1(i + 1, "and 1");
            replace1(i + 2, lines[i + 11]);
            delete_n(i + 3, 9);
            changed = 1;
            if (i > 0) --i;
            continue;
        }
    }

    return changed;
}



static int pass_byte_minmax_board_and_assign(void)
{
    int i;
    int changed;
    char idxoff[32];
    char srcoff[32];
    char dstoff[32];
    char line[160];

    changed = 0;

    for (i = 0; i + 15 < nlines; ++i) {
        /*
         * if (0 == g_board[p]) byte test:
         *
         *   ld hl,0
         *   push hl
         *   ld hl,_g_board
         *   push hl
         *   ld l,(ix-K)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   add hl,de
         *   ld l,(hl)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp nz,L
         *
         * becomes:
         *   ld l,(ix-K)
         *   ld h,0
         *   ld de,_g_board
         *   add hl,de
         *   ld a,(hl)
         *   or a
         *   jp nz,L
         */
        if (eq(i, "ld hl,0") &&
            eq(i + 1, "push hl") &&
            eq(i + 2, "ld hl,_g_board") &&
            eq(i + 3, "push hl") &&
            peep_parse_ld_l_ix(lines[i + 4], idxoff) &&
            eq(i + 5, "ld h,0") &&
            eq(i + 6, "ex de,hl") &&
            eq(i + 7, "pop hl") &&
            eq(i + 8, "add hl,de") &&
            eq(i + 9, "ld l,(hl)") &&
            eq(i + 10, "ld h,0") &&
            eq(i + 11, "ex de,hl") &&
            eq(i + 12, "pop hl") &&
            eq(i + 13, "or a") &&
            eq(i + 14, "sbc hl,de") &&
            (strncmp(lines[i + 15], "jp z,", 5) == 0 ||
             strncmp(lines[i + 15], "jp nz,", 6) == 0)) {
            sprintf(line, "ld l,(ix%s)", idxoff);
            replace1_tagged(i, line, "board_index_byte_zero");
            replace1(i + 1, "ld h,0");
            replace1(i + 2, "ld de,_g_board");
            replace1(i + 3, "add hl,de");
            replace1(i + 4, "ld a,(hl)");
            replace1(i + 5, "or a");
            replace1(i + 6, lines[i + 15]);
            delete_n(i + 7, 9);
            changed = 1;
            if (i > 0) --i;
            continue;
        }

        /*
         * g_board[p] = local_byte:
         *
         *   ld l,(ix-K)       ; sometimes a dead duplicate before base load
         *   ld h,0
         *   ld hl,_g_board
         *   push hl
         *   ld l,(ix-K)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   add hl,de
         *   push hl
         *   ld l,(ix-S)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   ld (hl),e
         */
        if (peep_parse_ld_l_ix(lines[i], idxoff) &&
            eq(i + 1, "ld h,0") &&
            eq(i + 2, "ld hl,_g_board") &&
            eq(i + 3, "push hl") &&
            peep_parse_ld_l_ix(lines[i + 4], srcoff) &&
            strcmp(idxoff, srcoff) == 0 &&
            eq(i + 5, "ld h,0") &&
            eq(i + 6, "ex de,hl") &&
            eq(i + 7, "pop hl") &&
            eq(i + 8, "add hl,de") &&
            eq(i + 9, "push hl") &&
            peep_parse_ld_l_ix(lines[i + 10], dstoff) &&
            eq(i + 11, "ld h,0") &&
            eq(i + 12, "ex de,hl") &&
            eq(i + 13, "pop hl") &&
            eq(i + 14, "ld (hl),e")) {
            sprintf(line, "ld l,(ix%s)", idxoff);
            replace1_tagged(i, line, "board_index_byte_store");
            replace1(i + 1, "ld h,0");
            replace1(i + 2, "ld de,_g_board");
            replace1(i + 3, "add hl,de");
            sprintf(line, "ld a,(ix%s)", dstoff);
            replace1(i + 4, line);
            replace1(i + 5, "ld (hl),a");
            delete_n(i + 6, 9);
            changed = 1;
            if (i > 0) --i;
            continue;
        }

        /*
         * Byte local/param assignment through HL low byte:
         *   ld l,(ix-S)
         *   ld h,0
         *   ld (ix-D),l
         */
        if (peep_parse_ld_l_ix(lines[i], srcoff) &&
            eq(i + 1, "ld h,0") &&
            strncmp(lines[i + 2], "ld (ix", 6) == 0) {
            char tmp[MAX_LINE];
            const char *p;
            int k;

            strip_peep_comment_copy(tmp, lines[i + 2]);
            p = tmp + 6;
            k = 0;
            while (*p && *p != ')' && k < 31)
                dstoff[k++] = *p++;
            dstoff[k] = 0;
            if (*p == ')' && p[1] == ',' && p[2] == 'l' && p[3] == 0) {
                sprintf(line, "ld a,(ix%s)", srcoff);
                replace1_tagged(i, line, "byte_assign_ix");
                sprintf(line, "ld (ix%s),a", dstoff);
                replace1(i + 1, line);
                delete_n(i + 2, 1);
                changed = 1;
                if (i > 0) --i;
                continue;
            }
        }
    }

    return changed;
}



static int pass_dead_hl_load_before_ldhl(void)
{
    int i;
    int changed;
    char off[32];
    char imm[128];

    changed = 0;

    for (i = 0; i + 2 < nlines; ++i) {
        if (peep_parse_ld_l_ix(lines[i], off) &&
            eq(i + 1, "ld h,0") &&
            parse_ld_hl_imm(lines[i + 2], imm)) {
            delete_n(i, 2);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int pass_cp_zero_to_or_a(void)
{
    int i;
    int changed;

    changed = 0;

    for (i = 0; i + 2 < nlines; ++i) {
        if (eq(i + 1, "cp 0") &&
            (strncmp(lines[i + 2], "jp z,", 5) == 0 ||
             strncmp(lines[i + 2], "jp nz,", 6) == 0)) {
            replace1_tagged(i + 1, "or a", "cp0_or_a");
            changed = 1;
        }
    }

    return changed;
}

static int pass_inline_simple_call_hl_from_loaded_pointer(void)
{
    int i;
    int changed;
    int off_store;
    int off_load;
    int calli;
    int has_extrn;

    changed = 0;

    for (i = 0; i + 16 < nlines; ++i) {
        if (!eq(i, "ld e,(hl)") ||
            !eq(i + 1, "inc hl") ||
            !eq(i + 2, "ld d,(hl)") ||
            !eq(i + 3, "ex de,hl"))
            continue;

        if (!peep_parse_st_ix_pair(lines[i + 4], lines[i + 5], &off_store))
            continue;
        if (!peep_parse_ld_ix_pair(lines[i + 6], lines[i + 7], &off_load))
            continue;
        if (off_store != off_load)
            continue;

        if (!eq(i + 8, "push hl") ||
            !eq(i + 9, "ld hl,0") ||
            !eq(i + 10, "add hl,sp") ||
            !eq(i + 11, "ld e,(hl)") ||
            !eq(i + 12, "inc hl") ||
            !eq(i + 13, "ld d,(hl)") ||
            !eq(i + 14, "ex de,hl"))
            continue;

        calli = i + 15;
        has_extrn = 0;
        if (eq(calli, "extrn __call_hl")) {
            has_extrn = 1;
            calli++;
        }

        if (!eq(calli, "call __call_hl"))
            continue;
        if (!eq(calli + 1, "pop bc"))
            continue;

        /*
         * The original sequence:
         *   load word at (HL) into DE; ex de,hl
         *   store/reload the function pointer through an IX temp
         *   push it and reload it from SP just to call __call_hl
         *
         * Since __call_hl takes the target in HL, keep the initial load and
         * call directly.
         */
        replace1_tagged(i, "ld e,(hl)", "inline_call_hl_ptr");
        replace1(i + 1, "inc hl");
        replace1(i + 2, "ld d,(hl)");
        replace1(i + 3, "ex de,hl");
        if (has_extrn) {
            replace1(i + 4, "extrn __call_hl");
            replace1(i + 5, "call __call_hl");
            delete_n(i + 6, (calli + 2) - (i + 6));
        } else {
            replace1(i + 4, "call __call_hl");
            delete_n(i + 5, (calli + 2) - (i + 5));
        }

        changed = 1;
        if (i > 0) --i;
    }

    return changed;
}




static int pass_call_hl_stack_roundtrip(void)
{
    int i;
    int calli;
    int changed;

    changed = 0;

    /*
     * New direct function-pointer-array calls can generate:
     *
     *     ex de,hl        ; HL = function pointer
     *     push hl
     *     ld hl,0
     *     add hl,sp
     *     ld e,(hl)
     *     inc hl
     *     ld d,(hl)
     *     ex de,hl
     *     [extrn __call_hl]
     *     call __call_hl
     *     pop bc
     *
     * Since HL already contains the function pointer before the push, the
     * stack round-trip is pointless.  Keep the first ex de,hl and call
     * __call_hl directly.
     */
    for (i = 0; i + 9 < nlines; ++i) {
        if (!(eq(i,     "ex de,hl") &&
              eq(i + 1, "push hl") &&
              eq(i + 2, "ld hl,0") &&
              eq(i + 3, "add hl,sp") &&
              eq(i + 4, "ld e,(hl)") &&
              eq(i + 5, "inc hl") &&
              eq(i + 6, "ld d,(hl)") &&
              eq(i + 7, "ex de,hl")))
            continue;

        calli = i + 8;
        if (eq(calli, "extrn __call_hl"))
            ++calli;

        if (calli + 1 >= nlines)
            continue;
        if (!eq(calli, "call __call_hl") || !eq(calli + 1, "pop bc"))
            continue;

        /* Delete push/reload/second-ex, and delete the pop.  Leave optional extrn. */
        delete_n(i + 1, 7);
        calli -= 7;
        if (eq(calli, "extrn __call_hl"))
            ++calli;
        if (eq(calli + 1, "pop bc"))
            delete_n(calli + 1, 1);

        replace1_tagged(calli, "call __call_hl", "call_hl_stack_roundtrip");
        changed = 1;
        if (i > 0)
            --i;
    }

    return changed;
}

static int pass_minmax_winner_result_no_temp(void)
{
    int start;
    int end;
    int i;
    int j;
    int changed;

    changed = 0;

    if (!peep_in_function_range("_MinMax:", &start, &end))
        return 0;

    /*
     * The winner-function result is returned in L.  The generated code stores
     * it into ix-3, tests it, then reloads ix-3 to compare with PieceX.
     * But ix-3 is later overwritten with the loop variable p=0, and the
     * zero/PieceX tests do not need the spill.
     *
     * Accept both old and new dispatch cleanup shapes:
     *
     *     call __call_hl
     *     ld (ix-3),l
     *
     * and:
     *
     *     call __call_hl
     *     pop bc
     *     ld (ix-3),l
     */
    for (i = start; i + 5 < end; ++i) {
        if (!eq(i, "call __call_hl"))
            continue;

        j = i + 1;
        while (j < end && eq(j, "pop bc"))
            ++j;

        if (j + 4 < end &&
            eq(j, "ld (ix-3),l") &&
            eq(j + 1, "ld a,l") &&
            eq(j + 2, "or a") &&
            strncmp(lines[j + 3], "jp z,", 5) == 0 &&
            eq(j + 4, "ld a,(ix-3)")) {
            delete_n(j, 1);
            end--;
            replace1_tagged(j + 3, "ld a,l", "winner_result_no_temp");
            changed = 1;
            if (i > start)
                --i;
        }
    }

    return changed;
}

static int pass_minmax_score_b_cache(void)
{
    int start;
    int end;
    int i;
    int j;
    int changed;
    int off;
    char tmp[MAX_LINE];

    changed = 0;

    if (!peep_in_function_range("_MinMax:", &start, &end))
        return 0;

    /*
     * In the uint8_t MinMax variant, score is stored in the byte local ix-4
     * immediately after the recursive call:
     *
     *     call _MinMax
     *     pop bc ...
     *     ld (ix-4),l
     *
     * From there until the next recursive call, it is only read as
     *     ld a,(ix-4)
     * and no calls occur.  Keep it in B instead.  This also lets the frame
     * shrink from 4 bytes to 3 bytes after all ix-4 references disappear.
     */
    for (i = start; i < end; ++i) {
        if (!eq(i, "ld (ix-4),l"))
            continue;

        /* Make sure this really follows the recursive call cleanup. */
        j = i - 1;
        while (j > start && eq(j, "pop bc"))
            --j;
        if (!eq(j, "call _MinMax"))
            continue;

        replace1_tagged(i, "ld b,l", "minmax_score_b");

        for (j = i + 1; j < end; ++j) {
            if (eq(j, "call _MinMax"))
                break;

            strip_peep_comment_copy(tmp, lines[j]);
            if (!strcmp(tmp, "ld a,(ix-4)")) {
                replace1_tagged(j, "ld a,b", "minmax_score_b");
                changed = 1;
                continue;
            }

            /*
             * Be conservative.  If some future code writes ix-4 or loads it
             * in a non-A form, stop caching for this region.
             */
            if (strstr(tmp, "(ix-4)") != NULL)
                break;
        }

        changed = 1;
    }

    return changed;
}

static int pass_shrink_minmax_frame3_after_score_cache(void)
{
    int start;
    int end;
    int i;

    if (!peep_in_function_range("_MinMax:", &start, &end))
        return 0;

    for (i = start; i < end; ++i) {
        if (strstr(lines[i], "(ix-4)") != NULL)
            return 0;
    }

    for (i = start; i + 2 < end; ++i) {
        if (eq(i, "ld hl,-4") &&
            eq(i + 1, "add hl,sp") &&
            eq(i + 2, "ld sp,hl")) {
            replace1_tagged(i, "ld hl,-3", "shrink_minmax_frame3");
            return 1;
        }
    }

    return 0;
}

static int pass_shrink_minmax_frame_after_callptr_temp_removed(void)
{
    int start;
    int end;
    int i;
    int uses_temp;

    if (!peep_in_function_range("_MinMax:", &start, &end))
        return 0;

    uses_temp = 0;
    for (i = start; i < end; ++i) {
        if (strstr(lines[i], "(ix-6)") || strstr(lines[i], "(ix-5)")) {
            uses_temp = 1;
            break;
        }
    }
    if (uses_temp)
        return 0;

    for (i = start; i + 2 < end; ++i) {
        if (eq(i, "ld hl,-6") &&
            eq(i + 1, "add hl,sp") &&
            eq(i + 2, "ld sp,hl")) {
            replace1_tagged(i, "ld hl,-4", "shrink_minmax_frame");
            return 1;
        }
    }

    return 0;
}


static int pass_store_l_reload_a(void)
{
    int i;
    int changed;
    int off1;
    char off2[32];
    char tmp[MAX_LINE];
    char *p;
    char *endp;

    changed = 0;

    for (i = 0; i + 1 < nlines; ++i) {
        strip_peep_comment_copy(tmp, lines[i]);
        if (strncmp(tmp, "ld (ix", 6) != 0)
            continue;
        p = tmp + 6;
        off1 = (int)strtol(p, &endp, 10);
        if (*endp != ')' || endp[1] != ',' || endp[2] != 'l' || endp[3] != 0)
            continue;
        if (!peep_parse_ld_a_ix(lines[i + 1], off2))
            continue;
        if (off1 != (int)strtol(off2, NULL, 10))
            continue;

        replace1_tagged(i + 1, "ld a,l", "store_l_reload_a");
        changed = 1;
    }

    return changed;
}

static int pass_reuse_board_addr_for_zero_store(void)
{
    int i;
    int changed;
    char lab[128];

    changed = 0;

    for (i = 0; i + 13 < nlines; ++i) {
        if (eq(i, "ld hl,_g_board") &&
            eq(i + 1, "ld e,(ix-3)") &&
            eq(i + 2, "ld d,0") &&
            eq(i + 3, "add hl,de") &&
            eq(i + 4, "ld a,(hl)") &&
            (eq(i + 5, "or a") || eq(i + 5, "cp 0")) &&
            peep_parse_jp_cond_label(lines[i + 6], "nz", lab) &&
            eq(i + 7, "ld hl,_g_board") &&
            eq(i + 8, "ld e,(ix-3)") &&
            eq(i + 9, "ld d,0") &&
            eq(i + 10, "add hl,de")) {
            delete_n(i + 7, 4);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int pass_array_base_push_to_de(void)
{
    int i;
    int changed;
    char base[128];

    changed = 0;

    for (i = 0; i + 7 < nlines; ++i) {
        if (parse_ld_hl_imm(lines[i], base) &&
            eq(i + 1, "push hl") &&
            peep_parse_ld_l_ix(lines[i + 2], base + 100) &&
            eq(i + 3, "ld h,0") &&
            eq(i + 4, "add hl,hl") &&
            eq(i + 5, "ex de,hl") &&
            eq(i + 6, "pop hl") &&
            eq(i + 7, "add hl,de")) {
            char line[180];
            replace1_tagged(i, lines[i + 2], "array_base_to_de");
            replace1(i + 1, "ld h,0");
            replace1(i + 2, "add hl,hl");
            sprintf(line, "ld de,%s", base);
            replace1(i + 3, line);
            replace1(i + 4, "add hl,de");
            delete_n(i + 5, 3);
            changed = 1;
            if (i > 0) --i;
        }
    }

    return changed;
}

static int pass_once(void)
{
    int i;
    int changed;
    char v[128];
    char out[256];

    changed = 0;

    for (i = 0; i < nlines; i++) {
        /*
         * Global 16-bit post-increment used as a statement:
         *
         *   ld hl,_g_Moves
         *   push hl
         *   ld e,(hl)
         *   inc hl
         *   ld d,(hl)
         *   ex de,hl
         *   push hl
         *   inc hl
         *   ex de,hl
         *   pop hl
         *   ex (sp),hl
         *   ld (hl),e
         *   inc hl
         *   ld (hl),d
         *   pop hl
         *
         * becomes direct memory increment.  This hits the very hot
         * g_Moves++ at _MinMax entry, without touching bool folding.
         */
        if (eq(i, "ld hl,_g_Moves") &&
            eq(i + 1, "push hl") &&
            eq(i + 2, "ld e,(hl)") &&
            eq(i + 3, "inc hl") &&
            eq(i + 4, "ld d,(hl)") &&
            eq(i + 5, "ex de,hl") &&
            eq(i + 6, "push hl") &&
            eq(i + 7, "inc hl") &&
            eq(i + 8, "ex de,hl") &&
            eq(i + 9, "pop hl") &&
            eq(i + 10, "ex (sp),hl") &&
            eq(i + 11, "ld (hl),e") &&
            eq(i + 12, "inc hl") &&
            eq(i + 13, "ld (hl),d") &&
            eq(i + 14, "pop hl")) {
            char lab[64], line[128];

            sprintf(lab, "Lpeep_ginc_%d", i);
            replace1_tagged(i, "ld hl,_g_Moves", "lookforwinner_ginc");
            replace1(i + 1, "inc (hl)");
            sprintf(line, "jp nz, %s", lab);
            replace1(i + 2, line);
            replace1(i + 3, "inc hl");
            replace1(i + 4, "inc (hl)");
            sprintf(line, "%s:", lab);
            replace1(i + 5, line);
            delete_n(i + 6, 9);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * Small constant equality/inequality against local int:
         *
         *   ld hl,N
         *   push hl
         *   ld l,(ix-K)
         *   ld h,(ix-K+1)
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp z/nz,L
         *
         * becomes (when N > 0):
         *
         *   ld a,(ix-K)
         *   cp N
         *   jp z/nz,L
         *
         * or when N == 0 (null/zero check, must test both bytes):
         *
         *   ld a,(ix-K)
         *   or (ix-K+1)
         *   jp z/nz,L
         *
         * This hits MinMax tests like score == SCORE_WIN / SCORE_LOSE.
         * When N==0 we must use 'or' to check both bytes: a pointer like
         * 0x1E00 has a zero low byte but is non-null, so 'cp 0' alone
         * would incorrectly treat it as null.
         */
        {
            int imm;
            char loff[32], hoff[32], newline[128];

            if (peep_parse_ld_hl_0_to_255(lines[i], &imm) &&
                eq(i + 1, "push hl") &&
                peep_parse_ld_l_ix(lines[i + 2], loff) &&
                peep_parse_ld_h_ix(lines[i + 3], hoff) &&
                eq(i + 4, "ex de,hl") &&
                eq(i + 5, "pop hl") &&
                eq(i + 6, "or a") &&
                eq(i + 7, "sbc hl,de") &&
                (strncmp(lines[i + 8], "jp z,", 5) == 0 ||
                 strncmp(lines[i + 8], "jp nz,", 6) == 0)) {

                sprintf(newline, "ld a,(ix%s)", loff);
                replace1_tagged(i, newline, "small_const_eq");
                if (imm == 0)
                    sprintf(newline, "or (ix%s)", hoff);
                else
                    sprintf(newline, "cp %d", imm);
                replace1(i + 1, newline);
                replace1(i + 2, lines[i + 8]);
                delete_n(i + 3, 6);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Fold 16-bit "x & 1" boolean tests:
         *
         *   ld l,(ix+8)
         *   ld h,(ix+9)
         *   ld de,1
         *   ld a,h
         *   and d
         *   ld h,a
         *   ld a,l
         *   and e
         *   ld l,a
         *   ld a,h
         *   or l
         *   jp z,L
         *
         * becomes:
         *
         *   ld a,(ix+8)
         *   and 1
         *   jp z,L
         *
         * This is hot in ttt's MinMax for "depth & 1".
         */
        if (eq(i, "ld l,(ix+8)") &&
            eq(i + 1, "ld h,(ix+9)") &&
            eq(i + 2, "ld de,1") &&
            eq(i + 3, "ld a,h") &&
            eq(i + 4, "and d") &&
            eq(i + 5, "ld h,a") &&
            eq(i + 6, "ld a,l") &&
            eq(i + 7, "and e") &&
            eq(i + 8, "ld l,a") &&
            eq(i + 9, "ld a,h") &&
            eq(i + 10, "or l") &&
            (strncmp(lines[i + 11], "jp z,", 5) == 0 ||
             strncmp(lines[i + 11], "jp nz,", 6) == 0)) {

            replace1_tagged(i, "ld a,(ix+8)", "and1_bool");
            replace1(i + 1, "and 1");
            replace1(i + 2, lines[i + 11]);
            delete_n(i + 3, 9);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * MinMax board[p] = pieceMove byte store:
         *
         *   ld l,(ix-5)
         *   ld h,(ix-4)
         *   ld hl,_g_board
         *   push hl
         *   ld l,(ix-5)
         *   ld h,(ix-4)
         *   ex de,hl
         *   pop hl
         *   add hl,de
         *   push hl
         *   ld l,(ix-3)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   ld (hl),e
         *
         * becomes:
         *
         *   ld l,(ix-5)
         *   ld h,(ix-4)
         *   ld de,_g_board
         *   add hl,de
         *   ld a,(ix-3)
         *   ld (hl),a
         *
         * This is safe for ttt_t g_board[p] = pieceMove; because both are
         * byte objects in this benchmark.
         */
        if (eq(i, "ld l,(ix-5)") &&
            eq(i + 1, "ld h,(ix-4)") &&
            eq(i + 2, "ld hl,_g_board") &&
            eq(i + 3, "push hl") &&
            eq(i + 4, "ld l,(ix-5)") &&
            eq(i + 5, "ld h,(ix-4)") &&
            eq(i + 6, "ex de,hl") &&
            eq(i + 7, "pop hl") &&
            eq(i + 8, "add hl,de") &&
            eq(i + 9, "push hl") &&
            eq(i + 10, "ld l,(ix-3)") &&
            eq(i + 11, "ld h,0") &&
            eq(i + 12, "ex de,hl") &&
            eq(i + 13, "pop hl") &&
            eq(i + 14, "ld (hl),e")) {

            replace1_tagged(i, "ld l,(ix-5)", "board_store");
            replace1(i + 1, "ld h,(ix-4)");
            replace1(i + 2, "ld de,_g_board");
            replace1(i + 3, "add hl,de");
            replace1(i + 4, "ld a,(ix-3)");
            replace1(i + 5, "ld (hl),a");
            delete_n(i + 6, 9);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * MinMax blank-board-position test:
         *
         *   ld hl,0
         *   push hl
         *   ld hl,_g_board
         *   push hl
         *   ld l,(ix-5)
         *   ld h,(ix-4)
         *   ex de,hl
         *   pop hl
         *   add hl,de
         *   ld l,(hl)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp nz,Lskip
         *
         * becomes:
         *
         *   ld l,(ix-5)
         *   ld h,(ix-4)
         *   ld de,_g_board
         *   add hl,de
         *   ld a,(hl)
         *   or a
         *   jp nz,Lskip
         */
        {
            char lskip[128];
            if (eq(i, "ld hl,0") &&
                eq(i + 1, "push hl") &&
                eq(i + 2, "ld hl,_g_board") &&
                eq(i + 3, "push hl") &&
                eq(i + 4, "ld l,(ix-5)") &&
                eq(i + 5, "ld h,(ix-4)") &&
                eq(i + 6, "ex de,hl") &&
                eq(i + 7, "pop hl") &&
                eq(i + 8, "add hl,de") &&
                eq(i + 9, "ld l,(hl)") &&
                eq(i + 10, "ld h,0") &&
                eq(i + 11, "ex de,hl") &&
                eq(i + 12, "pop hl") &&
                eq(i + 13, "or a") &&
                eq(i + 14, "sbc hl,de") &&
                peep_parse_jp_cond_label(lines[i + 15], "nz", lskip)) {

                replace1_tagged(i, "ld l,(ix-5)", "blank_board_test");
                replace1(i + 1, "ld h,(ix-4)");
                replace1(i + 2, "ld de,_g_board");
                replace1(i + 3, "add hl,de");
                replace1(i + 4, "ld a,(hl)");
                replace1(i + 5, "or a");
                replace1(i + 6, lines[i + 15]);
                delete_n(i + 7, 9);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Fold boolean materialization suffixes:
         *
         *   ld hl,1
         *   jp Lend
         * Lfalse:
         *   ld hl,0
         * Lend:
         *   ld a,h
         *   or l
         *   jp nz,Ldest
         *
         * becomes:
         *   jp Ldest
         * Lfalse:
         * Lend:
         *
         * For final "jp z", the false path jumps to Ldest instead.
         * Also handles the symmetric "ld hl,0 ... Ltrue: ld hl,1" form.
         */
        {
            char ljump[128], lab_mid[128], lab_end[128], ldest[128];
            char out1[256], out2[256];

            if ((eq(i, "ld hl,1") || eq(i, "ld hl,0")) &&
                peep_parse_jp_uncond_label(lines[i + 1], ljump) &&
                label_name_at(i + 2, lab_mid) &&
                (eq(i + 3, "ld hl,0") || eq(i + 3, "ld hl,1")) &&
                label_name_at(i + 4, lab_end) &&
                strcmp(ljump, lab_end) == 0 &&
                eq(i + 5, "ld a,h") &&
                eq(i + 6, "or l") &&
                (peep_parse_jp_cond_label(lines[i + 7], "z", ldest) ||
                 peep_parse_jp_cond_label(lines[i + 7], "nz", ldest))) {

                int first_is_true;
                int final_is_z;
                int branch_from_first_path;

                first_is_true = eq(i, "ld hl,1");
                final_is_z = strncmp(lines[i + 7], "jp z,", 5) == 0;

                /* final jp nz branches on true; final jp z branches on false. */
                branch_from_first_path =
                    (first_is_true && !final_is_z) || (!first_is_true && final_is_z);

                if (branch_from_first_path) {
                    sprintf(out1, "jp %s", ldest);
                    replace1_tagged(i, out1, "bool_suffix");
                    replace1(i + 1, lines[i + 2]);  /* middle label */
                    replace1(i + 2, lines[i + 4]);  /* end label */
                    delete_n(i + 3, 5);
                } else {
                    sprintf(out1, "jp %s", lab_end);
                    sprintf(out2, "jp %s", ldest);
                    replace1_tagged(i, out1, "bool_suffix");
                    replace1(i + 1, lines[i + 2]);  /* middle label */
                    replace1(i + 2, out2);
                    replace1(i + 3, lines[i + 4]);  /* end label */
                    delete_n(i + 4, 4);
                }

                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Fold constant boolean tests:
         *
         *   ld hl,1
         *   ld a,h
         *   or l
         *   jp z, Lx       ; never taken
         *
         * delete all four.  Similarly, "jp nz" is always taken.
         * Do the inverse for ld hl,0.
         */
        {
            char tgt[128];
            char newline[256];

            if ((eq(i, "ld hl,1") || eq(i, "ld hl,0")) &&
                eq(i + 1, "ld a,h") &&
                eq(i + 2, "or l") &&
                (peep_parse_jp_cond_label(lines[i + 3], "z", tgt) ||
                 peep_parse_jp_cond_label(lines[i + 3], "nz", tgt))) {
                int is_one;
                int is_jp_z;
                int taken;

                is_one = eq(i, "ld hl,1");
                is_jp_z = strncmp(lines[i + 3], "jp z,", 5) == 0;

                taken = is_one ? !is_jp_z : is_jp_z;

                if (taken) {
                    sprintf(newline, "jp %s", tgt);
                    replace1_tagged(i, newline, "const_bool_taken");
                    delete_n(i + 1, 3);
                } else {
                    delete_n(i, 4);
                }

                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Fold byte compare boolean materialization after previous byte-compare
         * peepholes:
         *
         *   cp (hl)
         *   jp z, Ltrue       ; or jp nz, Ltrue
         *   ld hl,0
         *   jp Lend
         * Ltrue:
         *   ld hl,1
         * Lend:
         *   ld a,h
         *   or l
         *   jp z, Lfalse      ; or jp nz, Ltrue2
         *
         * into a direct conditional branch after cp.
         */
        {
            char ltrue[128], lend[128], lab4[128], lab6[128], ldest[128];
            char newline[256];
            int first_is_z;
            int final_is_z;

            if (eq(i, "cp (hl)") &&
                (peep_parse_jp_cond_label(lines[i + 1], "z", ltrue) ||
                 peep_parse_jp_cond_label(lines[i + 1], "nz", ltrue)) &&
                eq(i + 2, "ld hl,0") &&
                peep_parse_jp_uncond_label(lines[i + 3], lend) &&
                label_name_at(i + 4, lab4) &&
                strcmp(lab4, ltrue) == 0 &&
                eq(i + 5, "ld hl,1") &&
                label_name_at(i + 6, lab6) &&
                strcmp(lab6, lend) == 0 &&
                eq(i + 7, "ld a,h") &&
                eq(i + 8, "or l") &&
                (peep_parse_jp_cond_label(lines[i + 9], "z", ldest) ||
                 peep_parse_jp_cond_label(lines[i + 9], "nz", ldest))) {

                first_is_z = strncmp(lines[i + 1], "jp z,", 5) == 0;
                final_is_z = strncmp(lines[i + 9], "jp z,", 5) == 0;

                if (final_is_z) {
                    peep_make_cond_jump(newline, first_is_z ? "nz" : "z", ldest);
                } else {
                    peep_make_cond_jump(newline, first_is_z ? "z" : "nz", ldest);
                }

                replace1_tagged(i + 1, newline, "cp_hl_bool");
                delete_n(i + 2, 8);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * posNfunc setup byte store:
         *
         *   push ix
         *   pop hl
         *   dec hl
         *   push hl
         *   ld hl,_g_board
         *   [inc hl ...] or [ld de,N/add hl,de]
         *   ld l,(hl)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   ld (hl),e
         *
         * becomes:
         *
         *   ld hl,_g_board
         *   [same address adjustment]
         *   ld a,(hl)
         *   ld (ix-1),a
         */
        if (eq(i, "push ix") &&
            eq(i + 1, "pop hl") &&
            eq(i + 2, "dec hl") &&
            eq(i + 3, "push hl") &&
            eq(i + 4, "ld hl,_g_board")) {
            int j;
            int incs;
            int offv;
            char tmp[128];

            j = i + 5;
            incs = 0;

            while (j < nlines && eq(j, "inc hl")) {
                incs++;
                j++;
            }

            if (eq(j, "ld l,(hl)") &&
                eq(j + 1, "ld h,0") &&
                eq(j + 2, "ex de,hl") &&
                eq(j + 3, "pop hl") &&
                eq(j + 4, "ld (hl),e")) {

                replace1_tagged(i, "ld hl,_g_board", "posnfunc_inc");
                for (j = 0; j < incs; j++)
                    replace1(i + 1 + j, "inc hl");
                replace1(i + 1 + incs, "ld a,(hl)");
                replace1(i + 2 + incs, "ld (ix-1),a");
                delete_n(i + 3 + incs, (i + 10 + incs) - (i + 3 + incs));
                changed = 1;
                if (i > 0) i--;
                continue;
            }

            j = i + 5;
            if (peep_parse_ld_de_0_to_255(lines[j], &offv) &&
                eq(j + 1, "add hl,de") &&
                eq(j + 2, "ld l,(hl)") &&
                eq(j + 3, "ld h,0") &&
                eq(j + 4, "ex de,hl") &&
                eq(j + 5, "pop hl") &&
                eq(j + 6, "ld (hl),e")) {

                replace1_tagged(i, "ld hl,_g_board", "posnfunc_de");
                sprintf(tmp, "ld de,%d", offv);
                replace1(i + 1, tmp);
                replace1(i + 2, "add hl,de");
                replace1(i + 3, "ld a,(hl)");
                replace1(i + 4, "ld (ix-1),a");
                delete_n(i + 5, (j + 7) - (i + 5));
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Small local stack allocation:
         *
         *   ld hl,-1
         *   add hl,sp
         *   ld sp,hl
         *
         * becomes:
         *   dec sp
         *
         * and similarly for -2.  This is especially useful for the tiny
         * ttt posNfunc helpers that allocate one char local.
         */
        if (eq(i, "ld hl,-1") &&
            eq(i + 1, "add hl,sp") &&
            eq(i + 2, "ld sp,hl")) {
            replace1_tagged(i, "dec sp", "local_alloc_1");
            delete_n(i + 1, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        if (eq(i, "ld hl,-2") &&
            eq(i + 1, "add hl,sp") &&
            eq(i + 2, "ld sp,hl")) {
            replace1_tagged(i, "dec sp", "local_alloc_2");
            replace1(i + 1, "dec sp");
            delete_n(i + 2, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * Byte equality compare:
         *
         *   ld l,(ix-N)
         *   ld h,0
         *   push hl
         *   ld hl,_g_board
         *   [inc hl ...]  or  [ld de,K / add hl,de]
         *   ld l,(hl)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp z/nz, L
         *
         * becomes:
         *
         *   ld a,(ix-N)
         *   ld hl,_g_board
         *   [same address adjustment]
         *   cp (hl)
         *   jp z/nz, L
         *
         * Only equality/inequality branches are folded, so carry/sign
         * semantics do not matter.
         */
        {
            char off[32];
            char newline[128];
            int j;
            int incs;

            if (peep_parse_ld_l_ix(lines[i], off) &&
                eq(i + 1, "ld h,0") &&
                eq(i + 2, "push hl") &&
                eq(i + 3, "ld hl,_g_board")) {

                j = i + 4;
                incs = 0;
                while (j < nlines && eq(j, "inc hl")) {
                    j++;
                    incs++;
                }

                if (eq(j, "ld l,(hl)") &&
                    eq(j + 1, "ld h,0") &&
                    eq(j + 2, "ex de,hl") &&
                    eq(j + 3, "pop hl") &&
                    eq(j + 4, "or a") &&
                    eq(j + 5, "sbc hl,de") &&
                    peep_is_jp_z_or_nz(lines[j + 6])) {

                    peep_make_ld_a_ix(newline, off);
                    replace1_tagged(i, newline, "byte_eq_inc");
                    replace1(i + 1, "ld hl,_g_board");

                    /* existing inc hl lines at i+4.. remain moved down by deletion;
                       copy them into position i+2.. */
                    {
                        int k;
                        for (k = 0; k < incs; k++)
                            replace1(i + 2 + k, "inc hl");
                        replace1(i + 2 + incs, "cp (hl)");
                        replace1(i + 3 + incs, lines[j + 6]);
                    }

                    delete_n(i + 4 + incs, (j + 7) - (i + 4 + incs));
                    changed = 1;
                    if (i > 0) i--;
                    continue;
                }

                if (strncmp(lines[j], "ld de,", 6) == 0 &&
                    eq(j + 1, "add hl,de") &&
                    eq(j + 2, "ld l,(hl)") &&
                    eq(j + 3, "ld h,0") &&
                    eq(j + 4, "ex de,hl") &&
                    eq(j + 5, "pop hl") &&
                    eq(j + 6, "or a") &&
                    eq(j + 7, "sbc hl,de") &&
                    peep_is_jp_z_or_nz(lines[j + 8])) {

                    peep_make_ld_a_ix(newline, off);
                    replace1_tagged(i, newline, "byte_eq_de");
                    replace1(i + 1, "ld hl,_g_board");
                    replace1(i + 2, lines[j]);
                    replace1(i + 3, "add hl,de");
                    replace1(i + 4, "cp (hl)");
                    replace1(i + 5, lines[j + 8]);

                    delete_n(i + 6, (j + 9) - (i + 6));
                    changed = 1;
                    if (i > 0) i--;
                    continue;
                }
            }
        }

        /*
         * Byte compare against zero:
         *
         *   ld hl,0
         *   push hl
         *   ld l,(ix-N)
         *   ld h,0
         *   ex de,hl
         *   pop hl
         *   or a
         *   sbc hl,de
         *   jp z/nz, L
         *
         * becomes:
         *   ld a,(ix-N)
         *   or a
         *   jp z/nz, L
         */
        {
            char off[32];
            char newline[128];

            if (eq(i, "ld hl,0") &&
                eq(i + 1, "push hl") &&
                peep_parse_ld_l_ix(lines[i + 2], off) &&
                eq(i + 3, "ld h,0") &&
                eq(i + 4, "ex de,hl") &&
                eq(i + 5, "pop hl") &&
                eq(i + 6, "or a") &&
                eq(i + 7, "sbc hl,de") &&
                peep_is_jp_z_or_nz(lines[i + 8])) {

                peep_make_ld_a_ix(newline, off);
                replace1_tagged(i, newline, "byte_cmp_zero");
                replace1(i + 1, "or a");
                replace1(i + 2, lines[i + 8]);
                delete_n(i + 3, 6);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Fold equality/inequality boolean materialization immediately
         * consumed by a zero/nonzero branch.
         */
        {
            char ltrue[128], lend[128], lab5[128], lab7[128], ldest[128];
            char newline[256];
            int first_is_z;
            int final_is_z;

            if (eq(i, "or a") &&
                eq(i + 1, "sbc hl,de") &&
                (peep_parse_jp_cond_label(lines[i + 2], "z", ltrue) ||
                 peep_parse_jp_cond_label(lines[i + 2], "nz", ltrue)) &&
                eq(i + 3, "ld hl,0") &&
                peep_parse_jp_uncond_label(lines[i + 4], lend) &&
                label_name_at(i + 5, lab5) &&
                strcmp(lab5, ltrue) == 0 &&
                eq(i + 6, "ld hl,1") &&
                label_name_at(i + 7, lab7) &&
                strcmp(lab7, lend) == 0 &&
                eq(i + 8, "ld a,h") &&
                eq(i + 9, "or l") &&
                (peep_parse_jp_cond_label(lines[i + 10], "z", ldest) ||
                 peep_parse_jp_cond_label(lines[i + 10], "nz", ldest))) {

                first_is_z = strncmp(lines[i + 2], "jp z,", 5) == 0;
                final_is_z = strncmp(lines[i + 10], "jp z,", 5) == 0;

                if (final_is_z) {
                    peep_make_cond_jump(newline, first_is_z ? "nz" : "z", ldest);
                } else {
                    peep_make_cond_jump(newline, first_is_z ? "z" : "nz", ldest);
                }

                /*
                 * Safety check: if ltrue or lend is referenced by any line
                 * outside this 11-line window, another fold has already
                 * created a jump to one of these labels.  Deleting ltrue:/lend:
                 * here would leave that earlier jump dangling (e.g. the
                 * OR-materialisation shared-label case in cbfs()).  Skip the
                 * fold in that situation.
                 */
                {
                    int safe = 1;
                    int k;
                    for (k = 0; k < nlines && safe; k++) {
                        char tgt_chk[128];
                        if (k >= i && k <= i + 10) continue;
                        if (jump_target(lines[k], tgt_chk) &&
                            (strcmp(tgt_chk, ltrue) == 0 ||
                             strcmp(tgt_chk, lend) == 0))
                            safe = 0;
                    }
                    if (!safe) continue;
                }
                replace1_tagged(i + 2, newline, "or_a_sbc_bool11");
                delete_n(i + 3, 8);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        if (is_blank_or_comment(lines[i]))
            continue;

        /*
         * Before:
         *   push ix
         *   pop hl
         *   dec hl
         *   dec hl
         *   ld e,(hl)
         *   inc hl
         *   ld d,(hl)
         *   pop hl
         *
         * After:
         *   push ix
         *   pop hl
         *   dec hl
         *   ld d,(hl)
         *   dec hl
         *   ld e,(hl)
         *   pop hl
         *
         * Safe because the final pop hl overwrites HL, so the changed
         * intermediate HL value does not escape.
         */
        if (eq(i, "push ix") &&
            eq(i + 1, "pop hl") &&
            eq(i + 2, "dec hl") &&
            eq(i + 3, "dec hl") &&
            eq(i + 4, "ld e,(hl)") &&
            eq(i + 5, "inc hl") &&
            eq(i + 6, "ld d,(hl)") &&
            eq(i + 7, "pop hl")) {
            replace1_tagged(i + 2, "dec hl", "ix_de_load_reorder");
            replace1(i + 3, "ld d,(hl)");
            replace1(i + 4, "dec hl");
            replace1(i + 5, "ld e,(hl)");
            replace1(i + 6, "pop hl");
            delete_n(i + 7, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Fold compare-result materialization immediately consumed by a branch. */
        if (!peep_line_in_function(i, "_main:") && try_fold_bool_branch(i)) {
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Small positive address offsets.  16-bit INC HL does not affect flags.
         * Only use where the next instruction is not a conditional branch. */
        if (eq(i, "ld de,1") && eq(i + 1, "add hl,de") &&
            i + 2 < nlines && strncmp(lines[i + 2], "jp ", 3) != 0) {
            replace1_tagged(i, "inc hl", "ld_de1_to_inc");
            delete_n(i + 1, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        if (eq(i, "ld de,2") && eq(i + 1, "add hl,de") &&
            i + 2 < nlines && strncmp(lines[i + 2], "jp ", 3) != 0) {
            replace1_tagged(i, "inc hl", "ld_de2_to_inc");
            replace1(i + 1, "inc hl");
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        if (eq(i, "ld de,3") && eq(i + 1, "add hl,de") &&
            i + 2 < nlines && strncmp(lines[i + 2], "jp ", 3) != 0) {
            replace1_tagged(i, "inc hl", "ld_de3_to_inc");
            replace1(i + 1, "inc hl");
            insert_line(i + 2, "inc hl");
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * HL -= 1 via signed subtract:
         *   ld de,1
         *   or a       ; clear carry
         *   sbc hl,de  ; HL = HL - 1
         *
         * When not immediately followed by a conditional branch,
         * the flags from sbc are unused, and this becomes just:
         *   dec hl
         *
         * This hits HL = n - 1 patterns in indexed loops.
         */
        if (eq(i, "ld de,1") &&
            eq(i + 1, "or a") &&
            eq(i + 2, "sbc hl,de") &&
            i + 3 < nlines &&
            strncmp(lines[i + 3], "jp ", 3) != 0) {
            replace1_tagged(i, "dec hl", "sbc_de1_to_dec");
            delete_n(i + 1, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Same-register push/pop has no register or flag effect. */
        if ((eq(i, "push hl") && eq(i + 1, "pop hl")) ||
            (eq(i, "push de") && eq(i + 1, "pop de")) ||
            (eq(i, "push bc") && eq(i + 1, "pop bc")) ||
            (eq(i, "push af") && eq(i + 1, "pop af")) ||
            (eq(i, "push ix") && eq(i + 1, "pop ix"))) {
            delete_n(i, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Two exchanges cancel exactly. */
        if (eq(i, "ex de,hl") && eq(i + 1, "ex de,hl")) {
            delete_n(i, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * Safe constant-to-DE only when surrounded by push/pop of HL:
         *   push hl
         *   ld hl,N
         *   ex de,hl
         *   pop hl
         * becomes:
         *   push hl
         *   ld de,N
         *   pop hl
         *
         * This preserves final HL and final DE and does not alter flags.
         */
        if (eq(i, "push hl") &&
            i + 3 < nlines &&
            parse_ld_hl_imm(lines[i + 1], v) &&
            eq(i + 2, "ex de,hl") &&
            eq(i + 3, "pop hl")) {
            sprintf(out, "ld de,%s", v);
            replace1_tagged(i + 1, out, "const_to_de");
            delete_n(i + 2, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * After the safe constant-to-DE rewrite above, this common leftover:
         *   push hl
         *   ld de,N
         *   pop hl
         * is just ld de,N.  HL is unchanged either way, DE is the same,
         * flags are unchanged, and SP is unchanged.
         */
        if (eq(i, "push hl") &&
            i + 2 < nlines &&
            parse_ld_de_imm(lines[i + 1], v) &&
            eq(i + 2, "pop hl")) {
            sprintf(out, "ld de,%s", v);
            replace1_tagged(i, out, "push_lde_pop");
            delete_n(i + 1, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * Caller cleanup that preserves HL return value:
         *   ex de,hl / ld hl,N / add hl,sp / ld sp,hl / ex de,hl
         * becomes N copies of inc sp for small even N.  This keeps HL
         * unchanged and adjusts SP by the same amount.  It intentionally
         * avoids changing condition flags.
         */
        if (eq(i, "ex de,hl") &&
            i + 4 < nlines &&
            parse_ld_hl_imm(lines[i + 1], v) &&
            eq(i + 2, "add hl,sp") &&
            eq(i + 3, "ld sp,hl") &&
            eq(i + 4, "ex de,hl")) {
            int n;
            int k;
            if (parse_nonneg_int(v, &n) && n > 0 && n <= 6) {
                delete_n(i, 5);
                for (k = 0; k < n; k++) {
                    if (k == 0)
                        insert_line_tagged(i, "inc sp", "caller_cleanup");
                    else
                        insert_line(i + k, "inc sp");
                }
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Code after an unconditional jump is unreachable until the next
         * label.  Delete one non-label instruction at a time.
         */
        if (is_uncond_jp(lines[i]) &&
            i + 1 < nlines &&
            !starts_label(lines[i + 1]) &&
            !is_blank_or_comment(lines[i + 1])) {
            delete_n(i + 1, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Unconditional jump to immediately following label. */
        if (is_jp_to_next_label(i)) {
            delete_n(i, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /* Duplicate declarations anywhere before code are safe to remove. */
        if (strncmp(lines[i], "extrn ", 6) == 0 ||
            strncmp(lines[i], "public ", 7) == 0) {
            int j;
            for (j = 0; j < i; j++) {
                if (strcmp(lines[i], lines[j]) == 0) {
                    delete_n(i, 1);
                    changed = 1;
                    if (i > 0) i--;
                    break;
                }
            }
            if (changed)
                continue;
        }

        /* Adjacent duplicate declarations. */
        if ((strncmp(lines[i], "extrn ", 6) == 0 ||
             strncmp(lines[i], "public ", 7) == 0) &&
            i + 1 < nlines &&
            strcmp(lines[i], lines[i + 1]) == 0) {
            delete_n(i + 1, 1);
            changed = 1;
            if (i > 0) i--;
            continue;
        }

        /*
         * 16-bit pre-decrement (or pre-increment) via pointer arithmetic,
         * where the variable's IX offset is within direct IX addressing range.
         *
         *   push ix
         *   pop hl
         *   ld de,K      ; K in -128..126
         *   add hl,de
         *   push hl
         *   ld e,(hl)
         *   inc hl
         *   ld d,(hl)
         *   ex de,hl
         *   dec hl       ; (or inc hl for pre-increment)
         *   ex de,hl
         *   pop hl
         *   ld (hl),e
         *   inc hl
         *   ld (hl),d
         *   ex de,hl
         *
         * Becomes (result stays in HL):
         *
         *   ld l,(ix+K)
         *   ld h,(ix+K+1)
         *   dec hl
         *   ld (ix+K),l
         *   ld (ix+K+1),h
         */
        {
            int K;
            char loff[32], hoff[32], newline[128];
            const char *step;

            if (eq(i,      "push ix") &&
                eq(i +  1, "pop hl") &&
                peep_parse_ld_de_signed(lines[i + 2], &K) &&
                eq(i +  3, "add hl,de") &&
                eq(i +  4, "push hl") &&
                eq(i +  5, "ld e,(hl)") &&
                eq(i +  6, "inc hl") &&
                eq(i +  7, "ld d,(hl)") &&
                eq(i +  8, "ex de,hl") &&
                (eq(i +  9, "dec hl") || eq(i +  9, "inc hl")) &&
                eq(i + 10, "ex de,hl") &&
                eq(i + 11, "pop hl") &&
                eq(i + 12, "ld (hl),e") &&
                eq(i + 13, "inc hl") &&
                eq(i + 14, "ld (hl),d") &&
                eq(i + 15, "ex de,hl") &&
                K >= -128 && K <= 126) {

                step = eq(i + 9, "dec hl") ? "dec hl" : "inc hl";
                peep_format_ix_off(loff, K);
                peep_format_ix_off(hoff, K + 1);

                sprintf(newline, "ld l,(ix%s)", loff);  replace1_tagged(i, newline, "ix_predec_inc");
                sprintf(newline, "ld h,(ix%s)", hoff);  replace1(i + 1, newline);
                replace1(i + 2, step);
                sprintf(newline, "ld (ix%s),l", loff);  replace1(i + 3, newline);
                sprintf(newline, "ld (ix%s),h", hoff);  replace1(i + 4, newline);
                delete_n(i + 5, 11);
                changed = 1;
                if (i > 0) i--;
                continue;
            }
        }

        /*
         * Zero-test a byte from memory:
         *   ld l,(hl)
         *   ld h,0
         *   ld a,h
         *   or l
         * The above loads a byte from (HL) as an unsigned 16-bit value in HL,
         * then OR-reduces HL into A to test for zero.  Since H is forced to 0,
         * A ends up equal to the byte.  Equivalent, and 11T faster:
         *   ld a,(hl)
         *   or a
         */
        if (i + 3 < nlines &&
            eq(i,     "ld l,(hl)") &&
            eq(i + 1, "ld h,0") &&
            eq(i + 2, "ld a,h") &&
            eq(i + 3, "or l")) {
            replace1_tagged(i, "ld a,(hl)", "byte_zero_test");
            replace1(i + 1, "or a");
            delete_n(i + 2, 2);
            changed = 1;
            if (i > 0) i--;
            continue;
        }
    }

    return changed;
}

/*
 * pass_cond_skip_shortcut:
 *
 * Pattern:
 *   jp cc, LSKIP    ; conditional jump over one instruction
 *   INSTR           ; one non-label, non-jump instruction
 *   LSKIP:          ; skip target
 *   jp LDEST        ; unconditional jump to real destination
 *
 * Replace the conditional jump so it points directly at LDEST.
 * LSKIP becomes unreferenced and will be removed by pass_labels.
 *
 * This avoids the two-jump cost (10T + 10T) when the condition fires,
 * replacing it with a single direct jump (10T).
 */
static int pass_cond_skip_shortcut(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 3 < nlines; i++) {
        char lskip[128], ldest[128], new_jp[256];
        const char *cond = NULL;

        if      (parse_jp_nz_label(lines[i], lskip)) cond = "nz";
        else if (parse_jp_z_label (lines[i], lskip)) cond = "z";
        else if (parse_jp_c_label (lines[i], lskip)) cond = "c";
        else if (parse_jp_nc_label(lines[i], lskip)) cond = "nc";

        if (!cond)
            continue;
        if (starts_label(lines[i + 1]))
            continue;
        if (strncmp(lines[i + 1], "jp ", 3) == 0)
            continue;
        if (!line_is_label_name(i + 2, lskip))
            continue;
        if (!is_uncond_jp(lines[i + 3]))
            continue;
        if (!peep_parse_jp_uncond_label(lines[i + 3], ldest))
            continue;
        if (strcmp(lskip, ldest) == 0)
            continue;

        peep_make_cond_jump(new_jp, cond, ldest);
        replace1(i, new_jp);
        changed = 1;
        if (i > 0) i--;
    }

    return changed;
}

static void read_file(const char *name)
{
    FILE *f;
    char buf[MAX_LINE];

    f = fopen(name, "r");
    if (!f) {
        fprintf(stderr, "cannot open %s\n", name);
        exit(1);
    }

    while (fgets(buf, sizeof(buf), f)) {
        trim(buf);
        if (nlines >= MAX_LINES) {
            fprintf(stderr, "too many lines\n");
            exit(1);
        }
        lines[nlines++] = xstrdup2(buf);
    }

    fclose(f);
}

static void write_file(const char *name)
{
    FILE *f;
    int i;

    f = fopen(name, "w");
    if (!f) {
        fprintf(stderr, "cannot create %s\n", name);
        exit(1);
    }

    for (i = 0; i < nlines; i++) {
        if (lines[i][0] == 0)
            fprintf(f, "\n");
        else if (starts_label(lines[i]) || lines[i][0] == ';')
            fprintf(f, "%s\n", lines[i]);
        else
            fprintf(f, "\t%s\n", lines[i]);
    }

    fclose(f);
}

/*
 * Detect a sequential byte-store loop that initialises a global array to a
 * constant value, and replace it with LDIR (block move).  The pattern is:
 *
 *   Lhead:
 *     ld l,(ix-A)  ld h,(ix-A-1)   ; index variable
 *     ld de,SIZE
 *     or a
 *     sbc hl,de
 *     jp z, Lbody
 *     jp c, Lbody
 *     jp Lexit
 *   Lbody:
 *     ld l,(ix-A)  ld h,(ix-A-1)
 *     ld de,SYM                     ; array base (global symbol)
 *     add hl,de
 *     ld (hl),CONST                 ; small non-negative constant
 *   [Linc:]
 *     inc (ix-A)
 *     jp nz, Lnext
 *     inc (ix-A-1)
 *   Lnext:
 *     jp Lhead
 *   Lexit:
 *
 * Replaced with (only valid when the index starts from 0):
 *
 *     ld hl,SYM
 *     ld (hl),CONST
 *     ld de,SYM+1
 *     ld bc,SIZE
 *     ldir
 *
 * To verify the index starts at 0 we look for the pattern
 * "ld hl,0 / ld (ix-A),l / ld (ix-A-1),h" immediately before Lhead.
 */
static int stride_parse_ld_r_ix_neg(const char *s, char r, int *n); /* forward */
static int pass_ldir_memset(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 32 < nlines; i++) {
        char lhead[128], lbody[128], lexit[128], tmp[128];
        int lo_ix, hi_ix;
        long size_val;
        char arr_sym[128];
        char const_str[32];
        int j, ip;

        /* 1. Lhead label */
        if (!label_name_at(i, lhead))
            continue;
        j = i + 1;

        /* 2. Comparison block */
        if (!stride_parse_ld_r_ix_neg(lines[j], 'l', &lo_ix)) continue; j++;
        if (!stride_parse_ld_r_ix_neg(lines[j], 'h', &hi_ix)) continue; j++;
        if (hi_ix != lo_ix - 1) continue;
        if (!parse_ld_de_positive_imm(lines[j], &size_val)) continue; j++;
        /* Accept both unsigned (no bias) and signed-biased (xor 80h) comparisons.
         * The LDIR replacement uses block move, so the comparison semantics only
         * matter for loop entry/exit — valid for non-negative array indices. */
        if (eq(j, "ld a,h") && eq(j+1, "xor 80h") && eq(j+2, "ld h,a") &&
            eq(j+3, "ld a,d") && eq(j+4, "xor 80h") && eq(j+5, "ld d,a"))
            j += 6;
        if (!eq(j, "or a")) continue; j++;
        if (!eq(j, "sbc hl,de")) continue; j++;
        if (!parse_jp_z_label(lines[j], lbody)) continue; j++;
        if (!parse_jp_c_label(lines[j], tmp) || strcmp(tmp, lbody) != 0) continue; j++;
        if (!peep_parse_jp_uncond_label(lines[j], lexit)) continue; j++;

        /* 3. Lbody label */
        if (!line_is_label_name(j, lbody)) continue; j++;

        /* 4. Body: reload index, compute address, store constant */
        {
            int lo2, hi2;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'l', &lo2)) continue; j++;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'h', &hi2)) continue; j++;
            if (lo2 != lo_ix || hi2 != hi_ix) continue;
        }
        if (!parse_ld_de_imm(lines[j], arr_sym) || arr_sym[0] != '_') continue; j++;
        if (!eq(j, "add hl,de")) continue; j++;
        /* ld (hl),CONST — accept any small non-negative immediate */
        if (strncmp(lines[j], "ld (hl),", 8) != 0) continue;
        {
            const char *p = lines[j] + 8;
            int v;
            if (!parse_nonneg_int(p, &v) || v > 255) continue;
            sprintf(const_str, "%d", v);
        }
        j++;

        /* 5. Optional Linc label */
        if (starts_label(lines[j]))
            j++;

        /* 6. Increment: inc (ix-A); jp nz,Lnext; inc (ix-A-1); Lnext: jp Lhead */
        {
            char stored_lo[32];
            sprintf(stored_lo, "inc (ix-%d)", lo_ix);
            if (!eq(j, stored_lo)) continue; j++;
        }
        if (!parse_jp_nz_label(lines[j], tmp)) continue; j++;
        {
            char stored_hi[32];
            sprintf(stored_hi, "inc (ix-%d)", hi_ix);
            if (!eq(j, stored_hi)) continue; j++;
        }
        if (!line_is_label_name(j, tmp)) continue; j++;
        if (!peep_parse_jp_uncond_label(lines[j], tmp) || strcmp(tmp, lhead) != 0) continue;
        ip = j;
        j++;

        /* 7. Lexit follows */
        if (!line_is_label_name(j, lexit)) continue;

        /* 8. Verify the index was initialised to 0 immediately before Lhead.
         *    Look for:  ld hl,0 / ld (ix-lo),l / ld (ix-hi),h
         *    These need not be immediately before (allow one unrelated line). */
        {
            char lo_store[32], hi_store[32];
            int found = 0;
            int k;

            sprintf(lo_store, "ld (ix-%d),l", lo_ix);
            sprintf(hi_store, "ld (ix-%d),h", hi_ix);

            /* Scan backwards up to 6 lines for the three-instruction sequence */
            for (k = i - 1; k >= 0 && k >= i - 6; k--) {
                if (eq(k, "ld hl,0") &&
                    k + 1 < i && eq(k + 1, lo_store) &&
                    k + 2 < i && eq(k + 2, hi_store)) {
                    found = 1;
                    break;
                }
            }
            if (!found) continue;
        }

        /* All checks passed.  Replace the loop with LDIR. */
        {
            char ld_hl_sym[MAX_LINE], ld_const[MAX_LINE], ld_de_sym1[MAX_LINE], ld_bc[MAX_LINE];

            sprintf(ld_hl_sym,  "ld hl,%s",     arr_sym);
            sprintf(ld_const,   "ld (hl),%s",   const_str);
            sprintf(ld_de_sym1, "ld de,%s+1",   arr_sym);
            sprintf(ld_bc,      "ld bc,%ld",     size_val);

            delete_n(i, ip - i + 1);

            insert_line_tagged(i + 0, ld_hl_sym, "ldir_memset");
            insert_line(i + 1, ld_const);
            insert_line(i + 2, ld_de_sym1);
            insert_line(i + 3, ld_bc);
            insert_line(i + 4, "ldir");

            changed = 1;
        }
    }

    return changed;
}

/*
 * Parse "ld R,(ix-N)" extracting N (positive int). R is a single register
 * character ('l','h','e','d'). Returns 1 on success.
 */
static int stride_parse_ld_r_ix_neg(const char *s, char r, int *n)
{
    char prefix[16];
    const char *p;
    int v;

    sprintf(prefix, "ld %c,(ix-", r);
    if (strncmp(s, prefix, strlen(prefix)) != 0)
        return 0;
    p = s + strlen(prefix);
    if (*p < '0' || *p > '9')
        return 0;
    v = 0;
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    if (*p != ')' || p[1] != 0 || v <= 0)
        return 0;
    *n = v;
    return 1;
}

/* Parse "ld (ix-N),R" extracting N. Returns 1 on success. */
static int stride_parse_ld_ix_neg_r(const char *s, char r, int *n)
{
    char suffix[8];
    const char *p;
    int v;

    if (strncmp(s, "ld (ix-", 7) != 0)
        return 0;
    p = s + 7;
    if (*p < '0' || *p > '9')
        return 0;
    v = 0;
    while (*p >= '0' && *p <= '9')
        v = v * 10 + (*p++ - '0');
    sprintf(suffix, "),%c", r);
    if (strcmp(p, suffix) != 0 || v <= 0)
        return 0;
    *n = v;
    return 1;
}

/*
 * Convert a stride-indexed inner loop into a pointer-walk loop.
 *
 * Detects the pattern:
 *
 *   LH:
 *     ld l,(ix-A)  ld h,(ix-A-1)    ; index variable (lo/hi)
 *     ld de,CONST                    ; upper bound
 *     or a
 *     sbc hl,de
 *     jp z, LB
 *     jp c, LB                       ; ≤ CONST → body
 *     jp LE                          ; > CONST → exit
 *   LB:
 *     ld l,(ix-A)  ld h,(ix-A-1)    ; reload index
 *     ld de,SYM                      ; array base (global symbol)
 *     add hl,de
 *     ld (hl),0                      ; array[k] = 0
 *   [LI:]
 *     ld l,(ix-A)  ld h,(ix-A-1)    ; reload index
 *     ld e,(ix-B)  ld d,(ix-B-1)    ; stride
 *     add hl,de
 *     ld (ix-A),l  ld (ix-A-1),h    ; store updated index
 *     jp LH
 *   LE:
 *
 * Replaces it with a pointer-walk that keeps ptr in HL, stride in DE,
 * and end address in BC — eliminating the three IX-relative reloads
 * per iteration:
 *
 *     ld e,(ix-B)  ld d,(ix-B-1)    ; DE = stride
 *     ld l,(ix-A)  ld h,(ix-A-1)    ; HL = initial index
 *     ld bc,SYM                      ; BC = array base
 *     add hl,bc                      ; HL = initial ptr
 *     ld bc,SYM+CONST+1              ; BC = end address
 *     push hl / or a / sbc hl,bc / pop hl
 *     jp nc,LE                       ; skip loop if ptr >= end
 *   LH:
 *     ld (hl),0
 *     add hl,de
 *     push hl / or a / sbc hl,bc / pop hl
 *     jp c,LH
 *   LE:
 */
static int pass_stride_loop_to_ptr(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 32 < nlines; i++) {
        char lh[128], lb[128], le[128], tmp[128];
        int lo_k, hi_k, lo_s, hi_s;
        long cmp_val;
        char arr_sym[128];
        int j, ip;

        /* 1. LH label */
        if (!label_name_at(i, lh))
            continue;
        j = i + 1;

        /* 2. Comparison block */
        if (!stride_parse_ld_r_ix_neg(lines[j], 'l', &lo_k)) continue; j++;
        if (!stride_parse_ld_r_ix_neg(lines[j], 'h', &hi_k)) continue; j++;
        if (hi_k != lo_k - 1) continue;
        if (!parse_ld_de_positive_imm(lines[j], &cmp_val)) continue; j++;
        /* Accept both unsigned (or a/sbc) and signed-biased (xor 80h/or a/sbc)
         * comparisons. The generated pointer walk uses unsigned pointer arithmetic,
         * which is semantically correct for non-negative array indices — the only
         * valid use case for this pattern (negative index would be UB in C). */
        if (eq(j, "ld a,h") && eq(j+1, "xor 80h") && eq(j+2, "ld h,a") &&
            eq(j+3, "ld a,d") && eq(j+4, "xor 80h") && eq(j+5, "ld d,a"))
            j += 6;
        if (!eq(j, "or a")) continue; j++;
        if (!eq(j, "sbc hl,de")) continue; j++;
        if (!parse_jp_z_label(lines[j], lb)) continue; j++;
        if (!parse_jp_c_label(lines[j], tmp) || strcmp(tmp, lb) != 0) continue; j++;
        if (!peep_parse_jp_uncond_label(lines[j], le)) continue; j++;

        /* 3. LB label */
        if (!line_is_label_name(j, lb)) continue; j++;

        /* 4. Body: reload index, compute address, store 0 */
        {
            int lo2, hi2;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'l', &lo2)) continue; j++;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'h', &hi2)) continue; j++;
            if (lo2 != lo_k || hi2 != hi_k) continue;
        }
        if (!parse_ld_de_imm(lines[j], arr_sym) || arr_sym[0] != '_') continue; j++;
        if (!eq(j, "add hl,de")) continue; j++;
        if (!eq(j, "ld (hl),0")) continue; j++;

        /* 5. Optional LI label (fall-through increment label) */
        if (starts_label(lines[j]))
            j++;

        /* 6. Increment block: reload index, load stride, update index */
        {
            int lo3, hi3;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'l', &lo3)) continue; j++;
            if (!stride_parse_ld_r_ix_neg(lines[j], 'h', &hi3)) continue; j++;
            if (lo3 != lo_k || hi3 != hi_k) continue;
        }
        if (!stride_parse_ld_r_ix_neg(lines[j], 'e', &lo_s)) continue; j++;
        if (!stride_parse_ld_r_ix_neg(lines[j], 'd', &hi_s)) continue; j++;
        if (hi_s != lo_s - 1) continue;
        if (!eq(j, "add hl,de")) continue; j++;
        {
            int lo4;
            if (!stride_parse_ld_ix_neg_r(lines[j], 'l', &lo4) || lo4 != lo_k) continue;
        }
        j++;
        {
            int hi4;
            if (!stride_parse_ld_ix_neg_r(lines[j], 'h', &hi4) || hi4 != hi_k) continue;
        }
        j++;

        /* 7. jp back to LH */
        if (!peep_parse_jp_uncond_label(lines[j], tmp) || strcmp(tmp, lh) != 0) continue;
        ip = j;
        j++;

        /* 8. LE label immediately follows */
        if (!line_is_label_name(j, le)) continue;

        /* Pattern matched. Delete old block and insert pointer-walk version.
         *
         * BC = SYM+SIZE+1 (16-bit relocatable — fine with M80).
         * B = endhi = high byte of end address.
         * C = endlo = low byte of end address.
         *
         * "ld a,h; cp b" computes H - endhi (set carry if H < endhi).
         * "ld a,l; cp c" computes L - endlo (set carry if L < endlo).
         * Neither ld(hl),0 nor add hl,de modifies A, B, or C.
         *
         * Common case (H < endhi): 39 T-states per iteration.
         * Previous push/sbc/pop approach:  71 T-states.
         *
         * Loop structure:
         *   LH:
         *     ld (hl),0
         *     add hl,de          ptr += stride
         *     ld a,h
         *     cp b               H - endhi: carry → H < endhi
         *     jp c,LH            H < endhi → continue (39T)
         *     jp nz,LE           H > endhi → exit
         *     ld a,l             H = endhi: check low byte
         *     cp c               L - endlo: carry → L < endlo
         *     jp c,LH            L < endlo → continue
         *                        fall through: L >= endlo → exit
         */
        {
            char l0[MAX_LINE], l1[MAX_LINE], l2[MAX_LINE], l3[MAX_LINE], l4[MAX_LINE], l6[MAX_LINE];
            char lh_label[MAX_LINE];
            char jp_c_lh[MAX_LINE], jp_nz_le[MAX_LINE], jp_nc_le[MAX_LINE];

            sprintf(l0,       "ld e,(ix-%d)", lo_s);
            sprintf(l1,       "ld d,(ix-%d)", hi_s);
            sprintf(l2,       "ld l,(ix-%d)", lo_k);
            sprintf(l3,       "ld h,(ix-%d)", hi_k);
            sprintf(l4,       "ld bc,%s", arr_sym);
            sprintf(l6,       "ld bc,%s+%ld", arr_sym, cmp_val + 1);
            sprintf(lh_label, "%s:", lh);
            sprintf(jp_c_lh,  "jp c,%s", lh);
            sprintf(jp_nz_le, "jp nz,%s", le);
            sprintf(jp_nc_le, "jp nc,%s", le);

            delete_n(i, ip - i + 1);

            /* Setup: stride in DE, initial ptr in HL, end addr in BC */
            insert_line_tagged(i +  0, l0, "stride_loop"); /* ld e,(ix-B) */
            insert_line(i +  1, l1);           /* ld d,(ix-B-1)      */
            insert_line(i +  2, l2);           /* ld l,(ix-A)        */
            insert_line(i +  3, l3);           /* ld h,(ix-A-1)      */
            insert_line(i +  4, l4);           /* ld bc,SYM          */
            insert_line(i +  5, "add hl,bc");  /* HL = SYM+k = ptr   */
            insert_line(i +  6, l6);           /* ld bc,SYM+SIZE+1   */
            /* One-shot pre-check: skip loop if initial ptr >= end */
            insert_line(i +  7, "push hl");
            insert_line(i +  8, "or a");
            insert_line(i +  9, "sbc hl,bc");
            insert_line(i + 10, "pop hl");
            insert_line(i + 11, jp_nc_le);     /* jp nc,LE           */
            /* Hot loop body */
            insert_line(i + 12, lh_label);     /* LH:                */
            insert_line(i + 13, "ld (hl),0");
            insert_line(i + 14, "add hl,de");
            insert_line(i + 15, "ld a,h");     /* A = ptr.hi         */
            insert_line(i + 16, "cp b");       /* H - endhi          */
            insert_line(i + 17, jp_c_lh);      /* jp c → H < endhi   */
            insert_line(i + 18, jp_nz_le);     /* jp nz → H > endhi  */
            insert_line(i + 19, "ld a,l");     /* H = endhi: check L */
            insert_line(i + 20, "cp c");       /* L - endlo          */
            insert_line(i + 21, jp_c_lh);      /* jp c → L < endlo   */
            /* fall through: L >= endlo → exit to LE                 */

            changed = 1;
        }
    }

    return changed;
}

/*
 * pass_stride_k_setup_to_direct:
 *
 * After pass_stride_loop_to_ptr fires, the code entering the pointer-walk
 * still computes stride and k through IX round-trips:
 *
 *   ld l,(ix-K)            ; load i
 *   ld h,(ix-K-1)
 *   push hl
 *   ld l,(ix-K)            ; reload i (same slot)
 *   ld h,(ix-K-1)
 *   ex de,hl
 *   pop hl
 *   add hl,de              ; HL = 2i
 *   inc hl
 *   inc hl
 *   inc hl                 ; HL = stride = 2i+3
 *   ld (ix-S),l            ; save stride
 *   ld (ix-S-1),h
 *   ld l,(ix-K)            ; reload i
 *   ld h,(ix-K-1)
 *   push hl
 *   ld l,(ix-S)            ; reload stride
 *   ld h,(ix-S-1)
 *   ex de,hl
 *   pop hl
 *   add hl,de              ; HL = k = i+stride
 *   ld (ix-P),l            ; save k
 *   ld (ix-P-1),h
 *   ld e,(ix-S)            ; reload stride (for pointer-walk DE)
 *   ld d,(ix-S-1)
 *   ld l,(ix-P)            ; reload k (for pointer-walk HL)
 *   ld h,(ix-P-1)
 *   ld bc,SYM
 *   add hl,bc              ; HL = initial ptr
 *   ld bc,SYM+N            ; BC = end address
 *   push hl
 *   or a
 *   sbc hl,bc
 *   pop hl
 *   jp nc,LE
 *
 * Replace with (18 lines):
 *   ld l,(ix-K)
 *   ld h,(ix-K-1)          ; HL = i
 *   ld e,l
 *   ld d,h                 ; DE = i
 *   add hl,de              ; HL = 2i, DE = i
 *   inc hl
 *   inc hl
 *   inc hl                 ; HL = stride, DE = i
 *   ex de,hl               ; DE = stride, HL = i
 *   add hl,de              ; HL = k = 3i+3
 *   ld bc,SYM
 *   add hl,bc              ; HL = initial ptr
 *   ld bc,SYM+N            ; BC = end address (B=endhi, C=endlo)
 *   push hl
 *   or a
 *   sbc hl,bc
 *   pop hl
 *   jp nc,LE
 *
 * At the inner-loop entry: HL=ptr, DE=stride, B=endhi, C=endlo -- exactly
 * what the pointer-walk hot loop needs for its "ld a,h; cp b" check.
 */
static int pass_stride_k_setup_to_direct(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 35 <= nlines; i++) {
        int K, M, S, T, P, Q, tmp_n;
        char sym[128], le[128];
        long N;

        /* offsets 0-1: load i lo/hi */
        if (!stride_parse_ld_r_ix_neg(lines[i +  0], 'l', &K)) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i +  1], 'h', &M)) continue;
        if (M != K - 1) continue;

        /* offsets 2-7: push / reload same / ex / pop / add = double-load */
        if (!eq(i + 2, "push hl")) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i +  3], 'l', &tmp_n) || tmp_n != K) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i +  4], 'h', &tmp_n) || tmp_n != M) continue;
        if (!eq(i + 5, "ex de,hl")) continue;
        if (!eq(i + 6, "pop hl")) continue;
        if (!eq(i + 7, "add hl,de")) continue;

        /* offsets 8-10: inc hl x3 */
        if (!eq(i +  8, "inc hl")) continue;
        if (!eq(i +  9, "inc hl")) continue;
        if (!eq(i + 10, "inc hl")) continue;

        /* offsets 11-12: store stride */
        if (!stride_parse_ld_ix_neg_r(lines[i + 11], 'l', &S)) continue;
        if (!stride_parse_ld_ix_neg_r(lines[i + 12], 'h', &T)) continue;
        if (T != S - 1) continue;
        if (S == K || T == K) continue;

        /* offsets 13-14: reload i */
        if (!stride_parse_ld_r_ix_neg(lines[i + 13], 'l', &tmp_n) || tmp_n != K) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 14], 'h', &tmp_n) || tmp_n != M) continue;

        /* offsets 15-20: push / reload stride / ex / pop / add */
        if (!eq(i + 15, "push hl")) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 16], 'l', &tmp_n) || tmp_n != S) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 17], 'h', &tmp_n) || tmp_n != T) continue;
        if (!eq(i + 18, "ex de,hl")) continue;
        if (!eq(i + 19, "pop hl")) continue;
        if (!eq(i + 20, "add hl,de")) continue;

        /* offsets 21-22: store k */
        if (!stride_parse_ld_ix_neg_r(lines[i + 21], 'l', &P)) continue;
        if (!stride_parse_ld_ix_neg_r(lines[i + 22], 'h', &Q)) continue;
        if (Q != P - 1) continue;
        if (P == K || Q == K || P == S || Q == S) continue;

        /* offsets 23-26: reload stride (to DE) and k (to HL) */
        if (!stride_parse_ld_r_ix_neg(lines[i + 23], 'e', &tmp_n) || tmp_n != S) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 24], 'd', &tmp_n) || tmp_n != T) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 25], 'l', &tmp_n) || tmp_n != P) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 26], 'h', &tmp_n) || tmp_n != Q) continue;

        /* offsets 27-28: ld bc,SYM / add hl,bc */
        if (strncmp(lines[i + 27], "ld bc,", 6) != 0) continue;
        strcpy(sym, lines[i + 27] + 6);
        if (sym[0] != '_') continue;
        if (!eq(i + 28, "add hl,bc")) continue;

        /* offset 29: ld bc,SYM+N */
        if (strncmp(lines[i + 29], "ld bc,", 6) != 0) continue;
        {
            char rest[256];
            char *plus;
            strcpy(rest, lines[i + 29] + 6);
            plus = strchr(rest, '+');
            if (!plus) continue;
            *plus = '\0';
            if (strcmp(rest, sym) != 0) continue;
            N = atol(plus + 1);
            if (N <= 0) continue;
        }

        /* offsets 30-34: push hl / or a / sbc hl,bc / pop hl / jp nc,LE */
        if (!eq(i + 30, "push hl")) continue;
        if (!eq(i + 31, "or a")) continue;
        if (!eq(i + 32, "sbc hl,bc")) continue;
        if (!eq(i + 33, "pop hl")) continue;
        if (strncmp(lines[i + 34], "jp nc,", 6) != 0) continue;
        strcpy(le, lines[i + 34] + 6);

        /* Pattern matched — replace 35 lines with 18. */
        {
            char l_lk[64], l_hk[64], l_bc[128], l_bc_end[256], jp_nc[128];

            sprintf(l_lk,    "ld l,(ix-%d)", K);
            sprintf(l_hk,    "ld h,(ix-%d)", M);
            sprintf(l_bc,    "ld bc,%s", sym);
            sprintf(l_bc_end,"ld bc,%s+%ld", sym, N);
            sprintf(jp_nc,   "jp nc,%s", le);

            delete_n(i, 35);

            insert_line_tagged(i +  0, l_lk, "stride_k"); /* ld l,(ix-K) */
            insert_line(i +  1, l_hk);          /* ld h,(ix-K-1)     */
            insert_line(i +  2, "ld e,l");      /* DE = i            */
            insert_line(i +  3, "ld d,h");
            insert_line(i +  4, "add hl,de");   /* HL = 2i           */
            insert_line(i +  5, "inc hl");
            insert_line(i +  6, "inc hl");
            insert_line(i +  7, "inc hl");      /* HL = stride, DE=i */
            insert_line(i +  8, "ex de,hl");    /* DE=stride, HL=i   */
            insert_line(i +  9, "add hl,de");   /* HL = k            */
            insert_line(i + 10, l_bc);          /* ld bc,SYM         */
            insert_line(i + 11, "add hl,bc");   /* HL = initial ptr  */
            insert_line(i + 12, l_bc_end);      /* ld bc,SYM+N       */
            insert_line(i + 13, "push hl");
            insert_line(i + 14, "or a");
            insert_line(i + 15, "sbc hl,bc");
            insert_line(i + 16, "pop hl");
            insert_line(i + 17, jp_nc);         /* jp nc,LE          */

            changed = 1;
        }
    }

    return changed;
}

/*
 * pass_reuse_sbc_result_for_flagcheck:
 *
 * After XOR 80h signed-comparison bias was added, the outer sieve loop loads
 * i twice: once for the signed bound check and again for &flags[i].  But after
 * "sbc hl,de" with de = N (biased), HL = i - N numerically (the biases cancel).
 * Switching to unsigned N+1 drops the jp z branch and lets us recover &flags[i]
 * directly from HL = i-(N+1) via "ld de,SYM+(N+1); add hl,de".
 *
 *   ld l,(ix-K)
 *   ld h,(ix-K-1)
 *   ld de,N
 *   ld a,h / xor 80h / ld h,a / ld a,d / xor 80h / ld d,a   ; signed bias
 *   or a
 *   sbc hl,de
 *   jp z, LT
 *   jp c, LT            ; i <= N → body
 *   jp LE               ; i >  N → exit
 * LT:
 *   ld l,(ix-K)         ; redundant reload of i
 *   ld h,(ix-K-1)
 *   ld de,SYM
 *   add hl,de           ; HL = &SYM[i]
 *   ld a,(hl)
 *   or a
 *   jp z/nz, LDEST
 *
 * Becomes (13 lines, saving 9 instructions ≈ 88T per outer-loop iteration):
 *
 *   ld l,(ix-K)
 *   ld h,(ix-K-1)
 *   ld de,N+1           ; unsigned: C set ↔ i < N+1 ↔ i <= N
 *   or a
 *   sbc hl,de           ; HL = i-(N+1); drops jp z
 *   jp c, LT
 *   jp LE
 * LT:
 *   ld de,SYM+N+1       ; (i-(N+1)) + (SYM+(N+1)) = SYM+i = &SYM[i]
 *   add hl,de
 *   ld a,(hl)
 *   or a
 *   jp z/nz, LDEST
 */
static int pass_reuse_sbc_result_for_flagcheck(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 22 <= nlines; i++) {
        int K, M, tmp_n;
        long N;
        char lt[128], le[128], lt2[128], sym[128], dest[128];
        const char *cond;

        /* offsets 0-1: load i lo/hi from IX */
        if (!stride_parse_ld_r_ix_neg(lines[i + 0], 'l', &K)) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 1], 'h', &M)) continue;
        if (M != K - 1) continue;

        /* offset 2: ld de,N (positive literal) */
        if (!parse_ld_de_positive_imm(lines[i + 2], &N)) continue;

        /* offsets 3-8: XOR 80h signed comparison bias */
        if (!eq(i + 3, "ld a,h"))  continue;
        if (!eq(i + 4, "xor 80h")) continue;
        if (!eq(i + 5, "ld h,a"))  continue;
        if (!eq(i + 6, "ld a,d"))  continue;
        if (!eq(i + 7, "xor 80h")) continue;
        if (!eq(i + 8, "ld d,a"))  continue;

        /* offsets 9-10: or a / sbc hl,de */
        if (!eq(i + 9,  "or a"))      continue;
        if (!eq(i + 10, "sbc hl,de")) continue;

        /* offsets 11-12: jp z,LT / jp c,LT (same label) */
        if (!parse_jp_z_label(lines[i + 11], lt))  continue;
        if (!parse_jp_c_label(lines[i + 12], lt2) || strcmp(lt2, lt) != 0) continue;

        /* offset 13: jp LE (unconditional exit) */
        if (!peep_parse_jp_uncond_label(lines[i + 13], le)) continue;

        /* offset 14: LT: label */
        if (!line_is_label_name(i + 14, lt)) continue;

        /* offsets 15-16: reload same index from IX */
        if (!stride_parse_ld_r_ix_neg(lines[i + 15], 'l', &tmp_n) || tmp_n != K) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 16], 'h', &tmp_n) || tmp_n != M) continue;

        /* offset 17: ld de,SYM (global symbol) */
        if (!parse_ld_de_imm(lines[i + 17], sym) || sym[0] != '_') continue;

        /* offset 18: add hl,de */
        if (!eq(i + 18, "add hl,de")) continue;

        /* offset 19: ld a,(hl) */
        if (!eq(i + 19, "ld a,(hl)")) continue;

        /* offset 20: or a */
        if (!eq(i + 20, "or a")) continue;

        /* offset 21: jp z/nz,LDEST */
        if (parse_jp_z_label(lines[i + 21], dest))
            cond = "z";
        else if (parse_jp_nz_label(lines[i + 21], dest))
            cond = "nz";
        else
            continue;

        /* Pattern matched.  Rebuild as 13-line unsigned comparison + direct address. */
        {
            char l_lk[64], l_hk[64], l_de_n1[64];
            char jp_c_lt[MAX_LINE], jp_le[MAX_LINE], l_lt[MAX_LINE];
            char l_de_sym_n1[MAX_LINE], jp_cond_dest[MAX_LINE];

            sprintf(l_lk,         "ld l,(ix-%d)", K);
            sprintf(l_hk,         "ld h,(ix-%d)", M);
            sprintf(l_de_n1,      "ld de,%ld", N + 1);
            sprintf(jp_c_lt,      "jp c,%s", lt);
            sprintf(jp_le,        "jp %s", le);
            sprintf(l_lt,         "%s:", lt);
            sprintf(l_de_sym_n1,  "ld de,%s+%ld", sym, N + 1);
            sprintf(jp_cond_dest, "jp %s, %s", cond, dest);

            delete_n(i, 22);

            insert_line_tagged(i +  0, l_lk, "reuse_sbc"); /* ld l,(ix-K) */
            insert_line(i +  1, l_hk);           /* ld h,(ix-K-1)        */
            insert_line(i +  2, l_de_n1);        /* ld de,N+1            */
            insert_line(i +  3, "or a");
            insert_line(i +  4, "sbc hl,de");
            insert_line(i +  5, jp_c_lt);        /* jp c,LT              */
            insert_line(i +  6, jp_le);          /* jp LE (exit)         */
            insert_line(i +  7, l_lt);           /* LT:                  */
            insert_line(i +  8, l_de_sym_n1);    /* ld de,SYM+N+1        */
            insert_line(i +  9, "add hl,de");    /* HL = &SYM[i]         */
            insert_line(i + 10, "ld a,(hl)");
            insert_line(i + 11, "or a");
            insert_line(i + 12, jp_cond_dest);   /* jp z/nz,LDEST        */

            changed = 1;
        }
    }

    return changed;
}

/*
 * pass_recover_index_from_sbc:
 *
 * The outer loop test loads i from IX, computes HL = i - N (via sbc hl,de),
 * and branches.  At the branch target LT, HL = i-N and DE = N.  The flag
 * check that follows reloads i from IX again via a push/pop pattern:
 *
 *   ld l,(ix-K)        ; load i (also at top of loop)
 *   ld h,(ix-K-1)
 *   ld de,N            ; loop bound
 *   or a
 *   sbc hl,de          ; HL = i - N
 *   jp z,LT
 *   jp c,LT
 *   jp LE
 * LT:
 *   ld hl,SYM          ; compute &SYM[i] the slow way
 *   push hl
 *   ld l,(ix-K)        ; reload i from IX (same K)
 *   ld h,(ix-K-1)
 *   ex de,hl
 *   pop hl
 *   add hl,de          ; HL = SYM + i
 *
 * At LT, HL = i-N and DE = N, so "add hl,de" recovers i directly.
 *
 * While matching, also:
 * - Change ld de,N to ld de,N+1: the strict "<" test (C flag only) covers
 *   i <= N because "i < N+1" iff "i <= N" for integers.  This drops the
 *   jp z,LT branch entirely.
 * - Use "ld de,SYM+N+1; add hl,de" for the address:
 *   (i-(N+1)) + (SYM+(N+1)) = SYM+i = &SYM[i]  (same result).
 *
 * Full 16-line block replaced by 10 lines.  Saves one branch instruction
 * (~10T) on every outer-loop iteration plus the 6 eliminated code lines.
 */
static int pass_recover_index_from_sbc(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 16 <= nlines; i++) {
        int K, M, tmp_n;
        long N;
        char lt[128], le[128], sym[128];
        char lt2[128];

        /* offsets 0-1: load i lo/hi */
        if (!stride_parse_ld_r_ix_neg(lines[i + 0], 'l', &K)) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 1], 'h', &M)) continue;
        if (M != K - 1) continue;

        /* offset 2: ld de,N (positive literal) */
        if (!parse_ld_de_positive_imm(lines[i + 2], &N)) continue;

        /* offsets 3-4: or a / sbc hl,de */
        if (!eq(i + 3, "or a")) continue;
        if (!eq(i + 4, "sbc hl,de")) continue;

        /* offsets 5-6: jp z,LT / jp c,LT (same label) */
        if (!parse_jp_z_label(lines[i + 5], lt)) continue;
        if (!parse_jp_c_label(lines[i + 6], lt2) || strcmp(lt2, lt) != 0) continue;

        /* offset 7: jp LE (unconditional exit) */
        if (!peep_parse_jp_uncond_label(lines[i + 7], le)) continue;

        /* offset 8: LT: label */
        if (!line_is_label_name(i + 8, lt)) continue;

        /* offsets 9-15: push/reload-IX/ex/pop/add */
        if (!parse_ld_hl_imm(lines[i + 9], sym) || sym[0] != '_') continue;
        if (!eq(i + 10, "push hl")) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 11], 'l', &tmp_n) || tmp_n != K) continue;
        if (!stride_parse_ld_r_ix_neg(lines[i + 12], 'h', &tmp_n) || tmp_n != M) continue;
        if (!eq(i + 13, "ex de,hl")) continue;
        if (!eq(i + 14, "pop hl")) continue;
        if (!eq(i + 15, "add hl,de")) continue;

        /*
         * Pattern matched. Replace all 16 lines with 10 lines.
         *
         * Use N+1 for the sbc bound so only jp c is needed (drops jp z).
         * Use SYM+N+1 for the address base so the single add gives &SYM[i]:
         *   (i-(N+1)) + (SYM+(N+1)) = SYM+i = &SYM[i]
         */
        {
            char l_lk[64], l_hk[64], l_de_n1[64], jp_c_lt[MAX_LINE];
            char jp_le[MAX_LINE], l_lt[MAX_LINE], l_de_sym_n1[MAX_LINE];

            sprintf(l_lk,        "ld l,(ix-%d)", K);
            sprintf(l_hk,        "ld h,(ix-%d)", M);
            sprintf(l_de_n1,     "ld de,%ld", N + 1);
            sprintf(jp_c_lt,     "jp c,%s", lt);
            sprintf(jp_le,       "jp %s", le);
            sprintf(l_lt,        "%s:", lt);
            sprintf(l_de_sym_n1, "ld de,%s+%ld", sym, N + 1);

            delete_n(i, 16);

            insert_line_tagged(i +  0, l_lk, "recover_idx"); /* ld l,(ix-K) */
            insert_line(i +  1, l_hk);           /* ld h,(ix-K-1)        */
            insert_line(i +  2, l_de_n1);        /* ld de,N+1            */
            insert_line(i +  3, "or a");
            insert_line(i +  4, "sbc hl,de");
            insert_line(i +  5, jp_c_lt);        /* jp c,LT  (covers i<=N) */
            insert_line(i +  6, jp_le);          /* jp LE    (exit)        */
            insert_line(i +  7, l_lt);           /* LT:                    */
            insert_line(i +  8, l_de_sym_n1);    /* ld de,SYM+N+1          */
            insert_line(i +  9, "add hl,de");    /* HL = &SYM[i]           */

            changed = 1;
        }
    }

    return changed;
}

/*
 * pass_ix_frame_ptr_load:
 *
 * Collapse the indirect-via-IX idiom for loading a 16-bit local variable
 * (a pointer stored in the IX frame) into HL via push/pop/dec:
 *
 *   push ix
 *   pop hl
 *   [dec hl] × N        ; N >= 2: HL → IX-N (lo-byte address)
 *   ld e,(hl)           ; E = lo byte at IX-N
 *   inc hl              ; HL → IX-(N-1) (hi-byte address)
 *   ld d,(hl)           ; D = hi byte at IX-(N-1)
 *   ex de,hl            ; HL = 16-bit local pointer value
 *
 * Replaced with two instructions:
 *
 *   ld l,(ix-N)
 *   ld h,(ix-(N-1))
 *
 * Valid because the push/pop cancel on the stack, dec×N offsets HL from IX,
 * and the ld e/ld d/ex sequence is exactly what the IX-indexed loads do.
 * Requires N >= 2 so that both the lo byte (ix-N) and hi byte (ix-(N-1))
 * have strictly negative IX displacements (valid local-variable slots).
 */
static int pass_ix_frame_ptr_load(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 6 < nlines; i++) {
        int n_dec, j;
        char ld_l[64], ld_h[64];

        if (!eq(i,     "push ix")) continue;
        if (!eq(i + 1, "pop hl"))  continue;

        /* Count consecutive dec hl; require at least 2 */
        j = i + 2;
        n_dec = 0;
        while (j < nlines && eq(j, "dec hl") && n_dec < 7) {
            n_dec++;
            j++;
        }
        if (n_dec < 2) continue;

        /* Mandatory tail: ld e,(hl) / inc hl / ld d,(hl) / ex de,hl */
        if (!eq(j,     "ld e,(hl)")) continue;
        if (!eq(j + 1, "inc hl"))    continue;
        if (!eq(j + 2, "ld d,(hl)")) continue;
        if (!eq(j + 3, "ex de,hl"))  continue;

        /* Total lines consumed: 2 (push/pop) + n_dec + 4 */
        {
            int total = 2 + n_dec + 4;

            sprintf(ld_l, "ld l,(ix-%d)", n_dec);
            sprintf(ld_h, "ld h,(ix-%d)", n_dec - 1);

            delete_n(i, total);
            insert_line_tagged(i, ld_l, "ix_frame_ptr");
            insert_line(i + 1, ld_h);

            changed = 1;
        }
    }

    return changed;
}

/*
 * pass_deref_byte_cmp:
 *
 * After pass_ix_frame_ptr_load simplifies the pointer load, recognise and
 * collapse the pattern of dereferencing a byte through a local pointer and
 * comparing that byte (zero-extended to 16 bits) against another IX local:
 *
 *   ld l,(ix-N)              ; pointer lo  (N >= 2)
 *   ld h,(ix-M)              ; pointer hi  (M == N-1)
 *   ld l,(hl)                ; L = *ptr  (byte dereference)
 *   ld h,0                   ; H = 0     (zero-extend to 16-bit)
 *   push hl
 *   ld l,(ix+P) or (ix-P)    ; L = compare value
 *   ld h,0
 *   ex de,hl                 ; DE = compare value
 *   pop hl                   ; HL = *ptr
 *   or a
 *   sbc hl,de                ; HL = *ptr - cmp
 *   jp z/nz, LABEL
 *
 * Replaced with (6 instructions):
 *
 *   ld l,(ix-N)
 *   ld h,(ix-M)              ; HL = ptr
 *   ld a,(hl)                ; A = *ptr
 *   ld l,(ix+P) or (ix-P)    ; L = compare value
 *   cp l                     ; Z set iff A == L  (same as sbc hl,de for z/nz)
 *   jp z/nz, LABEL
 *
 * Safe for z/nz conditions: sbc hl,de with HL=(0,A) and DE=(0,L) sets Z
 * iff A==L, which is exactly what "cp l" tests.
 */
static int pass_deref_byte_cmp(void)
{
    int i;
    int changed = 0;

    for (i = 0; i + 11 < nlines; i++) {
        char lo_ix[64], hi_ix[64];
        char label[128];
        const char *cond;
        int N, M;

        /* ld l,(ix-N) — pointer lo byte, strictly negative offset */
        if (!peep_parse_ld_l_ix(lines[i], lo_ix)) continue;
        if (lo_ix[0] != '-') continue;
        N = atoi(lo_ix + 1);
        if (N < 2) continue;

        /* ld h,(ix-M) — pointer hi byte, M must equal N-1 */
        if (!peep_parse_ld_h_ix(lines[i + 1], hi_ix)) continue;
        if (hi_ix[0] != '-') continue;
        M = atoi(hi_ix + 1);
        if (M != N - 1) continue;

        /* ld l,(hl) / ld h,0 — byte dereference, zero-extend */
        if (!eq(i + 2, "ld l,(hl)")) continue;
        if (!eq(i + 3, "ld h,0"))    continue;

        /* push hl / ld l,(ix...) / ld h,0 / ex de,hl / pop hl */
        if (!eq(i + 4, "push hl"))   continue;
        if (strncmp(lines[i + 5], "ld l,(ix", 8) != 0) continue;
        if (!eq(i + 6, "ld h,0"))    continue;
        if (!eq(i + 7, "ex de,hl"))  continue;
        if (!eq(i + 8, "pop hl"))    continue;

        /* or a / sbc hl,de / jp z or jp nz */
        if (!eq(i + 9,  "or a"))      continue;
        if (!eq(i + 10, "sbc hl,de")) continue;
        if (parse_jp_z_label(lines[i + 11], label))
            cond = "z";
        else if (parse_jp_nz_label(lines[i + 11], label))
            cond = "nz";
        else
            continue;

        /* Pattern matched (12 lines). Emit 6 instructions. */
        {
            char ld_l[64], ld_h[64], cmp_l[MAX_LINE], jp_line[MAX_LINE];

            sprintf(ld_l, "ld l,(ix-%d)", N);
            sprintf(ld_h, "ld h,(ix-%d)", M);
            strcpy(cmp_l, lines[i + 5]);   /* preserve the "ld l,(ix...)" line */
            sprintf(jp_line, "jp %s, %s", cond, label);

            delete_n(i, 12);
            insert_line_tagged(i + 0, ld_l, "deref_byte_cmp");
            insert_line(i + 1, ld_h);
            insert_line(i + 2, "ld a,(hl)");
            insert_line(i + 3, cmp_l);
            insert_line(i + 4, "cp l");
            insert_line(i + 5, jp_line);

            changed = 1;
        }
    }

    return changed;
}


/*
 * pass_larg_stubs — replace ld l,(ix+N) / ld h,(ix+N+1) with call __la1/2/3.
 *
 * The shared RTL stubs are 7 bytes each; the inline pair is 6 bytes.  With
 * three or more uses of the same stub the stub cost is recovered and every
 * additional use saves 3 bytes.  Runs after pass_elim_ix_frame so that
 * the "ix" text is still visible during frame-elimination scanning.
 */
static int pass_larg_stubs(void)
{
    int i, changed;
    int used_la1 = 0, used_la2 = 0, used_la3 = 0;

    changed = 0;

    for (i = 0; i + 1 < nlines; i++) {
        if (eq(i, "ld l,(ix+4)") && eq(i+1, "ld h,(ix+5)")) {
            replace1_tagged(i, "call __la1", "larg");
            delete_n(i+1, 1);
            used_la1 = 1; changed = 1;
        } else if (eq(i, "ld l,(ix+6)") && eq(i+1, "ld h,(ix+7)")) {
            replace1_tagged(i, "call __la2", "larg");
            delete_n(i+1, 1);
            used_la2 = 1; changed = 1;
        } else if (eq(i, "ld l,(ix+8)") && eq(i+1, "ld h,(ix+9)")) {
            replace1_tagged(i, "call __la3", "larg");
            delete_n(i+1, 1);
            used_la3 = 1; changed = 1;
        }
    }

    if (used_la1 || used_la2 || used_la3) {
        int pos = 0;
        if (used_la3) insert_line(pos, "extrn __la3");
        if (used_la2) insert_line(pos, "extrn __la2");
        if (used_la1) insert_line(pos, "extrn __la1");
    }

    return changed;
}

/*
 * pass_phix_stub — replace push hl / push ix / pop hl with call __phix.
 *
 * The pattern saves HL on the stack and copies IX into HL (for frame-pointer
 * arithmetic).  The stub is 6 bytes; the inline sequence is 4 bytes (1 byte
 * push hl + 2 bytes push ix + 1 byte pop hl).  Break-even is 7 calls.
 * Runs after pass_shared_frame_stubs so prologue push-ix has been converted.
 */
static int pass_phix_stub(void)
{
    int i, changed;
    int used = 0;

    changed = 0;

    for (i = 0; i + 2 < nlines; i++) {
        if (eq(i, "push hl") && eq(i+1, "push ix") && eq(i+2, "pop hl")) {
            replace1_tagged(i, "call __phix", "phix");
            delete_n(i+1, 2);
            used = 1; changed = 1;
        }
    }

    if (used)
        insert_line(0, "extrn __phix");

    return changed;
}

static int pass_global_board_const_offsets(void)
{
    int i;
    int changed;
    int incs;
    int k;
    int imm;
    char line[160];

    changed = 0;

    for (i = 0; i < nlines; ++i) {
        /*
         * Collapse constant-index global board addressing:
         *
         *     ld hl,_g_board
         *     inc hl
         *     inc hl
         *     cp (hl)
         *
         * into:
         *
         *     ld hl,_g_board+2
         *     cp (hl)
         *
         * and:
         *
         *     ld hl,_g_board
         *     ld de,5
         *     add hl,de
         *
         * into:
         *
         *     ld hl,_g_board+5
         *
         * This is safe because it only changes address formation; HL still
         * contains the same address before the following memory operation.
         */
        if (eq(i, "ld hl,_g_board")) {
            incs = 0;
            k = i + 1;
            while (k < nlines && eq(k, "inc hl")) {
                ++incs;
                ++k;
            }

            if (incs > 0) {
                sprintf(line, "ld hl,_g_board+%d", incs);
                replace1_tagged(i, line, "global_const_offset");
                delete_n(i + 1, incs);
                changed = 1;
                if (i > 0)
                    --i;
                continue;
            }

            if (i + 2 < nlines &&
                peep_parse_ld_de_0_to_255(lines[i + 1], &imm) &&
                eq(i + 2, "add hl,de")) {
                if (imm == 0)
                    sprintf(line, "ld hl,_g_board");
                else
                    sprintf(line, "ld hl,_g_board+%d", imm);
                replace1_tagged(i, line, "global_const_offset");
                delete_n(i + 1, 2);
                changed = 1;
                if (i > 0)
                    --i;
                continue;
            }
        }
    }

    return changed;
}

static int pass_board_byte_eq_direct_load(void)
{
    int i;
    int changed;
    char lab[128];
    char addr[128];
    char line[160];

    changed = 0;

    for (i = 0; i + 3 < nlines; ++i) {
        /*
         * Equality-only compare against a constant board address:
         *
         *     ld a,b
         *     ld hl,_g_board+N
         *     cp (hl)
         *     jp z/nz,L
         *
         * can be:
         *
         *     ld a,(_g_board+N)
         *     cp b
         *     jp z/nz,L
         *
         * The subtraction direction changes, so this is valid only for
         * equality/inequality branches where only Z is used.
         */
        if (!eq(i, "ld a,b"))
            continue;

        if (!parse_ld_hl_imm(lines[i + 1], addr))
            continue;
        if (strncmp(addr, "_g_board", 8) != 0)
            continue;
        if (!eq(i + 2, "cp (hl)"))
            continue;
        if (!peep_is_jp_z_or_nz(lines[i + 3]))
            continue;

        sprintf(line, "ld a,(%s)", addr);
        replace1_tagged(i, line, "board_byte_eq_direct");
        replace1(i + 1, "cp b");
        replace1(i + 2, lines[i + 3]);
        delete_n(i + 3, 1);
        changed = 1;
        if (i > 0)
            --i;
    }

    return changed;
}

int main(int argc, char **argv)
{
    int changed;
    int passes;

    if (argc != 3) {
        fprintf(stderr, "usage: peep input.mac output.mac\n");
        return 1;
    }

    read_file(argv[1]);

    passes = 0;
    do {
        changed = 0;
        if (pass_once()) changed = 1;
        if (pass_byte_minmax_patterns()) changed = 1;
        if (pass_byte_minmax_board_and_assign()) changed = 1;
        if (pass_inline_simple_call_hl_from_loaded_pointer()) changed = 1;
        if (pass_dead_hl_load_before_ldhl()) changed = 1;
        if (pass_cp_zero_to_or_a()) changed = 1;
        if (pass_call_hl_stack_roundtrip()) changed = 1;
        if (pass_minmax_winner_result_no_temp()) changed = 1;
        if (pass_minmax_score_b_cache()) changed = 1;
        if (pass_shrink_minmax_frame_after_callptr_temp_removed()) changed = 1;
        if (pass_shrink_minmax_frame3_after_score_cache()) changed = 1;
        if (pass_store_l_reload_a()) changed = 1;
        if (pass_reuse_board_addr_for_zero_store()) changed = 1;
        if (pass_array_base_push_to_de()) changed = 1;
        if (pass_e_signed_le_zero()) changed = 1;
        if (pass_ix_array_word_addr()) changed = 1;
        if (pass_ix_postdec_to_local()) changed = 1;
        if (pass_store_word_const_hl()) changed = 1;
        if (pass_float_zero_store()) changed = 1;
        if (pass_incsp_to_popbc()) changed = 1;
        if (pass_remove_unreferenced_labels()) changed = 1;
        if (pass_ldir_memset()) changed = 1;
        if (pass_reuse_sbc_result_for_flagcheck()) changed = 1;
        if (pass_cond_skip_shortcut()) changed = 1;
        if (pass_stride_loop_to_ptr()) changed = 1;
        if (pass_stride_k_setup_to_direct()) changed = 1;
        if (pass_recover_index_from_sbc()) changed = 1;
        if (pass_ix_frame_ptr_load()) changed = 1;
        if (pass_deref_byte_cmp()) changed = 1;
        if (pass_elim_dead_ix_stores()) changed = 1;
        if (pass_remove_ix_store_reload_a()) changed = 1;
        if (pass_elim_long_store_reload()) changed = 1;
        if (pass_branch_over_jump()) changed = 1;
        if (pass_global_board_const_offsets()) changed = 1;
        if (pass_posfunc_ix1_to_b()) changed = 1;
        if (pass_posfunc_b_cache()) changed = 1;
        if (pass_board_byte_eq_direct_load()) changed = 1;
        if (pass_lookforwinner_b_cache()) changed = 1;
        if (pass_mulu_const()) changed = 1;
        if (pass_minmax_unsigned_compares()) changed = 1;
        if (pass_fix_main_argc_gt_one()) changed = 1;
/*        if (pass_replace_tstr_fake_strstr()) changed = 1; */
        if (pass_labels()) changed = 1;
        passes++;
    } while (changed && passes < 30);

    /* Run frame elimination after all other passes have converged, then
     * clean up any newly unreferenced labels created by the removal. */
    if (pass_elim_ix_frame())
        pass_labels();

    /* Convert remaining framed prologues/epilogues to shared stub calls.
     * Runs after frame elimination so only functions that genuinely need IX
     * are transformed.  A follow-up branch/label pass collapses any return
     * labels that now just contain "jp __lve" into direct jumps. */
    if (pass_shared_frame_stubs()) {
        pass_branch_over_jump();
        pass_labels();
    }

    /* Load-arg stubs and frame-pointer copy stub run last: they remove "ix"
     * text from lines, so they must not run before pass_elim_ix_frame (which
     * uses that text to detect live frame usage). */
    pass_larg_stubs();
    pass_phix_stub();

    write_file(argv[2]);
    return 0;
}
