/* unistd.h - minimal POSIX file / process declarations for dcc */

#ifndef _UNISTD_H
#define _UNISTD_H

#include <stddef.h>

/* File operations */

typedef long off_t;

int unlink( const char *pathname );    /* remove a file */
int read( int fd, void *buf, size_t count );
int write( int fd, const void *buf, size_t count );
int close( int fd );
off_t lseek( int fd, off_t offset, int whence );
int fsync( int fd );

#endif /* _UNISTD_H */
