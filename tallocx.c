#include <stdio.h>
#include <stdlib.h>

static unsigned char *slots[24];
static unsigned int sizes[24];
static unsigned int seed;

static void fail(const char *msg)
{
    printf("FAIL %s\n", msg);
    exit(1);
}

static unsigned int rnd(void)
{
    seed = (unsigned int)(seed * 25173U + 13849U);
    return seed;
}

static unsigned char patt(int slot, unsigned int off)
{
    return (unsigned char)((slot * 37 + off * 13 + 91) & 255U);
}

static void fill(unsigned char *p, unsigned int n, int slot)
{
    unsigned int i;

    for (i = 0; i < n; i++)
        p[i] = patt(slot, i);
}

static void check(unsigned char *p, unsigned int n, int slot, const char *msg)
{
    unsigned int i;

    if (p == 0)
        fail("null check pointer");

    for (i = 0; i < n; i++) {
        if (p[i] != patt(slot, i))
            fail(msg);
    }
}

static void zcheck(unsigned char *p, unsigned int n, const char *msg)
{
    unsigned int i;

    if (p == 0)
        fail("null zero pointer");

    for (i = 0; i < n; i++) {
        if (p[i] != 0)
            fail(msg);
    }
}

static void t_zero(void)
{
    unsigned char *p;

    free(0);
    p = (unsigned char *)malloc(0U);
    if (p == 0)
        fail("malloc zero returned null");
    p[0] = 0x5a;
    free(p);
}

static void t_split(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;

    a = (unsigned char *)malloc(16U);
    if (a == 0)
        fail("split setup malloc failed");
    fill(a, 16U, 1);
    free(a);

    b = (unsigned char *)malloc(12U);
    c = (unsigned char *)malloc(1U);
    if (b != a)
        fail("split did not reuse block head");
    if ((unsigned)c != (unsigned)b + 15U)
        fail("split tail payload wrong address");
    c[0] = 0xa5;
    check(b, 12U, 1, "split head contents changed unexpectedly");
    free(b);
    free(c);
}

static void t_nosplit(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;

    a = (unsigned char *)malloc(16U);
    if (a == 0)
        fail("nosplit setup malloc failed");
    free(a);

    b = (unsigned char *)malloc(13U);
    c = (unsigned char *)malloc(1U);
    if (b != a)
        fail("nosplit did not reuse block head");
    if ((unsigned)c != (unsigned)b + 19U)
        fail("nosplit unexpectedly created tail fragment");
    free(b);
    free(c);
}

static void t_forward(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *p;

    a = (unsigned char *)malloc(1000U);
    b = (unsigned char *)malloc(1000U);
    c = (unsigned char *)malloc(1000U);
    if (a == 0 || b == 0 || c == 0)
        fail("forward setup malloc failed");
    fill(a, 1000U, 2);
    fill(c, 1000U, 3);

    free(b);
    free(c);
    p = (unsigned char *)malloc(2003U);
    if (p != b)
        fail("forward coalesce failed");
    check(a, 1000U, 2, "forward coalesce damaged previous block");
    fill(p, 2003U, 4);
    free(p);
    free(a);
}

static void t_reverse(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *p;

    a = (unsigned char *)malloc(1000U);
    b = (unsigned char *)malloc(1000U);
    c = (unsigned char *)malloc(1000U);
    if (a == 0 || b == 0 || c == 0)
        fail("reverse setup malloc failed");

    free(a);
    free(b);
    p = (unsigned char *)malloc(2003U);
    if (p != a)
        fail("reverse coalesce failed");
    fill(p, 2003U, 5);
    free(p);
    free(c);
}

static void t_bridge(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *p;

    a = (unsigned char *)malloc(1000U);
    b = (unsigned char *)malloc(1000U);
    c = (unsigned char *)malloc(1000U);
    if (a == 0 || b == 0 || c == 0)
        fail("bridge setup malloc failed");

    free(a);
    free(c);
    free(b);
    p = (unsigned char *)malloc(3006U);
    if (p != a)
        fail("bridge coalesce failed");
    fill(p, 3006U, 6);
    free(p);
}

