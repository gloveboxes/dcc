/* tc89size.c - C89 sizeof expression tests for dcc */

#include <stdio.h>

struct SzOne {
    char c;
    int i;
    long l;
};

static int fails;
static int ga[5];
static long gl[3];
static struct SzOne gs[2];
static char gstr[] = "abcd";

static void chki(const char *name, int got, int expect)
{
    if (got != expect) {
        printf("FAIL %s got %d expected %d\n", name, got, expect);
        fails = fails + 1;
    }
}

int main(void)
{
    struct SzOne s;
    int *ip;
    long *lp;
    struct SzOne *sp;
    char *cp;

    ip = ga;
    lp = gl;
    sp = gs;
    cp = gstr;
    fails = 0;

    chki("sizeof_char", sizeof(char), 1);
    chki("sizeof_int", sizeof(int), 2);
    chki("sizeof_long", sizeof(long), 4);
    chki("sizeof_ptr", sizeof(ip), 2);

    chki("sizeof_array", sizeof ga, 10);
    chki("sizeof_array_elem", sizeof ga[0], 2);
    chki("sizeof_star_ip", sizeof *ip, 2);
    chki("sizeof_ip_index", sizeof ip[0], 2);

    chki("sizeof_long_array", sizeof gl, 12);
    chki("sizeof_long_elem", sizeof gl[0], 4);
    chki("sizeof_star_lp", sizeof *lp, 4);
    chki("sizeof_lp_index", sizeof lp[0], 4);

    chki("sizeof_struct", sizeof s, 7);
    chki("sizeof_struct_array", sizeof gs, 14);
    chki("sizeof_struct_elem", sizeof gs[0], 7);
    chki("sizeof_sp_index", sizeof sp[0], 7);
    chki("sizeof_field_i", sizeof s.i, 2);
    chki("sizeof_field_l", sizeof s.l, 4);
    chki("sizeof_arrow_l", sizeof sp->l, 4);

    chki("sizeof_string_lit", sizeof "abc", 4);
    chki("sizeof_char_array", sizeof gstr, 5);
    chki("sizeof_char_elem", sizeof cp[0], 1);

    chki("sizeof_expr_long", sizeof(ga[0] + 1L), 4);
    chki("sizeof_expr_uint", sizeof(ga[0] + 1U), 2);
    chki("sizeof_compare", sizeof(ga[0] < 1L), 2);

    if (fails) {
        printf("tc89size failed: %d\n", fails);
        return 1;
    }

    printf("tc89size completed with great success\n");
    return 0;
}
