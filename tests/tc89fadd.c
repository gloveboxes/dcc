/* tc89fadd.c - C89 float add/sub regression for dcc */
#include <stdio.h>
#include <stdint.h>

static int fails;
static float gf;   /* global: exercises the normal_assign path via try_emit_float_rvalue_dehl */

static void chk(const char *name, unsigned char *p,
                unsigned int b0, unsigned int b1,
                unsigned int b2, unsigned int b3)
{
    if (p[0] != b0 || p[1] != b1 || p[2] != b2 || p[3] != b3) {
        printf("FAIL %s got %u %u %u %u expected %u %u %u %u\n",
               name, p[0], p[1], p[2], p[3], b0, b1, b2, b3);
        fails++;
    }
}

float fid(float x)
{
    return x;
}

int main(void)
{
    float a, b, c;
    unsigned char *p;

    fails = 0;
    a = 1.0f;
    b = 2.5f;
    c = a + b;
    p = (unsigned char *)&c;
    chk("add", p, 0, 0, 96, 64);       /* 3.5f */

    c = 5.5f - 2.0f;
    p = (unsigned char *)&c;
    chk("sub", p, 0, 0, 96, 64);       /* 3.5f */

    c = 1.25f;
    c += fid(2.25f);
    p = (unsigned char *)&c;
    chk("addeq", p, 0, 0, 96, 64);     /* 3.5f */

    c -= 1.5f;
    p = (unsigned char *)&c;
    chk("subeq", p, 0, 0, 0, 64);      /* 2.0f */

    /* Regression: global float LHS with binary float expression on RHS.
     * Previously, try_emit_float_rvalue_dehl() grabbed only the first
     * operand and returned success, silently dropping the rest of the
     * expression.  Local float LHS is unaffected (uses a different path). */
    gf = a + b;
    p = (unsigned char *)&gf;
    chk("global_add_vars", p, 0, 0, 96, 64);   /* 3.5f */

    gf = b - a;
    p = (unsigned char *)&gf;
    chk("global_sub_vars", p, 0, 0, 192, 63);  /* 1.5f */

    /* Regression: global float LHS with float literal binary expression.
     * The literal fast path also failed to check for a following operator. */
    gf = 1.0f + 2.5f;
    p = (unsigned char *)&gf;
    chk("global_add_lits", p, 0, 0, 96, 64);   /* 3.5f */

    if (fails) {
        printf("tc89fadd failed: %d\n", fails);
        return 1;
    }
    printf("tc89fadd ok\n");
    return 0;
}
