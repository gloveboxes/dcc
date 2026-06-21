# C89 conformance and C99 extensions

dcc implements most of C89, plus a few carefully chosen conveniences from later
standards. This page covers which keywords are recognized, the qualifiers that
are parsed but inert, and the post-C89 features you can rely on.

## Recognized keywords

dcc recognizes 31 of the 32 standard C89 keywords. The only C89 keyword that is
**not** recognized is `double`, because dcc has no 8-byte floating type — use
`float` instead.

| Category | Keywords |
| --- | --- |
| Types | `char`, `short`, `int`, `long`, `signed`, `unsigned`, `float`, `void` |
| Storage class | `auto`, `extern`, `register`, `static`, `typedef` |
| Type qualifiers | `const`, `volatile` |
| Aggregates / enums | `struct`, `union`, `enum` |
| Control flow | `if`, `else`, `switch`, `case`, `default`, `for`, `while`, `do`, `break`, `continue`, `goto`, `return` |
| Operators | `sizeof` |

### Not supported

| Keyword | Reason |
| --- | --- |
| `double` | No 8-byte floating type; `double` is unrecognized. Use `float`. |

## Accepted-but-inert qualifiers

A few keywords are parsed so your source compiles, but they do not change code
generation:

- `const` — honored only for constant folding of const-initialized variables. It
  does **not** place data in read-only memory.
- `volatile` — accepted but otherwise ignored.
- `register` — accepted as a hint only; it does not force register allocation.
- `auto` — accepted; since it is already the default storage for locals, it is a
  no-op.

## C99 extensions you can use

### `inline`

`inline` is accepted as a non-C89 extension and then ignored; functions are
always emitted normally. No other C99/C11 keywords (`restrict`, `_Bool`,
`_Complex`, and so on) are recognized.

The native `_Bool` keyword is **not** a compiler type — but `bool`, `true`, and
`false` are available as an ordinary library typedef by including
[`stdbool.h`](standard-lib/11-stdbool.md), so portable C99
source that uses them compiles unchanged.

### `for`-loop init declarations

`for (int i = 0; i < n; i++)` is accepted, and the loop variable has proper C99
loop scope: it shadows any outer variable of the same name for the duration of
the loop and is not visible afterward. Multiple declarators
(`for (int i = 0, j = n; ...)`) and pointer/array declarators in the init clause
are scoped the same way.

```c
int i = 99;
for (int i = 0; i < 3; i++)   /* this i shadows the outer one */
    use(i);                   /* 0, 1, 2 */
/* outer i is still 99 here */
```

Block-local declarations nest the same way: a variable declared inside any
`{ ... }` block (including `if`/`while`/`for`/`switch` bodies) shadows an outer
same-named local or parameter only for that block and is invisible afterward.

```c
int x = 10;
{
    int x = 20;               /* shadows the outer x */
    use(x);                   /* 20 */
}
/* outer x is still 10 here */
```

### `//` line comments

C99 `//` line comments are accepted everywhere C89 `/* ... */` block comments
are, including as trailing comments on preprocessor directives such as
`#define` (where, like block comments, they are stripped before macro
replacement). Both comment styles are correctly ignored inside string and
character literals, so a literal such as `"a // b"` is left intact.

## Identifier significance

dcc exceeds C89's identifier-significance minimum of 31 characters for internal
identifiers.
