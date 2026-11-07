/* Stack high-water probe (amarchy/stkprobe.h says how it is used). Compiled only with `make STKPROBE=1`. */
#include <exec/types.h>
#include <exec/tasks.h>
#include <proto/exec.h>
#include "sentinel_dos.h"
#include "stkprobe.h"

#define FILL 0xA5

static unsigned char *g_lo, *g_hi;
static long g_max;
static char g_name[24];

/* <prefix>_STK_<suffix>, then published; every write is bounded by the buffer */
static void publish(const char *prefix, const char *suffix, long n)
{
    unsigned i = 0;
    const char *part[3];
    int k;
    part[0] = prefix;
    part[1] = "_STK_";
    part[2] = suffix;
    for (k = 0; k < 3; k++) {
        const char *s;
        for (s = part[k]; *s && i < sizeof g_name - 1; s++)
            g_name[i++] = *s;
    }
    g_name[i] = 0;
    am_sentinel(g_name, n);
}

static char g_prefix[12];

void stk_probe_init(const char *prefix)
{
    struct Task *t = FindTask(0);
    unsigned char *p, *sp = (unsigned char *)&t - 256;
    int i;
    for (i = 0; prefix[i] && i < (int)sizeof g_prefix - 1; i++)
        g_prefix[i] = prefix[i];
    g_prefix[i] = 0;
    g_lo = (unsigned char *)t->tc_SPLower;
    g_hi = (unsigned char *)t->tc_SPUpper;
    publish(g_prefix, "LO", (long)g_lo);
    publish(g_prefix, "HI", (long)g_hi);
    if (sp < g_lo || sp >= g_hi) {          /* SP is not on the task's own stack: nothing can be measured */
        publish(g_prefix, "SIZE", -1);
        g_lo = g_hi = 0;
        return;
    }
    for (p = g_lo; p < sp; p++)
        *p = FILL;
    publish(g_prefix, "SIZE", (long)(g_hi - g_lo));
}

void stk_probe(void)
{
    unsigned char *p = g_lo;
    long used;
    if (!p)
        return;
    while (p < g_hi && *p == FILL)
        p++;
    used = (long)(g_hi - p);
    if (used > g_max) {
        g_max = used;
        publish(g_prefix, "USED", used);
    }
}
