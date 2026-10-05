/* keys -- see keys.h. */
#include <stdlib.h>
#include <string.h>
#include "keys.h"

void keys_init(cl_keys *k)
{
    memset(k, 0, sizeof(*k));
    jw_init(&k->pb);
    jw_init(&k->out);
}

void keys_free(cl_keys *k)
{
    jw_free(&k->pb);
    jw_free(&k->out);
    k->n = 0;
}

void keys_feed(cl_keys *k, const char *s, long n)
{
    while (n > 0) {
        long room = (long)sizeof(k->b) - k->n, take = n < room ? n : room;
        if (take <= 0) {
            /* a flood nobody reads: in a paste it is the paste's */
            if (k->paste) {
                jw_raw(&k->pb, (const char *)k->b, k->n);
                k->n = 0;
                continue;
            }
            return;
        }
        memcpy(k->b + k->n, s, (size_t)take);
        k->n += (int)take;
        s += take;
        n -= take;
    }
}

static void eat(cl_keys *k, int n)
{
    if (n > k->n)
        n = k->n;
    memmove(k->b, k->b + n, (size_t)(k->n - n));
    k->n -= n;
}

static int set(cl_key *key, int kind, unsigned long ch, int mods)
{
    memset(key, 0, sizeof(*key));
    key->k = kind;
    key->ch = ch;
    key->mods = mods;
    return 1;
}

/* a key named by its code (kitty's CSI code;mods u, modifyOtherKeys'
 * CSI 27;mods;code ~); mods as KM_* */
static int code_key(cl_key *key, long code, int mods)
{
    if (code == 13 || code == 57414) {      /* Return, keypad Enter */
        if ((mods & (KM_SHIFT | KM_ALT | KM_CTRL)) == KM_CTRL)
            return set(key, K_ENTER, 0, mods);  /* Ctrl+Enter: send what is queued now */
        return set(key, (mods & (KM_SHIFT | KM_ALT)) ? K_NEWLINE : K_ENTER, 0, mods);
    }
    if (code == 27)
        return set(key, K_ESC, 0, mods);
    if (code == 9)
        return set(key, (mods & KM_SHIFT) ? K_BTAB : K_TAB, 0, mods);
    if (code == 127 || code == 8)
        return (mods & (KM_ALT | KM_CTRL)) ? set(key, K_ALT, 0x7f, mods) : set(key, K_BS, 0, mods);
    if (code >= 57344 && code <= 63743)
        return 0;                           /* kitty's private-use keys: not ours */
    if ((mods & KM_CTRL) && code == '[')
        return set(key, K_ESC, 0, mods);    /* Ctrl+[ is Escape */
    if ((mods & KM_CTRL) && (code == '_' || code == '-'))
        return set(key, K_CTRL, '_', mods); /* Ctrl+_ / Ctrl+Shift+-: undo */
    if ((mods & KM_CTRL) && ((code >= 'a' && code <= 'z') || (code >= 'A' && code <= 'Z')))
        return set(key, K_CTRL, (unsigned long)(code | 0x20), mods);
    if (mods & (KM_ALT | KM_CTRL))
        return set(key, K_ALT, (unsigned long)code, mods);
    if (code < 0x20)
        return 0;
    return set(key, K_CHAR, (unsigned long)code, mods);
}

static int xmods(long p)
{
    int m = p > 1 ? (int)(p - 1) : 0;
    return (m & 1 ? KM_SHIFT : 0) | (m & 2 ? KM_ALT : 0) | (m & 4 ? KM_CTRL : 0);
}

/* a CSI sequence at b[p..]: its length from p (the final included), 0
 * incomplete, -1 malformed */
static int csi_len(const unsigned char *b, int n, int p)
{
    int i;
    for (i = p; i < n && i - p < 40; i++) {
        if (b[i] >= 0x40 && b[i] <= 0x7e)
            return i - p + 1;
        if (b[i] < 0x20 || b[i] > 0x3f)
            return -1;
    }
    return i - p >= 40 ? -1 : 0;
}

