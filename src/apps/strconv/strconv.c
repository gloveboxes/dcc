/*
 * strconv.c - C89 string-conversion / tokenising helpers for dcc.
 *
 * SOURCE OF TRUTH for strtol/strtoul/strtok, which are folded into DCCRTL.MAC
 * the same way as mathf.c (the math.h routines).  Written in portable dcc C89
 * on top of the runtime's existing helpers (strspn/strpbrk for strtok, the
 * 32-bit long divide/modulo primitives for strtol/strtoul, and errno).
 *
 * Build/merge procedure (identical in spirit to src/apps/mathf/mathf.c):
 *   1. dcc -c strconv.c -o strconv.mac
 *   2. dccpeep strconv.mac strconv.peep.mac   (ma.sh peeps apps, NOT the
 *      runtime, so merged blocks must be pre-optimized here)
 *   3. strip: "extrn " lines (those helpers live in DCCRTL.MAC and resolve
 *      locally), the "; dcc stage-1d" banner and the "cseg" line.  KEEP the
 *      data for strtok's saved pointer (see note below).
 *   4. rename local labels L<n> -> SCL<n> (perl -pe 's/\bL(\d+)\b/SCL$1/g')
 *      AND compiler temporaries _Z<n> -> SCZ<n>, so they cannot collide with
 *      an app's own L<n>/_Z<n> labels (the same whole-token collision class
 *      that bit the math merge; see repo memory).
 *   5. splice the public blocks into DCCRTL.MAC before "end start".
 *
 * NAME MANGLING: L80 only honours 6 significant characters in external
 * symbols, so the literal publics _strtol/_strtoul/_strtok all collapse to
 * "_STRTO" and collide.  dcc therefore maps these C names to short runtime
 * labels in src/dcc/dcc_asmname.c (and the src/ddc.c monolith copy):
 *   strtol -> __stol, strtoul -> __stou, strtok -> __stok.
 * The merged runtime blocks use those mangled public labels.
 *
 * NOTE on strtok state: C89 strtok keeps a saved pointer between calls.  The
 * static below compiles to a single 2-byte cell; when merging, that cell is
 * carried into DCCRTL.MAC as an explicit "dw 0" so the routine keeps its state.
 *
 * Constraints honoured: no double (long is 32-bit), 16-bit int, signed char.
 */

#include <stdlib.h>
#include <string.h>
#include <limits.h>
#include <errno.h>

/* True for the C locale white-space characters (space, \t \n \v \f \r). */
static int sc_isspace(int c)
{
    return c == ' ' || (c >= 9 && c <= 13);
}

/* Map a digit/letter to its value 0..35, or 99 if not alphanumeric. */
static int sc_digit(int c)
{
    if (c >= '0' && c <= '9') return c - '0';
    if (c >= 'a' && c <= 'z') return c - 'a' + 10;
    if (c >= 'A' && c <= 'Z') return c - 'A' + 10;
    return 99;
}

long strtol(const char *nptr, char **endptr, int base)
{
    const char *s;
    int neg;
    int any;
    int d;
    unsigned long acc;
    unsigned long cutoff;
    int cutlim;

    s = nptr;
    while (sc_isspace((unsigned char)*s)) s++;

    neg = 0;
    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        neg = 1;
        s++;
    }

    if ((base == 0 || base == 16) &&
        s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    /* Largest magnitude representable, split for overflow detection. */
    cutoff = neg ? (unsigned long)LONG_MAX + 1UL : (unsigned long)LONG_MAX;
    cutlim = (int)(cutoff % (unsigned long)base);
    cutoff = cutoff / (unsigned long)base;

    acc = 0;
    any = 0;
    for (;; s++) {
        d = sc_digit((unsigned char)*s);
        if (d >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && d > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * (unsigned long)base + (unsigned long)d;
        }
    }

    if (any < 0) {
        acc = neg ? (unsigned long)LONG_MIN : (unsigned long)LONG_MAX;
        errno = ERANGE;
    } else if (neg) {
        acc = (unsigned long)(0UL - acc);
    }

    if (endptr != NULL)
        *endptr = (char *)(any ? s : nptr);

    return (long)acc;
}

unsigned long strtoul(const char *nptr, char **endptr, int base)
{
    const char *s;
    int neg;
    int any;
    int d;
    unsigned long acc;
    unsigned long cutoff;
    int cutlim;

    s = nptr;
    while (sc_isspace((unsigned char)*s)) s++;

    neg = 0;
    if (*s == '+') {
        s++;
    } else if (*s == '-') {
        neg = 1;
        s++;
    }

    if ((base == 0 || base == 16) &&
        s[0] == '0' && (s[1] == 'x' || s[1] == 'X')) {
        s += 2;
        base = 16;
    } else if (base == 0) {
        base = (s[0] == '0') ? 8 : 10;
    }

    cutoff = ULONG_MAX / (unsigned long)base;
    cutlim = (int)(ULONG_MAX % (unsigned long)base);

    acc = 0;
    any = 0;
    for (;; s++) {
        d = sc_digit((unsigned char)*s);
        if (d >= base) break;
        if (any < 0 || acc > cutoff || (acc == cutoff && d > cutlim)) {
            any = -1;
        } else {
            any = 1;
            acc = acc * (unsigned long)base + (unsigned long)d;
        }
    }

    if (any < 0) {
        acc = ULONG_MAX;
        errno = ERANGE;
    } else if (neg) {
        acc = (unsigned long)(0UL - acc);
    }

    if (endptr != NULL)
        *endptr = (char *)(any ? s : nptr);

    return acc;
}

/* Saved scan position between strtok calls (C89 7.11.5.8). */
static char *sc_strtok_last;

char *strtok(char *s, const char *delim)
{
    char *tok;

    if (s == NULL)
        s = sc_strtok_last;
    if (s == NULL)
        return NULL;

    /* Skip leading delimiters. */
    s = s + strspn(s, delim);
    if (*s == '\0') {
        sc_strtok_last = NULL;
        return NULL;
    }

    tok = s;
    s = strpbrk(tok, delim);
    if (s == NULL) {
        sc_strtok_last = NULL;
    } else {
        *s = '\0';
        sc_strtok_last = s + 1;
    }
    return tok;
}
