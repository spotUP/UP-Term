#include <string.h>
#include "help_core.h"
#include "menu_core.h"

static void put(char *out, int cap, int *n, const char *s, int len)
{
    while (len-- > 0 && *n < cap - 1)
        out[(*n)++] = *s++;
    out[*n] = 0;
}

/* the bind whose action text "<prefix>[:<arg>]" equals act */
static const km_bind *find_bind(const keymap *k, const char *act)
{
    int i;
    for (i = 0; i < k->nbind; i++) {
        const km_bind *b = &k->bind[i];
        const char *pre = keymap_kind_name(b->kind);
        size_t pl = strlen(pre);
        if (!pl || strncmp(act, pre, pl))
            continue;
        if (act[pl] == 0 ? b->arg[0] == 0 : (act[pl] == ':' && !strcmp(act + pl + 1, b->arg)))
            return b;
    }
    return 0;
}

/* "style.theme" -> "Style > Theme": each prefix of the id names its row's label when the tree has one, else the
 * segment with a capital first letter */
static void menu_path(const keymap *k, const char *id, char *out, int cap)
{
    int n = 0, i, seg, il = (int)strlen(id);
    out[0] = 0;
    for (seg = 0; seg <= il; seg++) {
        const char *label = 0;
        if (id[seg] && id[seg] != '.')
            continue;
        for (i = 0; i < k->nmenu; i++)
            if ((int)strlen(k->menu[i].id) == seg && !strncmp(k->menu[i].id, id, seg)) {
                label = k->menu[i].label;
                break;
            }
        if (n)
            put(out, cap, &n, " > ", 3);
        if (label && *label)
            put(out, cap, &n, label, (int)strlen(label));
        else {
            int st = seg;
            char c;
            while (st > 0 && id[st - 1] != '.')
                st--;
            c = id[st];
            if (c >= 'a' && c <= 'z')
                c = (char)(c - 32);
            put(out, cap, &n, &c, 1);
            put(out, cap, &n, id + st + 1, seg - st - 1);
        }
    }
}

int hc_expand(const keymap *k, const char *tmpl, char *out, int cap)
{
    int n = 0;
    if (cap < 1)
        return 0;
    out[0] = 0;
    while (*tmpl) {
        const char *end = 0;
        int iskey = !strncmp(tmpl, "{key:", 5), ismenu = !strncmp(tmpl, "{menu:", 6);
        if (iskey || ismenu)
            end = strchr(tmpl, '}');
        if (end) {
            char arg[96], val[160];
            int off = iskey ? 5 : 6, al = (int)(end - tmpl) - off;
            if (al >= (int)sizeof arg)
                al = (int)sizeof arg - 1;
            memcpy(arg, tmpl + off, al);
            arg[al] = 0;
            val[0] = 0;
            if (iskey) {
                const km_bind *b = find_bind(k, arg);
                if (!b || !mn_spec(b, val, sizeof val))
                    val[0] = 0;
            } else if (arg[0])
                menu_path(k, arg, val, sizeof val);
            if (val[0])
                put(out, cap, &n, val, (int)strlen(val));
            else
                put(out, cap, &n, HC_UNBOUND, (int)strlen(HC_UNBOUND));
            tmpl = end + 1;
        } else {
            put(out, cap, &n, tmpl, 1);
            tmpl++;
        }
    }
    return n;
}

const char *hc_section(const char *text, long len, const char *name, long *blen)
{
    long i = 0, nl = (long)strlen(name), start = 0;
    const char *body = 0;
    while (i < len) {
        long e = i;
        while (e < len && text[e] != '\n')
            e++;
        if (text[i] == '[') {
            if (body) {
                *blen = i - start;
                return body;
            }
            if (e - i >= nl + 2 && !strncmp(text + i + 1, name, nl) && text[i + 1 + nl] == ']') {
                start = e < len ? e + 1 : len;
                body = text + start;
            }
        }
        i = e + 1;
    }
    if (body)
        *blen = len - start;
    return body;
}

int hc_lines(const char *body, long len, hc_line *out, int max)
{
    long i = 0;
    int n = 0;
    while (i < len && n < max) {
        long e = i;
        while (e < len && body[e] != '\n')
            e++;
        out[n].s = body + i;
        out[n].len = (int)(e - i);
        if (out[n].len && out[n].s[out[n].len - 1] == '\r')
            out[n].len--;
        n++;
        i = e + 1;
    }
    return n;
}

int hc_should_welcome(const am_prefs *p)
{
    return strcmp(p->val[AM_WELCOME], "on") != 0;
}

void hc_mark_shown(am_prefs *p)
{
    strcpy(p->val[AM_WELCOME], "on");
}

int hc_welcome_key(int ch, int *ticked, int *done)
{
    if (ch == ' ') {
        *ticked = !*ticked;
        return 1;
    }
    if (ch == '\r' || ch == '\n' || ch == 27) {
        *done = 1;
        return 1;
    }
    return 0;
}
