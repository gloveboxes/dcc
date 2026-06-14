/* tfarrsub.c - dcc regression: struct field array subscripted by a
 * non-constant expression that itself accesses a struct field.
 * Expected: PASS field_arr_subscript
 *
 * Three bug sites in dcc_expr.c, all of the form "read current_field_array_elem_size
 * after gen_expr() which clobbers it":
 *
 *  A) gen_lvalue_addr inner subscript while (line ~1369): write to
 *     ptr->arr_field[ptr->member] — covered by set_param / "ind write" checks.
 *
 *  B) gen_expr inner subscript while (line ~3230): read from
 *     ptr[i].arr_field[ptr->member] — covered by "ind read" checks.
 *
 *  C) gen_expr first subscript while (line ~3159): read from
 *     struct_var.arr_field[struct_var.member] — covered by "dir read" checks.
 *
 * (A) and (B) were the adaint TTT.ADA bug; (C) is a proactive fix for the
 * symmetric rvalue path.
 */
#include <stdio.h>

#define MAXNAME 24
#define MAXPARAM 8

struct Func {
    char name[MAXNAME];
    int entry;
    int nparam;
    int locals;
    unsigned char ret_esize;
    unsigned char pofs[MAXPARAM];
    unsigned char pesz[MAXPARAM];
};

struct State {
    struct Func *func;
    int nfunc;
};

/* For (C): direct embedded array subscripted by a sibling struct member. */
struct Direct {
    unsigned char arr[MAXPARAM];
    int n;
};

static struct Func funcs[4];
static struct State state;
static struct Direct ds;

static int failures;

static void check(const char *name, int got, int expected)
{
    if (got != expected) {
        printf("FAIL %s got %d expected %d\n", name, got, expected);
        failures++;
    }
}

/* Covers (A): lvalue inner subscript while.
 * state->func[fi].pofs[state->func[fi].nparam] = value;
 * The index is a struct member access, clobbering current_field_array_elem_size
 * inside gen_lvalue_addr before elem_size is used for scaling. */
static void set_param(int fi, int base_val, int esz_val)
{
    struct State *G;
    G = &state;
    G->func[fi].pofs[G->func[fi].nparam] = (unsigned char)base_val;
    G->func[fi].pesz[G->func[fi].nparam] = (unsigned char)esz_val;
    G->func[fi].nparam++;
}

/* Covers (C): lvalue first subscript while.
 * ds.arr[ds.n] = value — this path was already correct before the fix,
 * but included for symmetry with the read checks below. */
static void set_direct(int val)
{
    ds.arr[ds.n] = (unsigned char)val;
    ds.n++;
}

int main(void)
{
    struct Func *f;
    int fi;

    /* --- indirect (pointer-indexed) array --- */
    state.func = funcs;
    state.nfunc = 0;
    fi = 0;
    f = &funcs[fi];
    f->nparam = 0;

    /* (A): write pofs[0]=0, pofs[1]=2 via struct-member index */
    set_param(fi, 0, 2);
    set_param(fi, 2, 2);

    check("ind nparam",    (int)f->nparam,   2);
    check("ind write p[0]",(int)f->pofs[0],  0);
    check("ind write p[1]",(int)f->pofs[1],  2);
    check("ind write z[0]",(int)f->pesz[0],  2);
    check("ind write z[1]",(int)f->pesz[1],  2);
    check("ind write p[2]",(int)f->pofs[2],  0);
    check("ind write z[2]",(int)f->pesz[2],  0);

    /* (B): read pofs[1] via struct-member index (second field-access while) */
    check("ind read p[0]", (int)state.func[fi].pofs[f->nparam - 2], 0);
    check("ind read p[1]", (int)state.func[fi].pofs[f->nparam - 1], 2);

    /* --- direct (embedded) array --- */
    ds.n = 0;
    set_direct(77);
    set_direct(88);

    check("dir write[0]", (int)ds.arr[0], 77);
    check("dir write[1]", (int)ds.arr[1], 88);

    /* (C): read ds.arr[ds.n - 1] via struct-member index (first subscript while) */
    check("dir read[0]",  (int)ds.arr[ds.n - 2], 77);
    check("dir read[1]",  (int)ds.arr[ds.n - 1], 88);

    if (failures) {
        printf("FAIL field_arr_subscript (%d failures)\n", failures);
        return 1;
    }
    printf("PASS field_arr_subscript\n");
    return 0;
}
