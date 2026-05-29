#ifndef _STDLIB_H
#define _STDLIB_H

extern void exit( int code );
extern int rand(void);
extern void srand(unsigned int seed);

extern void * malloc( size_t size );
extern void * calloc( size_t num, size_t size );
void free( void * ptr );

#endif

