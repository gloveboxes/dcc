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

/* TE0..TE7 fill all NFILES (8) real-file slots; TE8 is the overflow that must
 * fail with EMFILE.  Keep this count in sync with NFILES / FOPEN_MAX. */
static const char *tmpnames[9] = {
    "TE0.TMP", "TE1.TMP", "TE2.TMP", "TE3.TMP",
    "TE4.TMP", "TE5.TMP", "TE6.TMP", "TE7.TMP", "TE8.TMP"
};

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
    int fds[8];
    int fdover;
    int i;
    int allok;
    char buf[4];
    char c;
    int r;
    long lr;

    fails = 0;
    c = 'X';
    errno = 0;

    /* Make the test repeatable.  These may fail with ENOENT; ignore here. */
    for (i = 0; i < 9; i++)
        unlink(tmpnames[i]);
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
    allok = 1;
    for (i = 0; i < 8; i++) {
        fds[i] = open(tmpnames[i], O_CREAT | O_TRUNC | O_RDWR);
        if (fds[i] < 3)
            allok = 0;
    }

    /* One more than NFILES must fail with EMFILE. */
    errno = 0;
    fdover = open(tmpnames[8], O_CREAT | O_TRUNC | O_RDWR);

    if (allok) {
        expect_errno("open too many", fdover, EMFILE);
    } else {
        printf("FAIL setup open slots errno=%d\n", errno);
        fails++;
        if (fdover >= 3)
            close(fdover);
    }

    for (i = 0; i < 8; i++)
        if (fds[i] >= 3)
            close(fds[i]);

    for (i = 0; i < 9; i++)
        unlink(tmpnames[i]);

    if (fails) {
        printf("terrno failed: %d\n", fails);
        return 1;
    }

    printf("terrno passed\n");
    return 0;
}
