/* tc89fltb.c - intentionally unsupported float operations */
float f;
float g;
int main(void)
{
    f = 1.0f;    /* should be rejected clearly */
    g = f + g;   /* should be rejected clearly */
    return 0;
}
