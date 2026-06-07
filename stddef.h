#ifndef _STDDEF_H
#define _STDDEF_H

#define NULL 0

typedef unsigned int size_t;
typedef int ptrdiff_t;
#ifndef _WCHAR_T
#define _WCHAR_T
typedef unsigned int wchar_t;
#endif

#define offsetof(type, member) ((size_t)&(((type *)0)->member))

#endif
