#include <stdio.h>
#include <stdlib.h>

/* Unit test for the inp()/outp() port-I/O extension (declared in <stdlib.h>).
 *
 * inp()/outp() drive the Z80 IN/OUT instructions, so the byte read back from a
 * port is hardware/emulator dependent.  This test therefore does NOT assert on
 * the value read; it verifies only the portable, deterministic guarantees:
 *   - the prototypes are accepted and the runtime symbols link,
 *   - outp() and inp() execute without trapping, and
 *   - inp() returns its 8-bit result zero-extended into the range 0..255.
 *
 * Port 0x12 is used because it is not wired to a device in the common CP/M
 * emulators, so writing and reading it has no observable side effects. */
int main(void)
{
    int v;

    outp(0x12, 0x34);                 /* OUT (0x12),0x34 -- must not trap */
    v = inp(0x12);                    /* IN  A,(0x12)                     */

    printf("inp/outp executed\n");
    printf("inp result 0..255: %s\n", (v >= 0 && v <= 255) ? "ok" : "FAIL");
    printf("port io ok\n");
    return 0;
}
