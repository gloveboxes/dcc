/* setjmp.h for dcc - Z80/CP-M */

#ifndef _SETJMP_H
#define _SETJMP_H

/* jmp_buf holds: return_addr(2), saved_sp(2), saved_ix(2), pad(2) = 8 bytes */
typedef unsigned int jmp_buf[4];

#endif

