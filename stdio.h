/* stdio.h - minimal CP/M-80 stdio declarations for dcc */

#ifndef _STDIO_H
#define _STDIO_H

typedef int FILE;

#define NULL    0
#define EOF     (-1)

#define SEEK_SET    0
#define SEEK_CUR    1
#define SEEK_END    2

/* Standard streams (defined in dccrtl.mac as word variables) */
extern FILE *stdin;
extern FILE *stdout;
extern FILE *stderr;

/* Console / formatted output */
extern int printf(const char *format, ...);
extern int puts(const char *s);
extern int putchar(int c);
extern int perror(const char *s);

/* Console / formatted input */
extern int getchar(void);
extern int scanf(const char *format, ...);

/* Buffered file I/O (FILE *) */
extern FILE *fopen(const char *filename, const char *mode);
extern int   fclose(FILE *stream);
extern int   fflush(FILE *stream);
extern char *fgets(char *s, int n, FILE *stream);
extern int   fputs(const char *s, FILE *stream);
extern int   fread(void *ptr, int size, int nmemb, FILE *stream);
extern int   fwrite(const void *ptr, int size, int nmemb, FILE *stream);
extern int   fprintf(FILE *stream, const char *format, ...);
extern int   fscanf(FILE *stream, const char *format, ...);
extern int   fseek(FILE *stream, long offset, int whence);
extern long  ftell(FILE *stream);
extern void  rewind(FILE *stream);
extern int   feof(FILE *stream);
extern int   ferror(FILE *stream);

/* Formatted-string helpers */
extern int sprintf(char *s, const char *format, ...);
extern int sscanf(const char *s, const char *format, ...);

#endif /* _STDIO_H */
