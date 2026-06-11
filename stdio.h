/* stdio.h - minimal CP/M-80 stdio declarations for dcc */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>

#ifndef NULL
#define NULL    0
#endif
#define EOF     (-1)

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

#define BUFSIZ 256 /* C89 (7.9.2) minimum */

/* Max streams open at once.  C89 (7.9.3) requires FOPEN_MAX >= 8 including the
 * three standard streams.  In this runtime stdin/stdout/stderr are console
 * pseudo-FILEs that do not use a real-file slot, and NFILES real files (fd 3..)
 * can be open concurrently; keep this in sync with NFILES in dccrtl.mac. */
#define FOPEN_MAX   8

typedef int FILE;

/* Standard streams (defined in dccrtl.mac as word variables) */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Console / formatted output */
int printf(const char *format, ...);
int puts(const char *s);
int putchar(int c);
void perror(const char *s);

/* Console / formatted input */
int getchar(void);
int scanf(const char *format, ...);

/* Buffered file I/O (FILE *) */
FILE  *fopen(const char *filename, const char *mode);
int    fclose(FILE *stream);
int    fflush(FILE *stream);
char  *fgets(char *s, int n, FILE *stream);
int    fputs(const char *s, FILE *stream);
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
int    fprintf(FILE *stream, const char *format, ...);
int    fscanf(FILE *stream, const char *format, ...);
int    fseek(FILE *stream, long offset, int whence);
long   ftell(FILE *stream);
void   rewind(FILE *stream);
int    feof(FILE *stream);
int    ferror(FILE *stream);
void   clearerr(FILE *stream);

/* Formatted-string helpers */
int sprintf(char *s, const char *format, ...);
int sscanf(const char *s, const char *format, ...);

#endif /* _STDIO_H */