static void t_sizes(void)
{
    unsigned char *g;
    unsigned char *a;
    unsigned char *b;
    unsigned char *p;
    unsigned char *q;
    unsigned int base;
    unsigned int need;

    for (base = 8U; base <= 80U; base += 7U) {
        g = (unsigned char *)malloc(5U);
        a = (unsigned char *)malloc(base);
        b = (unsigned char *)malloc(7U);
        if (g == 0 || a == 0 || b == 0)
            fail("size sweep split setup failed");

        free(a);
        need = base - 4U;
        p = (unsigned char *)malloc(need);
        q = (unsigned char *)malloc(1U);
        if (p != a)
            fail("size sweep split did not reuse head");
        if ((unsigned)q != (unsigned)a + need + 3U)
            fail("size sweep split tail wrong address");
        free(p);
        free(q);
        free(g);
        free(b);

        g = (unsigned char *)malloc(5U);
        a = (unsigned char *)malloc(base);
        b = (unsigned char *)malloc(7U);
        if (g == 0 || a == 0 || b == 0)
            fail("size sweep nosplit setup failed");

        free(a);
        need = base - 3U;
        p = (unsigned char *)malloc(need);
        q = (unsigned char *)malloc(1U);
        if (p != a)
            fail("size sweep nosplit did not reuse head");
        if ((unsigned)q == (unsigned)a + need + 3U)
            fail("size sweep nosplit incorrectly made tail");
        free(p);
        free(q);
        free(g);
        free(b);
    }
}

static void t_large(void)
{
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;

    p = (unsigned char *)malloc(32768U);
    if (p == 0)
        fail("large malloc32768 failed");
    p[0] = 0x12;
    p[32767U] = 0x34;
    if (p[0] != 0x12 || p[32767U] != 0x34)
        fail("large edge bytes changed");
    free(p);

    q = (unsigned char *)malloc(32U);
    if (q == 0)
        fail("large small split malloc failed");
    q[0] = 0x56;
    q[31U] = 0x78;
    free(q);

    p = (unsigned char *)malloc(32768U);
    if (p == 0)
        fail("large coalesced malloc32768 failed");
    if (p != q)
        fail("large coalesced malloc wrong address");
    free(p);

    q = (unsigned char *)malloc(1U);
    if (q == 0)
        fail("large guard malloc failed");
    r = (unsigned char *)malloc(65000U);
    if (r != 0)
        fail("large wrap malloc accepted impossible request");
    free(q);
}

static void t_calloc(void)
{
    unsigned char *p;
    unsigned char *q;

    p = (unsigned char *)calloc(37U, 5U);
    if (p == 0)
        fail("calloc returned null");
    zcheck(p, 185U, "calloc did not zero fresh block");
    fill(p, 185U, 7);
    free(p);

    q = (unsigned char *)calloc(185U, 1U);
    if (q == 0)
        fail("calloc reuse returned null");
    if (q != p)
        fail("calloc did not reuse freed block");
    zcheck(q, 185U, "calloc did not zero reused block");
    free(q);
}

static void t_realloc(void)
{
    unsigned char *p;
    unsigned char *q;
    unsigned char *r;
    unsigned char *s;

    p = (unsigned char *)malloc(64U);
    if (p == 0)
        fail("realloc setup malloc failed");
    fill(p, 64U, 8);

    q = (unsigned char *)realloc(p, 120U);
    if (q == 0)
        fail("realloc grow returned null");
    check(q, 64U, 8, "realloc grow did not preserve contents");
    fill(q, 120U, 9);

    r = (unsigned char *)realloc(q, 16U);
    if (r == 0)
        fail("realloc shrink returned null");
    check(r, 16U, 9, "realloc shrink did not preserve contents");

    s = (unsigned char *)realloc(r, 0U);
    if (s != 0)
        fail("realloc zero did not return null");

    p = (unsigned char *)realloc(0, 32U);
    if (p == 0)
        fail("realloc null did not allocate");
    fill(p, 32U, 10);
    free(p);
}

static void t_wrap(void)
{
    unsigned char *p;
    unsigned char *q;

    p = (unsigned char *)malloc(1U);
    if (p == 0)
        fail("wrap guard setup malloc failed");
    q = (unsigned char *)malloc(65000U);
    if (q != 0)
        fail("wrap guard accepted impossible malloc");
    free(p);
}

