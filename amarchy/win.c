/* The OS side of the win: verbs (win.h). win_core.c does every number; this file gathers a window's box under
 * LockIBase, hands it to win_core and applies the result.
 *
 * Lock discipline (plan 1.3, K6): the lookup, the membership check (walking Screen->FirstWindow) and the calls
 * that only queue a request to Intuition (ChangeWindowBox, ZipWindow, WindowToFront, WindowToBack) run with
 * IBase locked, so the window cannot die between the check and the call. ActivateWindow, the Screen* calls,
 * ShowTitle, pubscreen lookups and workbench.library calls run after UnlockIBase. A foreign window is never
 * closed. */
#include <exec/types.h>
#include <exec/memory.h>
#include <intuition/intuition.h>
#include <intuition/intuitionbase.h>
#include <intuition/screens.h>
#include <workbench/workbench.h>
#include <utility/tagitem.h>
#include <proto/exec.h>
#include <proto/intuition.h>
#include <proto/wb.h>
#include <string.h>
#include "win.h"
#include "win_core.h"
#include "win_apply.h"
#include "../handler/upactive.h"

#define WIN_MAXW 32          /* windows on one screen considered by cycle and focus */
#define MENU_TITLE "Amarchy" /* the drawn menu, the viewer and the text menu all carry it (gui.c, act.c) */

/* WorkbenchBase is wblaunch.c's global; opened and closed around each use there too. */
extern struct Library *WorkbenchBase;

static long g_wins;
static struct Window *g_ticket;
static win_box g_tkbox;                   /* the ticket window's box at take time (K5a) */
static win_ring g_ring;
static int g_ring_ready;

long win_count(void) { return g_wins; }

static int is_ours(const struct Window *w)
{
    return w->Title && strcmp((const char *)w->Title, MENU_TITLE) == 0;
}

/* IBase locked: w is a member of some screen's window list. */
static int member(const struct Window *w)
{
    const struct Screen *s;
    const struct Window *x;
    if (!w)
        return 0;
    for (s = IntuitionBase->FirstScreen; s; s = s->NextScreen)
        for (x = s->FirstWindow; x; x = x->NextWindow)
            if (x == w)
                return 1;
    return 0;
}

static int usable(const struct Window *w)
{
    return w && member(w) && !is_ours(w);
}

/* IBase locked: the ticket is still a window of some screen and nothing moved or sized it since the menu opened. */
static int ticket_valid(void)
{
    const struct Window *t = g_ticket;
    win_box now;
    if (!t)
        return 0;
    memset(&now, 0, sizeof now);
    now.left = t->LeftEdge;
    now.top = t->TopEdge;
    now.width = t->Width;
    now.height = t->Height;
    return win_ticket_valid(usable(t), &g_tkbox, &now);
}

void win_ticket_take(void)
{
    struct Window *aw;
    ULONG lock;
    if (!IntuitionBase)
        return;
    lock = LockIBase(0);
    aw = IntuitionBase->ActiveWindow;
    if (!aw || !member(aw))
        g_ticket = 0;
    else if (!is_ours(aw)) {
        g_ticket = aw;
        memset(&g_tkbox, 0, sizeof g_tkbox);
        g_tkbox.left = aw->LeftEdge;
        g_tkbox.top = aw->TopEdge;
        g_tkbox.width = aw->Width;
        g_tkbox.height = aw->Height;
    }
    UnlockIBase(lock);
}

int win_target_rect(struct Screen **sc, long *x, long *y, long *w, long *h)
{
    struct Window *t;
    ULONG lock;
    int ok = 0;
    if (!IntuitionBase)
        return 0;
    lock = LockIBase(0);
    t = ticket_valid() ? g_ticket : IntuitionBase->ActiveWindow;
    if (usable(t)) {
        *sc = t->WScreen;
        *x = t->LeftEdge;
        *y = t->TopEdge;
        *w = t->Width;
        *h = t->Height;
        ok = 1;
    }
    UnlockIBase(lock);
    return ok;
}

void win_active_terminal(int *in_term, int *full)
{
    struct Window *aw;
    ULONG lock;
    *in_term = *full = 0;
    if (!IntuitionBase || upactive_count() <= 0)
        return;
    lock = LockIBase(0);
    aw = IntuitionBase->ActiveWindow;
    if (aw && member(aw) && !is_ours(aw)) {
        win_screen s;
        *in_term = 1;
        s.width = aw->WScreen->Width;
        s.height = aw->WScreen->Height;
        s.title = 0;
        *full = win_is_fullscreen_terminal((aw->Flags & (WFLG_BACKDROP | WFLG_BORDERLESS)) == (WFLG_BACKDROP | WFLG_BORDERLESS),
                                           aw->LeftEdge, aw->TopEdge, aw->Width, aw->Height, &s);
    }
    UnlockIBase(lock);
}

static int close_drawers(void)
{
    struct List *list = 0;
    struct TagItem get[2], put[2];
    struct Node *n;
    if (!(WorkbenchBase = OpenLibrary((STRPTR)"workbench.library", 44)))
        return 0;
    get[0].ti_Tag = WBCTRLA_GetOpenDrawerList; get[0].ti_Data = (ULONG)&list;
    get[1].ti_Tag = TAG_DONE;
    if (WorkbenchControlA(0, get) && list) {
        for (n = list->lh_Head; n->ln_Succ; n = n->ln_Succ)
            if (n->ln_Name)
                CloseWorkbenchObjectA((CONST_STRPTR)n->ln_Name, 0);
        put[0].ti_Tag = WBCTRLA_FreeOpenDrawerList; put[0].ti_Data = (ULONG)list;
        put[1].ti_Tag = TAG_DONE;
        WorkbenchControlA(0, put);
    }
    CloseLibrary(WorkbenchBase);
    WorkbenchBase = 0;
    return 1;
}

