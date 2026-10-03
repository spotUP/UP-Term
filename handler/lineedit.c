/* The cooked line editor; see lineedit.h.
 *
 * The line is drawn by writing to the engine, like any output: it knows
 * where the line started (an absolute row, so it survives the screen
 * scrolling under it), and after an edit it rewrites the line from the
 * first changed character, draws the grey tail (a history suggestion, or
 * the search pattern), blanks what got shorter, and puts the cursor back
 * with CUP. A line longer than the window wraps like any text. */
#include "lineedit.h"
#include <string.h>

static void out(le_line *le, const void *b, long n)
{
    if (n > 0)
        le->out(le->user, (const unsigned char *)b, n);
}

static int is_cont(const le_line *le, unsigned char b)
{
    return le->utf8 && (b & 0xC0) == 0x80;
}

static int next_char(const le_line *le, int p)
{
    if (p >= le->len)
        return le->len;
    p++;
    while (p < le->len && is_cont(le, le->buf[p]))
        p++;
    return p;
}

static int prev_char(const le_line *le, int p)
{
    if (p <= 0)
        return 0;
    p--;
    while (p > 0 && is_cont(le, le->buf[p]))
        p--;
    return p;
}

/* characters in b[0..n) */
static int count_cells(const le_line *le, const unsigned char *b, int n)
{
    int i, c = 0;
    for (i = 0; i < n; i++)
        if (!is_cont(le, b[i]))
            c++;
    return c;
}

static int cells(const le_line *le, int p)
{
    return count_cells(le, le->buf, p);
}

static int is_word(unsigned char c)
{
    return c != ' ' && c != '/' && c != ':' && c != '"' && c != '=';
}

static int word_back(const le_line *le, int p)
{
    while (p > 0 && !is_word(le->buf[p - 1]))
        p--;
    while (p > 0 && is_word(le->buf[p - 1]))
        p--;
    return p;
}

static int word_forward(const le_line *le, int p)
{
    while (p < le->len && !is_word(le->buf[p]))
        p++;
    while (p < le->len && is_word(le->buf[p]))
        p++;
    return p;
}

static void start(le_line *le)
{
    int x, y;
    if (le->started)
        return;
    vt_cursor(le->t, &x, &y);
    le->start_row = y + vt_lines_scrolled(le->t);
    le->start_col = x;
    le->started = 1;
    le->shown = 0;
}

static void num(char *b, int *n, long v)
{
    char d[12];
    int k = 0;
    do {
        d[k++] = (char)('0' + v % 10);
        v /= 10;
    } while (v);
    while (k)
        b[(*n)++] = d[--k];
}

static void cup(le_line *le, long row, int col)
{
    char b[24];
    int n = 0;
    b[n++] = 0x1B;
    b[n++] = '[';
    num(b, &n, row + 1);
    b[n++] = ';';
    num(b, &n, col + 1);
    b[n++] = 'H';
    out(le, b, n);
}

/* Put the cursor before byte p of the line. */
static void go(le_line *le, int p)
{
    int cols = vt_cols(le->t), rows = vt_rows(le->t);
    long cell = (long)le->start_col + cells(le, p);
    long row = le->start_row + cell / cols - vt_lines_scrolled(le->t);
    int col = (int)(cell % cols);
    if (p > 0 && col == 0) {
        /* Right after a row's last character: rewrite that character, so
         * the engine stands where typing it would have left it (the
         * deferred wrap), even on the bottom row. */
        int q = prev_char(le, p);
        go(le, q);
        out(le, le->buf + q, p - q);
        return;
    }
    if (row < 0)
        row = 0;
    if (row > rows - 1)
        row = rows - 1;
    cup(le, row, col);
}

/* ---- history ------------------------------------------------------------ */

