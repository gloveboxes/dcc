/* stdio.h - minimal CP/M-80 stdio declarations for dcc */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>
#include <stdarg.h>

/** Null pointer constant, defined as 0 if not already defined. */
#ifndef NULL
#define NULL    0
#endif
/** End-of-file / error return value, -1. */
#define EOF     (-1)

/** fseek origin: beginning of file. */
#define SEEK_SET    0
/** fseek origin: current file position. */
#define SEEK_CUR    1
/** fseek origin: end of file. */
#define SEEK_END    2

/** Default buffer size, 256. */
#define BUFSIZ 256 /* C89 (7.9.2) minimum */
/** Minimum buffer size for tmpnam(), 13. */
#define L_tmpnam 13
/** Distinct filenames available from tmpnam() per session, 1000. */
#define TMP_MAX 1000

/* setvbuf() buffering modes (C89 7.9.1). */
/** Fully buffered setvbuf mode. */
#define _IOFBF 0
/** Line buffered setvbuf mode. */
#define _IOLBF 1
/** Unbuffered setvbuf mode. */
#define _IONBF 2

/** Maximum C89 stream count exposed by the header, 8. */
/* C89 (7.9.3) requires FOPEN_MAX >= 8 including the
 * three standard streams.  In this runtime stdin/stdout/stderr are console
 * pseudo-FILEs that do not use a real-file slot, and NFILES real files (fd 3..)
 * can be open concurrently; keep this in sync with NFILES in dccrtl.mac. */
#define FOPEN_MAX   8

/** Stream handle type. */
typedef int FILE;

/* Standard streams (defined in dccrtl.mac as word variables) */
/** Predefined console input stream. */
extern FILE *stdin;
/** Predefined console output stream. */
extern FILE *stdout;
/** Predefined console diagnostic stream. */
extern FILE *stderr;

/* Console / formatted output */
/** Formatted output to the console stdout. */
int printf(const char *format, ...);
/** Write a string plus a newline to the console. */
int puts(const char *s);
/** Write one character to the console. */
int putchar(int c);
/** Print a prefix plus the current error text. */
void perror(const char *s);

/* Console / formatted input */
/** Read one character from the console, blocking until available. */
int getchar(void);
/** Unsafe console line input with no bounds checking; prefer fgets. */
char *gets(char *s);
/** Parse formatted input from the console stdin. */
int scanf(const char *format, ...);

/* Non-echoing / polling console input (compiler maps kbhit->__kbht, getch->__gtch) */
/** Poll for a waiting console key without consuming it. */
int kbhit(void);
/** Read one console key without echo. */
int getch(void);

/* Single-character stream I/O.  The compiler maps these C names to runtime
 * entry points (getc->__getc, putc->__putc, fputc->__fpc), so calls link
 * regardless; these prototypes add type-checking and editor/IntelliSense
 * resolution. */
/** Read one character from a stream. */
int getc(FILE *stream);
/** Same as getc. */
int fgetc(FILE *stream);
/** Write one character to a stream. */
int putc(int c, FILE *stream);
/** Same as putc. */
int fputc(int c, FILE *stream);

/* Buffered file I/O (FILE *) */
/** Open a file stream. */
FILE  *fopen(const char *filename, const char *mode);
/** Delete a file. */
int    remove(const char *filename);
/** Close a stream. */
int    fclose(FILE *stream);
/** Flush buffered console output. */
int    fflush(FILE *stream);
/** Read a line from a stream. */
char  *fgets(char *s, int n, FILE *stream);
/** Write a string to a stream without adding a newline. */
int    fputs(const char *s, FILE *stream);
/** Read elements from a stream. */
size_t fread(void *ptr, size_t size, size_t nmemb, FILE *stream);
/** Write elements to a stream. */
size_t fwrite(const void *ptr, size_t size, size_t nmemb, FILE *stream);
/** Formatted output to a stream. */
int    fprintf(FILE *stream, const char *format, ...);
/** Parse formatted input from a stream. */
int    fscanf(FILE *stream, const char *format, ...);
/** Reposition a stream. */
int    fseek(FILE *stream, long offset, int whence);
/** Report the current stream position. */
long   ftell(FILE *stream);
/** Reposition a stream to the beginning. */
void   rewind(FILE *stream);
/** Test whether a stream has reached end-of-file. */
int    feof(FILE *stream);
/** Test whether a stream has an error flag set. */
int    ferror(FILE *stream);
/** Clear a stream's EOF and error flags. */
void   clearerr(FILE *stream);
/** Configure console buffering with a default size. */
void   setbuf(FILE *stream, char *buf);
/** Configure console buffering mode and buffer storage. */
int    setvbuf(FILE *stream, char *buf, int mode, size_t size);
/** Generate a filename not used by any current tmpfile() handle; if s is
 *  non-NULL write it there, else use an internal static buffer. */
char  *tmpnam(char *s);
/** Create a temporary file opened for update; removed at program exit. */
FILE  *tmpfile(void);

/* Formatted-string helpers */
/** Formatted output into a caller-supplied string buffer. */
int sprintf(char *s, const char *format, ...);
/** Parse formatted input from a string. */
int sscanf(const char *s, const char *format, ...);

/* Variadic (va_list) formatted output (C89 7.9.6.7-9) */
/** Formatted console output from a va_list. */
int vprintf(const char *format, va_list ap);
/** Formatted stream output from a va_list. */
int vfprintf(FILE *stream, const char *format, va_list ap);
/** Formatted string output from a va_list. */
int vsprintf(char *s, const char *format, va_list ap);

#endif /* _STDIO_H */