static int csi_key(cl_key *key, const unsigned char *s, int len)
{
    long p[4];
    int np = 0, i = 0, inter = 0, priv = 0;
    unsigned char fin = s[len - 1];
    memset(p, 0, sizeof(p));
    if (len > 1 && s[0] >= '<' && s[0] <= '?') {
        priv = 1;
        i = 1;
    }
    for (; i < len - 1; i++) {
        unsigned char c = s[i];
        if (c >= '0' && c <= '9') {
            if (np == 0)
                np = 1;
            if (np <= 4 && p[np - 1] < 100000L)
                p[np - 1] = p[np - 1] * 10 + (c - '0');
        } else if (c == ';') {
            np = np ? np + 1 : 2;
        } else if (c == ':') {
            /* sub-parameters (kitty's alternate keys): skip to the next ';' */
            while (i + 1 < len - 1 && s[i + 1] != ';')
                i++;
        } else if (c >= 0x20 && c <= 0x2f) {
            inter = c;
        }
    }
    if (priv)
        return 0;
    if (inter == ' ') {                     /* the Amiga's Shift+Right / Shift+Left */
        if (fin == '@')
            return set(key, K_RIGHT, 0, KM_SHIFT);
        if (fin == 'A')
            return set(key, K_LEFT, 0, KM_SHIFT);
        return 0;
    }
    switch (fin) {
    case 'A':
        return set(key, K_UP, 0, xmods(p[1]));
    case 'B':
        return set(key, K_DOWN, 0, xmods(p[1]));
    case 'C':
        return set(key, K_RIGHT, 0, xmods(p[1]));
    case 'D':
        return set(key, K_LEFT, 0, xmods(p[1]));
    case 'H':
        return set(key, K_HOME, 0, xmods(p[1]));
    case 'F':
        return set(key, K_END, 0, xmods(p[1]));
    case 'T':
        return set(key, K_UP, 0, KM_SHIFT);   /* Amiga Shift+Up */
    case 'S':
        return set(key, K_DOWN, 0, KM_SHIFT);
    case 'Z':
        return set(key, K_BTAB, 0, KM_SHIFT);
    case 'I':
    case 'O':
        if (np)
            return 0;
        set(key, K_FOCUS, 0, 0);    /* focus reports (?1004), asked for by the client */
        key->row = fin == 'I';
        return 1;
    case 'R':
        if (np >= 2) {
            set(key, K_CPR, 0, 0);
            key->row = (int)p[0];
            key->col = (int)p[1];
            return 1;
        }
        return 0;
    case 'u':
        return code_key(key, p[0], xmods(p[1]));
    case '~':
        switch (p[0]) {
        case 1:
        case 7:
            return set(key, K_HOME, 0, xmods(p[1]));
        case 4:
        case 8:
            return set(key, K_END, 0, xmods(p[1]));
        case 2:
            return set(key, K_INS, 0, 0);
        case 3:
            return set(key, K_DEL, 0, xmods(p[1]));
        case 5:
            return set(key, K_PGUP, 0, 0);
        case 6:
            return set(key, K_PGDN, 0, 0);
        case 27:
            return np >= 3 ? code_key(key, p[2], xmods(p[1])) : 0;
        default:
            return 0;
        }
    default:
        return 0;
    }
}

/* where the paste's end marker is in b, -1 none; *keep: bytes at the end
 * that may be the start of one */
static int paste_end(const unsigned char *b, int n, int *at, int *len)
{
    static const char e7[] = "\033[201~", e8[] = "\233201~";
    int i;
    for (i = 0; i < n; i++) {
        if (b[i] == 0x1b && n - i >= 6 && !memcmp(b + i, e7, 6)) {
            *at = i;
            *len = 6;
            return 1;
        }
        if (b[i] == 0x9b && n - i >= 5 && !memcmp(b + i, e8, 5)) {
            *at = i;
            *len = 5;
            return 1;
        }
    }
    /* a possible start of the marker at the end stays */
    for (i = n - 5 < 0 ? 0 : n - 5; i < n; i++)
        if (b[i] == 0x1b || b[i] == 0x9b) {
            *at = i;
            *len = 0;
            return 0;
        }
    *at = n;
    *len = 0;
    return 0;
}

