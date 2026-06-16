/* tcodegen.c - regression tests for 3 DCC codegen optimizations:
 *   1. 16-bit constant shifts >= 8 bits
 *   2. Direct byte store to global struct fields
 *   3. Signed 16-bit comparison against literal 0
 *
 * Function names kept short (<=5 chars after _) to stay within L80's
 * 6-character symbol limit.
 */

#include <stdio.h>

/* --- Optimization 1: 16-bit constant shifts --- */

unsigned int lsr8(x)  unsigned int x; { return x >> 8; }
unsigned int lsr12(x) unsigned int x; { return x >> 12; }
unsigned int lsr15(x) unsigned int x; { return x >> 15; }
unsigned int lsl8(x)  unsigned int x; { return x << 8; }
unsigned int lsl9(x)  unsigned int x; { return x << 9; }

int asr8(x)  int x; { return x >> 8; }
int asr9(x)  int x; { return x >> 9; }
int asr15(x) int x; { return x >> 15; }

/* --- Optimization 2: direct global struct byte field store --- */

struct Stat {
    unsigned char rdy;
    unsigned char cod;
    unsigned char cnt;
};

struct Stat g_stat;

void srdy(v) unsigned char v; { g_stat.rdy = v; }
void scod(v) unsigned char v; { g_stat.cod = v; }
void scnt(v) unsigned char v; { g_stat.cnt = v; }

/* --- Optimization 3: signed 16-bit < 0 and >= 0 --- */

int isneg(x)    int x; { return x < 0; }
int isnong(x)   int x; { return x >= 0; }

int tchk1(void)
{
    int ok;
    ok = 1;

    /* unsigned right shift >= 8 */
    if (lsr8(0x1200) != 0x12)  { printf("FAIL lsr8 a\n");  ok = 0; }
    if (lsr8(0xFF00) != 0xFF)  { printf("FAIL lsr8 b\n");  ok = 0; }
    if (lsr8(0x0000) != 0x00)  { printf("FAIL lsr8 c\n");  ok = 0; }
    if (lsr12(0x5000) != 0x05) { printf("FAIL lsr12 a\n"); ok = 0; }
    if (lsr12(0xF000) != 0x0F) { printf("FAIL lsr12 b\n"); ok = 0; }
    if (lsr15(0x8000) != 0x01) { printf("FAIL lsr15 a\n"); ok = 0; }
    if (lsr15(0x7FFF) != 0x00) { printf("FAIL lsr15 b\n"); ok = 0; }

    /* unsigned left shift >= 8 */
    if (lsl8(0x0012) != 0x1200)  { printf("FAIL lsl8 a\n");  ok = 0; }
    if (lsl8(0x00FF) != 0xFF00)  { printf("FAIL lsl8 b\n");  ok = 0; }
    if (lsl9(0x0040) != 0x8000)  { printf("FAIL lsl9 a\n");  ok = 0; }
    if (lsl9(0x0041) != 0x8200)  { printf("FAIL lsl9 b\n");  ok = 0; }

    /* signed arithmetic right shift >= 8 */
    if (asr8(-256) != -1)   { printf("FAIL asr8 a\n");  ok = 0; }
    if (asr8(256) != 1)     { printf("FAIL asr8 b\n");  ok = 0; }
    if (asr8(-1) != -1)     { printf("FAIL asr8 c\n");  ok = 0; }
    if (asr8(0) != 0)       { printf("FAIL asr8 d\n");  ok = 0; }
    if (asr9(-512) != -1)   { printf("FAIL asr9 a\n");  ok = 0; }
    if (asr9(512) != 1)     { printf("FAIL asr9 b\n");  ok = 0; }
    if (asr15(-1) != -1)    { printf("FAIL asr15 a\n"); ok = 0; }
    if (asr15(1) != 0)      { printf("FAIL asr15 b\n"); ok = 0; }

    return ok;
}

int tchk2(void)
{
    int ok;
    ok = 1;

    g_stat.rdy = 0;
    g_stat.cod = 0;
    g_stat.cnt = 0;

    srdy(1);
    scod(42);
    scnt(99);

    if (g_stat.rdy != 1)  { printf("FAIL rdy\n"); ok = 0; }
    if (g_stat.cod != 42) { printf("FAIL cod\n"); ok = 0; }
    if (g_stat.cnt != 99) { printf("FAIL cnt\n"); ok = 0; }

    srdy(0);
    scod(0);
    if (g_stat.rdy != 0)  { printf("FAIL rdy2\n"); ok = 0; }

    return ok;
}

int tchk3(void)
{
    int ok;
    ok = 1;

    if (!isneg(-1))   { printf("FAIL neg -1\n");  ok = 0; }
    if (!isneg(-128)) { printf("FAIL neg -128\n"); ok = 0; }
    if ( isneg(0))    { printf("FAIL neg 0\n");   ok = 0; }
    if ( isneg(1))    { printf("FAIL neg 1\n");   ok = 0; }
    if ( isneg(127))  { printf("FAIL neg 127\n"); ok = 0; }

    if (!isnong(0))   { printf("FAIL nng 0\n");   ok = 0; }
    if (!isnong(1))   { printf("FAIL nng 1\n");   ok = 0; }
    if (!isnong(127)) { printf("FAIL nng 127\n"); ok = 0; }
    if ( isnong(-1))  { printf("FAIL nng -1\n");  ok = 0; }
    if ( isnong(-128)){ printf("FAIL nng -128\n");ok = 0; }

    return ok;
}

int main()
{
    int ok;
    ok = 1;
    if (!tchk1()) ok = 0;
    if (!tchk2()) ok = 0;
    if (!tchk3()) ok = 0;
    if (ok)
        printf("tcodegen passed\n");
    return ok ? 0 : 1;
}
