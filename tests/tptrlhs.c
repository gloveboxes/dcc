/* tptrlhs.c - C89 regression test for complex pointer-expression lvalues.
 *
 * Intended for dcc / CP/M style targets: no stdlib dependency beyond printf.
 * Focus: assignments where the left hand side is reached through combinations
 * of struct fields, arrays, pointer arithmetic, globals, locals, and multiple
 * levels of indirection.
 *
 * Expected final output:
 * tptrlhs start
 * PASS
 */

#include <stdio.h>

struct Leaf {
    int v;
    int a[4];
};

struct Node {
    struct Leaf leaf[3];
    struct Leaf *pl;
    int *pi;
    int m[2][3];
};

struct Wrapper {
    struct Node n[2];
    struct Node *pn;
    struct Leaf *lp[3];
    int *ip[4];
};

static struct Leaf gleaf[5];
static struct Node gnode[3];
static struct Wrapper gwrap[2];
static int gint[16];
static int gmat[3][4];
static struct Wrapper *gpwrap;
static struct Node *gpnode;
static struct Leaf *gpleaf;
static int *gpint;

static int fails;

static void check_int(name, got, exp)
char *name;
int got;
int exp;
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails = fails + 1;
    }
}

static void init_all()
{
    int i, j, k;

    for (i = 0; i < 5; ++i) {
        gleaf[i].v = 0;
        for (j = 0; j < 4; ++j) gleaf[i].a[j] = 0;
    }

    for (i = 0; i < 3; ++i) {
        gnode[i].pl = &gleaf[i];
        gnode[i].pi = &gint[i * 2];
        for (j = 0; j < 3; ++j) {
            gnode[i].leaf[j].v = 0;
            for (k = 0; k < 4; ++k) gnode[i].leaf[j].a[k] = 0;
        }
        for (j = 0; j < 2; ++j)
            for (k = 0; k < 3; ++k)
                gnode[i].m[j][k] = 0;
    }

    for (i = 0; i < 16; ++i) gint[i] = 0;
    for (i = 0; i < 3; ++i)
        for (j = 0; j < 4; ++j)
            gmat[i][j] = 0;

    for (i = 0; i < 2; ++i) {
        gwrap[i].pn = &gnode[i];
        for (j = 0; j < 3; ++j) gwrap[i].lp[j] = &gleaf[j + i];
        for (j = 0; j < 4; ++j) gwrap[i].ip[j] = &gint[i * 4 + j];
        for (j = 0; j < 2; ++j) {
            gwrap[i].n[j].pl = &gleaf[i + j];
            gwrap[i].n[j].pi = &gint[8 + i * 2 + j];
            for (k = 0; k < 3; ++k) {
                gwrap[i].n[j].leaf[k].v = 0;
                gwrap[i].n[j].leaf[k].a[0] = 0;
                gwrap[i].n[j].leaf[k].a[1] = 0;
                gwrap[i].n[j].leaf[k].a[2] = 0;
                gwrap[i].n[j].leaf[k].a[3] = 0;
            }
            gwrap[i].n[j].m[0][0] = 0;
            gwrap[i].n[j].m[0][1] = 0;
            gwrap[i].n[j].m[0][2] = 0;
            gwrap[i].n[j].m[1][0] = 0;
            gwrap[i].n[j].m[1][1] = 0;
            gwrap[i].n[j].m[1][2] = 0;
        }
    }

    gpwrap = &gwrap[0];
    gpnode = &gnode[0];
    gpleaf = &gleaf[0];
    gpint = &gint[0];
}

static void touch_params(wpp, npp, lpp, ipp)
struct Wrapper **wpp;
struct Node **npp;
struct Leaf **lpp;
int **ipp;
{
    int ix;

    ix = 1;

    /* Through pointer-to-pointer parameters. */
    (*wpp)->n[ix].leaf[ix + 1].a[ix] = 101;
    (*wpp + 1)->n[0].leaf[2].v = 102;
    (*npp)->leaf[2].a[3] = 103;
    (*npp + 1)->pl->a[2] = 104;
    (*lpp)->a[1] = 105;
    (*lpp + 2)->v = 106;
    *(*ipp + 5) = 107;
    (*ipp)[6] = 108;

    check_int("param wpp n leaf a", gwrap[0].n[1].leaf[2].a[1], 101);
    check_int("param wpp plus n leaf v", gwrap[1].n[0].leaf[2].v, 102);
    check_int("param npp leaf a", gnode[0].leaf[2].a[3], 103);
    check_int("param npp plus pl a", gleaf[1].a[2], 104);
    check_int("param lpp a", gleaf[0].a[1], 105);
    check_int("param lpp plus v", gleaf[2].v, 106);
    check_int("param ipp star plus", gint[5], 107);
    check_int("param ipp subscript", gint[6], 108);
}

