/* t2darr.c - dcc regression: multidimensional arrays embedded in structs.
 * Expected output: PASS multidim_array
 *
 * Bare-local multidimensional arrays (e.g. local int a[3][4]) already worked
 * because they index through sym_array_index_elem_size.  The bug was in the
 * struct-FIELD array path in dcc_expr.c:
 *
 *   1) gen_lvalue_addr (the write/address path) never multiplied the first
 *      index by the inner dimensions, so `mx.cell[i][j] = v` scaled index i
 *      by the base element size instead of by sizeof(inner row).
 *   2) Both gen_lvalue_addr and gen_primary read current_field_array_dim_count
 *      / current_field_array_dims AFTER evaluating a non-constant index.  An
 *      index that performs its own struct field access (e.g. mx.cell[mx.r])
 *      clobbers those globals, so the stride collapsed to the base size.
 *
 * The fix snapshots the dimension metadata before any index is evaluated and
 * applies the full inner-dimension stride on every path.  This test exercises
 * 2D/3D char and int field arrays, constant and variable indices, indices that
 * are themselves struct-member reads (the clobber case), and a 2D array nested
 * inside an array of structs.
 */
#include <stdio.h>

struct Mat {
    unsigned char cell[3][4];   /* 2D byte array, row stride 4 */
    int r;
    int c;
};

struct IMat {
    int v[3][4];                /* 2D int array, row stride 4*2 = 8 */
    int r;
    int c;
};

struct Cube {
    int data[2][3][4];          /* 3D int array */
    int a;
    int b;
    int d;
};

struct Cell {
    unsigned char g[2][2];
    int tag;
};

struct Grid {
    struct Cell cells[3];       /* array of structs, each holding a 2D array */
};

static struct Mat  mx;
static struct IMat im;
static struct Cube cu;
static struct Grid gr;

static int failures;

static void check(const char *name, int got, int expected)
{
    if (got != expected) {
        printf("FAIL %s got %d expected %d\n", name, got, expected);
        failures++;
    }
}

/* Index supplied by a function call: clobbers the field-array dim globals
 * between parsing the subscript and scaling it. */
static int row_of(void) { return mx.r; }
static int col_of(void) { return mx.c; }

int main(void)
{
    int i, j, k;

    /* --- 2D byte field array: constant indices, write then read --- */
    mx.cell[0][0] = 1;
    mx.cell[1][0] = 15;
    mx.cell[2][3] = 42;
    mx.cell[1][2] = 99;

    check("char c[0][0]", (int)mx.cell[0][0], 1);
    check("char c[1][0]", (int)mx.cell[1][0], 15);
    check("char c[2][3]", (int)mx.cell[2][3], 42);
    check("char c[1][2]", (int)mx.cell[1][2], 99);

    /* fill via nested variable loops, verify with constants */
    for (i = 0; i < 3; i++)
        for (j = 0; j < 4; j++)
            mx.cell[i][j] = (unsigned char)(i * 4 + j);
    check("char loopfill[2][3]", (int)mx.cell[2][3], 11);
    check("char loopfill[1][1]", (int)mx.cell[1][1], 5);

    /* index by sibling struct members (clobber path), write + read */
    mx.r = 2; mx.c = 1;
    mx.cell[mx.r][mx.c] = 77;
    check("char member-idx const-read", (int)mx.cell[2][1], 77);
    check("char member-idx var-read",   (int)mx.cell[mx.r][mx.c], 77);

    /* index by function call (also clobbers globals) */
    mx.r = 1; mx.c = 3;
    mx.cell[row_of()][col_of()] = 55;
    check("char fncall-idx", (int)mx.cell[1][3], 55);

    /* --- 2D int field array (2-byte element, row stride 8 bytes) --- */
    im.v[0][0] = 1000;
    im.v[1][0] = 2000;
    im.v[2][3] = 3003;
    im.v[1][2] = 1202;

    check("int v[0][0]", im.v[0][0], 1000);
    check("int v[1][0]", im.v[1][0], 2000);
    check("int v[2][3]", im.v[2][3], 3003);
    check("int v[1][2]", im.v[1][2], 1202);

    im.r = 2; im.c = 2;
    im.v[im.r][im.c] = 4242;
    check("int member-idx const-read", im.v[2][2], 4242);
    check("int member-idx var-read",   im.v[im.r][im.c], 4242);

    /* --- 3D int field array --- */
    for (i = 0; i < 2; i++)
        for (j = 0; j < 3; j++)
            for (k = 0; k < 4; k++)
                cu.data[i][j][k] = i * 100 + j * 10 + k;

    check("3d d[0][0][0]", cu.data[0][0][0], 0);
    check("3d d[1][2][3]", cu.data[1][2][3], 123);
    check("3d d[1][0][0]", cu.data[1][0][0], 100);
    check("3d d[0][2][1]", cu.data[0][2][1], 21);

    cu.a = 1; cu.b = 2; cu.d = 3;
    check("3d member-idx", cu.data[cu.a][cu.b][cu.d], 123);

    /* --- 2D array nested in an array of structs --- */
    gr.cells[0].g[0][0] = 10;
    gr.cells[1].g[1][0] = 21;
    gr.cells[2].g[0][1] = 32;
    gr.cells[1].g[1][1] = 23;

    check("nest c0 g[0][0]", (int)gr.cells[0].g[0][0], 10);
    check("nest c1 g[1][0]", (int)gr.cells[1].g[1][0], 21);
    check("nest c2 g[0][1]", (int)gr.cells[2].g[0][1], 32);
    check("nest c1 g[1][1]", (int)gr.cells[1].g[1][1], 23);

    if (failures) {
        printf("FAIL multidim_array (%d failures)\n", failures);
        return 1;
    }
    printf("PASS multidim_array\n");
    return 0;
}
