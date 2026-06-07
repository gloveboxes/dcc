#include <stdio.h>

int g = 1234;
int *pg = &g;
char *gps = "hi";
char *gpa[3] = { "a", "bc", 0 };
int ga[5] = { 1, 2 };
int gb[2][3] = { { 1, 2 }, { 3 } };

struct Inner { int x; char name[4]; };
struct Outer { int a; struct Inner in; int v[3]; char *p; };

struct Outer go = { 7, { 8, "xy" }, { 9 }, "zz" };

int fail;

void ck(n, ok)
int n;
int ok;
{
    if (!ok) {
        printf("FAIL init %d\n", n);
        fail = fail + 1;
    }
}

int sum_local(x)
int x;
{
    int a[4] = { x, x + 1, 0 };
    int b[2][3] = { { x + 2 }, { x + 3, x + 4 } };
    struct Outer lo = { x + 5, { x + 6, "ab" }, { x + 7, 0, x + 8 }, "qq" };

    ck(20, a[0] == x && a[1] == x + 1 && a[2] == 0 && a[3] == 0);
    ck(21, b[0][0] == x + 2 && b[0][1] == 0 && b[0][2] == 0);
    ck(22, b[1][0] == x + 3 && b[1][1] == x + 4 && b[1][2] == 0);
    ck(23, lo.a == x + 5 && lo.in.x == x + 6);
    ck(24, lo.in.name[0] == 'a' && lo.in.name[1] == 'b' && lo.in.name[2] == 0);
    ck(25, lo.v[0] == x + 7 && lo.v[1] == 0 && lo.v[2] == x + 8);
    ck(26, lo.p[0] == 'q' && lo.p[1] == 'q' && lo.p[2] == 0);
    return a[0] + a[1] + b[1][1] + lo.v[2];
}

int main()
{
    ck(1, pg == &g && *pg == 1234);
    ck(2, gps[0] == 'h' && gps[1] == 'i' && gps[2] == 0);
    ck(3, gpa[0][0] == 'a' && gpa[1][0] == 'b' && gpa[1][1] == 'c' && gpa[2] == 0);
    ck(4, ga[0] == 1 && ga[1] == 2 && ga[2] == 0 && ga[4] == 0);
    ck(5, gb[0][0] == 1 && gb[0][1] == 2 && gb[0][2] == 0);
    ck(6, gb[1][0] == 3 && gb[1][1] == 0 && gb[1][2] == 0);
    ck(7, go.a == 7 && go.in.x == 8 && go.in.name[0] == 'x' && go.in.name[2] == 0);
    ck(8, go.v[0] == 9 && go.v[1] == 0 && go.v[2] == 0);
    ck(9, go.p[0] == 'z' && go.p[1] == 'z' && go.p[2] == 0);
    ck(10, sum_local(10) == 10 + 11 + 14 + 18);

    if (fail) {
        printf("tc89init failed: %d\n", fail);
        return 1;
    }
    printf("tc89init ok\n");
    return 0;
}
