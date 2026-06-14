/*
 * tlongsub.c - regression test for long * array subscript in conditions
 *
 * The DCC bug: snippet_simple_type() returned 0 (TYPE_INT) when an identifier
 * was followed by '[', so snippet_needs_long_compare() did not detect that
 * expressions like arr[i] have type long when arr is long * or long arr[N].
 * The fast condition path (gen_direct_rel_branch_until) then generated 16-bit
 * signed comparison code instead of a 32-bit __lts/__lgs/... call.
 *
 * Symptom: for arr[i]=9 and arr[j]=0x3FFFFFFF, the condition
 *     if (arr[i] + 9 < arr[j])
 * compared 9 (biased 0x8009) vs 0xFFFF (biased 0x7FFF) as 16-bit, yielding
 * "not less than", so the branch was never taken.
 *
 * NOTE: comparisons used as VALUES (e.g. as function arguments) go through
 * gen_rel() which correctly detects the long type; the bug only manifests in
 * CONDITION context (if/while/for) which uses the gen_direct_rel_branch_until
 * fast path.  Tests below use explicit if blocks to exercise that path.
 */

#include <stdio.h>

static int errors = 0;

static void chk(const char *tag, int got, int want)
{
    if (got != want) {
        printf("FAIL %s: got %d want %d\n", tag, got, want);
        errors++;
    }
}

/* Helpers: force the comparison into an if-condition (the buggy path). */
static int if_lt(long a, long b)  { if (a <  b) return 1; return 0; }
static int if_gt(long a, long b)  { if (a >  b) return 1; return 0; }
static int if_le(long a, long b)  { if (a <= b) return 1; return 0; }
static int if_ge(long a, long b)  { if (a >= b) return 1; return 0; }

static long ga[4];   /* global long array */

int main(void)
{
    long la[4];      /* local long array */
    long *p;
    int r, i, n;

    la[0] = 9L;
    la[1] = 0x3FFFFFFFL;   /* large positive; low word 0xFFFF triggers 16-bit bug */
    la[2] = -1L;
    la[3] = 0x40000000L;

    /* ---- if conditions: local array subscript ---- */

    r = 0; if (la[0] <  la[1]) r = 1; chk("if la[0]<la[1]",  r, 1);
    r = 0; if (la[1] <  la[0]) r = 1; chk("if la[1]<la[0]",  r, 0);
    r = 0; if (la[1] >  la[0]) r = 1; chk("if la[1]>la[0]",  r, 1);
    r = 0; if (la[0] >  la[1]) r = 1; chk("if la[0]>la[1]",  r, 0);
    r = 0; if (la[0] <= la[1]) r = 1; chk("if la[0]<=la[1]", r, 1);
    r = 0; if (la[0] <= la[0]) r = 1; chk("if la[0]<=la[0]", r, 1);
    r = 0; if (la[1] >= la[0]) r = 1; chk("if la[1]>=la[0]", r, 1);
    r = 0; if (la[0] >= la[1]) r = 1; chk("if la[0]>=la[1]", r, 0);
    r = 0; if (la[2] <  la[0]) r = 1; chk("if la[2]<la[0]",  r, 1); /* neg<pos */
    r = 0; if (la[1] <  la[3]) r = 1; chk("if la[1]<la[3]",  r, 1); /* large<larger */

    /* ---- if conditions: subscript + addition (the exact lzpack pattern) ---- */

    la[0] = 0L;
    la[1] = 0x3FFFFFFFL;

    r = 0; if (la[0] + 9  < la[1]) r = 1; chk("if la[0]+9<la[1]",  r, 1);
    r = 0; if (la[0] + 9  > la[1]) r = 1; chk("if la[0]+9>la[1]",  r, 0);
    r = 0; if (la[1] + 9  < la[0]) r = 1; chk("if la[1]+9<la[0]",  r, 0);
    r = 0; if (la[0] + 9 <= la[1]) r = 1; chk("if la[0]+9<=la[1]", r, 1);
    r = 0; if (la[0] + 9 >= la[1]) r = 1; chk("if la[0]+9>=la[1]", r, 0);

    /* ---- if conditions: pointer subscript ---- */

    la[0] = 9L;
    la[1] = 0x3FFFFFFFL;
    p = la;

    r = 0; if (p[0] <  p[1]) r = 1; chk("if p[0]<p[1]",   r, 1);
    r = 0; if (p[1] <  p[0]) r = 1; chk("if p[1]<p[0]",   r, 0);
    r = 0; if (p[0] + 9 < p[1]) r = 1; chk("if p[0]+9<p[1]", r, 1);

    /* ---- if conditions: global array ---- */

    ga[0] = 0L;
    ga[1] = 0x3FFFFFFFL;

    r = 0; if (ga[0] <  ga[1]) r = 1; chk("if ga[0]<ga[1]",   r, 1);
    r = 0; if (ga[0] + 9 < ga[1]) r = 1; chk("if ga[0]+9<ga[1]", r, 1);

    /* ---- while-loop condition: subscript (uses fast branch path) ---- */

    la[0] = 0L;
    la[1] = 0x3FFFFFFFL;
    n = 0;
    while (la[0] + 9L < la[1]) {
        n++;
        la[0] += 0x10000000L;
        if (n > 100) break;
    }
    chk("while ran",   (n > 0), 1);
    chk("while count", n, 4);   /* 0,268M,536M,805M pass; 1073M+9 > 0x3FFFFFFF */

    /* ---- for-loop condition ---- */

    la[0] = 0x3FFFFFFFL;
    la[1] = 0L;
    for (i = 0; la[1] < la[0]; i++) {
        la[1] += 0x10000000L;
        if (i > 100) break;
    }
    chk("for ran", (i > 0), 1);

    /* ---- value-context sanity (goes through gen_rel, not fast path) ---- */

    la[0] = 9L;
    la[1] = 0x3FFFFFFFL;
    chk("val la[0]<la[1]",  if_lt(la[0], la[1]), 1);
    chk("val la[1]<la[0]",  if_lt(la[1], la[0]), 0);
    chk("val la[0]+9<la[1]",if_lt(la[0] + 9, la[1]), 1);

    if (errors == 0)
        printf("tlongsub passed with great success\n");
    else
        printf("tlongsub %d failure(s)\n", errors);

    return errors;
}