void le_hist_add(le_line *le, const unsigned char *s, int n)
{
    int i;
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        n--;
    if (n <= 0)
        return;
    if (n > LE_HIST_LEN - 1)
        n = LE_HIST_LEN - 1;
    if (le->hist_n && !memcmp(le->hist[le->hist_n - 1], s, n) && le->hist[le->hist_n - 1][n] == 0)
        return; /* the same line again */
    if (le->hist_n == LE_HIST) {
        for (i = 1; i < LE_HIST; i++)
            memcpy(le->hist[i - 1], le->hist[i], LE_HIST_LEN);
        le->hist_n--;
    }
    memcpy(le->hist[le->hist_n], s, n);
    le->hist[le->hist_n][n] = 0;
    le->hist_n++;
    le->hist_pos = le->hist_n;
}

/* The newest history line that starts with the whole line and is longer:
 * its tail is the grey suggestion. */
static const unsigned char *suggestion(const le_line *le)
{
    int i;
    if (!le->suggest || le->searching || !le->len || le->pos != le->len)
        return 0;
    for (i = le->hist_n - 1; i >= 0; i--) {
        int n = (int)strlen((const char *)le->hist[i]);
        if (n > le->len && !memcmp(le->hist[i], le->buf, le->len))
            return le->hist[i] + le->len;
    }
    return 0;
}

static int lower(int c)
{
    return (c >= 'A' && c <= 'Z') ? c + 32 : c;
}

/* offset of pat in s (case-insensitive), -1 if absent */
static int find(const unsigned char *s, const unsigned char *pat, int pn)
{
    int i, k, n = (int)strlen((const char *)s);
    for (i = 0; i + pn <= n; i++) {
        for (k = 0; k < pn && lower(s[i + k]) == lower(pat[k]); k++)
            ;
        if (k == pn)
            return i;
    }
    return pn ? -1 : 0;
}

/* ---- the command word ---------------------------------------------------- */

static int first_word_len(const le_line *le)
{
    int i = 0;
    while (i < le->len && le->buf[i] == ' ')
        i++;
    while (i < le->len && le->buf[i] != ' ')
        i++;
    return i;
}

int le_first_word(const le_line *le, unsigned char *out, int max)
{
    int a = 0, b = first_word_len(le), n;
    while (a < b && le->buf[a] == ' ')
        a++;
    n = b - a;
    if (n > max - 1)
        n = max - 1;
    memcpy(out, le->buf + a, n);
    out[n] = 0;
    return n;
}

/* drop the colouring when the first word changed under it */
static void check_command_word(le_line *le)
{
    unsigned char w[64];
    le_first_word(le, w, sizeof(w));
    if (le->cmd_state && strcmp((const char *)w, (const char *)le->cmd_word))
        le->cmd_state = 0;
}

/* ---- drawing ------------------------------------------------------------ */

/* Show the line from byte p on, then its grey tail, blank the cells it no
 * longer covers, and leave the cursor at le->pos. */
static void redraw_from(le_line *le, int p)
{
    const unsigned char *sug;
    int now, i, fw;
    check_command_word(le);
    sug = suggestion(le);
    now = cells(le, le->len);
    fw = first_word_len(le);
    if (le->cmd_state && p < fw)
        p = 0; /* the command word is drawn whole, in its colour */
    go(le, p);
    if (le->cmd_state && p < fw) {
        out(le, le->cmd_state == 1 ? "\033[32m" : "\033[31m", 5); /* green / red */
        out(le, le->buf, fw);
        out(le, "\033[39m", 5);
        out(le, le->buf + fw, le->len - fw);
    } else {
        out(le, le->buf + p, le->len - p);
    }
    if (sug || le->searching) {
        out(le, "\033[2m", 4); /* faint: grey */
        if (sug) {
            int n = (int)strlen((const char *)sug);
            out(le, sug, n);
            now += count_cells(le, sug, n);
        } else {
            out(le, "  (search: ", 11);
            out(le, le->pat, le->pat_len);
            out(le, ")", 1);
            now += 12 + count_cells(le, le->pat, le->pat_len);
        }
        out(le, "\033[22m", 5);
    }
    for (i = now; i < le->shown; i++)
        out(le, " ", 1);
    le->shown = now;
    go(le, le->pos);
}