int keys_next(cl_keys *k, cl_key *key, int idle)
{
    for (;;) {
        unsigned char c;
        int l;
        if (k->paste) {
            int at, len;
            int done = paste_end(k->b, k->n, &at, &len);
            jw_raw(&k->pb, (const char *)k->b, at);
            eat(k, at + len);
            if (!done) {
                if (!idle || !k->n)
                    return 0;
                /* the marker never came: what is held is text */
                jw_raw(&k->pb, (const char *)k->b, k->n);
                k->n = 0;
            }
            k->paste = 0;
            jw_reset(&k->out);
            jw_raw(&k->out, k->pb.p ? k->pb.p : "", k->pb.n);
            jw_reset(&k->pb);
            set(key, K_PASTE, 0, 0);
            key->text = k->out.p ? k->out.p : "";
            key->n = k->out.n;
            return 1;
        }
        if (!k->n)
            return 0;
        c = k->b[0];
        if (c == 0x1b || c == 0x9b) {
            int p = c == 0x9b ? 1 : 2, ok;
            if (c == 0x1b) {
                if (k->n == 1) {
                    if (!idle)
                        return 0;
                    eat(k, 1);
                    return set(key, K_ESC, 0, 0);
                }
                if (k->b[1] == 'O') {
                    if (k->n < 3) {
                        if (!idle)
                            return 0;
                        eat(k, 2);
                        return set(key, K_ALT, 'O', KM_ALT);
                    }
                    c = k->b[2];
                    eat(k, 3);
                    switch (c) {
                    case 'A':
                        return set(key, K_UP, 0, 0);
                    case 'B':
                        return set(key, K_DOWN, 0, 0);
                    case 'C':
                        return set(key, K_RIGHT, 0, 0);
                    case 'D':
                        return set(key, K_LEFT, 0, 0);
                    case 'H':
                        return set(key, K_HOME, 0, 0);
                    case 'F':
                        return set(key, K_END, 0, 0);
                    case 'M':
                        return set(key, K_ENTER, 0, 0);
                    default:
                        continue;
                    }
                }
                if (k->b[1] != '[') {
                    unsigned long cp;
                    if (k->b[1] == 0x1b) {
                        eat(k, 1);
                        return set(key, K_ESC, 0, 0);
                    }
                    if (k->b[1] == '\r' || k->b[1] == '\n') {
                        eat(k, 2);
                        return set(key, K_NEWLINE, 0, KM_ALT);
                    }
                    if (k->b[1] == 0x7f || k->b[1] == 0x08) {
                        eat(k, 2);
                        return set(key, K_ALT, 0x7f, KM_ALT);
                    }
                    l = json_utf8((const char *)k->b + 1, k->n - 1, &cp);
                    if (!l) {
                        l = 1;
                        cp = k->b[1];
                    }
                    eat(k, 1 + l);
                    if (cp < 0x20)
                        continue;
                    return set(key, K_ALT, cp, KM_ALT);
                }
            }
            l = csi_len(k->b, k->n, p);
            if (l == 0) {
                if (!idle)
                    return 0;
                k->n = 0;           /* a sequence cut off: dropped */
                return 0;
            }
            if (l < 0) {
                eat(k, p);
                continue;
            }
            if (l == 4 && !memcmp(k->b + p, "200~", 4)) {
                eat(k, p + l);
                k->paste = 1;
                jw_reset(&k->pb);
                continue;
            }
            ok = csi_key(key, k->b + p, l);
            eat(k, p + l);
            if (ok)
                return 1;
            continue;
        }
        if (c < 0x20 || c == 0x7f) {
            eat(k, 1);
            switch (c) {
            case '\r':
                return set(key, K_ENTER, 0, 0);
            case '\n':
                return set(key, K_NEWLINE, 0, 0);
            case '\t':
                return set(key, K_TAB, 0, 0);
            case 0x7f:
            case 0x08:
                return set(key, K_BS, 0, 0);
            case 0:
                continue;
            case 0x1f:
                return set(key, K_CTRL, '_', KM_CTRL);
            default:
                if (c <= 26)
                    return set(key, K_CTRL, (unsigned long)('a' + c - 1), KM_CTRL);
                continue;
            }
        }
        {
            unsigned long cp;
            l = json_utf8((const char *)k->b, k->n, &cp);
            if (!l) {
                /* a UTF-8 lead whose rest has not come yet */
                int need = c >= 0xf0 ? 4 : c >= 0xe0 ? 3 : c >= 0xc2 ? 2 : 1;
                if (need > k->n && !idle) {
                    int i, cont = 1;
                    for (i = 1; i < k->n; i++)
                        if ((k->b[i] & 0xc0) != 0x80)
                            cont = 0;
                    if (cont)
                        return 0;
                }
                l = 1;
                cp = c;             /* Latin-1 */
            }
            eat(k, l);
            return set(key, K_CHAR, cp, 0);
        }
    }
}

/* ---- keybindings (A4 gaps 3) ---- */

static const char *const ctx_names[KC_COUNT] = { "Global", "Chat", "Autocomplete", "Confirmation", "Transcript",
                                                 "HistorySearch", "Task", "Help", "Select", "ThemePicker",
                                                 "MessageSelector" };

/* Claude Code's other contexts: valid in the file, nothing of them here */
static const char *const other_ctx[] = { "Settings", "Tabs", "Attachments", "Footer", "DiffDialog", "DiffPanel",
                                         "ModelPicker", "EffortSlider", "Plugin", "Pane", "PaneField", "Agents",
                                         "Scroll", 0 };

typedef struct kb_name {
    const char *name;
    int act;
} kb_name;

