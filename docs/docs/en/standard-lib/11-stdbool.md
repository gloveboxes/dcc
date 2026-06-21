# Boolean type (`stdbool.h`)

Include [`stdbool.h`](11-stdbool.md) for the C99 boolean spelling used by many
portable C programs.

## Types and Macros

<!-- STDBOOL-SYMBOL-TABLE: all -->

## Runtime model

dcc does not recognize the native C99 `_Bool` keyword. This header provides the
portable names as an ordinary typedef and macros: `bool` is `unsigned char`,
`true` is `1`, and `false` is `0`.

!!! warning "`bool` does not normalize"
    Because `bool` is just an unsigned character type, assigning a nonzero value
    such as `2` stores that value. It does not normalize every nonzero
    assignment to `1` the way C99 `_Bool` would. Compare against zero, or use
    `!!x`, when you need a canonical 0/1 result.

## Boolean values

Relational and logical operators already produce `0` or `1`, so they are the
cleanest way to assign boolean state:

```c
#include <stdbool.h>

bool ready = false;

ready = (count > 0);
if (ready)
    puts("go");
```

`__bool_true_false_are_defined` is provided for source compatibility with code
that checks whether `stdbool.h` has supplied the standard names.
