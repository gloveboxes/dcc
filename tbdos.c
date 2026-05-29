/* tbdos.c - simple CP/M BDOS validation test */

#include <stdio.h>

extern int bdos(int fn, int dearg);

static void putstr(const char *s)
{
    while (*s)
        bdos(2, *s++);
}

int main(void)
{
    int ver;

    printf("tbdos start\n");

    /* BDOS 2: console output */
    bdos(2, 'H');
    bdos(2, 'i');
    bdos(2, '!');
    bdos(2, '\r');
    bdos(2, '\n');

    /* BDOS 9: print $-terminated string */
    {
        static char msg[] = "BDOS 9 string output works$";
        bdos(9, (int)msg);
        bdos(2, '\r');
        bdos(2, '\n');
    }

    /* BDOS 12: get CP/M version */
    ver = bdos(12, 0);

    printf("BDOS version raw: %u\n", ver);

    /* low byte usually contains version, e.g. 0x22 for CP/M 2.2 */
    printf("BDOS version major=%u minor=%u\n",
           (ver >> 4) & 0x0f,
           ver & 0x0f);

    /* BDOS 11: console status */
    printf("Console status: %u\n", bdos(11, 0));

    /* BDOS 6: direct console I/O status poll */
    printf("Direct console poll: %u\n", bdos(6, 0xff));

    putstr("tbdos completed with great success\r\n");

    return 0;
}
