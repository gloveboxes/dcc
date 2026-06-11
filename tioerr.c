/* tioerr.c - exercise stdio edge cases fixed in the runtime memory pass:
 *   - fread/fwrite with size == 0 or count == 0 return 0 (no divide-by-zero)
 *   - feof / clearerr interaction
 *   - ferror reports a healthy stream as not in error
 *   - fgets file reads (which drive the _read sector cache)
 */
#include <stdio.h>
#include <string.h>
#include <unistd.h>
#include <stdlib.h>

char buf[64];

int main(void)
{
    FILE *f;
    unsigned int r;
    char *q;

    unlink("tioerr.tmp");

    f = fopen("tioerr.tmp", "w");
    if (!f) {
        puts("fopen w failed");
        return 1;
    }
    fwrite("abcdef\n", 1, 7, f);
    fwrite("0123456789\n", 1, 11, f);
    fclose(f);

    f = fopen("tioerr.tmp", "r");
    if (!f) {
        puts("fopen r failed");
        return 1;
    }

    /* size == 0 and count == 0 must read nothing and not corrupt position */
    r = (unsigned int)fread(buf, 0, 10, f);
    printf("fread size0: %u\n", r);
    r = (unsigned int)fread(buf, 10, 0, f);
    printf("fread count0: %u\n", r);

    /* fgets reads one byte at a time on CP/M, exercising the read cache */
    q = fgets(buf, sizeof(buf), f);
    if (q)
        printf("line1: %s", buf);
    q = fgets(buf, sizeof(buf), f);
    if (q)
        printf("line2: %s", buf);

    /* now at end of file */
    q = fgets(buf, sizeof(buf), f);
    printf("eof fgets null: %d\n", q == 0);
    printf("feof: %d\n", feof(f) != 0);
    printf("ferror: %d\n", ferror(f) != 0);

    /* clearerr must reset the EOF indicator */
    clearerr(f);
    printf("feof after clearerr: %d\n", feof(f) != 0);

    /* rewind must seek back to offset 0 (it is defined as
     * (void)fseek(fp, 0L, SEEK_SET) plus clearing error/eof).  A rewind that
     * only cleared EOF without seeking would read stale data from the current
     * position here instead of the first bytes of the file. */
    rewind(f);
    r = (unsigned int)fread(buf, 1, 5, f);
    buf[5] = 0;
    printf("rewind read %u: %s\n", r, buf);
    fclose(f);

    /* fwrite size == 0 returns 0 items */
    f = fopen("tioerr.tmp", "r+");
    if (f) {
        r = (unsigned int)fwrite("xyz", 0, 5, f);
        printf("fwrite size0: %u\n", r);
        fclose(f);
    }

    unlink("tioerr.tmp");
    puts("tioerr completed with great success");
    return 0;
}
