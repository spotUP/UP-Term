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
