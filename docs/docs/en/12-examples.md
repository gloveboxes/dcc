# Worked examples

Short, self-contained programs you can drop into your own project, build with
the `scripts/ma.ps1` helper script, and run under an emulator such as ntvcm. See
[Building and linking](02-build-and-link.md) for the build options and the
manual pipeline.

## Sorting and searching an `int` array

`qsort` orders the array, then `bsearch` locates a key with the *same*
comparator. The comparator returns negative / zero / positive — here the
branchless `(x > y) - (x < y)` idiom.

```c
--8<-- "tests/texsort.c:example"
```

Output: `found 13 at index 5`.

## Sorting an array of structs by a key field

Any element width works because `qsort` swaps whole elements byte-by-byte. The
comparator reads the field it sorts on — here a string member via `strcmp` — and
`bsearch` reuses it to look a record up by name.

```c
--8<-- "tests/texstrct.c:example"
```

Output:

```text
apples   9
kiwis    2
pears    4
kiwis: 2 in stock
```

## A `printf`-style logging wrapper

Forwarding a `va_list` to `vfprintf` lets you build your own diagnostic helpers
without re-parsing the arguments.

```c
--8<-- "tests/texlog.c:example"
```

## Reading a text file line by line

```c
--8<-- "tests/texfile.c:example"
```

## Parsing input with `sscanf`

`sscanf` reads from a string using the same conversion subset as `scanf` and
`fscanf` (integers and strings; no floating input). Each conversion stores
through a pointer argument.

```c
--8<-- "tests/texscan.c:example"
```

Output:

```text
value=-12 word=hello hexval=42
big=123456
```

## Buffered console output with a user-declared buffer

`setvbuf` lets you hand the runtime your own buffer and fully buffer the
console, so output accumulates instead of going to CP/M one character at a time.
A larger buffer means fewer BDOS calls. Drain it with `fflush`, and detach it
(`setvbuf(stdout, NULL, _IOLBF, 0)`) before the buffer's storage is reused — see
[Console output buffering](standard-lib/05-stdio.md#console-output-buffering).

```c
--8<-- "tests/tbufex.c:example"
```

Output ends with `sum of squares 1..20 = 2870`. The `static` buffer keeps it off
the small CP/M stack; a `malloc`'d buffer works too, but free it only *after*
detaching it from the stream. This snippet is pulled verbatim from the
`tests/tbufex.c` regression test, so the documented code is exactly what is
built and run by the suite.