static void touch_locals()
{
    struct Leaf lleaf[4];
    struct Node lnode[2];
    struct Wrapper lwrap;
    struct Wrapper *wp;
    struct Node *np;
    struct Leaf *lp;
    int lint[12];
    int lmat[2][5];
    int *ip;
    int i, j;

    for (i = 0; i < 4; ++i) {
        lleaf[i].v = 0;
        for (j = 0; j < 4; ++j) lleaf[i].a[j] = 0;
    }
    for (i = 0; i < 12; ++i) lint[i] = 0;
    for (i = 0; i < 2; ++i)
        for (j = 0; j < 5; ++j)
            lmat[i][j] = 0;

    lnode[0].pl = &lleaf[0];
    lnode[1].pl = &lleaf[1];
    lnode[0].pi = &lint[0];
    lnode[1].pi = &lint[6];
    for (i = 0; i < 2; ++i) {
        for (j = 0; j < 3; ++j) {
            lnode[i].leaf[j].v = 0;
            lnode[i].leaf[j].a[0] = 0;
            lnode[i].leaf[j].a[1] = 0;
            lnode[i].leaf[j].a[2] = 0;
            lnode[i].leaf[j].a[3] = 0;
        }
        lnode[i].m[0][0] = 0;
        lnode[i].m[0][1] = 0;
        lnode[i].m[0][2] = 0;
        lnode[i].m[1][0] = 0;
        lnode[i].m[1][1] = 0;
        lnode[i].m[1][2] = 0;
    }

    lwrap.n[0] = lnode[0];
    lwrap.n[1] = lnode[1];
    lwrap.pn = &lwrap.n[0];
    lwrap.lp[0] = &lleaf[0];
    lwrap.lp[1] = &lleaf[1];
    lwrap.lp[2] = &lleaf[2];
    lwrap.ip[0] = &lint[0];
    lwrap.ip[1] = &lint[3];
    lwrap.ip[2] = &lint[6];
    lwrap.ip[3] = &lint[9];

    wp = &lwrap;
    np = &lnode[0];
    lp = &lleaf[0];
    ip = &lint[0];

    /* Local arrays and local struct objects. */
    lmat[1][3] = 201;
    *(*(lmat + 0) + 4) = 202;
    (lp + 3)->a[2] = 203;
    (*(lp + 2)).v = 204;
    (np + 1)->leaf[2].a[1] = 205;
    (*(np + 0)).m[1][2] = 206;
    *(((np + 1)->pi) + 4) = 207;
    ((wp->pn) + 1)->pl->a[3] = 208;
    (*((wp->lp) + 2))->a[0] = 209;
    *((*(wp->ip + 1)) + 2) = 210;
    *(ip + (lmat[0][4] - 200)) = 211;       /* index 2 */

    check_int("local lmat subscript", lmat[1][3], 201);
    check_int("local lmat ptr", lmat[0][4], 202);
    check_int("local lp plus a", lleaf[3].a[2], 203);
    check_int("local lp plus v", lleaf[2].v, 204);
    check_int("local np leaf a", lnode[1].leaf[2].a[1], 205);
    check_int("local np m", lnode[0].m[1][2], 206);
    check_int("local np pi", lint[10], 207);
    check_int("local wp pn pl", lleaf[1].a[3], 208);
    check_int("local wp lp", lleaf[2].a[0], 209);
    check_int("local wp ip", lint[5], 210);
    check_int("local computed index", lint[2], 211);
}

