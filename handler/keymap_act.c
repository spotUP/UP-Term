#include <string.h>
#include "keymap_act.h"
#include "menu_ids.h"

static const struct { const char *name; long id; } ids[] = {
#define KACT_ROW(n) { #n, n },
    MENU_ID_LIST(KACT_ROW)
#undef KACT_ROW
};
#define NIDS ((int)(sizeof(ids) / sizeof(ids[0])))

long kact_menu_id(const char *name)
{
    int i;
    for (i = 0; i < NIDS; i++)
        if (!strcmp(ids[i].name, name))
            return ids[i].id;
    return 0;
}

const char *kact_menu_name(long id)
{
    int i;
    for (i = 0; i < NIDS; i++)
        if (ids[i].id == id)
            return ids[i].name;
    return 0;
}

int kact_resolve(const km_bind *b, kact *out)
{
    out->kind = KACT_NONE;
    out->id = 0;
    out->line = 0;
    switch (b->kind) {
    case KA_TAB:
        if (!strcmp(b->arg, "new"))
            out->id = MENU_TAB_NEW;
        else if (!strcmp(b->arg, "close"))
            out->id = MENU_TAB_CLOSE;
        else if (!strcmp(b->arg, "next"))
            out->id = MENU_TAB_NEXT;
        else if (!strcmp(b->arg, "prev"))
            out->id = MENU_TAB_PREV;
        else if (!strncmp(b->arg, "goto ", 5)) {
            long n = 0;
            const char *p = b->arg + 5;
            for (; *p >= '0' && *p <= '9' && n < 100; p++)
                n = n * 10 + (*p - '0');
            if (*p || n < 1 || n > 9)
                return KACT_NONE;
            out->id = MENU_TAB_GOTO0 + n;
        } else
            return KACT_NONE;
        out->kind = KACT_MENU;
        return KACT_MENU;
    case KA_MENUID:
        out->id = kact_menu_id(b->arg);
        if (!out->id)
            return KACT_NONE;
        out->kind = KACT_MENU;
        return KACT_MENU;
    case KA_SLASH:
        if (b->arg[0] != '/' || !b->arg[1])
            return KACT_NONE;
        if (!strcmp(b->arg, "/help")) {
            /* as the Help menu item: the requester or the printed list by the window's state */
            out->id = MENU_HELP_SLASH;
            out->kind = KACT_MENU;
            return KACT_MENU;
        }
        out->line = b->arg;
        out->kind = KACT_SLASH;
        return KACT_SLASH;
    case KA_MENU: case KA_VIEWER: case KA_RUN: case KA_REXX: case KA_OPEN:
        out->kind = KACT_DEFER;
        return KACT_DEFER;
    default:
        return KACT_NONE;
    }
}

static int key_char(unsigned code, char *ch)
{
    const char *n = keymap_key_name(code);
    if (!n)
        return 0;
    if (n[0] && !n[1]) {
        *ch = (n[0] >= 'a' && n[0] <= 'z') ? (char)(n[0] - 32) : n[0];
        return (*ch >= 'A' && *ch <= 'Z') || (*ch >= '0' && *ch <= '9');
    }
    if (!strcmp(n, "period"))
        *ch = '.';
    else if (!strcmp(n, "comma"))
        *ch = ',';
    else if (!strcmp(n, "plus"))
        *ch = '+';
    else if (!strcmp(n, "minus"))
        *ch = '-';
    else
        return 0;
    return 1;
}

int kact_comm_key(const keymap *k, long id, char out[2])
{
    int i;
    for (i = 0; i < k->nbind; i++) {
        const km_bind *b = &k->bind[i];
        kact a;
        char ch;
        if (b->qual != KM_Q_SUPER || kact_resolve(b, &a) != KACT_MENU || a.id != id)
            continue;
        if (!key_char(b->code, &ch) || (ch >= '0' && ch <= '9'))
            continue; /* a digit is a tab's, a command key would shadow Right Amiga 1-9 for nothing */
        if (keymap_find(k, KM_Q_SUPER | KM_Q_SHIFT, b->code))
            continue;
        out[0] = ch;
        out[1] = 0;
        return 1;
    }
    return 0;
}

int upreq_resolve(const upreq_body *r, kact *out, km_bind *tmp)
{
    const char *colon;
    size_t n;
    out->kind = KACT_NONE;
    out->id = 0;
    out->line = 0;
    if (r->version != UPREQ_VERSION)
        return UPREQ_BADVERSION;
    if (memchr(r->action, 0, sizeof r->action) == 0)
        return UPREQ_UNKNOWN;
    colon = strchr(r->action, ':');
    if (!colon || colon == r->action)
        return UPREQ_UNKNOWN;
    n = (size_t)(colon - r->action);
    memset(tmp, 0, sizeof *tmp);
    if (n == 3 && !memcmp(r->action, "tab", 3))
        tmp->kind = KA_TAB;
    else if (n == 6 && !memcmp(r->action, "menuid", 6))
        tmp->kind = KA_MENUID;
    else if (n == 5 && !memcmp(r->action, "slash", 5))
        tmp->kind = KA_SLASH;
    else if ((n == 4 && (!memcmp(r->action, "menu", 4) || !memcmp(r->action, "open", 4))) ||
             (n == 3 && (!memcmp(r->action, "run", 3))) || (n == 4 && !memcmp(r->action, "rexx", 4)))
        return UPREQ_NOTMINE;
    else
        return UPREQ_UNKNOWN;
    if (strlen(colon + 1) >= KM_ARG)
        return UPREQ_UNKNOWN;
    tmp->arg = colon + 1;       /* inside the request, which outlives the answer */
    if (kact_resolve(tmp, out) == KACT_NONE)
        return UPREQ_UNKNOWN;
    return UPREQ_OK;
}

int upreq_fill(upreq_body *r, int kind, const char *arg)
{
    const char *head = kind == KA_TAB ? "tab" : kind == KA_MENUID ? "menuid" :
                       kind == KA_SLASH ? "slash" : 0;
    memset(r, 0, sizeof *r);
    if (!head || strlen(head) + 1 + strlen(arg) >= sizeof r->action)
        return 0;
    r->version = UPREQ_VERSION;
    strcpy(r->action, head);
    strcat(r->action, ":");
    strcat(r->action, arg);
    return 1;
}
