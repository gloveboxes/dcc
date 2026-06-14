/*
 * tforblk.c - general C block scope (scope-stack).
 *
 * dcc historically kept one flat set of locals per function, so a variable
 * declared in an inner { } block aliased an outer same-named one and leaked
 * after the block.  With the block scope-stack a name declared in an inner
 * block shadows (does not clobber) an outer local/parameter/for-init variable
 * and is invisible once the block ends.  These tests pin that behaviour.
 */
#include <stdio.h>

static int fails;
static void chk(long got, long want, char *name)
{
    if (got != want) { printf("FAIL %s got=%ld want=%ld\n", name, got, want); fails++; }
}

static int param_shadow(int x)
{
    int acc = 0;
    {
        int x = 5;              /* shadows the parameter */
        acc += x;               /* 5 */
    }
    acc += x;                   /* parameter value restored */
    return acc;
}

static int static_sibling_blocks(void)
{
    int total = 0;

    {
        static int x = 10;
        x++;
        total += x;
    }

    {
        static int x = 100;
        x++;
        total += x;
    }

    return total;
}

static int static_shadows_auto(void)
{
    int x = 7;
    int inner;

    {
        static int x = 40;
        x++;
        inner = x;
    }

    return x * 100 + inner;
}

static int static_pointer_initializer(void)
{
    int result = 0;

    {
        static int x = 5;
        static int *p = &x;
        *p = *p + 2;
        result += x;
    }

    return result;
}

int main(void)
{
    /* 1. basic inner-block shadow leaves the outer value intact */
    {
        int x = 10;
        {
            int x = 20;
            chk(x, 20L, "inner x");
        }
        chk(x, 10L, "outer x after block");
    }

    /* 2. sibling blocks each get their own variable */
    {
        long s = 0;
        { int a = 3; s += a; }
        { int a = 4; s += a; }
        chk(s, 7L, "sibling blocks");
    }

    /* 3. three-deep nested shadow */
    {
        int v = 1;
        {
            int v = 2;
            {
                int v = 3;
                chk(v, 3L, "deep inner");
            }
            chk(v, 2L, "mid after deep");
        }
        chk(v, 1L, "outer after nested");
    }

    /* 4. shadow with a different (wider) type */
    {
        int n = 100;
        {
            long n = 100000L;
            chk(n, 100000L, "long shadow");
        }
        chk(n, 100L, "int after long shadow");
    }

    /* 5. an inner block redeclares the for-init loop variable */
    {
        long acc = 0;
        for (int i = 0; i < 3; i++) {
            int i = 50;                 /* shadows the loop variable */
            acc += i;                   /* 3 * 50 = 150 */
        }
        chk(acc, 150L, "for body shadows loop var");
    }

    /* 5b. the loop counter is unaffected by the inner shadow */
    {
        int count = 0;
        for (int i = 0; i < 4; i++) {
            long i = 999L;
            if (i == 999L) count++;
        }
        chk(count, 4L, "loop counter intact despite inner shadow");
    }

    /* 6. a block variable shadows a parameter */
    chk(param_shadow(7), 12L, "param shadow");   /* 5 + 7 */

    /* 7. block scope inside an if body */
    {
        int y = 1;
        if (y == 1) {
            int y = 8;
            chk(y, 8L, "if-block shadow");
        }
        chk(y, 1L, "y after if-block");
    }

    /* 8. block scope inside a while body */
    {
        int w = 0;
        int outer = 77;
        long sum = 0;
        while (w < 2) {
            int outer = w;              /* shadow; outer loop var untouched */
            sum += outer;               /* 0 + 1 */
            w++;
        }
        chk(outer, 77L, "while-block outer intact");
        chk(sum, 1L, "while-block inner used");
    }

    /* 9. block scope inside a switch case */
    {
        int sel = 1;
        int r = 0;
        switch (sel) {
            case 1: {
                int t = 11;
                r = t;
                break;
            }
            default:
                r = -1;
        }
        chk(r, 11L, "switch case block");
    }

    /* 10. a declaration after a sibling block gets a fresh slot (no leak) */
    {
        long total = 0;
        { int z = 5; total += z; }
        {
            int z = 9;
            total += z;
        }
        chk(total, 14L, "post-block redeclare");
    }

    /* 11. nested for-loops reusing the same counter name */
    {
        long acc = 0;
        for (int i = 0; i < 3; i++)
            for (int i = 0; i < 2; i++)
                acc += i;               /* 3 * (0+1) = 3 */
        chk(acc, 3L, "nested same-name for");
    }

    /* 12. for-init shadows an outer local, restored after the loop */
    {
        int i = 42;
        long acc = 0;
        for (int i = 0; i < 5; i++)
            acc += i;                   /* 0+1+2+3+4 = 10 */
        chk(acc, 10L, "for-init sum");
        chk(i, 42L, "outer i restored after loop");
    }

    /* 13. block-scope static locals with the same source name are distinct */
    chk(static_sibling_blocks(), 112L, "static sibling blocks first call");
    chk(static_sibling_blocks(), 114L, "static sibling blocks second call");

    /* 14. a block static shadows an automatic local only inside the block */
    chk(static_shadows_auto(), 741L, "static shadows auto first call");
    chk(static_shadows_auto(), 742L, "static shadows auto second call");

    /* 15. static initializers use the backing symbol for block statics */
    chk(static_pointer_initializer(), 7L, "static pointer initializer first call");
    chk(static_pointer_initializer(), 9L, "static pointer initializer second call");

    if (fails == 0) printf("tforblk passed with great success\n");
    else printf("tforblk FAILED: %d\n", fails);
    return fails != 0;
}