static void save_state(le_state *s, const le_line *le)
{
    memcpy(s->buf, le->buf, le->len);
    s->len = le->len;
    s->pos = le->pos;
}

static void load_state(le_line *le, const le_state *s)
{
    memcpy(le->buf, s->buf, s->len);
    le->len = s->len;
    le->pos = s->pos;
    redraw_from(le, 0);
}

static void push_undo(le_line *le)
{
    if (le->undo_n == LE_UNDO) {
        memmove(&le->undo[0], &le->undo[1], (LE_UNDO - 1) * sizeof(le_state));
        le->undo_n--;
    }
    save_state(&le->undo[le->undo_n++], le);
}

static void erase(le_line *le, int a, int b)
{
    if (b <= a)
        return;
    push_undo(le);
    le->typing = 0;
    memmove(le->buf + a, le->buf + b, le->len - b);
    le->len -= b - a;
    if (le->pos > b)
        le->pos -= b - a;
    else if (le->pos > a)
        le->pos = a;
    redraw_from(le, a);
}

static void set_line(le_line *le, const unsigned char *s, int pos)
{
    int n = (int)strlen((const char *)s);
    if (n > LE_MAX - 2)
        n = LE_MAX - 2;
    memcpy(le->buf, s, n);
    le->len = n;
    le->pos = pos < 0 || pos > n ? n : pos;
    redraw_from(le, 0);
}

static void move_to(le_line *le, int p)
{
    int had = suggestion(le) != 0;
    le->pos = p;
    le->typing = 0;
    if (had || suggestion(le))
        redraw_from(le, 0); /* the grey tail shows only at the end */
    else
        go(le, p);
}

static void insert(le_line *le, const unsigned char *b, int n)
{
    const unsigned char *sug;
    if (le->len + n > LE_MAX - 2)
        return;
    if (!le->typing)
        push_undo(le);
    le->typing = 1;
    sug = suggestion(le);
    memmove(le->buf + le->pos + n, le->buf + le->pos, le->len - le->pos);
    memcpy(le->buf + le->pos, b, n);
    le->len += n;
    le->pos += n;
    check_command_word(le);
    if (le->pos == le->len && !sug && !suggestion(le) && le->shown <= cells(le, le->len - n) &&
        (!le->cmd_state || le->pos - n >= first_word_len(le))) {
        /* typing at the end with no grey tail: just echo, the engine
         * wraps and scrolls */
        le->shown = cells(le, le->len);
        out(le, b, n);
        return;
    }
    redraw_from(le, le->pos - n);
}

static void history(le_line *le, int dir, int search)
{
    int i = le->hist_pos;
    int plen = search ? le->pos : 0;
    for (;;) {
        i += dir;
        if (i < 0 || i > le->hist_n)
            return;
        if (i == le->hist_n || !plen || !memcmp(le->hist[i], le->buf, plen))
            break;
    }
    le->hist_pos = i;
    le->typing = 0;
    if (i == le->hist_n) {
        if (!search)
            set_line(le, (const unsigned char *)"", -1);
        return;
    }
    set_line(le, le->hist[i], search ? plen : -1);
}

/* Ctrl-R: the newest entry at or before `from` containing the pattern. */
static void search_from(le_line *le, int from)
{
    int i, at;
    for (i = from; i >= 0; i--) {
        at = find(le->hist[i], le->pat, le->pat_len);
        if (at >= 0) {
            le->search_idx = i;
            set_line(le, le->hist[i], at + le->pat_len);
            return;
        }
    }
    redraw_from(le, 0); /* no match: the line stays, the pattern shows */
}

/* Ctrl-L: clear the window, then prompt and line again at the top. The
 * prompt is what the start row held before the line (the program wrote it;
 * the console only knows it from the screen). */
