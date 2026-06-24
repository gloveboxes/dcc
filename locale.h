#ifndef _LOCALE_H
#define _LOCALE_H

/* Locale category selectors (C89 7.4). */
/** All locale categories. */
#define LC_ALL      0
/** String collation. */
#define LC_COLLATE  1
/** Character classification and conversion. */
#define LC_CTYPE    2
/** Monetary formatting. */
#define LC_MONETARY 3
/** Numeric formatting (decimal point etc.). */
#define LC_NUMERIC  4
/** Date and time formatting. */
#define LC_TIME     5

/** Numeric and monetary formatting conventions. */
struct lconv {
    char *decimal_point;      /** Decimal-point character. */
    char *thousands_sep;      /** Thousands-group separator. */
    char *grouping;           /** Digit grouping sizes. */
    char *int_curr_symbol;    /** International currency symbol. */
    char *currency_symbol;    /** Local currency symbol. */
    char *mon_decimal_point;  /** Monetary decimal-point character. */
    char *mon_thousands_sep;  /** Monetary thousands separator. */
    char *mon_grouping;       /** Monetary digit grouping. */
    char *positive_sign;      /** Non-negative value sign. */
    char *negative_sign;      /** Negative value sign. */
    char  int_frac_digits;    /** International fractional digits. */
    char  frac_digits;        /** Local fractional digits. */
    char  p_cs_precedes;      /** Currency symbol precedes non-negative. */
    char  p_sep_by_space;     /** Space between currency and non-negative. */
    char  n_cs_precedes;      /** Currency symbol precedes negative. */
    char  n_sep_by_space;     /** Space between currency and negative. */
    char  p_sign_posn;        /** Non-negative sign position. */
    char  n_sign_posn;        /** Negative sign position. */
};

/** Set or query the program locale.
 *  Only "C" and "" (implementation default, also "C") are supported.
 *  Returns "C" on success, NULL if the requested locale is unavailable. */
char *setlocale(int category, const char *locale);

/** Return a pointer to the current locale's formatting conventions. */
struct lconv *localeconv(void);

#endif /* _LOCALE_H */
