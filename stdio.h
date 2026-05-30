/* stdio.h - minimal CP/M-80 stdio declarations for dcc */

#ifndef _STDIO_H
#define _STDIO_H

#include <stddef.h>

typedef int FILE;

#define NULL    0
#define EOF     (-1)

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Standard streams (defined in dccrtl.mac as word variables) */
FILE *stdin;
FILE *stdout;
FILE *stderr;

/* Console / formatted output */
int printf(const char *format, ...);
int puts(const char *s);
int putchar(int c);
int perror(const char *s);

/* Console / formatted input */
int getchar(void);
int scanf(const char *format, ...);

/* Buffered file I/O (FILE *) */
FILE *fopen(const char *filename, const char *mode);
int   fclose(FILE *stream);
int   fflush(FILE *stream);
char *fgets(char *s, int n, FILE *stream);
int   fputs(const char *s, FILE *stream);
int   fread(void *ptr, int size, int nmemb, FILE *stream);
int   fwrite(const void *ptr, int size, int nmemb, FILE *stream);
int   fprintf(FILE *stream, const char *format, ...);
int   fscanf(FILE *stream, const char *format, ...);
int   fseek(FILE *stream, long offset, int whence);
long  ftell(FILE *stream);
void  rewind(FILE *stream);
int   feof(FILE *stream);
int   ferror(FILE *stream);

/* Formatted-string helpers */
int sprintf(char *s, const char *format, ...);
int sscanf(const char *s, const char *format, ...);

#endif /* _STDIO_H */
