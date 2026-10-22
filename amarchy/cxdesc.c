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

void cx_ixspec(const km_bind *b, cx_ixspec_t *x)
{
    /* The raw code is exact, the up-stroke bit included (key down only). Left Amiga is the one
     * qualifier demanded; every other qualifier bit is "don't care" here, the gate decides with
     * cx_qual_mask. A description string cannot carry the code: ParseIX takes a key NAME or a
     * character of the current keymap, and rejects a number (the rig saw rows=0). */
    x->code = b->code;
    x->code_mask = 0;
    x->qual = IEQ_LCOMMAND;
    x->qual_mask = (unsigned short)~IEQ_LCOMMAND;
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
