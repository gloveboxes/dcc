#include <stdio.h>
#include <stdlib.h>
#include <string.h>

struct Word { char name[18]; int kind; int val; };
struct State { struct Word *words; int nw; int *st; int sp; };
static struct State *G;
#define words G->words
#define nw G->nw
#define st G->st
#define sp G->sp

static void push(int v) { st[sp++] = v; }
static int pop(void) { return st[--sp]; }

static int add_word(const char *name, int kind, int val)
{
    int i;
    struct Word *wp;
    i = nw++;
    wp = words + i;
    memset(wp, 0, sizeof(struct Word));
    strcpy(wp->name, name);
    wp->kind = kind;
    wp->val = val;
    return i;
}

static int find_word(const char *name)
{
    int i;
    for (i = nw - 1; i >= 0; --i)
        if (!strcmp((words + i)->name, name)) return i;
    return -1;
}

int main(void)
{
    int w, v;
    G = (struct State *)calloc(1, sizeof(struct State));
    words = (struct Word *)calloc(4, sizeof(struct Word));
    st = (int *)calloc(8, sizeof(int));
    push(200);
    v = pop();
    add_word("DIGITS", 3, v);
    add_word("ARRAY", 4, 0);
    w = find_word("DIGITS");
    push((words + w)->val);
    v = pop();
    printf("v=%d need=%d\n", v, 2 + v);
    return v == 200 ? 0 : 1;
}
