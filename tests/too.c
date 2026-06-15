/* too.c - "object-oriented C" stress test for the dcc compiler.
 *
 * Wired into the runall harness (runall.sh / runall.bat); also build/run
 * standalone with:
 *     ./ma.sh too peep   &&  ntvcm TOO
 *     ./ma.sh too nopeep &&  ntvcm TOO
 *
 * The point of this program is to lean hard on the parts of dcc that are easy
 * to get wrong on a 16-bit, single-pass, syntax-directed code generator:
 *
 *   - vtable polymorphism: structs of function pointers reached as
 *     self->vt->method(self), plus arrays/tables of function pointers.
 *   - single inheritance by first-member embedding, with up-casts
 *     (Derived* -> Shape*) and down-casts (Shape* -> Derived*).
 *   - polymorphic containers: arrays of base pointers, pointer-to-pointer
 *     iteration, and a visitor callback applied across the collection.
 *   - multidimensional data crossing call boundaries: pointer-to-2D-array
 *     parameters (Tile (*b)[COLS]), 2D arrays embedded in structs reached
 *     through a pointer (the dcc_expr.c field-array stride path), and 2D
 *     arrays nested inside an array of structs.
 *   - self-referential aggregates: an intrusive linked list and a recursive
 *     binary search tree allocated from a static object pool.
 *   - 32-bit fixed-point arithmetic (areas/perimeters scaled by 100) so the
 *     whole thing stays deterministic without float / -ffloatio.
 *
 * Every interesting value is checked against a hand-computed expected result;
 * the program prints a running log and finishes with either
 *     too passed with great success
 * or a non-zero exit and a FAIL line.
 */

#include <stdio.h>
#include <string.h>

/* ------------------------------------------------------------------ */
/* tiny self-checking harness                                          */
/* ------------------------------------------------------------------ */

static int g_checks;
static int g_fails;

static void check_l(const char *name, long got, long expected)
{
    g_checks++;
    if (got != expected) {
        g_fails++;
        printf("  FAIL %s: got %ld expected %ld\n", name, got, expected);
    }
}

static void check_i(const char *name, int got, int expected)
{
    check_l(name, (long)got, (long)expected);
}

static void check_s(const char *name, const char *got, const char *expected)
{
    g_checks++;
    if (strcmp(got, expected) != 0) {
        g_fails++;
        printf("  FAIL %s: got \"%s\" expected \"%s\"\n", name, got, expected);
    }
}

/* print a fixed-point (value * 100) quantity as N.NN */
static void put_fixed(long v100)
{
    long whole = v100 / 100;
    long frac = v100 % 100;
    if (frac < 0)
        frac = -frac;
    printf("%ld.%02ld", whole, frac);
}

/* integer square root of a non-negative long */
static long isqrt_l(long n)
{
    long x, y;

    if (n <= 0)
        return 0;
    x = n;
    y = (x + 1) / 2;
    while (y < x) {
        x = y;
        y = (x + n / x) / 2;
    }
    return x;
}

/* ------------------------------------------------------------------ */
/* polymorphic Shape hierarchy (vtable dispatch + embedded base)       */
/* ------------------------------------------------------------------ */

struct Shape;   /* forward declaration for the vtable signatures */

typedef long (*area_fn)(const struct Shape *self);
typedef long (*perim_fn)(const struct Shape *self);
typedef void (*scale_fn)(struct Shape *self, int pct);

typedef struct ShapeVTable {
    const char *name;
    area_fn   area;     /* fixed-point area  * 100 */
    perim_fn  perim;    /* fixed-point perim * 100 */
    scale_fn  scale;    /* scale every linear dimension by pct/100 */
} ShapeVTable;

typedef struct Shape {
    const ShapeVTable *vt;   /* virtual dispatch table (must work through ptr) */
    int id;
} Shape;

/* generic dispatchers - the only code that "knows" about Shape */
static long shape_area(const Shape *s)  { return s->vt->area(s); }
static long shape_perim(const Shape *s) { return s->vt->perim(s); }
static void shape_scale(Shape *s, int pct) { s->vt->scale(s, pct); }

static void shape_describe(const Shape *s)
{
    printf("  #%d %-9s area=", s->id, s->vt->name);
    put_fixed(shape_area(s));
    printf(" perim=");
    put_fixed(shape_perim(s));
    printf("\n");
}

/* ---- Rectangle ---- */

typedef struct Rectangle {
    Shape base;     /* MUST be first member for the up-cast to be valid */
    int w;
    int h;
} Rectangle;

static long rect_area(const struct Shape *self)
{
    const Rectangle *r = (const Rectangle *)self;   /* down-cast */
    return (long)r->w * r->h * 100L;
}

