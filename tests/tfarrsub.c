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
 *
 * Extended coverage added later (same clobber class, different strides/shapes):
 *  D) direct embedded int array (2-byte stride) indexed by a sibling member.
 *  E) direct embedded struct-element array (multi-byte stride), member index.
 *  F) indirect (pointer-indexed) struct with a 2-byte-stride int field,
 *     member index on the inner-subscript paths.
 *  G) index expression is a function call that reads a struct field, so it
 *     clobbers current_field_array_elem_size between parse and scaling.
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

/* For (D): direct embedded 2-byte-stride (int) array indexed by a sibling
 * member.  Exercises elem_size = type_size(int) = 2 on the rvalue/lvalue
 * first-subscript paths (the byte array above can't catch a stride bug). */
struct IntVec {
    int vals[MAXPARAM];
    int n;
};

/* For (E): direct embedded array whose element is itself a struct
 * (multi-byte stride), indexed by a sibling member.  Verifies the stride
 * comes from type_size(struct Pair), not a clobbered global. */
struct Pair {
    unsigned char a;
    unsigned char b;
    int w;
};
struct PairVec {
    struct Pair pairs[MAXPARAM];
    int n;
};

/* For (F): indirect (pointer-indexed) struct array with a 2-byte-stride int
 * field indexed by a struct member — the inner-subscript paths in both
 * gen_lvalue_addr and gen_primary with a non-unit element size. */
struct Row {
    int vals[MAXPARAM];
    int count;
};
struct Grid {
    struct Row *rows;
    int nrows;
};

static struct Func funcs[4];
static struct State state;
static struct Direct ds;
static struct IntVec iv;
static struct PairVec pv;
static struct Row rows_storage[3];
static struct Grid grid;

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

/* Covers (D): direct 2-byte-stride array, member index on write. */
static void set_intvec(int val)
{
    iv.vals[iv.n] = val;
    iv.n++;
}

/* Covers (E): direct struct-element array, member index on write into a
 * sub-field of the indexed element. */
static void set_pair(int av, int bv, int wv)
{
    pv.pairs[pv.n].a = (unsigned char)av;
    pv.pairs[pv.n].b = (unsigned char)bv;
    pv.pairs[pv.n].w = wv;
    pv.n++;
}

/* Covers (F): indirect 2-byte-stride array, member index on write. */
static void set_grid(int gi, int val)
{
    struct Grid *G;
    G = &grid;
    G->rows[gi].vals[G->rows[gi].count] = val;
    G->rows[gi].count++;
}

/* Covers (G): index is a function call that itself reads a struct field, so
 * it clobbers current_field_array_elem_size between parse and scaling. */
static int iv_index(void)
{
    return iv.n - 1;
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

    /* --- (D) direct 2-byte-stride (int) embedded array --- */
    iv.n = 0;
    set_intvec(1001);
    set_intvec(2002);
    set_intvec(3003);

    check("ivec write[0]", iv.vals[0], 1001);
    check("ivec write[1]", iv.vals[1], 2002);
    check("ivec write[2]", iv.vals[2], 3003);
    /* read with member index: a stride bug here would read a neighbouring
     * int (off by 2 bytes) and return the wrong element. */
    check("ivec read[0]",  iv.vals[iv.n - 3], 1001);
    check("ivec read[1]",  iv.vals[iv.n - 2], 2002);
    check("ivec read[2]",  iv.vals[iv.n - 1], 3003);

    /* (G): index is a function call clobbering the elem-size global. */
    check("ivec fncall idx", iv.vals[iv_index()], 3003);

    /* --- (E) direct struct-element (multi-byte stride) embedded array --- */
    pv.n = 0;
    set_pair(10, 11, 1200);
    set_pair(20, 21, 2200);

    check("pair a[0]", (int)pv.pairs[0].a, 10);
    check("pair b[0]", (int)pv.pairs[0].b, 11);
    check("pair w[0]", pv.pairs[0].w,      1200);
    check("pair a[1]", (int)pv.pairs[1].a, 20);
    check("pair w[1]", pv.pairs[1].w,      2200);
    /* read element fields via member index (struct-sized stride). */
    check("pair read a", (int)pv.pairs[pv.n - 1].a, 20);
    check("pair read w", pv.pairs[pv.n - 1].w,      2200);

    /* --- (F) indirect 2-byte-stride array via pointer-indexed struct --- */
    grid.rows = rows_storage;
    grid.nrows = 0;
    grid.rows[0].count = 0;
    set_grid(0, 555);
    set_grid(0, 666);

    check("grid count",   grid.rows[0].count,  2);
    check("grid write[0]", grid.rows[0].vals[0], 555);
    check("grid write[1]", grid.rows[0].vals[1], 666);
    /* read with member index on the indirect inner-subscript path. */
    check("grid read[0]", grid.rows[0].vals[grid.rows[0].count - 2], 555);
    check("grid read[1]", grid.rows[0].vals[grid.rows[0].count - 1], 666);

    if (failures) {
        printf("FAIL field_arr_subscript (%d failures)\n", failures);
        return 1;
    }
    printf("PASS field_arr_subscript\n");
    return 0;
}
