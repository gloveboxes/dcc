#include <stdio.h>
#include <errno.h>

/* DCC/CP-M low-level file API prototypes. */
extern int open(const char *path, int flags);
extern int close(int fd);
extern int read(int fd, void *buf, unsigned int count);
extern int write(int fd, const void *buf, unsigned int count);
extern long lseek(int fd, long offset, int whence);
extern int unlink(const char *path);
extern char *strerror(int errnum);

#define O_RDONLY 0
#define O_RDWR   2
#define O_CREAT  0x0040
#define O_TRUNC  0x0200

#define SEEK_SET 0
#define SEEK_CUR 1
#define SEEK_END 2

static int fails;

static void expect_errno(const char *name, int rv, int want)
{
    if (rv == -1 && errno == want) {
        printf("PASS %s errno=%d %s\n", name, errno, strerror(errno));
    } else {
        printf("FAIL %s rv=%d errno=%d expected rv=-1 errno=%d\n",
               name, rv, errno, want);
        fails++;
    }
}

static void expect_long_errno(const char *name, long rv, int want)
{
    if (rv == -1L && errno == want) {
        printf("PASS %s errno=%d %s\n", name, errno, strerror(errno));
    } else {
        printf("FAIL %s rv=%ld errno=%d expected rv=-1 errno=%d\n",
               name, rv, errno, want);
        fails++;
    }
}

static void expect_ok_fd(const char *name, int fd)
{
    if (fd >= 3) {
        printf("PASS %s fd=%d\n", name, fd);
    } else {
        printf("FAIL %s fd=%d errno=%d\n", name, fd, errno);
        fails++;
    }
}

int main(void)
{
    int fd;
    int fd0, fd1, fd2, fd3, fd4;
    char buf[4];
    char c;
    int r;
    long lr;

    fails = 0;
    c = 'X';
    errno = 0;

    /* Make the test repeatable.  These may fail with ENOENT; ignore here. */
    unlink("TE0.TMP");
    unlink("TE1.TMP");
    unlink("TE2.TMP");
    unlink("TE3.TMP");
    unlink("TE4.TMP");
    unlink("NOFILE.X");

    errno = 0;
    fd = open("NOFILE.X", O_RDONLY);
    expect_errno("open missing", fd, ENOENT);

    errno = 0;
    r = unlink("NOFILE.X");
    expect_errno("unlink missing", r, ENOENT);

    errno = 0;
    r = read(99, buf, sizeof(buf));
    expect_errno("read bad fd", r, EBADF);

    errno = 0;
    r = write(99, &c, 1);
    expect_errno("write bad fd", r, EBADF);

    errno = 0;
    r = close(99);
    expect_errno("close bad fd", r, EBADF);

    errno = 0;
    lr = lseek(99, 0L, SEEK_SET);
    expect_long_errno("lseek bad fd", lr, EBADF);

    errno = 0;
    fd = open("TE0.TMP", O_CREAT | O_TRUNC | O_RDWR);
    expect_ok_fd("open create", fd);

    if (fd >= 3) {
        errno = 0;
        lr = lseek(fd, 0L, 99);
        expect_long_errno("lseek bad whence", lr, EINVAL);

        close(fd);
    }

    errno = 0;
    fd0 = open("TE0.TMP", O_CREAT | O_TRUNC | O_RDWR);
    fd1 = open("TE1.TMP", O_CREAT | O_TRUNC | O_RDWR);
    fd2 = open("TE2.TMP", O_CREAT | O_TRUNC | O_RDWR);
    fd3 = open("TE3.TMP", O_CREAT | O_TRUNC | O_RDWR);
    fd4 = open("TE4.TMP", O_CREAT | O_TRUNC | O_RDWR);

    if (fd0 >= 3 && fd1 >= 3 && fd2 >= 3 && fd3 >= 3) {
        expect_errno("open too many", fd4, EMFILE);
    } else {
        printf("FAIL setup open slots fd=%d,%d,%d,%d errno=%d\n",
               fd0, fd1, fd2, fd3, errno);
        fails++;
        if (fd4 >= 3)
            close(fd4);
    }

    if (fd0 >= 3) close(fd0);
    if (fd1 >= 3) close(fd1);
    if (fd2 >= 3) close(fd2);
    if (fd3 >= 3) close(fd3);

    unlink("TE0.TMP");
    unlink("TE1.TMP");
    unlink("TE2.TMP");
    unlink("TE3.TMP");
    unlink("TE4.TMP");

    if (fails) {
        printf("terrno failed: %d\n", fails);
        return 1;
    }

    printf("terrno passed\n");
    return 0;
}
