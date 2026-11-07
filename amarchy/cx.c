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
#include "bar.h"

struct Library *CxBase;

static struct MsgPort *g_port;
static CxObj *g_broker, *g_pass;
static int g_enabled = 1, g_nrows;
/* The commodity's whole network is five objects for any number of rows: ONE filter (Left Amiga,
 * any raw code) with ONE custom gate, ONE sender and ONE swallow behind it. The gate looks the
 * row up in the installed keymap (row index = bind index) and queues it for the sender, whose
 * message cx_next pairs with the queue head (one message per queued row, in order). */
static const keymap *volatile g_km;
#define CXQ 16
static unsigned char g_q[CXQ];
static volatile unsigned char g_qh, g_qt;

/* The gate behind the one filter. A key goes on down the network (to the application) when no
 * row matches its exact code and qualifier mask, or when a window of ours has focus and the
 * row is the terminal's own (scope T): the handler runs those. Anything else is queued and
 * falls through to the sender, which reports it, and the CxTranslate(NULL) behind it swallows
 * the key. B rows (run:, menu:, viewer) are the commodity's even inside a terminal, because
 * the handler defers them (keymap_act.c KACT_DEFER). */
static void gate(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co)
{
    struct InputEvent *ie = (struct InputEvent *)CxMsgData(msg);
    const keymap *km = g_km;
    int i, n = km ? km->nbind : 0;
    (void)co;
    for (i = 0; i < n; i++) {
        const km_bind *b = &km->bind[i];
        if (cx_row_installed(b) && cx_row_takes(b->code, b->qual, ie->ie_Code, ie->ie_Qualifier)) {
            unsigned char nt = (unsigned char)((g_qt + 1) % CXQ);
            if (nt == g_qh || (upactive_count() > 0 && keymap_action_scope(b->kind) == KS_TERMINAL))
                break;   /* queue full: the key goes to the application rather than a lost row */
            g_q[g_qt] = (unsigned char)i;
            g_qt = nt;
            return;
        }
    }
    DivertCxMsg(msg, g_pass, g_pass);
}

/* The bar's right click. A window hears a press only when it is the active one, so the bar's click was lost after a
 * panel, Escape or a launch. The press is an input event: this second filter (right button down) takes it when the
 * pointer is over the bar's agents segment (bar_takes_right_press), queues the marker CXQ_BAR for the sender and the
 * swallow behind it, and lets every other press go on to the active window. The active window keeps the focus. */
#define CXQ_BAR 255
static void rgate(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co)
{
    (void)co;
    if (bar_takes_right_press()) {
        unsigned char nt = (unsigned char)((g_qt + 1) % CXQ);
        if (nt != g_qh) {
            g_q[g_qt] = CXQ_BAR;
            g_qt = nt;
            return;
        }
    }
    DivertCxMsg(msg, g_pass, g_pass);
}

static void nop(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co) { (void)msg; (void)co; }

int cx_open(int priority)
{
    struct NewBroker nb;
    LONG err = 0;
    IX ix;
    CxObj *f, *c, *s, *rf, *rc, *rs;
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
    f = CxFilter((STRPTR)CX_FILTER_DESC);
    c = CxCustom((APTR)gate, 0);
    s = CxSender(g_port, 0);
    if (!g_pass || !f || !c || !s || CxObjError(f)) {
        DeleteCxObj(c); DeleteCxObj(s); DeleteCxObj(f);
        return 0;
    }
    /* any raw code with Left Amiga down: the code is the gate's to compare exactly */
    ix.ix_Version = IX_VERSION;
    ix.ix_Class = IECLASS_RAWKEY;
    ix.ix_Code = 0;
    ix.ix_CodeMask = 0;
    ix.ix_Qualifier = IEQUALIFIER_LCOMMAND;
    ix.ix_QualMask = IEQUALIFIER_LCOMMAND;
    ix.ix_QualSame = 0;
    SetFilterIX(f, &ix);
    AttachCxObj(f, c);
    AttachCxObj(f, s);
    AttachCxObj(f, CxTranslate(0));
    AttachCxObj(g_broker, f);
    rf = CxFilter((STRPTR) "rawmouse");
    rc = CxCustom((APTR)rgate, 0);
    rs = CxSender(g_port, 0);
    if (!rf || !rc || !rs || CxObjError(rf)) {
        DeleteCxObj(rc); DeleteCxObj(rs); DeleteCxObj(rf);
    } else {
        IX rx;
        rx.ix_Version = IX_VERSION;
        rx.ix_Class = IECLASS_RAWMOUSE;
        rx.ix_Code = IECODE_RBUTTON;
        rx.ix_CodeMask = 0xFFFF;
        rx.ix_Qualifier = 0;
        rx.ix_QualMask = 0;
        rx.ix_QualSame = 0;
        SetFilterIX(rf, &rx);
        AttachCxObj(rf, rc);
        AttachCxObj(rf, rs);
        AttachCxObj(rf, CxTranslate(0));
        AttachCxObj(g_broker, rf);
    }
    ActivateCxObj(g_broker, 1);
    return 1;
}

