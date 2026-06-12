/* setjmp.h for dcc - Z80/CP-M */

#ifndef _SETJMP_H
#define _SETJMP_H

/* jmp_buf holds: return_addr(2), saved_sp(2), saved_ix(2), pad(2) = 8 bytes */
typedef unsigned int jmp_buf[4];

/* Implemented in dccrtl.mac (_setjmp / _longjmp).  C89 specifies setjmp as a
 * macro, but dcc's runtime entry reads the caller frame via IX, so an ordinary
 * prototype matches how it is actually called and gives the editor type info. */
int  setjmp(jmp_buf env);
void longjmp(jmp_buf env, int val);

#endif