static int prompt_from_screen(le_line *le, unsigned char *prompt, int max)
{
    int n, i, k = 0;
    const vt_cell *c = vt_row(le->t, (int)(le->start_row - vt_lines_scrolled(le->t)), &n);
    if (c)
        for (i = 0; i < le->start_col && i < n && k < max - 6; i++) {
            unsigned long cp = c[i].ch;
            if (cp < 0x80 || !le->utf8) {
                prompt[k++] = (unsigned char)(cp < 0x100 ? cp : '?');
            } else if (cp < 0x800) {
                prompt[k++] = (unsigned char)(0xC0 | (cp >> 6));
                prompt[k++] = (unsigned char)(0x80 | (cp & 0x3F));
            } else {
                prompt[k++] = (unsigned char)(0xE0 | (cp >> 12));
                prompt[k++] = (unsigned char)(0x80 | ((cp >> 6) & 0x3F));
                prompt[k++] = (unsigned char)(0x80 | (cp & 0x3F));
            }
        }
    return k;
}

static void clear_screen(le_line *le)
{
    unsigned char prompt[256];
    int k = prompt_from_screen(le, prompt, sizeof(prompt));
    out(le, "\033[H\033[2J", 7);
    out(le, prompt, k);
    le->started = 0;
    start(le);
    redraw_from(le, 0);
}

/* ---- keys ------------------------------------------------------------------ */

void le_init(le_line *le, vt_term *t, void (*o)(void *, const unsigned char *, long), void *user)
{
    memset(le, 0, sizeof(*le));
    le->t = t;
    le->out = o;
    le->user = user;
    le->utf8 = 1;
    le->suggest = 1;
}

void le_reset(le_line *le)
{
    le->len = 0;
    le->pos = 0;
    le->started = 0;
    le->shown = 0;
    le->hist_pos = le->hist_n;
    le->searching = 0;
    le->undo_n = 0;
    le->typing = 0;
}

static int search_key(le_line *le, long key, const unsigned char *b, int n)
{
    /* returns 1 when the key ended the search and should run as usual */
    if (n == 1 && b[0] == 0x12) { /* Ctrl-R again: older */
        search_from(le, le->search_idx - 1);
        return 0;
    }
    if (n == 1 && (b[0] == 0x07 || b[0] == 0x1B)) { /* Ctrl-G / Esc: cancel */
        le->searching = 0;
        load_state(le, &le->before_search);
        return 0;
    }
    if (key == VT_KEY_BACKSPACE) {
        if (le->pat_len)
            le->pat_len--;
        search_from(le, le->hist_n - 1);
        return 0;
    }
    if (key < 0x110000 && n >= 1 && b[0] >= 0x20 && le->pat_len + n < (int)sizeof(le->pat)) {
        memcpy(le->pat + le->pat_len, b, n);
        le->pat_len += n;
        search_from(le, le->search_idx);
        return 0;
    }
    /* anything else keeps the found line and does its own work */
    le->searching = 0;
    redraw_from(le, 0);
    return 1;
}

