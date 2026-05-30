#ifndef _STDLIB_H
#define _STDLIB_H

void exit( int code );
int rand(void);
void srand(unsigned int seed);
const void *bsearch(const void *key, const void *base, size_t num, size_t size, 
                    int (*compare)(const void *, const void *));
void qsort(void *base, size_t num, size_t size, int (*compare)(const void *, const void *));

void * malloc( size_t size );
void * calloc( size_t num, size_t size );
void free( void * ptr );

#endif

