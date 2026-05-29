#ifndef _ERRNO_H
#define _ERRNO_H

extern int errno; // just one thread so use an int not a macro looking at tls

#endif