static const kb_name act_names[] = {
    { "app:interrupt", KA_INTERRUPT }, { "app:exit", KA_EXIT }, { "app:redraw", KA_REDRAW },
    { "app:toggleTodos", KA_TODOS }, { "app:toggleTranscript", KA_TRANSCRIPT },
    { "history:search", KA_HIST_SEARCH }, { "history:previous", KA_HIST_PREV }, { "history:next", KA_HIST_NEXT },
    { "chat:cancel", KA_CANCEL }, { "chat:clearInput", KA_CLEAR_INPUT }, { "chat:clearScreen", KA_CLEAR_SCREEN },
    { "chat:killAgents", KA_KILL_AGENTS }, { "chat:cycleMode", KA_CYCLE_MODE },
    { "chat:modelPicker", KA_MODEL_PICKER }, { "chat:fastMode", KA_FAST_MODE },
    { "chat:thinkingToggle", KA_THINKING }, { "chat:submit", KA_SUBMIT }, { "chat:queueSubmit", KA_QUEUE_SUBMIT },
    { "chat:sendNow", KA_SEND_NOW }, { "chat:newline", KA_NEWLINE }, { "chat:undo", KA_UNDO },
    { "chat:externalEditor", KA_EXT_EDITOR }, { "chat:stash", KA_STASH }, { "chat:imagePaste", KA_IMAGE_PASTE },
    { "autocomplete:accept", KA_AC_ACCEPT }, { "autocomplete:dismiss", KA_AC_DISMISS },
    { "autocomplete:previous", KA_AC_PREV }, { "autocomplete:next", KA_AC_NEXT },
    { "confirm:yes", KA_YES }, { "confirm:no", KA_NO }, { "confirm:previous", KA_PREV }, { "confirm:next", KA_NEXT },
    { "confirm:nextField", KA_NEXT_FIELD }, { "confirm:previousField", KA_PREV_FIELD },
    { "confirm:toggle", KA_TOGGLE }, { "confirm:cycleMode", KA_CONFIRM_CYCLE },
    { "permission:toggleDebug", KA_PERM_DEBUG },
    { "transcript:toggleShowAll", KA_TR_SHOW_ALL }, { "transcript:exit", KA_TR_EXIT },
    { "historySearch:next", KA_HS_NEXT }, { "historySearch:accept", KA_HS_ACCEPT },
    { "historySearch:cancel", KA_HS_CANCEL }, { "historySearch:execute", KA_HS_EXECUTE },
    { "historySearch:cycleScope", KA_HS_SCOPE },
    { "task:background", KA_TASK_BG }, { "help:dismiss", KA_HELP_DISMISS },
    { "select:next", KA_SEL_NEXT }, { "select:previous", KA_SEL_PREV }, { "select:pageUp", KA_SEL_PGUP },
    { "select:pageDown", KA_SEL_PGDN }, { "select:first", KA_SEL_FIRST }, { "select:last", KA_SEL_LAST },
    { "select:accept", KA_SEL_ACCEPT }, { "select:cancel", KA_SEL_CANCEL },
    { "theme:toggleSyntaxHighlighting", KA_THEME_SYNTAX },
    /* the rewind list's names before v2.1.283: the Select actions they were */
    { "messageSelector:up", KA_SEL_PREV }, { "messageSelector:down", KA_SEL_NEXT },
    { "messageSelector:top", KA_SEL_FIRST }, { "messageSelector:bottom", KA_SEL_LAST },
    { "messageSelector:select", KA_SEL_ACCEPT },
    /* Claude Code's actions for what C:Claude does not have (tabs, the
     * attachments, the footer, the diff viewer and panel, fullscreen's
     * scrolling, the model picker's effort, plugins, agent view, voice) */
    { "tabs:next", KA_INERT }, { "tabs:previous", KA_INERT }, { "attachments:next", KA_INERT },
    { "attachments:previous", KA_INERT }, { "attachments:remove", KA_INERT }, { "attachments:exit", KA_INERT },
    { "footer:next", KA_INERT }, { "footer:previous", KA_INERT }, { "footer:up", KA_INERT },
    { "footer:down", KA_INERT }, { "footer:openSelected", KA_INERT }, { "footer:clearSelection", KA_INERT },
    { "footer:dismiss", KA_INERT }, { "diff:dismiss", KA_INERT }, { "diff:previousSource", KA_INERT },
    { "diff:nextSource", KA_INERT }, { "diff:previousFile", KA_INERT }, { "diff:nextFile", KA_INERT },
    { "diff:back", KA_INERT }, { "diff:viewDetails", KA_INERT }, { "app:toggleReplTab", KA_INERT },
    { "app:cycleDiffBase", KA_INERT }, { "app:diffFileListUp", KA_INERT }, { "app:diffFileListDown", KA_INERT },
    { "app:toggleDiffNoiseFilter", KA_INERT }, { "app:toggleDiffPreSession", KA_INERT },
    { "scroll:lineUp", KA_INERT }, { "scroll:lineDown", KA_INERT }, { "scroll:pageUp", KA_INERT },
    { "scroll:pageDown", KA_INERT }, { "scroll:top", KA_INERT }, { "scroll:bottom", KA_INERT },
    { "scroll:halfPageUp", KA_INERT }, { "scroll:halfPageDown", KA_INERT }, { "scroll:fullPageUp", KA_INERT },
    { "scroll:fullPageDown", KA_INERT }, { "selection:copy", KA_INERT }, { "selection:clear", KA_INERT },
    { "selection:extendLeft", KA_INERT }, { "selection:extendRight", KA_INERT },
    { "selection:extendUp", KA_INERT }, { "selection:extendDown", KA_INERT },
    { "selection:extendLineStart", KA_INERT }, { "selection:extendLineEnd", KA_INERT },
    { "modelPicker:decreaseEffort", KA_INERT }, { "modelPicker:increaseEffort", KA_INERT },
    { "modelPicker:thisSessionOnly", KA_INERT }, { "effortSlider:decreaseEffort", KA_INERT },
    { "effortSlider:increaseEffort", KA_INERT }, { "effortSlider:toggleUltracode", KA_INERT },
    { "effortSlider:thisSessionOnly", KA_INERT }, { "plugin:toggle", KA_INERT }, { "plugin:install", KA_INERT },
    { "plugin:favorite", KA_INERT }, { "settings:search", KA_INERT }, { "settings:retry", KA_INERT },
    { "agents:switchView", KA_INERT }, { "agents:togglePin", KA_INERT }, { "agents:find", KA_INERT },
    { "agents:rename", KA_INERT }, { "agents:previousGroup", KA_INERT }, { "agents:nextGroup", KA_INERT },
    { "voice:pushToTalk", KA_INERT },
    { 0, 0 }
};

