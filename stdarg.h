/* stdarg.h - minimal C89 stdarg support for dcc */
#ifndef _STDARG_H
#define _STDARG_H

/** Variadic argument cursor that walks the caller's stack frame. */
typedef char *va_list;

/** Start reading variadic arguments after the named parameter last. */
#define va_start(ap,last) __va_start(ap,last)
/** Fetch the next variadic argument as type. */
#define va_arg(ap,type)   (*(type *)__va_arg(ap,sizeof(type)))
/** Finish reading variadic arguments. */
#define va_end(ap)        __va_end(ap)

#endif /* _STDARG_H */