static void t_recoalesce(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *q;
    unsigned char *r;

    /* Lay out A(200) then B(100), then free A so the following realloc reuses
     * and splits A's block. */
    a = (unsigned char *)malloc(200U);
    b = (unsigned char *)malloc(100U);
    if (a == 0 || b == 0)
        fail("recoalesce setup malloc failed");
    fill(b, 100U, 12);
    free(a);

    /* realloc(B,150) reuses A's 200-byte block as 150 used + a 47-byte free
     * fragment, then frees the old B block.  That free must coalesce with the
     * fragment so the reclaimed space is reused, not stranded. */
    q = (unsigned char *)realloc(b, 150U);
    if (q == 0)
        fail("recoalesce realloc failed");
    check(q, 100U, 12, "recoalesce realloc lost contents");

    /* The fragment sits at q + 150 + HDRSIZE.  If the old-block free coalesced,
     * a 120-byte malloc reuses it at exactly that address; otherwise malloc is
     * forced to extend the heap and returns a higher address. */
    r = (unsigned char *)malloc(120U);
    if (r == 0)
        fail("recoalesce post malloc failed");
    if ((unsigned)r != (unsigned)q + 153U)
        fail("realloc free did not coalesce (heap fragmented)");
    free(q);
    free(r);
}

static void t_rezero_coalesce(void)
{
    unsigned char *a;
    unsigned char *b;
    unsigned char *c;
    unsigned char *p;

    a = (unsigned char *)malloc(100U);
    b = (unsigned char *)malloc(100U);
    c = (unsigned char *)malloc(100U);
    if (a == 0 || b == 0 || c == 0)
        fail("rezero setup malloc failed");

    /* Free B, then realloc(C,0) frees C.  With coalescing the B and C blocks
     * (and the trailing free space) merge into one region, so a 250-byte malloc
     * reuses B's address.  Without coalescing the two 100-byte holes are too
     * small and malloc returns a different, later block. */
    free(b);
    p = (unsigned char *)realloc(c, 0U);
    if (p != 0)
        fail("realloc zero did not return null");

    p = (unsigned char *)malloc(250U);
    if (p == 0)
        fail("rezero post malloc failed");
    if (p != b)
        fail("realloc(ptr,0) free did not coalesce (heap fragmented)");
    free(p);
    free(a);
}

static void t_stress(void)
{
    int i;
    int idx;
    int op;
    unsigned int n;
    unsigned int old;
    unsigned int keep;
    unsigned char *p;

    seed = 0xACE1U;
    for (i = 0; i < 24; i++) {
        slots[i] = 0;
        sizes[i] = 0;
    }

    for (i = 0; i < 420; i++) {
        idx = (int)(rnd() % 24U);
        if (slots[idx] != 0) {
            check(slots[idx], sizes[idx], idx + 11, "stress contents changed");
            op = (int)(rnd() % 4U);
            if (op == 0) {
                old = sizes[idx];
                n = (rnd() % 220U) + 1U;
                p = (unsigned char *)realloc(slots[idx], n);
                if (p == 0)
                    fail("stress realloc returned null");
                keep = old < n ? old : n;
                check(p, keep, idx + 11, "stress realloc contents changed");
                slots[idx] = p;
                sizes[idx] = n;
                fill(slots[idx], sizes[idx], idx + 11);
            }
            else {
                free(slots[idx]);
                slots[idx] = 0;
                sizes[idx] = 0;
            }
        }
        else {
            n = (rnd() % 220U) + 1U;
            if ((rnd() & 1U) != 0) {
                p = (unsigned char *)calloc(n, 1U);
                if (p == 0)
                    fail("stress calloc returned null");
                zcheck(p, n, "stress calloc not zero");
            }
            else {
                p = (unsigned char *)malloc(n);
                if (p == 0)
                    fail("stress malloc returned null");
            }
            slots[idx] = p;
            sizes[idx] = n;
            fill(slots[idx], sizes[idx], idx + 11);
        }
    }

    for (i = 0; i < 24; i += 2) {
        if (slots[i] != 0) {
            check(slots[i], sizes[i], i + 11, "stress even final changed");
            free(slots[i]);
            slots[i] = 0;
        }
    }
    for (i = 1; i < 24; i += 2) {
        if (slots[i] != 0) {
            check(slots[i], sizes[i], i + 11, "stress odd final changed");
            free(slots[i]);
            slots[i] = 0;
        }
    }

    p = (unsigned char *)malloc(12000U);
    if (p == 0)
        fail("stress final large malloc failed");
    fill(p, 12000U, 35);
    check(p, 12000U, 35, "stress final large changed");
    free(p);
}

int main(void)
{
    t_split();
    t_nosplit();
    t_forward();
    t_reverse();
    t_bridge();
    t_sizes();
    t_large();
    t_zero();
    t_calloc();
    t_realloc();
    t_wrap();
    t_recoalesce();
    t_rezero_coalesce();
    t_stress();

    printf("tallocx: all tests passed\n");
    return 0;
}
