#include <stdio.h>

extern int printf();
extern int memset();

struct Pair {
    char a;
    int b;
    char c;
};

struct Node {
    int value;
    struct Pair *pairp;
};

struct Pair g_pair;
struct Pair g_arr[2];
struct Node g_node;

int main()
{
    struct Pair local;
    struct Pair *pp;

    g_pair.a = 3;
    g_pair.b = 1000;
    g_pair.c = 7;

    local.a = 4;
    local.b = 2000;
    local.c = 8;

    g_arr[0].a = 1;
    g_arr[0].b = 11;
    g_arr[0].c = 2;

    g_arr[1].a = 5;
    g_arr[1].b = 22;
    g_arr[1].c = 6;

    pp = &g_arr[1];

    g_node.value = 1234;
    g_node.pairp = pp;

    printf("g=%d %d %d\n", g_pair.a, g_pair.b, g_pair.c);
    printf("l=%d %d %d\n", local.a, local.b, local.c);
    printf("a=%d %d %d\n", g_arr[1].a, g_arr[1].b, g_arr[1].c);
    printf("p=%d %d %d\n", pp->a, pp->b, pp->c);
    printf("n=%d %d\n", g_node.value, g_node.pairp->b);

    return 0;
}
