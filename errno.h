#ifndef _ERRNO_H
#define _ERRNO_H

/** Global error indicator for the single-threaded dcc runtime. */
extern int errno;

/* Conventional small-system/POSIX errno values.
 * C89 only requires EDOM and ERANGE, but the DCC CP/M runtime also
 * sets the file-related values below from open/read/write/close/lseek/unlink.
 */
/** Operation not permitted. */
#define EPERM   1
/** No such file or directory. */
#define ENOENT  2
/** No such process. */
#define ESRCH   3
/** Interrupted function call. */
#define EINTR   4
/** Input/output error. */
#define EIO     5
/** No such device or address. */
#define ENXIO   6
/** Argument list too long. */
#define E2BIG   7
/** Executable file format error. */
#define ENOEXEC 8
/** Bad file descriptor. */
#define EBADF   9
/** No child processes. */
#define ECHILD 10
/** Resource temporarily unavailable. */
#define EAGAIN 11
/** Not enough memory. */
#define ENOMEM 12
/** Permission denied. */
#define EACCES 13
/** Bad address. */
#define EFAULT 14
/** Device or resource busy. */
#define EBUSY  16
/** File exists. */
#define EEXIST 17
/** Cross-device link. */
#define EXDEV  18
/** No such device. */
#define ENODEV 19
/** Not a directory. */
#define ENOTDIR 20
/** Is a directory. */
#define EISDIR 21
/** Invalid argument. */
#define EINVAL 22
/** Too many files open in system. */
#define ENFILE 23
/** Too many open files. */
#define EMFILE 24
/** Inappropriate I/O control operation. */
#define ENOTTY 25
/** File too large. */
#define EFBIG  27
/** No space left on device. */
#define ENOSPC 28
/** Invalid seek. */
#define ESPIPE 29
/** Read-only file system. */
#define EROFS  30
/** Too many links. */
#define EMLINK 31
/** Broken pipe. */
#define EPIPE  32
/** Domain error. */
#define EDOM   33
/** Result too large or out of range. */
#define ERANGE 34
/** Directory not empty. */
#define ENOTEMPTY 39

#endif
