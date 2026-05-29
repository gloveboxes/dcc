/* unistd.h - minimal POSIX file / process declarations for dcc */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

/* File operations */

typedef long off_t;

extern int unlink( const char * pathname );    /* remove a file */
extern size_t read( int fd, void *buf, size_t count );
extern size_t write( int fd, const void *buf, size_t count );
extern int close( int fd );
extern off_t lseek( int fd, off_t offset, int whence );
extern int fsync( int fd );

#endif /* _UNISTD_H */
