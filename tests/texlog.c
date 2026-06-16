/* texlog.c - worked example: a printf-style logging wrapper via vfprintf.
 * Pulled into docs/docs/en/12-examples.md via the snippet markers below. */

#include <stdio.h>
#include <stdarg.h>

/* --8<-- [start:example] */
static void logmsg(const char *fmt, ...)
{
    va_list ap;
    va_start(ap, fmt);
    vfprintf(stderr, fmt, ap);
    va_end(ap);
}

int main(void)
{
    logmsg("ready: %d items, %lx flags\n", 3, 0xBEEFL);
    return 0;
}
/* --8<-- [end:example] */
