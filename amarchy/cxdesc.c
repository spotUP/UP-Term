#include "cxdesc.h"

/* IEQUALIFIER_* values (devices/inputevent.h), kept here so the host builds */
#define IEQ_LSHIFT   0x0001
#define IEQ_RSHIFT   0x0002
#define IEQ_CONTROL  0x0008
#define IEQ_LALT     0x0010
#define IEQ_RALT     0x0020
#define IEQ_LCOMMAND 0x0040
#define IEQ_RCOMMAND 0x0080
#define IEQ_REPEAT   0x0200

static int put(char *out, int n, int *at, const char *s)
{
    while (*s) {
        if (*at >= n - 1)
            return 0;
        out[(*at)++] = *s++;
    }
    return 1;
}

int cx_describe(const km_bind *b, char *out, int n)
{
    static const char hex[] = "0123456789abcdef";
    char num[5];
    int at = 0;
    if (!b || !out || n < 1)
        return 0;
    num[0] = '0'; num[1] = 'x'; num[2] = hex[(b->code >> 4) & 15]; num[3] = hex[b->code & 15]; num[4] = 0;
    if (!put(out, n, &at, "rawkey lcommand "))
        return 0;
    if ((b->qual & KM_Q_RSHIFT) && !put(out, n, &at, "rshift "))
        return 0;
    if ((b->qual & KM_Q_SHIFT) && !put(out, n, &at, "shift "))
        return 0;
    if ((b->qual & KM_Q_ALT) && !put(out, n, &at, "alt "))
        return 0;
    if (!put(out, n, &at, num))
        return 0;
    out[at] = 0;
    return 1;
}

unsigned cx_qual_mask(unsigned q)
{
    unsigned m = KM_Q_SUPER;
    if (!(q & IEQ_LCOMMAND) || (q & (IEQ_CONTROL | IEQ_RCOMMAND | IEQ_REPEAT)))
        return 0;
    if (q & (IEQ_LALT | IEQ_RALT))
        m |= KM_Q_ALT;
    if ((q & IEQ_RSHIFT) && !(q & IEQ_LSHIFT))
        m |= KM_Q_RSHIFT;
    else if (q & (IEQ_LSHIFT | IEQ_RSHIFT))
        m |= KM_Q_SHIFT;
    return m;
}

int cx_row_installed(const km_bind *b)
{
    return keymap_action_scope(b->kind) != KS_TERMINAL;
}