/* Claude Code's default bindings (keybindings.md, 2026-10-05), as the file
 * would write them: context, keystrokes, action */
static const char *const defaults[][3] = {
    { "Global", "ctrl+c", "app:interrupt" },
    { "Global", "ctrl+d", "app:exit" },
    { "Global", "ctrl+t", "app:toggleTodos" },
    { "Global", "ctrl+o", "app:toggleTranscript" },
    { "Chat", "ctrl+r", "history:search" },
    { "Chat", "up", "history:previous" },
    { "Chat", "down", "history:next" },
    { "Chat", "escape", "chat:cancel" },
    { "Chat", "ctrl+l", "chat:clearInput" },
    { "Chat", "ctrl+x ctrl+k", "chat:killAgents" },
    { "Chat", "shift+tab", "chat:cycleMode" },
    { "Chat", "meta+p", "chat:modelPicker" },
    { "Chat", "meta+o", "chat:fastMode" },
    { "Chat", "meta+t", "chat:thinkingToggle" },
    { "Chat", "enter", "chat:submit" },
    { "Chat", "ctrl+x enter", "chat:queueSubmit" },
    { "Chat", "ctrl+enter", "chat:sendNow" },
    { "Chat", "ctrl+x ctrl+s", "chat:sendNow" },
    { "Chat", "ctrl+j", "chat:newline" },
    { "Chat", "ctrl+_", "chat:undo" },
    { "Chat", "ctrl+g", "chat:externalEditor" },
    { "Chat", "ctrl+x ctrl+e", "chat:externalEditor" },
    { "Chat", "ctrl+s", "chat:stash" },
    { "Chat", "ctrl+v", "chat:imagePaste" },
    { "Autocomplete", "tab", "autocomplete:accept" },
    { "Autocomplete", "escape", "autocomplete:dismiss" },
    { "Autocomplete", "up", "autocomplete:previous" },
    { "Autocomplete", "down", "autocomplete:next" },
    { "Confirmation", "enter", "confirm:yes" },
    { "Confirmation", "escape", "confirm:no" },
    { "Confirmation", "up", "confirm:previous" },
    { "Confirmation", "down", "confirm:next" },
    { "Confirmation", "tab", "confirm:nextField" },
    { "Confirmation", "space", "confirm:toggle" },
    { "Confirmation", "shift+tab", "confirm:cycleMode" },
    { "Transcript", "ctrl+e", "transcript:toggleShowAll" },
    { "Transcript", "q", "transcript:exit" },
    { "Transcript", "ctrl+c", "transcript:exit" },
    { "Transcript", "escape", "transcript:exit" },
    { "HistorySearch", "ctrl+r", "historySearch:next" },
    { "HistorySearch", "escape", "historySearch:accept" },
    { "HistorySearch", "tab", "historySearch:accept" },
    { "HistorySearch", "ctrl+c", "historySearch:cancel" },
    { "HistorySearch", "enter", "historySearch:execute" },
    { "HistorySearch", "ctrl+s", "historySearch:cycleScope" },
    { "Task", "ctrl+b", "task:background" },
    { "Task", "ctrl+x ctrl+b", "task:background" },
    { "ThemePicker", "ctrl+t", "theme:toggleSyntaxHighlighting" },
    { "Help", "escape", "help:dismiss" },
    { "Select", "down", "select:next" },
    { "Select", "j", "select:next" },
    { "Select", "ctrl+n", "select:next" },
    { "Select", "up", "select:previous" },
    { "Select", "k", "select:previous" },
    { "Select", "ctrl+p", "select:previous" },
    { "Select", "pageup", "select:pageUp" },
    { "Select", "pagedown", "select:pageDown" },
    { "Select", "home", "select:first" },
    { "Select", "end", "select:last" },
    { "Select", "enter", "select:accept" },
    { "Select", "escape", "select:cancel" }
};
#define NDEFAULTS ((int)(sizeof(defaults) / sizeof(defaults[0])))

