#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno; /* single-threaded dcc runtime */

/* Conventional small-system/POSIX errno values.
 * C89 only requires EDOM and ERANGE, but the DCC CP/M runtime also
 * sets the file-related values below from open/read/write/close/lseek/unlink.
 */
#define EPERM   1
#define ENOENT  2
#define ESRCH   3
#define EINTR   4
#define EIO     5
#define ENXIO   6
#define E2BIG   7
#define ENOEXEC 8
#define EBADF   9
#define ECHILD 10
#define EAGAIN 11
#define ENOMEM 12
#define EACCES 13
#define EFAULT 14
#define EBUSY  16
#define EEXIST 17
#define EXDEV  18
#define ENODEV 19
#define ENOTDIR 20
#define EISDIR 21
#define EINVAL 22
#define ENFILE 23
#define EMFILE 24
#define ENOTTY 25
#define EFBIG  27
#define ENOSPC 28
#define ESPIPE 29
#define EROFS  30
#define EMLINK 31
#define EPIPE  32
#define EDOM   33
#define ERANGE 34
#define ENOTEMPTY 39

#endif
