/* Runs every host suite, or only the one named by argv[1]. */
#include "harness.h"

void suite_xterm(void);
void suite_keys(void);
void suite_amiga(void);
void suite_pcansi(void);
void suite_glyph(void);
void suite_mirror(void);

static const h_suite suites[] = {
    { "xterm", suite_xterm },
    { "keys", suite_keys },
    { "amiga", suite_amiga },
    { "pcansi", suite_pcansi },
    { "glyph", suite_glyph },
    { "mirror", suite_mirror },
    { 0, 0 }
};

int main(int argc, char **argv)
{
    int i, ran = 0;
    for (i = 0; suites[i].name; i++) {
        int f0 = h_failures, c0 = h_checks;
        if (argc > 1 && argv[1][0] && strcmp(argv[1], suites[i].name))
            continue;
        suites[i].fn();
        printf("[%s] %s: %d checks, %d failed\n", h_failures == f0 ? "OK" : "FAIL",
               suites[i].name, h_checks - c0, h_failures - f0);
        ran++;
    }
    if (!ran) {
        printf("[ERROR] no suite named %s\n", argc > 1 ? argv[1] : "");
        return 2;
    }
    return h_failures ? 1 : 0;
}
