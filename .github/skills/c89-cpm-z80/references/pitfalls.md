# dcc C89 pitfalls and idioms

Genuine, general gotchas when writing or porting C89 for dcc (CP/M 2.2 / Z80).
Each has a symptom and a fix.

## No `double` (the headline constraint)

- `double` is not a recognised keyword — using it is a **compile error**. Use
  `float` (32-bit) everywhere.
- Unsuffixed floating constants like `3.14` are already `float`, not `double`.
- There is no `float`→`double` promotion in variadic calls (there is no
  double), so `printf("%f", x)` consumes a 32-bit `float` directly.
- No `atof`/`strtod` (both return `double`). `<math.h>` *does* provide the
  single-precision transcendentals (`sinf`/`cosf`/`expf`/`logf`/`powf`/… with
  unsuffixed aliases), and `strtol`/`strtoul` parse integers — but for
  decimal-string→`float` you still parse it yourself:

```c
/* minimal signed decimal -> float */
static float parse_f(const char *s)
{
    float v = 0.0f, frac = 0.0f, scale = 1.0f;
    int neg = 0;
    if (*s == '-') { neg = 1; s++; }
    while (*s >= '0' && *s <= '9') { v = v * 10.0f + (*s - '0'); s++; }
    if (*s == '.') { s++; while (*s >= '0' && *s <= '9') {
        scale *= 10.0f; frac = frac * 10.0f + (*s - '0'); s++; } }
    v += frac / scale;
    return neg ? -v : v;
}
```

## `%f` needs the `-ffloatio` build flag

Float `printf` formatting is only linked when you compile with `-f` /
`-ffloatio`. Without it, `%f` produces nothing useful — so any program that
prints floats must be built with that flag.

## 16-bit `int` (and `size_t`)

`int`/`short` are 16-bit (±32767), and `size_t` is 16-bit too, so a single
object or allocation can't exceed ~64 KB.

- Use `long` + `%ld` for counters, sizes, or accumulators that can exceed ±32767.
- File offsets are `long` (`ftell`/`fseek`/`lseek`, `off_t`) — don't truncate
  them into an `int`.

## `char` is signed

Bytes ≥ 0x80 read as negative, so `c == 0xE9` is never true and table
indexing / sign-extension can misbehave. Use `unsigned char` for raw bytes and
when indexing a table by character value.

## CP/M filenames: 8.3, uppercase, extension ≤ 3 chars

- Source `foo.c` builds `FOO.COM`; run it as `ntvcm FOO` (uppercase, no ext).
- ntvcm's filename mapper **asserts on any extension longer than 3 characters**,
  so delete stray `.DS_Store` files before directory-enumeration tests or a full
  `runall.sh` (VS Code / file search may hide them).

## printf / scanf are a subset

- `printf`: no `+`, space, or `#` flags, and no `*` (run-time) width/precision —
  bake widths into the format string (`%6d`, `%-8s`, `%.4d`); write `0x`
  prefixes literally.
- `scanf` / `sscanf` / `fscanf`: integer and string only — no `%f`/`%e`/`%g`,
  scansets (`%[...]`), `%n`, or `%p`.

## No stack/heap guard

The heap grows up from BSS, the stack grows down, and nothing stops them
colliding. Reserve stack with `-stack N` (default 512) for deep recursion or
large automatic arrays, and prefer `static`/global for big buffers.
(`spsmash.c` shows an optional manual stack-depth check.)
