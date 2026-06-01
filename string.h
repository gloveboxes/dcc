#ifndef _STRING_H
#define _STRING_H

#include <stddef.h>

/* implemented in dccrtl.mac */

void *   memcpy( void *, const void *, size_t );
void *   memset( void *, int, size_t );
void *   memmove( void *, const void *, size_t );
int      memcmp( const void *, const void *, size_t );
size_t   strlen( const char * );
int      strcmp( const char *, const char * );
char *   strcpy( char *, const char * );
char *   strerror( int );
char *   strncat( char *, const char *, size_t );
char *   strrchr( const char *, int );
char *   strchr(const char * str, char c );
char *   strstr( const char *, const char * );
int      strncmp(const char *a, const char *b, int n );

/* unimplemented */

void *   memchr( const void *, int, size_t );
char *   strcat( char *, const char * );
int      strcoll( const char *, const char * );
size_t   strcspn( const char *, const char * );
char *   strncpy( char *, const char *, size_t );
char *   strpbrk( const char *, const char * );
size_t   strspn( const char *, const char * );
char *   strdup( const char * );

#endif