/* Screen verbs (no target window). */
static int do_screen(const win_cmd *c)
{
    struct Screen *first, *last, *s = 0;
    ULONG lock;
    int ok = 1;
    if (c->arg == WA_WORKBENCH) {
        if (!(s = LockPubScreen((STRPTR)"Workbench")))
            return 0;
        ScreenToFront(s);
        UnlockPubScreen(0, s);
        return 1;
    }
    lock = LockIBase(0);
    first = IntuitionBase->FirstScreen;
    for (last = first; last && last->NextScreen; last = last->NextScreen)
        ;
    UnlockIBase(lock);
    if (!first)
        return 0;
    switch (c->arg) {
    case WA_NEXT: case WA_BACK:
        if (c->arg == WA_NEXT && first == last)
            return 0;
        ScreenToBack(first);
        break;
    case WA_PREV:
        if (first == last)
            return 0;
        ScreenToFront(last);
        break;
    default: ok = 0; break;
    }
    return ok;
}

static int do_title(const win_cmd *c)
{
    struct Screen *s;
    ULONG lock;
    LONG show = 1;
    lock = LockIBase(0);
    s = IntuitionBase->FirstScreen;
    if (s && c->arg == WA_TOGGLE)
        show = (s->Flags & SHOWTITLE) ? 0 : 1;
    else if (c->arg == WA_HIDE)
        show = 0;
    UnlockIBase(lock);
    if (!s)
        return 0;
    ShowTitle(s, show);
    return 1;
}

static int win_exec(const char *arg, int via_port)
{
    win_cmd c;
    win_box b, boxes[WIN_MAXW];
    win_screen scr;
    struct Window *w = 0, *ws[WIN_MAXW], *act = 0, *aw;
    ULONG lock;
    int ok = 0, n, me, pick;
    if (!win_parse(arg, &c))
        return -1;
    if (!IntuitionBase)
        return 0;
    if (c.verb == WV_SCREEN)
        ok = do_screen(&c);
    else if (c.verb == WV_TITLE)
        ok = do_title(&c);
    else if (c.verb == WV_CLOSEDRAWERS)
        ok = close_drawers();
    else {
        lock = LockIBase(0);
        aw = IntuitionBase->ActiveWindow;
        if (via_port && aw && is_ours(aw) && member(aw) && ticket_valid())
            w = g_ticket;                 /* K5a: the menu is active, the owner meant the ticket's window */
        else if (usable(aw))
            w = aw;
        g_ticket = 0;                     /* one use; a hotkey or a live target drops it too */
        if (w) {
            win_box_of(w, &b, &scr);
            ok = 1;
            switch (c.verb) {
            case WV_FRONT: win_raise(w); break;
            case WV_BACK:  WindowToBack(w); break;
            case WV_ZIP:   ZipWindow(w); break;
            case WV_MOVE:  if (win_move(&b, c.arg, c.pixels, &scr)) win_apply_box(w, &b); break;
            case WV_SIZE:  if (win_size(&b, c.arg, c.pixels, &scr)) win_apply_box(w, &b); break;
            case WV_FULLWIDTH:  if (win_fullwidth(&b, &scr)) win_apply_box(w, &b); break;
            case WV_FULLSCREEN: if (win_fullscreen(&b, &scr)) win_apply_box(w, &b); break;
            case WV_SAVE:
                if (!g_ring_ready) { win_ring_clear(&g_ring); g_ring_ready = 1; }
                win_save(&g_ring, (long)w, &b);
                break;
            case WV_RESTORE: {
                win_box old;
                if (g_ring_ready && win_restore(&g_ring, (long)w, &old)) {
                    old.minw = b.minw; old.minh = b.minh; old.maxw = b.maxw; old.maxh = b.maxh;
                    win_apply_box(w, &old);
                } else
                    ok = 0;
                break;
            }
            case WV_CYCLE: case WV_FOCUS: {
                const struct Window *x;
                n = 0; me = -1;
                for (x = w->WScreen->FirstWindow; x && n < WIN_MAXW; x = x->NextWindow) {
                    if (is_ours(x))
                        continue;
                    if (x == w)
                        me = n;
                    ws[n] = (struct Window *)x;
                    win_box_of(x, &boxes[n], &scr);
                    n++;
                }
                pick = me < 0 ? -1 : win_pick(boxes, n, me, c.arg);
                if (pick < 0)
                    ok = 0;
                else {
                    act = ws[pick];
                    if (c.verb == WV_CYCLE)
                        win_raise(act);
                }
                break;
            }
            default: ok = 0; break;
            }
        }
        UnlockIBase(lock);
        if (act) {
            ActivateWindow(act);
            /* act was a member under the lock a moment ago; ActivateWindow validates itself */
        }
    }
    if (ok)
        g_wins++;
    else if (!(c.verb == WV_NONE))
        DisplayBeep(0);
    return ok;
}

int act_win(const char *arg)
{
    return win_exec(arg, 0) > 0;
}

int win_port(const char *arg)
{
    int r = win_exec(arg, 1);
    return r < 0 ? WIN_RC_UNKNOWN : r ? WIN_RC_OK : WIN_RC_REFUSED;
}