static void touch_globals()
{
    int i;

    i = 1;

    /* Global arrays, global pointer vars, arrays of pointers, and matrices. */
    gleaf[i + 1].a[i] = 301;
    (gpleaf + 3)->v = 302;
    (*(gpleaf + 4)).a[2] = 303;
    gnode[2].leaf[1].a[3] = 304;
    (gpnode + 1)->m[1][2] = 305;
    *((gpnode + 2)->pi + 1) = 306;
    gwrap[1].lp[2]->a[3] = 307;
    (*((gwrap[0].lp) + 1))->v = 308;
    *(gwrap[1].ip[3]) = 309;
    *((*(gwrap[0].ip + 2)) + 1) = 310;
    gpwrap->pn->leaf[1].a[0] = 311;
    ((gpwrap + 1)->pn + 1)->m[0][2] = 312;
    (*(gpwrap + 1)).n[1].pl->v = 313;
    gmat[2][1] = 314;
    *(*(gmat + 1) + 3) = 315;
    *(gpint + 15) = 316;

    check_int("global gleaf a", gleaf[2].a[1], 301);
    check_int("global gpleaf v", gleaf[3].v, 302);
    check_int("global gpleaf a", gleaf[4].a[2], 303);
    check_int("global gnode leaf", gnode[2].leaf[1].a[3], 304);
    check_int("global gpnode m", gnode[1].m[1][2], 305);
    check_int("global gpnode pi", gint[5], 306);
    check_int("global gwrap lp", gleaf[3].a[3], 307);
    check_int("global gwrap lp star", gleaf[1].v, 308);
    check_int("global gwrap ip", gint[7], 309);
    check_int("global gwrap ip star", gint[3], 310);
    check_int("global gpwrap pn", gnode[0].leaf[1].a[0], 311);
    check_int("global gpwrap plus pn", gnode[2].m[0][2], 312);
    check_int("global gpwrap n pl", gleaf[2].v, 313);
    check_int("global gmat subscript", gmat[2][1], 314);
    check_int("global gmat ptr", gmat[1][3], 315);
    check_int("global gpint", gint[15], 316);
}

static void touch_alias_mix()
{
    struct Wrapper *wa[2];
    struct Node *na[3];
    struct Leaf *la[5];
    int *ia[4];
    int sel;

    wa[0] = &gwrap[0];
    wa[1] = &gwrap[1];
    na[0] = &gnode[0];
    na[1] = &gwrap[0].n[1];
    na[2] = &gwrap[1].n[0];
    la[0] = &gleaf[0];
    la[1] = &gnode[1].leaf[2];
    la[2] = &gwrap[0].n[0].leaf[1];
    la[3] = &gwrap[1].n[1].leaf[2];
    la[4] = &gleaf[4];
    ia[0] = &gint[0];
    ia[1] = &gint[4];
    ia[2] = &gmat[0][0];
    ia[3] = &gmat[2][0];

    sel = 1;

    (*(wa + sel))->n[sel].leaf[sel].a[sel + 1] = 401;
    ((*(wa + 0))->pn + sel)->leaf[0].v = 402;
    (*(na + 2))->pl->a[1] = 403;
    ((*(na + 1))->pl)->a[2] = 404;
    (*(la + 3))->a[3] = 405;
    (*(la + 1))->v = 406;
    *(*(ia + 0) + 11) = 407;
    *(*(ia + 1) + 3) = 408;
    *(*(ia + 2) + 6) = 409;  /* gmat[1][2] because gmat is contiguous */
    *(*(ia + 3) + 3) = 410;  /* gmat[2][3] */

    check_int("alias wa n leaf", gwrap[1].n[1].leaf[1].a[2], 401);
    check_int("alias wa pn plus", gnode[1].leaf[0].v, 402);
    check_int("alias na pl", gleaf[1].a[1], 403);
    check_int("alias na dot pl", gleaf[1].a[2], 404);
    check_int("alias la a", gwrap[1].n[1].leaf[2].a[3], 405);
    check_int("alias la v", gnode[1].leaf[2].v, 406);
    check_int("alias ia gint", gint[11], 407);
    check_int("alias ia gint offset", gint[7], 408);
    check_int("alias ia gmat mid", gmat[1][2], 409);
    check_int("alias ia gmat end", gmat[2][3], 410);
}

int main()
{
    struct Wrapper *wp;
    struct Node *np;
    struct Leaf *lp;
    int *ip;

    printf("tptrlhs start\n");
    init_all();

    touch_globals();
    touch_locals();

    wp = &gwrap[0];
    np = &gnode[0];
    lp = &gleaf[0];
    ip = &gint[0];
    touch_params(&wp, &np, &lp, &ip);

    touch_alias_mix();

    if (fails == 0) {
        printf("PASS\n");
        return 0;
    }
    printf("tptrlhs failed: %d\n", fails);
    return 1;
}
