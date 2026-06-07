#include <ctype.h>
#include <stdlib.h>
#include <stdio.h>
#include <string.h>

static int failures = 0;

static void chk_int(int cond, const char *name)
{
    if (!cond) {
        printf("FAIL %s\n", name);
        failures++;
    }
}

int main(void)
{
    char *p;

    chk_int(isalpha('A') != 0, "isalpha A");
    chk_int(isalpha('z') != 0, "isalpha z");
    chk_int(isalpha('7') == 0, "isalpha 7");
    chk_int(isalnum('7') != 0, "isalnum 7");
    chk_int(isdigit('9') != 0, "isdigit 9");
    chk_int(isspace('\n') != 0, "isspace newline");
    chk_int(isupper('Q') != 0, "isupper Q");
    chk_int(islower('q') != 0, "islower q");
    chk_int(isxdigit('f') != 0, "isxdigit f");
    chk_int(isprint(' ') != 0, "isprint space");
    chk_int(iscntrl('\r') != 0, "iscntrl cr");
    chk_int(ispunct('!') != 0, "ispunct bang");
    chk_int(toupper('m') == 'M', "toupper m");
    chk_int(tolower('M') == 'm', "tolower M");

    p = (char *)malloc(4);
    if (!p) {
        printf("FAIL malloc 4\n");
        return 1;
    }

    strcpy(p, "abc");

    p = (char *)realloc(p, 8);
    if (!p) {
        printf("FAIL realloc grow null\n");
        return 1;
    }

    chk_int(strcmp(p, "abc") == 0, "realloc grow preserve");

    p[3] = 'd';
    p[4] = 0;

    p = (char *)realloc(p, 3);
    if (!p) {
        printf("FAIL realloc shrink null\n");
        return 1;
    }

    chk_int(p[0] == 'a', "realloc shrink byte0");
    chk_int(p[1] == 'b', "realloc shrink byte1");

    free(p);

    if (failures) {
        printf("ctype/realloc failed: %d\n", failures);
        return 1;
    }

    printf("ctype/realloc ok\n");
    return 0;
}
