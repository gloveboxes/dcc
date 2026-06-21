#ifndef _ASSERT_H
#define _ASSERT_H

#ifdef NDEBUG
/** Check expression unless assertions are disabled by NDEBUG. */
#define assert(expression) ((void)0)
#else
#include <stdio.h>
#include <stdlib.h>

static void _assfail(const char *diagnostic_msg)
{
    fprintf(stderr, "Assertion Failed: %s\n", diagnostic_msg);
    fflush(stderr);
    exit(1);
}

/* Internal helpers for turning __LINE__ into a string literal. */
#define STR_HELPER(x) #x
#define STR_CONVERT(x) STR_HELPER(x)
/** Check expression unless assertions are disabled by NDEBUG. */
#define assert(expression) ((expression) ? (void)0 : _assfail(#expression " at " __FILE__ ":" STR_CONVERT(__LINE__)))
#endif

#endif
