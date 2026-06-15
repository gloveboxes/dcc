# Strings and characters

This page covers [`string.h`](07-strings-and-ctype.md) (string and memory
blocks) and [`ctype.h`](07-strings-and-ctype.md) (character classification).
Both are fully implemented in the runtime, and most routines are individually
cheap — they pull in nothing beyond themselves.

## `string.h`

| Function | Summary |
| --- | --- |
| `size_t strlen(const char *s)` | Length excluding the NUL. |
| `char *strcpy(char *d, const char *s)` | Copy including the NUL. |
| `char *strncpy(char *d, const char *s, size_t n)` | Copy at most `n`, NUL-pad the remainder. |
| `char *strcat(char *d, const char *s)` | Append `s` to `d`. |
| `char *strncat(char *d, const char *s, size_t n)` | Append at most `n` chars plus NUL. |
| `int strcmp(const char *a, const char *b)` | Lexicographic compare. |
| `int strncmp(const char *a, const char *b, size_t n)` | Compare at most `n` chars. |
| `int strcoll(const char *a, const char *b)` | Locale compare (same as `strcmp` here). |
| `char *strchr(const char *s, int c)` | First occurrence of `c`. |
| `char *strrchr(const char *s, int c)` | Last occurrence of `c`. |
| `char *strstr(const char *h, const char *n)` | First occurrence of substring `n`. |
| `size_t strspn(const char *s, const char *set)` | Length of leading run in `set`. |
| `size_t strcspn(const char *s, const char *set)` | Length of leading run *not* in `set`. |
| `char *strpbrk(const char *s, const char *set)` | First char that is in `set`. |
| `char *strtok(char *s, const char *set)` | Split `s` into tokens delimited by `set`. |
| `char *strdup(const char *s)` | Heap copy of `s` (free it later). |
| `void *memcpy(void *d, const void *s, size_t n)` | Copy `n` bytes (no overlap). |
| `void *memmove(void *d, const void *s, size_t n)` | Copy `n` bytes (overlap safe). |
| `void *memset(void *d, int c, size_t n)` | Fill `n` bytes with `c`. |
| `int memcmp(const void *a, const void *b, size_t n)` | Compare `n` bytes. |
| `void *memchr(const void *s, int c, size_t n)` | Find `c` within `n` bytes. |
| `char *strerror(int err)` | Text for an error number. |

```c
char dst[16];
strcpy(dst, "hello");
if (strcmp(dst, "hello") == 0)
    memset(dst, 0, sizeof dst);
```

Tokenizing a copy of a string with `strtok` (it writes NULs into the buffer, so
never pass a string literal):

```c
char line[] = "alpha,beta,,gamma";
char *tok = strtok(line, ",");
while (tok) {
    puts(tok);                 /* alpha, beta, gamma */
    tok = strtok(NULL, ",");
}
```

!!! note "`strdup` is the exception"
    Most string routines pull in nothing extra, but `strdup` allocates, so it
    drags in the whole `malloc` chain. See the
    [appendix](appendix/01-dccrtlstrip.md).

## `ctype.h`

Each function takes an `int` and returns non-zero for a match (or the converted
character for the two case functions).

| Function | True when the character is… |
| --- | --- |
| `int isalpha(int c)` | a letter |
| `int isdigit(int c)` | a decimal digit |
| `int isalnum(int c)` | a letter or digit |
| `int isspace(int c)` | whitespace |
| `int isupper(int c)` | an uppercase letter |
| `int islower(int c)` | a lowercase letter |
| `int isxdigit(int c)` | a hex digit |
| `int isprint(int c)` | printable (including space) |
| `int iscntrl(int c)` | a control character |
| `int ispunct(int c)` | punctuation |
| `int toupper(int c)` | — returns the uppercase form |
| `int tolower(int c)` | — returns the lowercase form |

```c
char *p;
for (p = s; *p; ++p)
    *p = toupper(*p);          /* upper-case in place */
```
