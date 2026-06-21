#ifndef _STDLIB_H
#define _STDLIB_H

#include <stddef.h>

#ifndef NULL
/** Null pointer constant. */
#define NULL 0
#endif

/** Successful program termination status. */
#define EXIT_SUCCESS 0
/** Unsuccessful program termination status. */
#define EXIT_FAILURE 1
/** Maximum value returned by rand. */
#define RAND_MAX 32767

/** Quotient and remainder pair returned by div. */
typedef struct {
    int quot;
    int rem;
} div_t;

/** Quotient and remainder pair returned by ldiv. */
typedef struct {
    long quot;
    long rem;
} ldiv_t;

/** Terminate the program after flushing runtime output. */
void exit( int code );
/** Return the next pseudo-random integer in the range 0 through RAND_MAX. */
int rand(void);
/** Seed the pseudo-random number generator. */
void srand(unsigned int seed);
/** Convert the leading decimal text in nptr to int. */
int atoi(const char *nptr);
/** Convert the leading decimal text in nptr to long. */
long atol(const char *nptr);
/** Convert text in nptr to long using base 2 through 36, or base 0 for auto-detection. */
long strtol(const char *nptr, char **endptr, int base);
/** Convert text in nptr to unsigned long using base 2 through 36, or base 0 for auto-detection. */
unsigned long strtoul(const char *nptr, char **endptr, int base);
/** Absolute value of a signed int. */
int abs(int j);
/** Absolute value of a signed long. */
long labs(long j);
/** Signed int division returning quotient and remainder. */
div_t div(int numer, int denom);
/** Signed long division returning quotient and remainder. */
ldiv_t ldiv(long numer, long denom);
/** Binary-search a sorted array. */
const void *bsearch(const void *key, const void *base, size_t num, size_t size, int (*compare)(const void *, const void *));
/** Sort an array in place. */
void qsort(void *base, size_t num, size_t size, int (*compare)(const void *, const void *));

/** Allocate size bytes from the heap. */
void *malloc( size_t size );
/** Allocate and zero num * size bytes from the heap. */
void *calloc( size_t num, size_t size );
/** Resize a heap allocation, preserving contents up to the smaller size. */
void *realloc( void *ptr, size_t size );
/** Release a heap allocation. */
void free( void *ptr );

/* dcc extensions (not C89) for talking to CP/M and hardware directly:
 *   bdos()      calls the CP/M BDOS entry point (fn -> C, dearg -> DE); the
 *               byte result is returned in the low byte of the int.  Calls
 *               whose useful result is an FCB/DMA region return it through the
 *               memory that dearg points at, not in the return value.
 *   inp()/outp() do direct Z80 8-bit port I/O.  inp() runs IN A,(port) and
 *               returns the byte zero-extended to int (0..255); outp() runs
 *               OUT (port),A.  Only the low 8 bits of port are significant. */
/** Call the CP/M BDOS entry point. */
int  bdos( int fn, int dearg );
/** Read an 8-bit Z80 I/O port. */
int  inp( unsigned port );
/** Write an 8-bit Z80 I/O port. */
void outp( unsigned port, unsigned val );

#endif