int le_key(le_line *le, long key, int mods, const unsigned char *b, int n)
{
    int shift = (mods & VT_MOD_SHIFT) != 0, ctrl = (mods & VT_MOD_CTRL) != 0;
    int meta = (mods & VT_MOD_ALT) != 0;
    start(le);
    if (le->searching && !search_key(le, key, b, n))
        return 0;
    if (key == VT_KEY_RETURN || key == VT_KEY_KP_ENTER) {
        le->pos = le->len;
        le->searching = 0;
        if (le->shown > cells(le, le->len)) {
            int keep = le->suggest;
            le->suggest = 0; /* the grey tail goes, the line stays */
            redraw_from(le, le->len);
            le->suggest = keep;
        }
        go(le, le->pos);
        out(le, "\r\n", 2);
        le_hist_add(le, le->buf, le->len);
        le->buf[le->len++] = '\n';
        return 1;
    }
    switch (key) {
    case VT_KEY_LEFT:
        move_to(le, shift ? 0 : ctrl ? word_back(le, le->pos) : prev_char(le, le->pos));
        return 0;
    case VT_KEY_RIGHT:
        if (!shift && !ctrl && le->pos == le->len && suggestion(le)) {
            const unsigned char *s = suggestion(le);
            insert(le, s, (int)strlen((const char *)s)); /* take the suggestion */
            return 0;
        }
        move_to(le, shift ? le->len : ctrl ? word_forward(le, le->pos) : next_char(le, le->pos));
        return 0;
    case VT_KEY_HOME:
        move_to(le, 0);
        return 0;
    case VT_KEY_END:
        if (le->pos == le->len && suggestion(le)) {
            const unsigned char *s = suggestion(le);
            insert(le, s, (int)strlen((const char *)s));
            return 0;
        }
        move_to(le, le->len);
        return 0;
    case VT_KEY_BACKSPACE:
        if (meta)
            erase(le, word_back(le, le->pos), le->pos);
        else
            erase(le, prev_char(le, le->pos), le->pos);
        return 0;
    case VT_KEY_DELETE:
        erase(le, le->pos, next_char(le, le->pos));
        return 0;
    case VT_KEY_UP:
        history(le, -1, shift);
        return 0;
    case VT_KEY_DOWN:
        history(le, 1, shift);
        return 0;
    default:
        break;
    }
    if (key >= 0x110000)
        return 0; /* other special keys: nothing in a cooked line */
    if (meta && key < 0x80) {
        switch (key) {
        case 'b': move_to(le, word_back(le, le->pos)); return 0;
        case 'f': move_to(le, word_forward(le, le->pos)); return 0;
        case 'd': erase(le, le->pos, word_forward(le, le->pos)); return 0;
        default: return 0;
        }
    }
    if (n == 1 && b[0] < 0x20 && b[0] != '\t') {
        int p;
        switch (b[0]) {
        case 0x01: move_to(le, 0); break;                     /* Ctrl-A */
        case 0x05:                                            /* Ctrl-E */
            if (le->pos == le->len && suggestion(le)) {
                const unsigned char *s = suggestion(le);
                insert(le, s, (int)strlen((const char *)s));
            } else {
                move_to(le, le->len);
            }
            break;
        case 0x0B: erase(le, le->pos, le->len); break;        /* Ctrl-K */
        case 0x0C: clear_screen(le); break;                   /* Ctrl-L */
        case 0x12:                                            /* Ctrl-R */
            save_state(&le->before_search, le);
            le->searching = 1;
            le->pat_len = 0;
            le->search_idx = le->hist_n - 1;
            redraw_from(le, 0);
            break;
        case 0x15: erase(le, 0, le->pos); break;              /* Ctrl-U */
        case 0x17:                                            /* Ctrl-W */
            p = le->pos;
            while (p > 0 && le->buf[p - 1] == ' ')
                p--;
            while (p > 0 && le->buf[p - 1] != ' ')
                p--;
            erase(le, p, le->pos);
            break;
        case 0x18: erase(le, 0, le->len); break;              /* Ctrl-X */
        case 0x1F:                                            /* Ctrl-_ undo */
        case 0x1A:
            if (le->undo_n) {
                le->typing = 0;
                load_state(le, &le->undo[--le->undo_n]);
            }
            break;
        default:
            break;
        }
        return 0;
    }
    insert(le, b, n);
    return 0;
}

void le_set_command(le_line *le, const unsigned char *word, int found)
{
    unsigned char w[64];
    le_first_word(le, w, sizeof(w));
    if (!w[0] || strcmp((const char *)w, (const char *)word))
        return; /* the line moved on; a newer answer will come */
    strncpy((char *)le->cmd_word, (const char *)w, sizeof(le->cmd_word) - 1);
    if (le->cmd_state == (found ? 1 : 2))
        return;
    le->cmd_state = found ? 1 : 2;
    redraw_from(le, 0);
}

void le_replace_word(le_line *le, int from, const unsigned char *s, int n)
{
    if (from < 0 || from > le->pos || le->len - (le->pos - from) + n > LE_MAX - 2)
        return;
    start(le); /* a line put into a fresh prompt (ACTION_FORCE) begins here */
    push_undo(le);
    le->typing = 0;
    memmove(le->buf + from + n, le->buf + le->pos, le->len - le->pos);
    le->len += n - (le->pos - from);
    memcpy(le->buf + from, s, n);
    le->pos = from + n;
    redraw_from(le, from);
}

