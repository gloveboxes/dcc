/* dirent.h - small CP/M-80 directory enumeration API for dcc */

#ifndef _DIRENT_H
#define _DIRENT_H

#ifndef NULL
#define NULL 0
#endif

typedef struct __dir DIR;

struct dirent {
    char d_name[13];        /* 8.3 name, e.g. "FOO.TXT" */
};

/*
 * M80/L80 symbols are short-significant, so the real external names are
 * deliberately short and unique.  The macros preserve the POSIX-like API
 * spelling in C source while avoiding collisions such as _closedir vs _close
 * and _readdir vs _read.
 */
#define opendir  dopn
#define readdir  drd
#define closedir dcls

extern DIR *dopn(const char *path);
extern struct dirent *drd(DIR *dirp);
extern int dcls(DIR *dirp);

#endif /* _DIRENT_H */
