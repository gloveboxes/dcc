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
long strtol(const char *nptr, char **endptr, int base);
unsigned long strtoul(const char *nptr, char **endptr, int base);
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

/* dcc extensions (not C89) for talking to CP/M and hardware directly:
 *   bdos()      calls the CP/M BDOS entry point (fn -> C, dearg -> DE); the
 *               byte result is returned in the low byte of the int.  Calls
 *               whose useful result is an FCB/DMA region return it through the
 *               memory that dearg points at, not in the return value.
 *   inp()/outp() do direct Z80 8-bit port I/O.  inp() runs IN A,(port) and
 *               returns the byte zero-extended to int (0..255); outp() runs
 *               OUT (port),A.  Only the low 8 bits of port are significant. */
int  bdos( int fn, int dearg );
int  inp( unsigned port );
void outp( unsigned port, unsigned val );

#endif
