#include <exec/types.h>
#include <exec/memory.h>
#include <devices/inputevent.h>
#include <libraries/commodities.h>
#include <proto/exec.h>
#include <proto/commodities.h>
#include <string.h>
#include "cx.h"
#include "cxdesc.h"
#include "../handler/upactive.h"

struct Library *CxBase;

static struct MsgPort *g_port;
static CxObj *g_broker, *g_pass;
static int g_enabled = 1, g_nrows;
/* Row index = the CxSender id. The gate (input.device's task) reads only the small
 * match table; the full bind is looked up in the installed keymap by g_idx. */
typedef struct row_key { unsigned short qual; unsigned char code, kind; } row_key;
static row_key g_rows[KM_MAX_BIND];
static unsigned char g_idx[KM_MAX_BIND];
static const keymap *g_km;
static CxObj *g_objs[KM_MAX_BIND];

/* The gate in front of every hotkey's sender; it tests only its own row (its CxCustom id). A key goes on down the network
 * (to the next hotkey filter, then to the application) when no row matches its
 * exact qualifier mask, or when a window of ours has focus and the row is the
 * terminal's own (scope T): the handler runs those. Anything else falls through
 * to the sender, which reports it, and the CxTranslate(NULL) behind it swallows
 * the key. B rows (run:, menu:, viewer) are the commodity's even inside a
 * terminal, because the handler defers them (keymap_act.c KACT_DEFER). */
static void gate(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);
    long i = CxMsgID(msg);   /* this filter's own row: the id given to its CxCustom */
    (void)co;
    if (i < 0 || i >= g_nrows || !cx_row_takes(g_rows[i].code, g_rows[i].qual, ie->ie_Code, ie->ie_Qualifier) ||
        (upactive_count() > 0 && keymap_action_scope(g_rows[i].kind) == KS_TERMINAL))
        DivertCxMsg(msg, g_pass, g_pass);
}

static void nop(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co) { (void)msg; (void)co; }

int cx_open(int priority)
{
    struct NewBroker nb;
    LONG err = 0;
    if (!(CxBase = OpenLibrary((STRPTR)"commodities.library", 37)))
        return 0;
    if (!(g_port = CreateMsgPort()))
        return 0;
    memset(&nb, 0, sizeof(nb));
    nb.nb_Version = NB_VERSION;
    nb.nb_Name = (STRPTR)"Amarchy";
    nb.nb_Title = (STRPTR)"Amarchy Super keys";
    nb.nb_Descr = (STRPTR)"Left Amiga key bindings from the keymap file";
    nb.nb_Unique = NBU_UNIQUE | NBU_NOTIFY;
    nb.nb_Flags = COF_SHOW_HIDE;
    nb.nb_Pri = (BYTE)priority;
    nb.nb_Port = g_port;
    g_broker = CxBroker(&nb, &err);
    if (!g_broker)
        return 0;
    g_pass = CxCustom((APTR)nop, 0);
    ActivateCxObj(g_broker, 1);
    return 1;
}

void cx_close(void)
{
    CxMsg *m;
    if (g_broker)
        DeleteCxObjAll(g_broker);
    g_broker = 0;
    if (g_pass)
        DeleteCxObj(g_pass);
    g_pass = 0;
    if (g_port) {
        while ((m = (CxMsg *)GetMsg(g_port)) != 0)
            ReplyMsg((struct Message *)m);
        DeleteMsgPort(g_port);
    }
    g_port = 0;
    if (CxBase)
        CloseLibrary(CxBase);
    CxBase = 0;
}

ULONG cx_sigmask(void) { return g_port ? 1UL << g_port->mp_SigBit : 0; }

int cx_install(const keymap *km)
{
    int i, n = 0;
    cx_ixspec_t xs;
    IX ix;
    CxObj *f, *c, *s;
    for (i = 0; i < g_nrows; i++)
        if (g_objs[i])
            DeleteCxObjAll(g_objs[i]);
    g_nrows = 0;
    g_km = km;
    for (i = 0; i < km->nbind; i++) {
        const km_bind *b = &km->bind[i];
        if (!cx_row_installed(b))
            continue;
        g_rows[n].qual = b->qual;
        g_rows[n].code = b->code;
        g_rows[n].kind = b->kind;
        g_idx[n] = (unsigned char)i;
        f = CxFilter((STRPTR)CX_FILTER_DESC);
        c = CxCustom((APTR)gate, n);
        s = CxSender(g_port, n);
        if (!f || !c || !s || CxObjError(f)) {
            DeleteCxObj(f); DeleteCxObj(c); DeleteCxObj(s);
            continue;
        }
        cx_ixspec(b, &xs);
        ix.ix_Version = IX_VERSION;
        ix.ix_Class = IECLASS_RAWKEY;
        ix.ix_Code = xs.code;
        ix.ix_CodeMask = xs.code_mask;
        ix.ix_Qualifier = xs.qual;
        ix.ix_QualMask = xs.qual_mask;
        ix.ix_QualSame = 0;
        SetFilterIX(f, &ix);
        AttachCxObj(f, c);
        AttachCxObj(f, s);
        AttachCxObj(f, CxTranslate(0));
        AttachCxObj(g_broker, f);
        g_objs[n++] = f;
    }
    g_nrows = n;
    return n;
}

int cx_next(int *row)
{
    CxMsg *m;
    while ((m = (CxMsg *)GetMsg(g_port)) != 0) {
        ULONG type = CxMsgType(m);
        ULONG id = CxMsgID(m);
        ReplyMsg((struct Message *)m);
        if (type == CXM_IEVENT) {
            *row = (int)id;
            return CXE_KEY;
        }
        if (type == CXM_COMMAND) {
            switch (id) {
            case CXCMD_ENABLE:     g_enabled = 1; ActivateCxObj(g_broker, 1); return CXE_ENABLE;
            case CXCMD_DISABLE:    g_enabled = 0; ActivateCxObj(g_broker, 0); return CXE_DISABLE;
            case CXCMD_KILL:       return CXE_KILL;
            case CXCMD_UNIQUE:     return CXE_UNIQUE;
            case CXCMD_APPEAR:     return CXE_APPEAR;
            case CXCMD_DISAPPEAR:  return CXE_DISAPPEAR;
            }
        }
    }
    return CXE_NONE;
}

int cx_enabled(void) { return g_enabled; }
const km_bind *cx_row(int row) { return g_km && row >= 0 && row < g_nrows ? &g_km->bind[g_idx[row]] : 0; }
int cx_nrows(void) { return g_nrows; }