static long rect_perim(const struct Shape *self)
{
    const Rectangle *r = (const Rectangle *)self;
    return (long)(2 * (r->w + r->h)) * 100L;
}

static void rect_scale(struct Shape *self, int pct)
{
    Rectangle *r = (Rectangle *)self;
    r->w = (int)((long)r->w * pct / 100);
    r->h = (int)((long)r->h * pct / 100);
}

static const ShapeVTable RECT_VT = { "Rectangle", rect_area, rect_perim, rect_scale };

static void rect_init(Rectangle *r, int id, int w, int h)
{
    r->base.vt = &RECT_VT;
    r->base.id = id;
    r->w = w;
    r->h = h;
}

/* ---- Circle ---- */

typedef struct Circle {
    Shape base;
    int radius;
} Circle;

/* pi * 10000 == 31416; areas/perims are kept * 100 fixed point */
static long circ_area(const struct Shape *self)
{
    const Circle *c = (const Circle *)self;
    return 31416L * c->radius * c->radius / 100L;
}

static long circ_perim(const struct Shape *self)
{
    const Circle *c = (const Circle *)self;
    return 62832L * c->radius / 100L;
}

static void circ_scale(struct Shape *self, int pct)
{
    Circle *c = (Circle *)self;
    c->radius = (int)((long)c->radius * pct / 100);
}

static const ShapeVTable CIRC_VT = { "Circle", circ_area, circ_perim, circ_scale };

static void circ_init(Circle *c, int id, int radius)
{
    c->base.vt = &CIRC_VT;
    c->base.id = id;
    c->radius = radius;
}

/* ---- right Triangle (legs b,h) ---- */

typedef struct Triangle {
    Shape base;
    int b;
    int h;
} Triangle;

static long tri_area(const struct Shape *self)
{
    const Triangle *t = (const Triangle *)self;
    return (long)t->b * t->h * 50L;   /* b*h/2, * 100 */
}

static long tri_perim(const struct Shape *self)
{
    const Triangle *t = (const Triangle *)self;
    long hyp = isqrt_l((long)t->b * t->b + (long)t->h * t->h);
    return ((long)t->b + t->h + hyp) * 100L;
}

static void tri_scale(struct Shape *self, int pct)
{
    Triangle *t = (Triangle *)self;
    t->b = (int)((long)t->b * pct / 100);
    t->h = (int)((long)t->h * pct / 100);
}

static const ShapeVTable TRI_VT = { "Triangle", tri_area, tri_perim, tri_scale };

static void tri_init(Triangle *t, int id, int b, int h)
{
    t->base.vt = &TRI_VT;
    t->base.id = id;
    t->b = b;
    t->h = h;
}

/* ------------------------------------------------------------------ */
/* polymorphic container + visitor                                     */
/* ------------------------------------------------------------------ */

#define MAX_SHAPES 8

typedef struct World {
    Shape *items[MAX_SHAPES];   /* array of base pointers (up-cast on insert) */
    int count;
    long area_accum;            /* mutated by the visitor below */
} World;

typedef void (*visit_fn)(Shape *s, World *w);

static void world_init(World *w)
{
    w->count = 0;
    w->area_accum = 0;
}

static void world_add(World *w, Shape *s)
{
    if (w->count < MAX_SHAPES)
        w->items[w->count++] = s;
}

/* visit each element through a pointer-to-pointer walk */
static void world_visit(World *w, visit_fn fn)
{
    Shape **pp = w->items;
    Shape **end = w->items + w->count;
    while (pp < end) {
        fn(*pp, w);
        pp++;
    }
}

static void accum_area_visitor(Shape *s, World *w)
{
    w->area_accum += shape_area(s);
}

static void scale_all_visitor(Shape *s, World *w)
{
    (void)w;
    shape_scale(s, 200);    /* double every shape */
}

/* ------------------------------------------------------------------ */
/* function-pointer dispatch table (a "method selector")              */
/* ------------------------------------------------------------------ */

typedef long (*query_fn)(const Shape *s);

typedef struct Query {
    const char *label;
    query_fn fn;
} Query;

static const Query QUERIES[] = {
    { "area",  shape_area },
    { "perim", shape_perim }
};

static long run_query(const Shape *s, int which)
{
    return QUERIES[which].fn(s);
}

/* ------------------------------------------------------------------ */
/* multidimensional data across call boundaries                        */
/* ------------------------------------------------------------------ */

#define ROWS 3
#define COLS 4

typedef struct Tile {
    char ch;
    int weight;
} Tile;

typedef struct Board {
    Tile grid[ROWS][COLS];   /* 2D array of structs embedded in a struct */
    int generation;
} Board;

static void board_fill(Board *bd)
{
    int r, c;
    for (r = 0; r < ROWS; r++) {
        for (c = 0; c < COLS; c++) {
            bd->grid[r][c].ch = (char)('A' + r * COLS + c);
            bd->grid[r][c].weight = r * 10 + c;   /* field 2D array via ptr */
        }
    }
    bd->generation = 1;
}

