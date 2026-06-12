/* tlongidx.c - regression for dcc long-index array copy bug.
 * Expected output:
 *   PASS longidx
 *
 * This mimics cint_preprocfix's old failure shape: a function copies
 * characters from one buffer to another using long indexes i/o.  The dcc
 * miscompile made the output appear empty or skipped all recognisable tokens.
 */
#include <stdio.h>
#include <string.h>
#include <ctype.h>

static char srcbuf[256];
static char outbuf[256];

static void copy_long_index(const char *in)
{
    long i;
    long o;
    int c;

    i = 0;
    o = 0;
    while (in[i]) {
        c = (unsigned char)in[i++];
        if (c == '/' && in[i] == '/') {
            while (in[i] && in[i] != '\n')
                i++;
            if (in[i] == '\n')
                outbuf[o++] = in[i++];
            continue;
        }
        if (c != 0x1a)
            outbuf[o++] = (char)c;
    }
    outbuf[o] = 0;
}

static int count_words_long_index(const char *s)
{
    long p;
    int n;
    int inword;
    int c;

    p = 0;
    n = 0;
    inword = 0;
    while (s[p]) {
        c = (unsigned char)s[p++];
        if (isalnum(c) || c == '_') {
            if (!inword) {
                n++;
                inword = 1;
            }
        } else {
            inword = 0;
        }
    }
    return n;
}

int main(void)
{
    strcpy(srcbuf,
           "#define X 1\n"
           "int main() {\n"
           "  int i; // comment to strip\n"
           "  i = 1;\n"
           "  return i;\n"
           "}\n");

    copy_long_index(srcbuf);

    if (outbuf[0] == 0) {
        printf("FAIL empty output\n");
        return 1;
    }
    if (strstr(outbuf, "int main") == 0) {
        printf("FAIL main missing: '%s'\n", outbuf);
        return 2;
    }
    if (count_words_long_index(outbuf) < 8) {
        printf("FAIL word count %d\n", count_words_long_index(outbuf));
        return 3;
    }

    printf("PASS tlongidx\n");
    return 0;
}
