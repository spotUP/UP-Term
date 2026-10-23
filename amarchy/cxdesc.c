#include <string.h>
#include "cxdesc.h"
#include "../config/upconf.h"

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
    /* commodities.library masks are CARE masks (match = ((ie ^ ix) & mask) == 0; the NDK header
     * comment says "don't care", the library does the opposite, as creep-cterm/src/hide.c shows).
     * The raw code is exact, the up-stroke bit included (key down only). Left Amiga is the one
     * qualifier tested here; every other qualifier bit is ignored, the gate decides with
     * cx_qual_mask. A description string cannot carry the code: ParseIX takes a key NAME or a
     * character of the current keymap, and rejects a number (the rig saw rows=0). */
    x->code = b->code;
    x->code_mask = 0xFFFF;
    x->qual = IEQ_LCOMMAND;
    x->qual_mask = IEQ_LCOMMAND;
}

int cx_row_takes(unsigned short row_code, unsigned row_qual, unsigned ie_code, unsigned ie_qual)
{
    return row_code == ie_code && row_qual == cx_qual_mask(ie_qual);
}

int cx_helper_cmd(char *out, int cap, const char *dir, const char *prog, const char *args)
{
    int n = 0, dl = (int)strlen(dir), al = (int)strlen(args), pl = (int)strlen(prog);
    /* '"' dir ['/'] prog '"' ' ' args NUL */
    int need = 1 + dl + 1 + pl + 1 + 1 + al + 1;
    if (dl == 0 || need > cap)
        return 0;
    out[n++] = '"';
    memcpy(out + n, dir, dl); n += dl;
    if (dir[dl - 1] != ':' && dir[dl - 1] != '/')
        out[n++] = '/';
    memcpy(out + n, prog, pl); n += pl;
    out[n++] = '"';
    out[n++] = ' ';
    memcpy(out + n, args, al); n += al;
    out[n] = 0;
    return 1;
}

int cx_menu_cmd(char *out, int cap, const char *dir, int kind, const char *node, int style, int *text)
{
    char args[KM_ARG + 8];
    int other, is_text;
    if (kind == KA_MENU || kind == KA_VIEWER)
        other = 0;
    else if (kind == KA_OTHERMENU || kind == KA_OTHERVIEWER)
        other = 1;
    else
        return 0;
    is_text = (style == UC_MENU_TEXT) != other;
    if (kind == KA_MENU || kind == KA_OTHERMENU) {
        if (strlen(node) + 6 >= sizeof args)
            return 0;
        strcpy(args, "menu ");
        strcat(args, node[0] ? node : "root");
    } else {
        strcpy(args, "viewer");
    }
    if (text)
        *text = is_text;
    return cx_helper_cmd(out, cap, dir, is_text ? "amarchy-menu" : "amarchy-gui", args);
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
