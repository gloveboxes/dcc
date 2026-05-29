#ifndef _STRING_H
#define _STRING_H

#include <stdint.h>
#include <stddef.h>

/* implemented in dccrtl.mac */

size_t   strlen (const char *);
void *   memcpy (void *__restrict, const void *__restrict, size_t);
void *   memset (void *, int, size_t);
int      strcmp (const char *, const char *);
char *   strcpy (char *__restrict, const char *__restrict);

/* unimplemented */

void *   memchr (const void *, int, size_t);
void *   memmove (void *, const void *, size_t);
char *   strcat (char *__restrict, const char *__restrict);
int      strcoll (const char *, const char *);
size_t   strcspn (const char *, const char *);
char *   strerror (int);
char *   strncat (char *__restrict, const char *__restrict, size_t);
char *   strncpy (char *__restrict, const char *__restrict, size_t);
char *   strpbrk (const char *, const char *);
size_t   strspn (const char *, const char *);

/* implemented in string.c */

char *   strchr(const char * str, char c);
int      strncmp(const char *a, const char *b, int n);
int      memcmp (const void *, const void *, size_t);
char *   strrchr (const char *, int);
char *   strstr (const char *, const char *);

#endif
