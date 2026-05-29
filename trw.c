/* tests unbuffer C89 I/O. This is tough for the crt on cp/m since that OS only supports full 128 byte record read/write */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <fcntl.h>
#include <unistd.h>

#define BLOCKS 256

char buf[511];

int sizes[16] = {
    1,
    2,
    3,
    17,
    63,
    64,
    65,
    127,
    128,
    129,
    130,
    251,
    255,
    256,
    257,
    0
};

int fail(char *msg)
{
    printf("error: %s\n", msg);
    exit(1);
    return 0;
}

int pat(int i)
{
    return (i * 37 + 11) & 255;
}

int rpat(int i)
{
    return (i * 53 + 0x40) & 255;
}

void fill_buf(int size, int v)
{
    v = v & 255;
    memset(buf, v, size);
}

void clear_buf(int size)
{
    memset(buf, 0x69, size);
}

void check_buf(int size, int v)
{
    int j;
    int got;

    v = v & 255;

    for (j = 0; j < size; j++) {
        got = *(buf + j);
        got = got & 255;

        if (got != v) {
            printf("bad data size=%d v=%x j=%x got=%x\n",
                   size, v, j, got);
            fail("verify failed");
        }
    }
}

void must_seek(int fd, int off, char *where)
{
    int result;

    result = lseek(fd, off, 0);

    if (result != off) {
        printf("seek result=%d off=%d\n", result, off);
        fail(where);
    }
}

void run_test(int size)
{
    int fd;
    int i;
    int result;
    int off;

    printf("testing size %d\n", size);

    fd = open("trw.dat", O_CREAT | O_RDWR | O_TRUNC, 0);
    if (fd < 0)
        fail("create");

    for (i = 0; i < BLOCKS; i++) {
        fill_buf(size, pat(i));
        check_buf(size, pat(i));

        result = write(fd, &buf[0], size);

        if (result != size) {
            printf("write result=%d i=%d\n", result, i);
            fail("write sequential");
        }
    }

    fsync(fd);
    close(fd);

    fd = open("trw.dat", O_RDONLY, 0);
    if (fd < 0)
        fail("open read");

    for (i = 0; i < BLOCKS; i++) {
        clear_buf(size);

        result = read(fd, &buf[0], size);

        if (result != size) {
            printf("read result=%d i=%d\n", result, i);
            fail("read sequential");
        }

        check_buf(size, pat(i));
    }

    close(fd);

    fd = open("trw.dat", O_RDWR, 0);
    if (fd < 0)
        fail("open rw");

    for (i = 0; i < BLOCKS; i++) {
        if ((i & 7) == 0) {
            off = i * size;

            must_seek(fd, off, "seek write");

            fill_buf(size, rpat(i));
            check_buf(size, rpat(i));

            result = write(fd, &buf[0], size);

            if (result != size) {
                printf("rewrite result=%d i=%d\n",
                       result, i);
                fail("rewrite");
            }
        }
    }

    fsync(fd);
    close(fd);

    fd = open("trw.dat", O_RDONLY, 0);
    if (fd < 0)
        fail("open read 2");

    for (i = BLOCKS - 1; i >= 0; i--) {
        off = i * size;

        must_seek(fd, off, "seek read");

        clear_buf(size);

        result = read(fd, &buf[0], size);

        if (result != size) {
            printf("read result=%d i=%d\n",
                   result, i);
            fail("read random");
        }

        if ((i & 7) == 0)
            check_buf(size, rpat(i));
        else
            check_buf(size, pat(i));
    }

    close(fd);
    unlink( "trw.dat" );

    printf("size %d passed\n", size);
}

int main()
{
    int i;

    printf("trw start\n");

    for (i = 0; sizes[i] != 0; i++)
        run_test(sizes[i]);

    printf("trw completed with great success\n");

    return 0;
}