int le_kc_word(const le_line *le, int *quote_at)
{
    int i, quotes = 0, last = -1, a;
    for (i = 0; i < le->pos; i++)
        if (le->buf[i] == '"') {
            quotes++;
            last = i;
        }
    if (quotes & 1) {
        *quote_at = last;
        return last + 1;
    }
    *quote_at = -1;
    for (a = le->pos; a > 0; a--) {
        unsigned char ch = le->buf[a - 1];
        if (ch == ' ' || ch == ',' || ch == '>' || ch == '<' || ch == '`')
            break;
    }
    return a;
}

void le_kc_insert(le_line *le, int start, int quote_at, const unsigned char *entry, int n)
{
    unsigned char out[LE_MAX];
    int from = start, i, m = 0, need = quote_at >= 0, body = n;
    for (i = start; i < le->pos; i++)
        if (le->buf[i] == '/' || le->buf[i] == ':')
            from = i + 1;
    if (n > 0 && entry[n - 1] == ' ')
        body = n - 1; /* the file's space is not part of the name */
    for (i = start; i < from && !need; i++)
        need = le->buf[i] == ' ';
    for (i = 0; i < body && !need; i++)
        need = entry[i] == ' ';
    if (need && quote_at < 0) {
        /* the opening quote goes in front: rewrite the word from its start */
        out[m++] = '"';
        for (i = start; i < from && m < LE_MAX - 4; i++)
            out[m++] = le->buf[i];
        from = start;
    }
    for (i = 0; i < body && m < LE_MAX - 3; i++)
        out[m++] = entry[i];
    if (body < n) {
        if (need)
            out[m++] = '"';
        out[m++] = ' ';
    }
    le_replace_word(le, from, out, m);
}

void le_kc_redo(le_line *le, const unsigned char *snap, int snap_pos, int start, int quote_at,
                const unsigned char *entry, int n)
{
    le_replace_word(le, 0, snap, snap_pos);
    le_kc_insert(le, start, quote_at, entry, n);
}

int le_kc_fncmode(const char *letters)
{
    int m = 0;
    for (; letters && *letters; letters++)
        switch (*letters | 0x20) {
        case 'w': m |= LE_KC_WINDOW; break;
        case 'l': m |= LE_KC_LIST; break;
        case 'b': m |= LE_KC_CYCLE; break;
        case 'c': m |= LE_KC_COMMON; break;
        case 's': m |= LE_KC_SILENT; break;
        }
    if (!(m & (LE_KC_WINDOW | LE_KC_LIST | LE_KC_CYCLE | LE_KC_COMMON)))
        m |= LE_KC_WINDOW;
    if (m & LE_KC_WINDOW)
        m &= ~(LE_KC_LIST | LE_KC_CYCLE);
    return m;
}

void le_show_list(le_line *le, const char *names, int len)
{
    int widest = 0, k = 0, col, per, cols = vt_cols(le->t), i;
    unsigned char prompt[256];
    int pl = prompt_from_screen(le, prompt, sizeof(prompt));
    while (k < len) {
        int n = (int)strlen(names + k);
        if (n > widest)
            widest = n;
        k += n + 1;
    }
    widest += 2;
    per = cols / widest;
    if (per < 1)
        per = 1;
    le->pos = le->len;
    go(le, le->len);
    out(le, "\r\n", 2);
    for (k = 0, col = 0; k < len; col++) {
        int n = (int)strlen(names + k);
        if (col == per) {
            out(le, "\r\n", 2);
            col = 0;
        }
        out(le, names + k, n);
        for (i = n; i < widest && col < per - 1; i++)
            out(le, " ", 1);
        k += n + 1;
    }
    out(le, "\r\n", 2);
    out(le, prompt, pl);
    le->started = 0;
    start(le);
    le->shown = 0;
    redraw_from(le, 0);
}