/* pointer-to-2D-array parameter: b decays to Tile(*)[COLS] */
static long board_weight(Tile (*b)[COLS], int rows)
{
    long sum = 0;
    int r, c;
    for (r = 0; r < rows; r++)
        for (c = 0; c < COLS; c++)
            sum += b[r][c].weight;
    return sum;
}

/* return the address of an element of a struct-embedded 2D array */
static Tile *tile_at(Board *bd, int r, int c)
{
    return &bd->grid[r][c];
}

/* ---- 2D array nested inside an array of structs ---- */

typedef struct Cell {
    unsigned char g[2][2];
    int tag;
} Cell;

#define NCELLS 3

static Cell g_cells[NCELLS];

static void cells_fill(void)
{
    int k, i, j;
    for (k = 0; k < NCELLS; k++) {
        Cell *cp = &g_cells[k];          /* pointer into array of structs */
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++)
                cp->g[i][j] = (unsigned char)((k * 16 + i * 4 + j) & 255);
        cp->tag = k * 100;
    }
}

static long cells_checksum(void)
{
    long sum = 0;
    int k, i, j;
    for (k = 0; k < NCELLS; k++) {
        const Cell *cp = &g_cells[k];
        for (i = 0; i < 2; i++)
            for (j = 0; j < 2; j++)
                sum += cp->g[i][j];
    }
    return sum;
}

/* ------------------------------------------------------------------ */
/* self-referential aggregates: intrusive list + BST from a pool       */
/* ------------------------------------------------------------------ */

typedef struct Node {
    Shape *shape;
    struct Node *next;
} Node;

#define MAX_NODES 8
static Node g_nodes[MAX_NODES];
static int g_node_used;

static Node *node_new(Shape *s)
{
    Node *n = &g_nodes[g_node_used++];
    n->shape = s;
    n->next = 0;
    return n;
}

/* push-front build, returns new head */
static Node *list_push(Node *head, Shape *s)
{
    Node *n = node_new(s);
    n->next = head;
    return n;
}

static long list_area_sum(const Node *head)
{
    long sum = 0;
    const Node *p = head;
    while (p != 0) {
        sum += shape_area(p->shape);
        p = p->next;
    }
    return sum;
}

/* ---- binary search tree of ints from a static pool ---- */

typedef struct TNode {
    int v;
    struct TNode *l;
    struct TNode *r;
} TNode;

#define MAX_TNODES 16
static TNode g_tpool[MAX_TNODES];
static int g_tused;

static TNode *tnode_new(int v)
{
    TNode *t = &g_tpool[g_tused++];
    t->v = v;
    t->l = 0;
    t->r = 0;
    return t;
}

static TNode *bst_insert(TNode *root, int v)
{
    if (root == 0)
        return tnode_new(v);
    if (v < root->v)
        root->l = bst_insert(root->l, v);
    else
        root->r = bst_insert(root->r, v);
    return root;
}

static long bst_inorder_sum(const TNode *root)
{
    if (root == 0)
        return 0;
    return bst_inorder_sum(root->l) + root->v + bst_inorder_sum(root->r);
}

static int bst_height(const TNode *root)
{
    int hl, hr;
    if (root == 0)
        return 0;
    hl = bst_height(root->l);
    hr = bst_height(root->r);
    return 1 + (hl > hr ? hl : hr);
}

/* ------------------------------------------------------------------ */
/* drivers                                                             */
/* ------------------------------------------------------------------ */

static Rectangle g_rect;
static Circle    g_circ;
static Triangle  g_tri;
static World     g_world;

static void test_shapes(void)
{
    printf("shapes:\n");

    rect_init(&g_rect, 1, 4, 6);
    circ_init(&g_circ, 2, 5);
    tri_init(&g_tri, 3, 3, 4);

    /* dispatch through the base pointer (up-cast) */
    shape_describe((Shape *)&g_rect);
    shape_describe((Shape *)&g_circ);
    shape_describe((Shape *)&g_tri);

    check_l("rect.area",  shape_area((Shape *)&g_rect),  2400L);
    check_l("rect.perim", shape_perim((Shape *)&g_rect), 2000L);
    check_l("circ.area",  shape_area((Shape *)&g_circ),  7854L);
    check_l("circ.perim", shape_perim((Shape *)&g_circ), 3141L);
    check_l("tri.area",   shape_area((Shape *)&g_tri),    600L);
    check_l("tri.perim",  shape_perim((Shape *)&g_tri),  1200L);

    /* virtual name through the vtable */
    check_s("rect.name", ((Shape *)&g_rect)->vt->name, "Rectangle");
    check_s("circ.name", ((Shape *)&g_circ)->vt->name, "Circle");
    check_s("tri.name",  ((Shape *)&g_tri)->vt->name,  "Triangle");
}

