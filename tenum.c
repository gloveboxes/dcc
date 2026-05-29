/* C89 enum regression test */
#include <stdio.h>

enum Color {
    RED,
    GREEN = 5,
    BLUE,
    CYAN = -1
};

enum State {
    OFF = 0,
    ON = 1
};

int main(void)
{
    enum Color c;
    enum State s;
    int ok;

    c = BLUE;
    s = ON;

    ok = 1;

    if (RED != 0) ok = 0;
    if (GREEN != 5) ok = 0;
    if (BLUE != 6) ok = 0;
    if (CYAN != -1) ok = 0;

    if (c != 6) ok = 0;
    if (s != 1) ok = 0;

    c = CYAN;
    if ((int)c != -1) ok = 0;

    if (!ok) {
        printf("enum test failed\n");
        return 1;
    }

    printf("enum test passed with great success\n");
    return 0;
}
