/* sprintf tests */

#include <stdio.h>
#include <string.h>

static void check(const char *expect, const char *actual)
{
    if (strcmp(expect, actual) != 0) {
        printf("FAIL expected='%s' actual='%s'\n", expect, actual);
    }
}

int main()
{
    char buf[128];

    /* %d signed decimal */
    sprintf(buf, "%d", 0);
    check("0", buf);

    sprintf(buf, "%d", 1);
    check("1", buf);

    sprintf(buf, "%d", -1);
    check("-1", buf);

    sprintf(buf, "%d", 12345);
    check("12345", buf);

    sprintf(buf, "%d", -12345);
    check("-12345", buf);

    sprintf(buf, "%d", 32767);
    check("32767", buf);

    sprintf(buf, "%d", -32767);
    check("-32767", buf);

    /* %u unsigned decimal */
    sprintf(buf, "%u", 0);
    check("0", buf);

    sprintf(buf, "%u", 1);
    check("1", buf);

    sprintf(buf, "%u", 12345);
    check("12345", buf);

    sprintf(buf, "%u", 32767);
    check("32767", buf);

    /* %x hex */
    sprintf(buf, "%x", 0);
    check("0000", buf);

    sprintf(buf, "%x", 255);
    check("00ff", buf);

    sprintf(buf, "%x", 291);
    check("0123", buf);

    sprintf(buf, "%x", 2748);
    check("0abc", buf);

    sprintf(buf, "%x", 32767);
    check("7fff", buf);

    /* %c */
    sprintf(buf, "%c", 65);
    check("A", buf);

    sprintf(buf, "%c", 97);
    check("a", buf);

    sprintf(buf, "%c", 48);
    check("0", buf);

    /* %s */
    sprintf(buf, "%s", "hello");
    check("hello", buf);

    sprintf(buf, "%s", "world");
    check("world", buf);

    /* %% */
    sprintf(buf, "100%%");
    check("100%", buf);

    /* field width */
    sprintf(buf, "[%6d]", 0);
    check("[     0]", buf);

    sprintf(buf, "[%6d]", 42);
    check("[    42]", buf);

    sprintf(buf, "[%6d]", -42);
    check("[   -42]", buf);

    sprintf(buf, "[%6d]", 32767);
    check("[ 32767]", buf);

    sprintf(buf, "[%6u]", 0);
    check("[     0]", buf);

    sprintf(buf, "[%6u]", 42);
    check("[    42]", buf);

    sprintf(buf, "[%6s]", "abc");
    check("[   abc]", buf);

    sprintf(buf, "[%6s]", "hello");
    check("[ hello]", buf);

    sprintf(buf, "[%6x]", 255);
    check("[  00ff]", buf);

    sprintf(buf, "[%6x]", 2748);
    check("[  0abc]", buf);

    /* multiple arguments */
    sprintf(buf, "%d %d %d", 1, 2, 3);
    check("1 2 3", buf);

    sprintf(buf, "%s=%d", "ans", 42);
    check("ans=42", buf);

    sprintf(buf, "%c%c%c", 65, 66, 67);
    check("ABC", buf);

    printf("sprintf tests complted with great success\n");
    return 0;
}
