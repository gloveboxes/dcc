/* stdarg.h - minimal C89 stdarg support for dcc */
#ifndef _STDARG_H
#define _STDARG_H

typedef char *va_list;

#define va_start(ap,last) __va_start(ap,last)
#define va_arg(ap,type)   (*(type *)__va_arg(ap,sizeof(type)))
#define va_end(ap)        __va_end(ap)

#endif /* _STDARG_H */
