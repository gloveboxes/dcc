/* tasm.c - test #asm / #endasm inline assembly blocks
 *
 * Implements add_asm(a, b) directly in Z80 assembly inside a #asm block,
 * then calls it from C and checks results.  Verifies that:
 *   - the #asm block is emitted to the .mac output exactly once
 *   - the public label is reachable from C with the normal DCC ABI
 *     (IX+4/5 = first arg, IX+6/7 = second arg, result in HL)
 */

#include <stdio.h>

extern int add_asm(int a, int b);

#asm
	; Inline Z80 implementation of add_asm(a, b) -> HL.
	; Uses standard DCC frame: IX+4/5 = a, IX+6/7 = b.
	public _add_asm
_add_asm:
	push ix
	ld ix,0
	add ix,sp
	ld l,(ix+4)
	ld h,(ix+5)
	ld e,(ix+6)
	ld d,(ix+7)
	add hl,de
	ld sp,ix
	pop ix
	ret
#endasm

int main(void)
{
    int ok;
    ok = 1;

    if (add_asm(3, 4) != 7)       { printf("FAIL: 3+4\n");       ok = 0; }
    if (add_asm(100, 200) != 300)  { printf("FAIL: 100+200\n");   ok = 0; }
    if (add_asm(-5, 5) != 0)       { printf("FAIL: -5+5\n");      ok = 0; }
    if (add_asm(0, 0) != 0)        { printf("FAIL: 0+0\n");       ok = 0; }
    if (add_asm(1000, -1) != 999)  { printf("FAIL: 1000-1\n");    ok = 0; }

    if (ok)
        printf("tasm passed\n");
    return ok ? 0 : 1;
}
