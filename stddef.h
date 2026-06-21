#ifndef _STDDEF_H
#define _STDDEF_H

/** Unsigned 16-bit object size type. */
typedef unsigned int size_t;
/** Signed 16-bit pointer difference type. */
typedef int ptrdiff_t;

#ifndef _WCHAR_T
#define _WCHAR_T
/** Unsigned 16-bit wide character type. */
typedef unsigned int wchar_t;
#endif

#ifndef NULL
/** Null pointer constant. */
#define NULL 0
#endif

/** Compile-time byte offset of member within type. */
#define offsetof(type, member) __offsetof(type, member)

#endif
