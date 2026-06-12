#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifndef NULL
#define NULL 0
#endif

#define EXIT_SUCCESS 0
#define EXIT_FAILURE 1
#define RAND_MAX 32767

typedef struct {
    int quot;
    int rem;
} div_t;

typedef struct {
    long quot;
    long rem;
} ldiv_t;

void exit( int code );
int rand(void);
void srand(unsigned int seed);
int atoi(const char *nptr);
long atol(const char *nptr);
int abs(int j);
long labs(long j);
div_t div(int numer, int denom);
ldiv_t ldiv(long numer, long denom);
const void *bsearch(const void *key, const void *base, size_t num, size_t size,
                    int (*compare)(const void *, const void *));
void qsort(void *base, size_t num, size_t size,
           int (*compare)(const void *, const void *));

void *malloc( size_t size );
void *calloc( size_t num, size_t size );
void *realloc( void *ptr, size_t size );
void free( void *ptr );

/* dcc extension (not C89): direct 8-bit port I/O.  inp() runs IN A,(port)
 * and outp() runs OUT (port),A; only the low 8 bits of port are significant. */
int  inp( unsigned port );
void outp( unsigned port, unsigned val );

#endif
