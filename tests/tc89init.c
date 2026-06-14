#include <stdio.h>
#include <string.h>

struct Pair {
    char c;
    int n;
    long l;
};

static int gi[5] = { 1, 2 };
static char gs[] = "abc";
static struct Pair gp = { 'A', 1234, 56789L };
static struct Pair ga[2] = { { 'B', -2, -3000L }, { 'C', 3, 4000L } };

static int fails;

static void ci(const char *name, int got, int exp)
{
    if (got != exp) {
        printf("FAIL %s got %d expected %d\n", name, got, exp);
        fails++;
    }
}

static void cl(const char *name, long got, long exp)
{
    if (got != exp) {
        printf("FAIL %s got %ld expected %ld\n", name, got, exp);
        fails++;
    }
}

static void cs(const char *name, const char *got, const char *exp)
{
    if (strcmp(got, exp) != 0) {
        printf("FAIL %s got %s expected %s\n", name, got, exp);
        fails++;
    }
}

int main(void)
{
    char ls[] = "xyz";
    int li[4] = { 5, 6 };

    fails = 0;

    ci("gi0", gi[0], 1);
    ci("gi1", gi[1], 2);
    ci("gi2", gi[2], 0);
    ci("gi4", gi[4], 0);
    cs("gs", gs, "abc");

    ci("gp.c", gp.c, 'A');
    ci("gp.n", gp.n, 1234);
    cl("gp.l", gp.l, 56789L);

    ci("ga0.c", ga[0].c, 'B');
    ci("ga0.n", ga[0].n, -2);
    cl("ga0.l", ga[0].l, -3000L);
    ci("ga1.c", ga[1].c, 'C');
    ci("ga1.n", ga[1].n, 3);
    cl("ga1.l", ga[1].l, 4000L);

    cs("ls", ls, "xyz");
    ci("li0", li[0], 5);
    ci("li1", li[1], 6);
    ci("li2", li[2], 0);
    ci("li3", li[3], 0);

    if (fails) {
        printf("tc89init failed %d\n", fails);
        return 1;
    }
    printf("tc89init completed with great success\n");
    return 0;
}
