# Utility headers

This page groups the smaller standard headers: non-local jumps, variadic
arguments, common definitions, and the boolean type.

## `setjmp.h` — non-local jumps

Include `setjmp.h`. `jmp_buf` is an 8-byte buffer (saved return address, SP, and
IX).

| Function | Summary |
| --- | --- |
| `int setjmp(jmp_buf env)` | Save the current context; returns 0 on the direct call. |
| `void longjmp(jmp_buf env, int val)` | Jump back to the `setjmp`, which returns `val` (or 1). |

```c
jmp_buf env;

if (setjmp(env) == 0) {
    work(env);        /* normal path; somewhere inside: longjmp(env, 1) */
} else {
    recover();        /* arrived here via longjmp */
}
```

## `stdarg.h` — variadic arguments

Include `stdarg.h`. `va_list` is a `char *` walking the stack frame.

| Macro | Summary |
| --- | --- |
| `va_start(ap, last)` | Begin traversal after the named argument `last`. |
| `va_arg(ap, type)` | Fetch the next argument of `type`. |
| `va_end(ap)` | Finish traversal. |

```c
int sum(int count, ...)
{
    va_list ap;
    int total = 0, i;

    va_start(ap, count);
    for (i = 0; i < count; ++i)
        total += va_arg(ap, int);
    va_end(ap);
    return total;
}
```

A `va_list` can be forwarded straight to `vprintf`/`vfprintf`/`vsprintf` (see
[Console and file I/O](05-stdio.md#formatted-output)) to build `printf`-style
wrappers without re-parsing the arguments yourself.

## `stddef.h` — common definitions

Include `stddef.h` for common C definitions used by library headers and portable
data-structure code.

| Name | Summary |
| --- | --- |
| `size_t` | Unsigned 16-bit size type. |
| `offsetof(type, member)` | Compile-time byte offset of a struct/union member. |

`offsetof` accepts struct and union member designators, including nested member
access and constant array indexes.

```c
#include <stddef.h>

struct rec {
    char tag;
    int  value;
};

int off = offsetof(struct rec, value);   /* 1 on this 16-bit target */
```

## `stdbool.h` — boolean type

Include `stdbool.h` for the C99 boolean spelling. dcc does not recognize the
native `_Bool` keyword, so this header provides the names as an ordinary library
typedef and macros:

| Name | Definition |
| --- | --- |
| `bool` | `typedef unsigned char bool` |
| `true` | `1` |
| `false` | `0` |

Because `bool` is just `unsigned char`, it stores one byte and follows the
normal integer-conversion rules. It does **not** carry C99 `_Bool` semantics:
assigning a nonzero value such as `2` keeps that value rather than normalizing it
to `1`. Compare against zero (or use `!!x`) when you need a canonical 0/1 result.

```c
#include <stdbool.h>

bool ready = false;
ready = (count > 0);        /* 0 or 1 from the relational operator */
if (ready)
    puts("go");
```
