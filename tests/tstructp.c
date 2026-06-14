#include <stdio.h>

struct Pair { int a; long b; char c; };

static long sum_pair(struct Pair p)
{
    return p.a + p.b + p.c;
}

int main(int argc, char **argv)
{
    struct Pair x;
    struct Pair y;
    char buf[sizeof(struct Pair) * 2];
    struct Pair *p;
    struct Pair *q;

    x.a = 3;
    x.b = 1000;
    x.c = 7;

    p = (struct Pair *)buf;
    q = (struct Pair *)(buf + sizeof(struct Pair));

    *p = x;
    y = *p;
    printf("deref copy %d %ld %d %ld\n", y.a, y.b, y.c, sum_pair(y));

    q->a = 4;
    q->b = 2000;
    q->c = 8;
    *(struct Pair *)buf = *q;
    y = *(struct Pair *)buf;
    printf("cast copy %d %ld %d %ld\n", y.a, y.b, y.c, sum_pair(y));

    printf("tstructptr completed\n");
    return 0;
}