void cx_close(void)
{
    CxMsg *m;
    cx_watch_input(0);                            /* the demo's filters leave the broker before DeleteCxObjAll */
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

void cx_detach(void)
{
    g_nrows = 0;
    g_km = 0;
}

int cx_install(const keymap *km)
{
    int i, n = 0;
    g_nrows = 0;
    g_km = 0;
    g_qh = g_qt = 0;
    for (i = 0; i < km->nbind; i++)
        if (cx_row_installed(&km->bind[i]))
            n++;
    g_km = km;
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
            (void)id;
            if (g_qh == g_qt)
                continue;
            *row = g_q[g_qh];
            g_qh = (unsigned char)((g_qh + 1) % CXQ);
            if (*row == CXQ_BAR)
                return CXE_BAR_RIGHT;
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
const km_bind *cx_row(int row) { return g_km && row >= 0 && row < g_km->nbind && cx_row_installed(&g_km->bind[row]) ? &g_km->bind[row] : 0; }
int cx_nrows(void) { return g_nrows; }

/* The demo tour's end-on-input watch (demo.c): while on, two more filters sit in front of the hotkey ones (priority 1),
 * one for every raw key and one for every raw mouse event, each with a CxCustom that only records that a key went down
 * or a button was pressed. Nothing is translated, sent or swallowed: the event goes on to its window. The callback runs
 * on the input.device task, so it sets one flag and does nothing else. */
static CxObj *g_wk, *g_wm;
static volatile int g_seen;
static volatile long g_info;                      /* class * 1000 + code of the event that ended the demo */

static void watch(__reg("a0") CxMsg *msg, __reg("a1") CxObj *co)
{
    const struct InputEvent *ie = (const struct InputEvent *)CxMsgData(msg);
    (void)co;
    if (ie->ie_Qualifier & IEQUALIFIER_REPEAT)
        return;                                   /* a held key (the one that started the demo) is not a new press */
    if (ie->ie_Class == IECLASS_RAWKEY) {
        if (!(ie->ie_Code & IECODE_UP_PREFIX))
            { g_seen = 1; g_info = 1000L + ie->ie_Code; }
    } else if (ie->ie_Class == IECLASS_RAWMOUSE) {
        if (ie->ie_Code == IECODE_LBUTTON || ie->ie_Code == IECODE_RBUTTON)
            { g_seen = 1; g_info = 2000L + ie->ie_Code; }
    }
}

static CxObj *watch_filter(UBYTE cls)
{
    /* the string is a ParseIX description, so it must parse: an arbitrary name leaves CxObjError set */
    CxObj *f = CxFilter((STRPTR)(cls == IECLASS_RAWKEY ? "rawkey" : "rawmouse")), *c = CxCustom((APTR)watch, 0);
    IX ix;
    if (!f || !c || CxObjError(f)) {
        DeleteCxObj(c);
        DeleteCxObj(f);
        return 0;
    }
    ix.ix_Version = IX_VERSION;
    ix.ix_Class = cls;
    ix.ix_Code = 0;
    ix.ix_CodeMask = 0;                           /* every code: the callback picks key downs and button presses */
    ix.ix_Qualifier = 0;
    ix.ix_QualMask = 0;
    ix.ix_QualSame = 0;
    SetFilterIX(f, &ix);
    AttachCxObj(f, c);
    SetCxObjPri(f, 1);
    EnqueueCxObj(g_broker, f);
    return f;
}

void cx_watch_input(int on)
{
    if (on) {
        if (!g_broker || g_wk || g_wm)
            return;
        g_seen = 0;
        g_wk = watch_filter(IECLASS_RAWKEY);
        g_wm = watch_filter(IECLASS_RAWMOUSE);
        return;
    }
    if (g_wk) {
        RemoveCxObj(g_wk);
        DeleteCxObjAll(g_wk);                     /* the filter and the custom behind it */
        g_wk = 0;
    }
    if (g_wm) {
        RemoveCxObj(g_wm);
        DeleteCxObjAll(g_wm);
        g_wm = 0;
    }
}

int cx_input_seen(void) { return g_seen; }
long cx_input_info(void) { return g_info; }