static void test_dispatch_table(void)
{
    Shape *s = (Shape *)&g_rect;
    printf("dispatch table:\n");
    check_l("query.area",  run_query(s, 0), 2400L);
    check_l("query.perim", run_query(s, 1), 2000L);
    printf("  query[0]=%s query[1]=%s\n", QUERIES[0].label, QUERIES[1].label);
}

static void test_world(void)
{
    long before, after;

    printf("world:\n");
    world_init(&g_world);
    world_add(&g_world, (Shape *)&g_rect);
    world_add(&g_world, (Shape *)&g_circ);
    world_add(&g_world, (Shape *)&g_tri);
    check_i("world.count", g_world.count, 3);

    g_world.area_accum = 0;
    world_visit(&g_world, accum_area_visitor);
    before = g_world.area_accum;
    check_l("world.area_sum", before, 10854L);
    printf("  total area before scale=");
    put_fixed(before);
    printf("\n");

    /* polymorphic mutation, then re-measure: each shape doubled linearly,
       so areas grow ~4x (circle/rect) and triangle 4x too. */
    world_visit(&g_world, scale_all_visitor);
    g_world.area_accum = 0;
    world_visit(&g_world, accum_area_visitor);
    after = g_world.area_accum;

    /* rect 8x12 -> 9600; circle r10 -> 31416; tri 6x8 -> 2400 = 43416 */
    check_l("world.area_after", after, 43416L);
    printf("  total area after 200%% scale=");
    put_fixed(after);
    printf("\n");
}

static Board g_board;

static void test_multidim(void)
{
    Tile *t;
    long w_struct, w_ptr;

    printf("multidim:\n");
    board_fill(&g_board);

    /* sum via pointer-to-2D-array parameter (array decay) */
    w_ptr = board_weight(g_board.grid, ROWS);
    check_l("board.weight_ptr", w_ptr, 138L);

    /* sum directly through the struct pointer (field 2D array stride) */
    w_struct = 0;
    {
        int r, c;
        for (r = 0; r < ROWS; r++)
            for (c = 0; c < COLS; c++)
                w_struct += g_board.grid[r][c].weight;
    }
    check_l("board.weight_struct", w_struct, 138L);

    /* address-of a struct-embedded 2D element returned across a call */
    t = tile_at(&g_board, 2, 3);
    check_i("tile_at.weight", t->weight, 23);
    check_i("tile_at.ch", (int)t->ch, (int)('A' + 2 * COLS + 3));

    cells_fill();
    check_l("cells.checksum", cells_checksum(), 222L);
    check_i("cells[2].tag", g_cells[2].tag, 200);
    printf("  board weight=%ld cells checksum=%ld\n",
           w_ptr, cells_checksum());
}

static void test_aggregates(void)
{
    Node *head = 0;
    TNode *root = 0;
    int seq[9];
    int i;

    printf("aggregates:\n");

    /* intrusive list of the original (pre-scale) shapes would have changed;
       rebuild fresh shapes so the area sum is deterministic. */
    rect_init(&g_rect, 1, 4, 6);
    circ_init(&g_circ, 2, 5);
    tri_init(&g_tri, 3, 3, 4);

    g_node_used = 0;
    head = list_push(head, (Shape *)&g_rect);
    head = list_push(head, (Shape *)&g_circ);
    head = list_push(head, (Shape *)&g_tri);
    check_l("list.area_sum", list_area_sum(head), 10854L);
    check_i("list.head_id", head->shape->id, 3);

    /* BST insert + recursive traversals */
    seq[0] = 5; seq[1] = 3; seq[2] = 8; seq[3] = 1; seq[4] = 4;
    seq[5] = 7; seq[6] = 9; seq[7] = 2; seq[8] = 6;
    g_tused = 0;
    for (i = 0; i < 9; i++)
        root = bst_insert(root, seq[i]);

    check_l("bst.sum", bst_inorder_sum(root), 45L);
    check_i("bst.root", root->v, 5);
    check_i("bst.height", bst_height(root), 4);
    printf("  list area sum=");
    put_fixed(list_area_sum(head));
    printf(" bst sum=%ld height=%d\n", bst_inorder_sum(root), bst_height(root));
}

int main(void)
{
    printf("too: OO-style C stress test for dcc\n");

    test_shapes();
    test_dispatch_table();
    test_world();
    test_multidim();
    test_aggregates();

    printf("ran %d checks, %d failures\n", g_checks, g_fails);
    if (g_fails == 0) {
        printf("too passed with great success\n");
        return 0;
    }
    printf("too FAILED\n");
    return 1;
}
