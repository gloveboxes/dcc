#ifndef _STDDEF_H
#define _STDDEF_H

typedef unsigned int size_t;
typedef int ptrdiff_t;          /* result of subtracting two 16-bit pointers */

#ifndef _WCHAR_T
#define _WCHAR_T
typedef unsigned int wchar_t;   /* 16-bit, matches L"..." literals and <stdint.h> */
#endif

#ifndef NULL
#define NULL 0
#endif

#define offsetof(type, member) __offsetof(type, member)

#endif
