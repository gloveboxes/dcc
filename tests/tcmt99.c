/*
 * tcmt99.c - C99 "//" line-comment support, alongside C89 block comments.
 *
 * Pins the behaviour that matters for the dcc lexer and preprocessor:
 *   - text after a line comment is ignored to end of line,
 *   - a lone slash is still the divide operator (only a doubled slash starts
 *     a comment),
 *   - a doubled slash inside a string literal is ordinary data,
 *   - a line comment trailing a #define (object- and function-like) is
 *     stripped before macro replacement,
 *   - line and block comments coexist, and a line comment hides a following
 *     block-comment opener so it cannot start a block,
 *   - a line comment may be the final thing in the file.
 *
 * If line-comment support regressed, this file would either fail to compile
 * (the comment text would lex as stray tokens) or print wrong values, so
 * runall would catch it.
 */
#include <stdio.h>
#include <string.h>

#define VAL 42           // object-like macro, trailing line comment
#define DBL(x) ((x)+(x)) // function-like macro, trailing line comment
#define WID 7 /* b */    // block comment then line comment on one #define

static int fails;
static void chk(long got, long want, char *name)
{
    if (got != want) { printf("FAIL %s got=%ld want=%ld\n", name, got, want); fails++; }
}

int main(void)
{
    int x;
    char *s;

    x = 5;          // x = 999
    chk(x, 5L, "basic");

    x = 10 / 2;     // a lone slash still divides
    chk(x, 5L, "single slash divides");

    // full-line comment with tricky text: /* not a block */ "not a string" 'x'
    x = VAL;
    chk(x, 42L, "object macro trailing comment");
    chk(DBL(5), 10L, "function macro trailing comment");
    chk(WID, 7L, "block-then-line on one define");

    s = "a//b//c";  // the // sequences live inside the string
    chk((long)strlen(s), 7L, "slashes inside string");

    // /* a line comment hides this block opener, so the next line stays live
    x = 11;
    chk(x, 11L, "line comment hides block opener");

    x = 22; /* a block comment may contain // safely */ x += 1;
    chk(x, 23L, "block comment contains slashes");

    x = 0;          // reset
    x += 1;         // ++
    x += 1;         // ++
    chk(x, 2L, "interleaved trailing comments");

    if (fails == 0) printf("tcmt99 passed with great success\n");
    else printf("tcmt99 FAILED: %d\n", fails);
    return fails != 0;
}
// the final token in this file is a line comment
