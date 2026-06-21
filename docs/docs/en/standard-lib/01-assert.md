# Diagnostics (`assert.h`)

Include [`assert.h`](01-assert.md) to check assumptions at runtime in builds
where `NDEBUG` is not defined.

## Macros

<!-- ASSERT-SYMBOL-TABLE: all -->

## Runtime model

`assert` is implemented in the header, not by a separate debug runtime. When
`NDEBUG` is not defined, `assert(expression)` evaluates `expression`. If it is
false, the header's helper prints a diagnostic to `stderr` with `fprintf`,
flushes `stderr`, and terminates the program with `exit(1)`.

When `NDEBUG` is defined before including `assert.h`, `assert(expression)`
expands to `((void)0)` and does not evaluate `expression`.

## Assertions

Use assertions for programmer assumptions, not normal input validation:

```c
#include <assert.h>

void use_buffer(char *buf, int len)
{
    assert(buf != 0);
    assert(len > 0);
}
```

The diagnostic includes the failed expression, source file, and line number.
Because failed assertions call `exit`, normal exit-time flushing and cleanup
still run.
