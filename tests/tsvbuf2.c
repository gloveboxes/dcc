/* tsvbuf2.c - exercise console buffering with CALLER-SUPPLIED buffers.
 *
 * This RTL ADOPTS the buffer you pass to setvbuf()/setbuf(): console output is
 * accumulated in YOUR array and drained (with BDOS-fn9 '$'-splitting) only when
 * it fills, on fflush(), before stdin input, or at program exit.  The tests
 * below prove the buffer is really used by inspecting its bytes after buffered
 * writes (the runtime stores '\n' as CR,LF), and stress the '$'-split flush
 * with leading/trailing/embedded/consecutive dollars and writes that overflow
 * the buffer.  Output is identical regardless of buffering mode. */

#include <stdio.h>
#include <stdlib.h>
#include <string.h>

static int g_fail = 0;

static char *make_buf(size_t n)
{
    char *b = (char *) malloc(n);
    if (b == 0) {
        printf("FAIL: malloc(%u) returned NULL\n", (unsigned) n);
        exit(1);
    }
    memset(b, 0, n);
    return b;
}

/* Confirm the caller's buffer holds the given bytes at offset 0, proving the
 * RTL is buffering console output into it. */
static void expect_prefix(const char *buf, const char *want, const char *tag)
{
    int n = (int) strlen(want);
    int i;
    for (i = 0; i < n; i = i + 1) {
        if (buf[i] != want[i]) {
            g_fail = g_fail + 1;
            printf("FAIL %s: buf[%d]=0x%02x want 0x%02x\n",
                   tag, i, (unsigned char) buf[i], (unsigned char) want[i]);
            return;
        }
    }
    printf("ok: %s adopted (buffered %d bytes)\n", tag, n);
}

int main(void)
{
    char *big;          /* the 4 KB buffer */
    char *line;
    int i;

    printf("tsvbuf2 start\n");

    /* ---- 1. allocate a 4 KB buffer and hand it to setvbuf ---- */
    big = make_buf(4096);
    if (setvbuf(stdout, big, _IOFBF, 4096) != 0)
        printf("FAIL: setvbuf 4K _IOFBF rejected\n");

    /* Fully buffered: newlines do NOT flush, so several lines accumulate in the
     * 4 KB buffer.  Their total is far below 4 KB, so nothing drains yet. */
    printf("4kbuf line A\n");
    printf("4kbuf line B\n");
    printf("4kbuf cost $5 and $10\n");

    /* The buffer must now contain those lines, with '\n' stored as CR,LF. */
    expect_prefix(big, "4kbuf line A\r\n4kbuf line B\r\n", "4K-IOFBF");

    fflush(stdout);     /* drain all three lines at once */

    /* ---- 2. '$' torture, all delivered while fully buffered in big[] ---- */
    printf("d1: leading $dollar\n");
    printf("d2: trailing dollar$\n");
    printf("d3: $only$dollars$\n");
    printf("d4: $$$$ four in a row\n");
    printf("d5: lone -> $\n");
    printf("$$$$$$$$\n");                  /* a whole line of dollars */
    printf("mix: a$b$c$d$e$f\n");
    fflush(stdout);

    /* ---- 3. a write that overflows a SMALL adopted buffer, forcing an
     *         internal flush partway through the line ---- */
    {
        char *small = make_buf(32);
        setvbuf(stdout, small, _IOFBF, 32);   /* capacity 31 before auto-flush */
        printf("small-buf line definitely longer than thirty-one bytes $x$\n");
        fflush(stdout);
        free(small);
    }

    /* ---- 4. a 200-char line with dollars, overflowing back into big[] ---- */
    setvbuf(stdout, big, _IOFBF, 4096);
    line = make_buf(256);
    for (i = 0; i < 200; i = i + 1) {
        line[i] = (char) ('0' + (i % 10));
        if (i % 37 == 0) line[i] = '$';
    }
    line[200] = '\n';
    line[201] = '\0';
    printf("boundary: %s", line);
    fflush(stdout);
    free(line);

    /* ---- 5. line buffering with the SAME 4 KB buffer ---- */
    setvbuf(stdout, big, _IOLBF, 4096);
    fputs("linebuf via fputs: cost is $7$\n", stdout);
    puts("linebuf via puts: $$ and done");

    /* ---- 6. unbuffered (NULL buffer reverts to the internal one) ---- */
    setvbuf(stdout, (char *) 0, _IONBF, 0);
    printf("nobuf: $a$b$ ");
    putchar('X');
    putchar('$');
    putchar('Y');
    putchar('\n');

    /* ---- 7. setbuf adopts a buffer (_IOFBF); setbuf(NULL) is unbuffered ---- */
    {
        char *sbuf = make_buf(BUFSIZ);
        setbuf(stdout, sbuf);
        printf("setbuf-adopt: $a$\n");
        expect_prefix(sbuf, "setbuf-adopt: $", "setbuf");
        fflush(stdout);
        free(sbuf);
        setbuf(stdout, (char *) 0);
        printf("setbuf-null: $unbuffered$\n");
    }

    /* ---- 8. fprintf to stdout and stderr (both are the console) ---- */
    fprintf(stdout, "fprintf stdout: %d dollars $%d$\n", 3, 100);
    fprintf(stderr, "fprintf stderr: warn $!$\n");

    /* ---- 9. invalid mode rejected ---- */
    if (setvbuf(stdout, (char *) 0, 7, 0) == 0)
        printf("FAIL: invalid mode 7 accepted\n");
    else
        printf("ok: invalid mode rejected\n");

    /* ---- 10. setvbuf on a real file must not reconfigure the console ---- */
    {
        FILE *fp;
        char *fbuf;

        memset(big, 0, 4096);
        setvbuf(stdout, big, _IOFBF, 4096);
        printf("file-noop A\n");

        fp = fopen("SVBUF2.TMP", "w");
        if (fp == 0) {
            printf("FAIL: fopen SVBUF2.TMP\n");
        } else {
            fbuf = make_buf(64);
            if (setvbuf(fp, fbuf, _IONBF, 64) != 0)
                printf("FAIL: file setvbuf rejected\n");
            setbuf(fp, fbuf);
            fprintf(fp, "file output $ok$\n");
            fclose(fp);
            free(fbuf);
        }

        printf("file-noop B\n");
        expect_prefix(big, "file-noop A\r\nfile-noop B\r\n", "file-noop");
        fflush(stdout);
        remove("SVBUF2.TMP");
    }

    /* Revert to the internal buffer BEFORE freeing big, so the exit-flush does
     * not touch freed memory, then leave a no-newline tail to prove exit-flush. */
    setvbuf(stdout, (char *) 0, _IOLBF, 0);
    free(big);

    if (g_fail == 0)
        printf("tsvbuf2 passed with great success\n");
    else
        printf("tsvbuf2 FAILED (%d)\n", g_fail);

    printf("tsvbuf2 done $tail-no-newline$");
    return 0;
}
