/* unistd.h - minimal POSIX file / process declarations for dcc */

#ifndef _UNISTD_H
#define _UNISTD_H

/* File operations */
extern int unlink( const char * pathname );    /* remove a file */
extern int read();
extern int write();
extern int close();
extern int lseek();
extern int fsync();

#endif /* _UNISTD_H */