static void km_warn(cl_keymap *m, const char *a, const char *b, const char *c)
{
    jw_rawz(&m->warn, a);
    if (b)
        jw_rawz(&m->warn, b);
    if (c)
        jw_rawz(&m->warn, c);
    jw_raw(&m->warn, "\n", 1);
    m->nwarn++;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

/* a word of a keystroke equal (any case) to name */
static int word_is(const char *s, long n, const char *name)
{
    long i;
    for (i = 0; i < n && name[i]; i++)
        if (lower((unsigned char)s[i]) != name[i])
            return 0;
    return i == n && !name[i];
}

static const struct {
    const char *name;
    int k;
} key_words[] = { { "escape", K_ESC }, { "esc", K_ESC }, { "enter", K_ENTER }, { "return", K_ENTER },
                  { "tab", K_TAB }, { "up", K_UP }, { "down", K_DOWN }, { "left", K_LEFT }, { "right", K_RIGHT },
                  { "pageup", K_PGUP }, { "pagedown", K_PGDN }, { "home", K_HOME }, { "end", K_END },
                  { "backspace", K_BS }, { "delete", K_DEL }, { "insert", K_INS }, { 0, 0 } };

/* "ctrl+shift+k": 0 with *st set, -1 no key (a warning says why) */
static int parse_stroke(cl_keymap *m, const char *s, long n, kb_stroke *st, const char *whole)
{
    long a = 0, i;
    memset(st, 0, sizeof(*st));
    for (i = 0; i <= n; i++) {
        long l;
        if (i < n && !(s[i] == '+' && i > a))
            continue;
        l = i - a;
        if (i < n) {
            /* a modifier */
            if (word_is(s + a, l, "ctrl") || word_is(s + a, l, "control"))
                st->mods |= KM_CTRL;
            else if (word_is(s + a, l, "shift"))
                st->mods |= KM_SHIFT;
            else if (word_is(s + a, l, "alt") || word_is(s + a, l, "opt") || word_is(s + a, l, "option") ||
                     word_is(s + a, l, "meta"))
                st->mods |= KM_ALT;
            else if (word_is(s + a, l, "cmd") || word_is(s + a, l, "command") || word_is(s + a, l, "super") ||
                     word_is(s + a, l, "win"))
                st->k = -1;         /* the Super key: no terminal here reports it */
            else
                km_warn(m, "keybindings: an unknown modifier dropped in ", whole, 0);
            a = i + 1;
            continue;
        }
        /* the key */
        if (l <= 0) {
            km_warn(m, "keybindings: no key in ", whole, 0);
            return -1;
        }
        if (st->k == -1)
            return 0;
        if (word_is(s + a, l, "space")) {
            st->k = K_CHAR;
            st->ch = ' ';
        } else if (word_is(s + a, l, "wheelup") || word_is(s + a, l, "wheeldown")) {
            st->k = -1;             /* the mouse wheel: fullscreen rendering only */
        } else if (l == 1) {
            st->k = K_CHAR;
            st->ch = (unsigned long)lower((unsigned char)s[a]);
        } else {
            int w;
            for (w = 0; key_words[w].name && !word_is(s + a, l, key_words[w].name); w++)
                ;
            if (!key_words[w].name) {
                km_warn(m, "keybindings: an unknown key in ", whole, 0);
                return -1;
            }
            st->k = key_words[w].k;
        }
        /* Ctrl+_ and Ctrl+Shift+- are one key (undo); Shift+a printable
         * that is no letter is the character itself */
        if (st->k == K_CHAR && (st->mods & KM_CTRL) && (st->ch == '-' || st->ch == '_')) {
            st->ch = '_';
            st->mods = KM_CTRL;
        }
        if (st->k == K_CHAR && !(st->ch >= 'a' && st->ch <= 'z') && !(st->mods & KM_CTRL))
            st->mods &= ~KM_SHIFT;
        return 0;
    }
    return -1;
}

/* a key as a keystroke (k -2: never bound: a paste, a report) */
static void key_stroke(const cl_key *k, kb_stroke *s)
{
    memset(s, 0, sizeof(*s));
    s->k = -2;
    switch (k->k) {
    case K_CHAR:
    case K_ALT:
    case K_CTRL:
        if (k->k == K_ALT && k->ch == 0x7f) {
            s->k = K_BS;
            s->mods = KM_ALT;
            return;
        }
        s->k = K_CHAR;
        s->ch = k->ch;
        s->mods = k->k == K_CTRL ? KM_CTRL | (k->mods & KM_ALT) : k->k == K_ALT ? KM_ALT | (k->mods & KM_CTRL) : 0;
        if (s->ch >= 'A' && s->ch <= 'Z') {
            s->ch += 32;
            s->mods |= KM_SHIFT;
        } else if (k->k == K_CTRL && (k->mods & KM_SHIFT) && s->ch != '_')
            s->mods |= KM_SHIFT;
        return;
    case K_NEWLINE:
        if (!(k->mods & (KM_SHIFT | KM_ALT))) {
            s->k = K_CHAR;          /* Ctrl+J */
            s->ch = 'j';
            s->mods = KM_CTRL;
        } else {
            s->k = K_ENTER;
            s->mods = k->mods & (KM_SHIFT | KM_ALT);
        }
        return;
    case K_BTAB:
        s->k = K_TAB;
        s->mods = KM_SHIFT;
        return;
    case K_ENTER:
    case K_TAB:
    case K_ESC:
    case K_UP:
    case K_DOWN:
    case K_LEFT:
    case K_RIGHT:
    case K_HOME:
    case K_END:
    case K_PGUP:
    case K_PGDN:
    case K_INS:
    case K_BS:
    case K_DEL:
        s->k = k->k;
        s->mods = k->k == K_ESC ? 0 : k->mods;
        return;
    default:
        return;
    }
}

static int same(const kb_stroke *a, const kb_stroke *b)
{
    return a->k == b->k && a->k >= 0 && a->ch == b->ch && a->mods == b->mods;
}

/* the keys no binding may take (Claude Code's reserved shortcuts) */
static int reserved(const kb_stroke *s)
{
    return s->k == K_CHAR && s->mods == KM_CTRL &&
           (s->ch == 'c' || s->ch == 'd' || s->ch == 'm' || s->ch == '[' || s->ch == 'i' || s->ch == 'h');
}

static int add(cl_keymap *m, int ctx, int act, const kb_stroke *s, int n)
{
    if (m->n == m->cap) {
        int cap = m->cap ? m->cap * 2 : 96;
        kb_bind *b = (kb_bind *)realloc(m->b, (size_t)cap * sizeof(kb_bind));
        if (!b)
            return -1;
        m->b = b;
        m->cap = cap;
    }
    m->b[m->n].ctx = ctx;
    m->b[m->n].act = act;
    m->b[m->n].n = n;
    m->b[m->n].s[0] = s[0];
    m->b[m->n].s[1] = n > 1 ? s[1] : s[0];
    m->n++;
    return 0;
}

static int act_of(const char *name)
{
    int i;
    for (i = 0; act_names[i].name; i++)
        if (!strcmp(act_names[i].name, name))
            return act_names[i].act;
    return -1;
}

/* one "keys": action of a context's block; user: the file's (checked) */
static int bind(cl_keymap *m, int ctx, const char *keys, const char *action, int user)
{
    kb_stroke s[2];
    long n = (long)strlen(keys), sp;
    int ns = 1, act;
    const char *p = keys;
    while (*p == ' ')
        p++;
    n = (long)strlen(p);
    while (n && p[n - 1] == ' ')
        n--;
    for (sp = 0; sp < n && p[sp] != ' '; sp++)
        ;
    if (parse_stroke(m, p, sp, &s[0], keys))
        return 0;
    if (sp < n) {
        long b = sp;
        while (b < n && p[b] == ' ')
            b++;
        if (memchr(p + b, ' ', (size_t)(n - b))) {
            km_warn(m, "keybindings: a chord has two keystrokes at most: ", keys, 0);
            return 0;
        }
        if (parse_stroke(m, p + b, n - b, &s[1], keys))
            return 0;
        ns = 2;
    }
    if (!action)
        act = -1;
    else if ((act = act_of(action)) < 0) {
        km_warn(m, "keybindings: an unknown action \"", action, "\" (the default stays)");
        return 0;
    }
    if (user && ns == 1 && reserved(&s[0])) {
        /* the reserved keys keep what they do; the file may list their
         * defaults (the file /keybindings writes does) */
        int i;
        for (i = 0; i < m->ndef; i++)
            if (m->b[i].ctx == ctx && m->b[i].n == 1 && m->b[i].act == act && same(&m->b[i].s[0], &s[0]))
                return 0;
        km_warn(m, "keybindings: a reserved shortcut cannot be rebound: ", keys, 0);
        return 0;
    }
    if (ctx < 0)
        return 0;                   /* a context with nothing of it here */
    return add(m, ctx, act, s, ns);
}

int km_init(cl_keymap *m)
{
    int i;
    memset(m, 0, sizeof(*m));
    jw_init(&m->warn);
    for (i = 0; i < NDEFAULTS; i++) {
        int c;
        for (c = 0; c < KC_COUNT && strcmp(ctx_names[c], defaults[i][0]); c++)
            ;
        if (bind(m, c, defaults[i][1], defaults[i][2], 0))
            return -1;
    }
    m->ndef = m->n;
    return 0;
}

void km_free(cl_keymap *m)
{
    free(m->b);
    jw_free(&m->warn);
    memset(m, 0, sizeof(*m));
}

void km_reset(cl_keymap *m)
{
    m->n = m->ndef;
    m->pending = 0;
    jw_reset(&m->warn);
    m->nwarn = 0;
}

int km_load(cl_keymap *m, const char *json, long n)
{
    jv o, arr, blk, x, k, v;
    jit it, bi;
    km_reset(m);
    if (json_parse(json, n, &o) || json_type(o) != J_OBJ) {
        km_warn(m, "keybindings: the file is no JSON object; the defaults apply", 0, 0);
        return -1;
    }
    if (!json_get(o, "bindings", &arr) || json_type(arr) != J_ARR) {
        km_warn(m, "keybindings: no \"bindings\" array", 0, 0);
        return 0;
    }
    json_iter(arr, &it);
    while (json_next(&it, 0, &blk)) {
        char cname[32];
        int c, i, first = m->n;
        if (json_type(blk) != J_OBJ || !json_get(blk, "context", &x) || json_type(x) != J_STR ||
            !json_get(blk, "bindings", &v) || json_type(v) != J_OBJ) {
            km_warn(m, "keybindings: a block without a \"context\" and a \"bindings\" object", 0, 0);
            continue;
        }
        json_str(x, cname, sizeof(cname));
        for (c = 0; c < KC_COUNT && strcmp(ctx_names[c], cname); c++)
            ;
        if (c == KC_COUNT) {
            for (i = 0; other_ctx[i] && strcmp(other_ctx[i], cname); i++)
                ;
            if (!other_ctx[i]) {
                km_warn(m, "keybindings: an unknown context \"", cname, "\"");
                continue;
            }
            c = -1;
        }
        json_iter(v, &bi);
        while (json_next(&bi, &k, &x)) {
            char keys[64], action[64];
            json_str(k, keys, sizeof(keys));
            if (json_type(x) != J_STR && json_type(x) != J_NULL) {
                km_warn(m, "keybindings: an action must be a string or null: ", keys, 0);
                continue;
            }
            if (json_type(x) == J_STR)
                json_str(x, action, sizeof(action));
            if (bind(m, c, keys, json_type(x) == J_STR ? action : 0, 1))
                return -1;
            /* the same keystroke twice in one block */
            for (i = first; c >= 0 && i < m->n - 1; i++)
                if (m->b[m->n - 1].n == m->b[i].n && same(&m->b[i].s[0], &m->b[m->n - 1].s[0]) &&
                    (m->b[i].n == 1 || same(&m->b[i].s[1], &m->b[m->n - 1].s[1]))) {
                    km_warn(m, "keybindings: bound twice in ", cname, ": the later one counts");
                    break;
                }
        }
    }
    return 0;
}

/* the newest binding of context c for these keystrokes: its index, -1 none */
static int find(const cl_keymap *m, int c, const kb_stroke *s0, const kb_stroke *s1)
{
    int i;
    for (i = m->n - 1; i >= 0; i--) {
        const kb_bind *b = &m->b[i];
        if (b->ctx != c || b->n != (s1 ? 2 : 1) || !same(&b->s[0], s0) || (s1 && !same(&b->s[1], s1)))
            continue;
        return i;
    }
    return -1;
}

int km_action(cl_keymap *m, const int *ctx, int nctx, const cl_key *k, unsigned long now_ms)
{
    kb_stroke s;
    int c, i;
    key_stroke(k, &s);
    if (s.k < 0)
        return KA_NONE;
    if (m->pending) {
        m->pending = 0;
        if (now_ms - m->first_ms <= 3000UL) {
            for (c = 0; c < nctx; c++) {
                i = find(m, ctx[c], &m->first, &s);
                if (i >= 0)
                    return m->b[i].act >= 0 ? m->b[i].act : KA_CHORD_MISS;
            }
            return KA_CHORD_MISS;
        }
        m->expired = 1;             /* too slow: this key is a new one */
    }
    /* the first keystroke of a chord still bound in an active context */
    for (c = 0; c < nctx; c++)
        for (i = 0; i < m->n; i++) {
            const kb_bind *b = &m->b[i];
            int j;
            if (b->ctx != ctx[c] || b->n != 2 || !same(&b->s[0], &s))
                continue;
            j = find(m, ctx[c], &b->s[0], &b->s[1]);
            if (j >= 0 && m->b[j].act >= 0) {
                m->pending = 1;
                m->first = s;
                m->first_ms = now_ms;
                return KA_PENDING;
            }
        }
    for (c = 0; c < nctx; c++) {
        i = find(m, ctx[c], &s, 0);
        if (i >= 0 && m->b[i].act >= 0)
            return m->b[i].act;
        /* unbound (null) here: the next context may have it */
    }
    return KA_NONE;
}

void km_defaults_json(jw *out)
{
    int c, i, any;
    jw_rawz(out, "{\n  \"$schema\": \"https://www.schemastore.org/claude-code-keybindings.json\",\n"
                 "  \"$docs\": \"https://code.claude.com/docs/en/keybindings\",\n  \"bindings\": [");
    any = 0;
    for (c = 0; c < KC_COUNT; c++) {
        int n = 0;
        for (i = 0; i < NDEFAULTS; i++) {
            if (strcmp(defaults[i][0], ctx_names[c]))
                continue;
            if (!n++) {
                jw_rawz(out, any++ ? ",\n    {\n      \"context\": " : "\n    {\n      \"context\": ");
                jw_strz(out, ctx_names[c]);
                jw_rawz(out, ",\n      \"bindings\": {");
            }
            jw_rawz(out, n > 1 ? ",\n        " : "\n        ");
            jw_strz(out, defaults[i][1]);
            jw_rawz(out, ": ");
            jw_strz(out, defaults[i][2]);
        }
        if (n)
            jw_rawz(out, "\n      }\n    }");
    }
    jw_rawz(out, "\n  ]\n}\n");
}
