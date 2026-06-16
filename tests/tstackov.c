/*
 * tstackov.c - exercises the lightweight stack-overflow guard (dcc
 * -fstack-check).  The DCC_STACK_CHECK marker below tells ma.sh / ma.bat to
 * build this one program with -fstack-check; every other test stays unguarded.
 *
 * Each recursion level allocates a local frame, so unbounded recursion walks
 * the C stack down into the heap region.  With the guard enabled the runtime
 * prints "?stack overflow" from the function prologue and exits to CP/M
 * (return code 0FFh) instead of silently corrupting memory.  Without the guard
 * the same recursion would scribble over the heap and crash unpredictably.
 *
 * DCC_STACK_CHECK
 */
#include <stdio.h>

/* volatile-ish sink so the optimizer cannot discard the frame or the call. */
int g_sink;

int descend(int depth)
{
    int local[8];
    int i;

    for (i = 0; i < 8; ++i)
        local[i] = depth + i;

    g_sink = local[depth & 7];

    /* Unbounded recursion: returns only after the guard aborts the program. */
    return descend(depth + 1) + local[0];
}

int main(void)
{
    printf("tstackov start\n");
    g_sink = descend(1);
    /* Unreachable: the stack guard exits before we return from descend(). */
    printf("tstackov should not reach here %d\n", g_sink);
    return 0;
}
