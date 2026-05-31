#include <stddef.h>
#include <string.h>

char * strchr( const char * str, char c )
{
    while ( *str )
    {
        if ( *str == c )
            return (char *) str; 
        str++;
    }

    return 0;
}

int strncmp(const char *a, const char *b, int n)
{
    unsigned char ca;
    unsigned char cb;

    while (n > 0) {
        ca = (unsigned char)*a;
        cb = (unsigned char)*b;

        if (ca != cb)
            return (int)ca - (int)cb;

        if (ca == 0)
            return 0;

        ++a;
        ++b;
        --n;
    }

    return 0;
}

int memcmp(const void *s1, const void *s2, size_t n) 
{
    const unsigned char *p1 = (const unsigned char *) s1;
    const unsigned char *p2 = (const unsigned char *) s2;
    while (n-- > 0) 
    {
        if (*p1 != *p2)
            return (*p1 < *p2) ? -1 : 1;
        p1++;
        p2++;
    }

    return 0;
}

char * strrchr(const char *str, int c) 
{
    const char *last_occurrence = NULL;
    while (*str != '\0') 
    {
        if (*str == c)
            last_occurrence = str;
        str++;
    }
    return (char *)last_occurrence;
}

char * strstr(const char *haystack, const char *needle) 
{
    if (!*needle)
        return (char *)haystack;

    while (*haystack) 
    {
        const char *h = haystack;
        const char *n = needle;

        while (*h && *n && *h == *n) 
        {
            h++;
            n++;
        }

        if (!*n)
            return (char *)haystack;

        haystack++;
    }
    return NULL;
}


