#include <stdio.h>
#include <math.h>
static volatile double xs[] = {0.0, 0.5, 1.0, 2.5, -3.75, 100.125};
double (*volatile psin)(double) = sin;
double (*volatile pcos)(double) = cos;
__attribute__((noinline)) void fused(double x, double *s, double *c) { *s = sin(x); *c = cos(x); }
int main(void) {
    int i, bad = 0;
    for (i = 0; i < 6; i++) {
        double s, c, x = xs[i];
        fused(x, &s, &c);
        if (s != psin(x) || c != pcos(x)) bad++;
    }
    printf("%s\n", bad ? "FAIL" : "PASS");
    return bad != 0;
}
