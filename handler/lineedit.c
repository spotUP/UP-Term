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

#if defined(VT_AMIGA_EXEC_ALLOC)
/* code without a C startup (the handler): exec memory, no libc heap */
#include <exec/memory.h>
#include <proto/exec.h>
#define LE_MALLOC(n) AllocVec((ULONG)(n), MEMF_ANY)
#define LE_FREE(p) FreeVec(p)
#elif defined(VT_COUNT_ALLOC)
#define LE_MALLOC(n) vt_count_malloc((unsigned long)(n))
#define LE_FREE(p) vt_count_free(p)
#else
#include <stdlib.h>
#define LE_MALLOC(n) malloc(n)
#define LE_FREE(p) free(p)
#endif

#define HIST(le, i) ((le)->hist + (le)->hist_at[i])

/* *buf grown to hold need bytes (from 256, doubling, never past max), its
 * used bytes kept. 0 when there is no memory: *buf is left as it was. */
static int grow(unsigned char **buf, long *cap, long used, long need, long max)
{
    long n = *cap ? *cap : 256;
    unsigned char *b;
    if (need <= *cap)
        return 1;
    if (need > max)
        return 0;
    while (n < need)
        n *= 2;
    if (n > max)
        n = max;
    b = (unsigned char *)LE_MALLOC(n);
    if (!b)
        return 0;
    if (used)
        memcpy(b, *buf, (size_t)used);
    if (*buf)
        LE_FREE(*buf);
    *buf = b;
    *cap = n;
    return 1;
}

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

static void hist_drop(le_line *le);

static int hist_max(const le_line *le)
{
    return le->hist_max > 0 ? le->hist_max : LE_HIST;
}

void le_hist_limit(le_line *le, int n)
{
    le->hist_max = n < 1 ? 0 : n > LE_HIST_CEIL ? LE_HIST_CEIL : n;
    while (le->hist_n > hist_max(le))
        hist_drop(le);
    le->hist_pos = le->hist_n;
}

/* The oldest history line out; the others move down. */
static void hist_drop(le_line *le)
{
    long n = (long)strlen((const char *)le->hist) + 1;
    int i;
    memmove(le->hist, le->hist + n, (size_t)(le->hist_used - n));
    le->hist_used -= n;
    for (i = 1; i < le->hist_n; i++)
        le->hist_at[i - 1] = le->hist_at[i] - n;
    le->hist_n--;
}

void le_hist_add(le_line *le, const unsigned char *s, int n)
{
    while (n > 0 && (s[n - 1] == '\n' || s[n - 1] == '\r'))
        n--;
    if (n <= 0)
        return;
    if (n > LE_HIST_LEN - 1)
        n = LE_HIST_LEN - 1;
    if (le->hist_n && (int)strlen((const char *)HIST(le, le->hist_n - 1)) == n &&
        !memcmp(HIST(le, le->hist_n - 1), s, n))
        return; /* the same line again */
    while (le->hist_n >= hist_max(le))
        hist_drop(le);
    /* no memory for more: the oldest lines make room, as a full history does */
    while (!grow(&le->hist, &le->hist_cap, le->hist_used, le->hist_used + n + 1,
                 (long)hist_max(le) * LE_HIST_LEN) && le->hist_n)
        hist_drop(le);
    if (le->hist_used + n + 1 > le->hist_cap)
        return;
    if (le->hist_n >= le->hist_at_cap) {
        int nc = le->hist_at_cap ? le->hist_at_cap * 2 : 4;
        unsigned long *na;
        if (nc > hist_max(le))
            nc = hist_max(le);
        na = (unsigned long *)LE_MALLOC((size_t)nc * sizeof(unsigned long));
        if (!na) {
            if (!le->hist_n)
                return;
            hist_drop(le);   /* no memory for the index: the oldest line makes room */
        } else {
            if (le->hist_n)
                memcpy(na, le->hist_at, (size_t)le->hist_n * sizeof(unsigned long));
            if (le->hist_at)
                LE_FREE(le->hist_at);
            le->hist_at = na;
            le->hist_at_cap = nc;
        }
    }
    le->hist_at[le->hist_n] = le->hist_used;
    memcpy(le->hist + le->hist_used, s, n);
    le->hist[le->hist_used + n] = 0;
    le->hist_used += n + 1;
    le->hist_n++;
    le->hist_pos = le->hist_n;
}

int le_hist_wants(const le_line *le)
{
    return !((le->hist_ctl & LE_HC_IGNORESPACE) && le->len && le->buf[0] == ' ');
}

int le_hist_count(const le_line *le)
{
    return le->hist_n;
}

int le_hist_get(const le_line *le, int i, unsigned char *buf, int max)
{
    int n;
    if (i < 0 || i >= le->hist_n || max < 1)
        return -1;
    n = (int)strlen((const char *)HIST(le, i));
    if (n > max - 1)
        n = max - 1;
    memcpy(buf, HIST(le, i), (size_t)n);
    buf[n] = 0;
    return n;
}

int le_hist_del(le_line *le, int i)
{
    long n, k;
    if (i < 0 || i >= le->hist_n)
        return -1;
    n = (long)strlen((const char *)HIST(le, i)) + 1;
    memmove(le->hist + le->hist_at[i], le->hist + le->hist_at[i] + n,
            (size_t)(le->hist_used - le->hist_at[i] - n));
    le->hist_used -= n;
    for (k = i + 1; k < le->hist_n; k++)
        le->hist_at[k - 1] = le->hist_at[k] - n;
    le->hist_n--;
    le->hist_pos = le->hist_n;
    return 0;
}

void le_hist_clear(le_line *le)
{
    le->hist_used = 0;
    le->hist_n = 0;
    le->hist_pos = 0;
}

static unsigned long line_key(const le_line *le)
{
    unsigned long k = 2166136261UL ^ (unsigned long)le->len;
    int i;
    for (i = 0; i < le->len; i++)
        k = (k ^ le->buf[i]) * 16777619UL;
    return k ? k : 1;
}

/* The newest history line that starts with the whole line and is longer:
 * its tail is the grey suggestion; failing that, the completion ghost. */
static const unsigned char *hist_suggestion(const le_line *le)
{
    int i;
    if (!le->suggest || le->searching || !le->len || le->pos != le->len)
        return 0;
    for (i = le->hist_n - 1; i >= 0; i--) {
        const unsigned char *h = HIST(le, i);
        int n = (int)strlen((const char *)h);
        if (n > le->len && !memcmp(h, le->buf, le->len))
            return h + le->len;
    }
    return 0;
}

static const unsigned char *suggestion(const le_line *le)
{
    const unsigned char *s = hist_suggestion(le);
    if (s)
        return s;
    if (le->suggest && le->ghost_n && !le->searching && le->len && le->pos == le->len &&
        le->ghost_key == line_key(le))
        return le->ghost;
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

/* A countdown on the caller's clock (the /theme list's rest, the command
 * word's): 1 once *rest_us has run out, then 0 until it is set again. */
static int rest_down(long *rest_us, long waited_us)
{
    if (*rest_us <= 0)
        return 0;
    *rest_us -= waited_us;
    if (*rest_us > 0)
        return 0;
    *rest_us = 0;
    return 1;
}

/* The cursor is in the first word: at its start, inside, or right after it. */
static int in_first_word(const le_line *le)
{
    int a = 0, b = first_word_len(le);
    while (a < b && le->buf[a] == ' ')
        a++;
    return b > a && le->pos >= a && le->pos <= b;
}

/* The colour the first word shows: its answer, once the keys have rested. */
static int cmd_colour(const le_line *le)
{
    return le->cmd_rest_us > 0 ? 0 : le->cmd_state;
}

/* The first word (from the line's start) written in colour col (0 plain);
 * the cursor after it. */
static void paint_word(le_line *le, int fw, int col)
{
    go(le, 0);
    if (col)
        out(le, col == 1 ? "\033[32m" : "\033[31m", 5); /* green / red */
    out(le, le->buf, fw);
    if (col)
        out(le, "\033[39m", 5);
    le->cmd_drawn = col;
}

/* The first word's cells made what they should show, and nothing else.
 * 1 when they were written (the cursor is then after the word). */
static int cmd_show(le_line *le)
{
    int fw, col;
    if (!le->started)
        return 0;
    check_command_word(le);
    fw = first_word_len(le);
    col = cmd_colour(le);
    if (!fw || col == le->cmd_drawn)
        return 0;
    paint_word(le, fw, col);
    return 1;
}

/* A typed edit (the first word was `before`): the word changed with the
 * cursor in it -- plain until the keys rest; the cursor out of the word --
 * its colour at once. */
static void cmd_typed(le_line *le, const unsigned char *before)
{
    unsigned char w[64];
    int in = in_first_word(le);
    le_first_word(le, w, sizeof(w));
    if (in && strcmp((const char *)w, (const char *)before))
        le->cmd_rest_us = LE_CMD_REST_US;
    else if (!in)
        le->cmd_rest_us = 0;
}

/* ---- drawing ------------------------------------------------------------ */

/* Show the line from byte p on, then its grey tail, blank the cells it no
 * longer covers, and leave the cursor at le->pos. */
static void redraw_from(le_line *le, int p)
{
    const unsigned char *sug;
    int now, i, fw, col;
    check_command_word(le);
    sug = suggestion(le);
    now = cells(le, le->len);
    fw = first_word_len(le);
    col = cmd_colour(le);
    if (fw && (col || le->cmd_drawn) && (p < fw || col != le->cmd_drawn)) {
        /* the command word drawn whole, in its colour (or plain again) */
        paint_word(le, fw, col);
        if (p < fw)
            p = fw;
        else if (p > fw)
            go(le, p);
    } else {
        if (!fw)
            le->cmd_drawn = 0;
        go(le, p);
    }
    out(le, le->buf + p, le->len - p);
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
    le->cmd_rest_us = 0;
    redraw_from(le, 0);
}

/* The oldest undo snapshot out; the others move down. */
static void undo_drop(le_line *le)
{
    long n = le->undo[0].len;
    int i;
    memmove(le->undo_buf, le->undo_buf + n, (size_t)(le->undo_used - n));
    le->undo_used -= n;
    for (i = 1; i < le->undo_n; i++) {
        le->undo[i - 1] = le->undo[i];
        le->undo[i - 1].at -= n;
    }
    le->undo_n--;
}

static void push_undo(le_line *le)
{
    le_snap *u;
    if (le->undo_n == LE_UNDO)
        undo_drop(le);
    while (!grow(&le->undo_buf, &le->undo_cap, le->undo_used, le->undo_used + le->len, LE_UNDO_BYTES) &&
           le->undo_n)
        undo_drop(le);
    if (le->undo_used + le->len > le->undo_cap)
        return; /* no memory: no snapshot */
    u = &le->undo[le->undo_n++];
    u->at = le->undo_used;
    u->len = le->len;
    u->pos = le->pos;
    if (le->len)
        memcpy(le->undo_buf + u->at, le->buf, (size_t)le->len);
    le->undo_used += le->len;
}

/* Ctrl-_: the last snapshot back on the line. */
static void pop_undo(le_line *le)
{
    le_snap *u = &le->undo[--le->undo_n];
    if (u->len)
        memcpy(le->buf, le->undo_buf + u->at, (size_t)u->len);
    le->len = u->len;
    le->pos = u->pos;
    le->undo_used = u->at;
    le->cmd_rest_us = 0;
    redraw_from(le, 0);
}

static void erase(le_line *le, int a, int b)
{
    unsigned char before[64];
    if (b <= a)
        return;
    push_undo(le);
    le->typing = 0;
    le_first_word(le, before, sizeof(before));
    memmove(le->buf + a, le->buf + b, le->len - b);
    le->len -= b - a;
    if (le->pos > b)
        le->pos -= b - a;
    else if (le->pos > a)
        le->pos = a;
    cmd_typed(le, before);
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
    le->cmd_rest_us = 0; /* history, a search: not typed */
    redraw_from(le, 0);
}

static void move_to(le_line *le, int p)
{
    int had = suggestion(le) != 0;
    le->pos = p;
    le->typing = 0;
    if (!in_first_word(le))
        le->cmd_rest_us = 0; /* the cursor left the word: its colour now */
    if (had || suggestion(le)) {
        redraw_from(le, 0); /* the grey tail shows only at the end */
    } else {
        cmd_show(le);
        go(le, p);
    }
}

/* typed: a key's character (the command word rests while it is typed);
 * else a suggestion taken, coloured at once */
static void insert(le_line *le, const unsigned char *b, int n, int typed)
{
    const unsigned char *sug;
    unsigned char before[64];
    if (le->len + n > LE_MAX - 2)
        return;
    if (!le->typing)
        push_undo(le);
    le->typing = 1;
    sug = suggestion(le);
    le_first_word(le, before, sizeof(before));
    memmove(le->buf + le->pos + n, le->buf + le->pos, le->len - le->pos);
    memcpy(le->buf + le->pos, b, n);
    le->len += n;
    le->pos += n;
    if (typed)
        cmd_typed(le, before);
    else
        le->cmd_rest_us = 0;
    check_command_word(le);
    if (le->pos == le->len && !sug && !suggestion(le) && le->shown <= cells(le, le->len - n) &&
        cmd_colour(le) == le->cmd_drawn && (!le->cmd_drawn || le->pos - n >= first_word_len(le))) {
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
        if (i == le->hist_n || !plen || !memcmp(HIST(le, i), le->buf, plen))
            break;
    }
    le->hist_pos = i;
    le->typing = 0;
    if (i == le->hist_n) {
        if (!search)
            set_line(le, (const unsigned char *)"", -1);
        return;
    }
    set_line(le, HIST(le, i), search ? plen : -1);
}

/* Ctrl-R: the newest entry at or before `from` containing the pattern. */
static void search_from(le_line *le, int from)
{
    int i, at;
    for (i = from; i >= 0; i--) {
        at = find(HIST(le, i), le->pat, le->pat_len);
        if (at >= 0) {
            le->search_idx = i;
            set_line(le, HIST(le, i), at + le->pat_len);
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
        for (i = 0; i < le->start_col && i < n && k < max - VT_CELL_UTF8_MAX; i++) {
            if (!c[i].width)
                continue; /* the right half of a wide character */
            if (le->utf8) {
                k += vt_cell_utf8(le->t, &c[i], (char *)prompt + k);
            } else {
                vt_u32 cp = vt_cell_char(le->t, &c[i]);
                prompt[k++] = (unsigned char)(cp < 0x100 ? cp : '?');
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
    le->cmd_drawn = 0; /* the screen lost it */
    start(le);
    redraw_from(le, 0);
}

/* ---- medium mode ------------------------------------------------------------ */

int le_medium_report(const le_line *le, long key, int mods, unsigned char *out)
{
    int code = key == VT_KEY_TAB ? ((mods & VT_MOD_SHIFT) ? 13 : 12)
             : key == VT_KEY_UP && !mods ? 2 : key == VT_KEY_DOWN && !mods ? 3 : 0;
    int n = 0, v[3], i;

    if (!code)
        return 0;
    v[0] = code;
    v[1] = le->len;
    v[2] = le->pos + 1;
    out[n++] = 0x9b;
    for (i = 0; i < 3; i++) {
        unsigned char d[8];
        int m = 0, x = v[i];
        do
            d[m++] = (unsigned char)('0' + x % 10);
        while ((x /= 10) != 0);
        while (m)
            out[n++] = d[--m];
        out[n++] = i < 2 ? ';' : 'U';
    }
    return n;
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

void le_free(le_line *le)
{
    if (le->hist)
        LE_FREE(le->hist);
    if (le->hist_at)
        LE_FREE(le->hist_at);
    le->hist_at = 0;
    le->hist_at_cap = 0;
    if (le->undo_buf)
        LE_FREE(le->undo_buf);
    le->hist = le->undo_buf = 0;
    le->hist_used = le->hist_cap = le->undo_used = le->undo_cap = 0;
    le->hist_n = le->hist_pos = le->undo_n = 0;
}

void le_reset(le_line *le)
{
    le_ghost_clear(le);
    le->len = 0;
    le->pos = 0;
    le->started = 0;
    le->shown = 0;
    le->hist_pos = le->hist_n;
    le->searching = 0;
    le->vi_cmd = le->vi_op = le->vi_pend = le->vi_count = le->vi_count2 = le->vi_replace = 0;
    le->vi_rec_n = le->vi_changed = 0;
    le->undo_n = 0;
    le->undo_used = 0;
    le->typing = 0;
    le->cmd_rest_us = 0;
    le->cmd_drawn = 0;
}

void le_resized(le_line *le)
{
    int x, y, cols = vt_cols(le->t);
    long at, row;
    if (!le->started)
        return;
    vt_cursor(le->t, &x, &y);
    /* the cursor stands before byte pos: the line starts that many cells
     * earlier, counted in rows of the new width */
    at = (long)y * cols + x + vt_wrap_pending(le->t) - cells(le, le->pos);
    row = at >= 0 ? at / cols : -((-at + cols - 1) / cols);
    le->start_row = row + vt_lines_scrolled(le->t);
    le->start_col = (int)(at - row * cols);
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

/* ---- V91: vi editing mode -------------------------------------------------- */

static int vi_cls(unsigned char c)
{
    if (c == ' ' || c == '\t')
        return 0;
    return (c >= '0' && c <= '9') || (c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || c == '_' || c >= 0x80 ? 1 : 2;
}

static int vi_w(const le_line *le, int p) /* the start of the next word */
{
    int c;
    if (p >= le->len)
        return le->len;
    c = vi_cls(le->buf[p]);
    if (c)
        while (p < le->len && vi_cls(le->buf[p]) == c)
            p = next_char(le, p);
    while (p < le->len && !vi_cls(le->buf[p]))
        p = next_char(le, p);
    return p;
}

static int vi_e(const le_line *le, int p) /* the last character of this or the next word */
{
    int c, q;
    if (p >= le->len)
        return le->len;
    p = next_char(le, p);
    while (p < le->len && !vi_cls(le->buf[p]))
        p = next_char(le, p);
    if (p >= le->len)
        return le->len;
    c = vi_cls(le->buf[p]);
    while ((q = next_char(le, p)) < le->len && vi_cls(le->buf[q]) == c)
        p = q;
    return p;
}

static int vi_b(const le_line *le, int p) /* the start of this or the previous word */
{
    int c;
    if (p <= 0)
        return 0;
    p = prev_char(le, p);
    while (p > 0 && !vi_cls(le->buf[p]))
        p = prev_char(le, p);
    c = vi_cls(le->buf[p]);
    while (p > 0 && vi_cls(le->buf[prev_char(le, p)]) == c)
        p = prev_char(le, p);
    return p;
}

/* Where a motion key goes, count times; -1 when ch is no motion. *incl: an operator includes the character there. */
static int vi_motion(const le_line *le, int ch, int cnt, int *incl)
{
    int p = le->pos, i;
    *incl = 0;
    switch (ch) {
    case 'h':
        for (i = 0; i < cnt; i++) p = prev_char(le, p);
        return p;
    case 'l': case ' ':
        for (i = 0; i < cnt; i++) p = p < le->len ? next_char(le, p) : p;
        return p;
    case 'w':
        for (i = 0; i < cnt; i++) p = vi_w(le, p);
        return p;
    case 'b':
        for (i = 0; i < cnt; i++) p = vi_b(le, p);
        return p;
    case 'e':
        for (i = 0; i < cnt; i++) p = vi_e(le, p);
        *incl = 1;
        return p;
    case '0': return 0;
    case '$': return le->len;
    case '^':
        for (p = 0; p < le->len && !vi_cls(le->buf[p]); p++)
            ;
        return p;
    default: return -1;
    }
}

static void vi_rec_add(le_line *le, int c)
{
    if (!le->vi_replay && le->vi_rec_n < (int)sizeof(le->vi_rec))
        le->vi_rec[le->vi_rec_n++] = (unsigned char)c;
}

/* The command is over (idle in command mode): a change becomes what "." repeats. */
static void vi_settle(le_line *le)
{
    if (le->vi_op || le->vi_pend || le->vi_count || !le->vi_cmd)
        return;
    if (le->vi_changed && !le->vi_replay && le->vi_rec_n < (int)sizeof(le->vi_rec)) {
        memcpy(le->vi_last, le->vi_rec, (size_t)le->vi_rec_n);
        le->vi_last_n = le->vi_rec_n;
    }
    le->vi_changed = 0;
    le->vi_rec_n = 0;
}

static void vi_to_cmd(le_line *le)
{
    le->vi_cmd = 1;
    le->vi_replace = 0;
    le->typing = 0;
    if (le->pos > 0)
        move_to(le, prev_char(le, le->pos));
}

static void vi_to_insert(le_line *le)
{
    le->vi_cmd = 0;
    le->vi_changed = 1;
    le->typing = 0;
}

static void vi_clamp(le_line *le)
{
    if (le->vi_cmd && le->pos >= le->len && le->len > 0)
        move_to(le, prev_char(le, le->len));
}

/* y d c over [a, b) */
static void vi_range(le_line *le, int op, int a, int b)
{
    if (b < a) {
        int t = a;
        a = b;
        b = t;
    }
    if (b > a) {
        le->vi_reg_n = b - a < (int)sizeof(le->vi_reg) ? b - a : (int)sizeof(le->vi_reg);
        memcpy(le->vi_reg, le->buf + a, (size_t)le->vi_reg_n);
    }
    if (op == 'y') {
        move_to(le, a);
        return;
    }
    le->pos = a;
    erase(le, a, b);
    le->vi_changed = 1;
    if (op == 'c')
        vi_to_insert(le);
    else
        vi_clamp(le);
}

static void vi_paste(le_line *le, int after, int cnt)
{
    int i;
    if (!le->vi_reg_n)
        return;
    if (after && le->pos < le->len)
        move_to(le, next_char(le, le->pos));
    le->typing = 0;
    for (i = 0; i < cnt; i++)
        insert(le, le->vi_reg, le->vi_reg_n, 0);
    le->typing = 0;
    le->vi_changed = 1;
    if (le->pos > 0)
        move_to(le, prev_char(le, le->pos));
}

static int vi_search(le_line *le, int dir)
{
    int i;
    if (!le->pat_len)
        return 0;
    for (i = le->hist_pos + dir; i >= 0 && i < le->hist_n; i += dir)
        if (find(HIST(le, i), le->pat, le->pat_len) >= 0) {
            le->hist_pos = i;
            le->typing = 0;
            set_line(le, HIST(le, i), 0);
            return 1;
        }
    return 0;
}

static int vi_key(le_line *le, long key, int mods, const unsigned char *b, int n);

static void vi_feed(le_line *le, int c)
{
    unsigned char ch = (unsigned char)c;
    if (!vi_key(le, ch, 0, &ch, 1) && c >= 0x20)
        insert(le, &ch, 1, 1);
}

/* 1: the key was the vi mode's; 0: the editor treats it as in emacs mode (inserts it, an arrow key, a Ctrl key) */
static int vi_key(le_line *le, long key, int mods, const unsigned char *b, int n)
{
    int ch, cnt, t, incl, i, a;
    if (key == VT_KEY_ESCAPE || (n == 1 && b[0] == 0x1B)) {
        if (!le->vi_cmd) {
            vi_rec_add(le, 0x1B);
            vi_to_cmd(le);
        }
        le->vi_op = le->vi_pend = le->vi_count = le->vi_count2 = 0;
        vi_settle(le);
        return 1;
    }
    if (key >= 0x110000 || n < 1)
        return 0;
    if (!le->vi_cmd) {
        if (b[0] >= 0x20 && b[0] != 0x7F) {
            vi_rec_add(le, b[0]); /* a multibyte character: its first byte only is repeated by "." */
            if (le->vi_replace && le->pos < le->len)
                erase(le, le->pos, next_char(le, le->pos));
        }
        return 0;
    }
    if ((mods & (VT_MOD_CTRL | VT_MOD_ALT)) || n != 1 || b[0] < 0x20)
        return 0;
    ch = b[0];
    if (le->vi_pend == 'r') {
        vi_rec_add(le, ch);
        cnt = le->vi_count ? le->vi_count : 1;
        le->vi_pend = le->vi_count = 0;
        for (a = le->pos, i = 0; i < cnt && a < le->len; i++)
            a = next_char(le, a);
        if (i == cnt) {
            unsigned char c = (unsigned char)ch;
            int at = le->pos;
            erase(le, at, a);
            le->pos = at;
            for (i = 0; i < cnt; i++)
                insert(le, &c, 1, 0);
            le->typing = 0;
            move_to(le, prev_char(le, le->pos));
            le->vi_changed = 1;
        }
        vi_settle(le);
        return 1;
    }
    if (ch >= '1' && ch <= '9' ? 1 : (ch == '0' && le->vi_count)) {
        le->vi_count = le->vi_count * 10 + (ch - '0');
        if (le->vi_count > 9999)
            le->vi_count = 9999;
        vi_rec_add(le, ch);
        return 1;
    }
    vi_rec_add(le, ch);
    cnt = le->vi_count ? le->vi_count : 1;
    if (le->vi_op) {
        int op = le->vi_op, m = ch;
        cnt *= le->vi_count2 ? le->vi_count2 : 1;
        le->vi_op = le->vi_count = le->vi_count2 = 0;
        if (ch == op) {
            vi_range(le, op, 0, le->len);
        } else {
            if (op == 'c' && ch == 'w' && le->pos < le->len && vi_cls(le->buf[le->pos]))
                m = 'e'; /* cw changes to the end of the word */
            t = vi_motion(le, m, cnt, &incl);
            if (t >= 0) {
                if (incl && t < le->len)
                    t = next_char(le, t);
                vi_range(le, op, le->pos, t);
            }
        }
        vi_settle(le);
        return 1;
    }
    le->vi_count = 0;
    switch (ch) {
    case 'd': case 'c': case 'y':
        le->vi_op = ch;
        le->vi_count2 = cnt == 1 ? 0 : cnt;
        return 1;
    case 'D': case 'C': case 'Y':
        vi_range(le, ch == 'D' ? 'd' : ch == 'C' ? 'c' : 'y', le->pos, le->len);
        break;
    case 'x': case 's':
        for (a = le->pos, i = 0; i < cnt && a < le->len; i++)
            a = next_char(le, a);
        vi_range(le, ch == 'x' ? 'd' : 'c', le->pos, a);
        break;
    case 'X':
        for (a = le->pos, i = 0; i < cnt && a > 0; i++)
            a = prev_char(le, a);
        vi_range(le, 'd', a, le->pos);
        break;
    case 'S': vi_range(le, 'c', 0, le->len); break;
    case 'i': vi_to_insert(le); break;
    case 'a':
        if (le->pos < le->len)
            move_to(le, next_char(le, le->pos));
        vi_to_insert(le);
        break;
    case 'A': move_to(le, le->len); vi_to_insert(le); break;
    case 'I': move_to(le, 0); vi_to_insert(le); break;
    case 'R': vi_to_insert(le); le->vi_replace = 1; break;
    case 'p': vi_paste(le, 1, cnt); break;
    case 'P': vi_paste(le, 0, cnt); break;
    case 'r': le->vi_pend = 'r'; le->vi_count = cnt == 1 ? 0 : cnt; return 1;
    case '~':
        for (i = 0; i < cnt && le->pos < le->len; i++) {
            unsigned char c = le->buf[le->pos];
            int at = le->pos;
            c = c >= 'a' && c <= 'z' ? c - 32 : c >= 'A' && c <= 'Z' ? c + 32 : c;
            erase(le, at, next_char(le, at));
            le->pos = at;
            insert(le, &c, 1, 0);
            le->typing = 0;
        }
        vi_clamp(le);
        le->vi_changed = 1;
        break;
    case 'u':
        if (le->undo_n) {
            le->typing = 0;
            pop_undo(le);
            vi_clamp(le);
        }
        break;
    case '.': {
        unsigned char rec[sizeof(le->vi_last)];
        int len = le->vi_last_n;
        memcpy(rec, le->vi_last, (size_t)len);
        le->vi_rec_n = 0;
        le->vi_replay = 1;
        for (i = 0; i < len; i++)
            vi_feed(le, rec[i]);
        le->vi_replay = 0;
        break;
    }
    case 'k': case '-': history(le, -1, 0); vi_clamp(le); break;
    case 'j': case '+': history(le, 1, 0); vi_clamp(le); break;
    case '/': case '?':
        le->vi_dir = ch == '/' ? -1 : 1;
        save_state(&le->before_search, le);
        le->searching = 1;
        le->pat_len = 0;
        le->search_idx = le->hist_n - 1;
        redraw_from(le, 0);
        break;
    case 'n': vi_search(le, le->vi_dir ? le->vi_dir : -1); break;
    case 'N': vi_search(le, le->vi_dir ? -le->vi_dir : 1); break;
    default:
        t = vi_motion(le, ch, cnt, &incl);
        if (t >= 0) {
            if (t >= le->len && le->len > 0)
                t = prev_char(le, le->len);
            move_to(le, t);
        }
        break;
    }
    vi_settle(le);
    return 1;
}

void le_set_vi(le_line *le, int on)
{
    le->vi = on != 0;
    le->vi_cmd = 0;
    le->vi_op = le->vi_pend = le->vi_count = le->vi_count2 = le->vi_replace = 0;
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
        le->cmd_rest_us = 0; /* the line is finished: the word's colour now */
        if (le->shown > cells(le, le->len)) {
            int keep = le->suggest;
            le->suggest = 0; /* the grey tail goes, the line stays */
            redraw_from(le, le->len);
            le->suggest = keep;
        }
        cmd_show(le);
        go(le, le->pos);
        out(le, "\r\n", 2);
        if (le_hist_wants(le)) {
            if (le->hist_ctl & LE_HC_ERASEDUPS) {
                int k, n = le->len;
                while (n > 0 && (le->buf[n - 1] == '\n' || le->buf[n - 1] == '\r'))
                    n--;
                for (k = le->hist_n - 1; k >= 0; k--)
                    if ((int)strlen((const char *)HIST(le, k)) == n && !memcmp(HIST(le, k), le->buf, (size_t)n))
                        le_hist_del(le, k);
            }
            le_hist_add(le, le->buf, le->len);
        }
        le->buf[le->len++] = '\n';
        return 1;
    }
    if (le->vi && vi_key(le, key, mods, b, n))
        return 0;
    switch (key) {
    case VT_KEY_LEFT:
        move_to(le, shift ? 0 : ctrl ? word_back(le, le->pos) : prev_char(le, le->pos));
        return 0;
    case VT_KEY_RIGHT:
        if (!shift && !ctrl && le->pos == le->len && suggestion(le)) {
            const unsigned char *s = suggestion(le);
            insert(le, s, (int)strlen((const char *)s), 0); /* take the suggestion */
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
            insert(le, s, (int)strlen((const char *)s), 0);
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
        le_run(le, le_key_fn(le, LE_META + (int)key));
        return 0;
    }
    if (n == 1 && b[0] < 0x20 && b[0] != '\t') {
        le_run(le, le_key_fn(le, b[0]));
        return 0;
    }
    insert(le, b, n, 1);
    return 0;
}

/* V92: the function a Ctrl or Meta key runs: the shell's binding (bind, .inputrc), else emacs's (le_fns.h) */
int le_key_fn(const le_line *le, int key)
{
    static const unsigned char emacs[][2] = {
#define LE_EK(k, f) { k, f },
        LE_EMACS_KEYS(LE_EK)
#undef LE_EK
    };
    int i;
    for (i = 0; i < le->nbinds; i++)
        if (le->binds[i][0] == key)
            return le->binds[i][1];
    for (i = 0; i < (int)(sizeof(emacs) / sizeof(emacs[0])); i++)
        if (emacs[i][0] == key)
            return emacs[i][1];
    return 0;
}

void le_set_binds(le_line *le, const unsigned char *pairs)
{
    le->nbinds = 0;
    while (pairs && pairs[0] && pairs[1] && le->nbinds < LE_BINDS) {
        le->binds[le->nbinds][0] = pairs[0];
        le->binds[le->nbinds++][1] = (unsigned char)(pairs[1] - 0x20);
        pairs += 2;
    }
}

/* one of le_fns.h's functions (0: nothing) */
void le_run(le_line *le, int fn)
{
    int p = le->pos;
    switch (fn) {
    case 1: move_to(le, prev_char(le, p)); break;
    case 2: erase(le, prev_char(le, p), p); break;
    case 3: erase(le, word_back(le, p), p); break;
    case 4: move_to(le, word_back(le, p)); break;
    case 5: move_to(le, 0); break;
    case 6: clear_screen(le); break;
    case 7: erase(le, p, next_char(le, p)); break;
    case 8:
        if (p == le->len && suggestion(le)) {
            const unsigned char *s = suggestion(le);
            insert(le, s, (int)strlen((const char *)s), 0);
        } else
            move_to(le, le->len);
        break;
    case 9: move_to(le, next_char(le, p)); break;
    case 10: move_to(le, word_forward(le, p)); break;
    case 11: erase(le, p, le->len); break;
    case 12: erase(le, 0, le->len); break;
    case 13: erase(le, p, word_forward(le, p)); break;
    case 14: history(le, 1, 0); break;
    case 15: history(le, -1, 0); break;
    case 16:
        save_state(&le->before_search, le);
        le->searching = 1;
        le->pat_len = 0;
        le->search_idx = le->hist_n - 1;
        redraw_from(le, 0);
        break;
    case 17:
        if (le->undo_n) {
            le->typing = 0;
            pop_undo(le);
        }
        break;
    case 18: erase(le, 0, p); break;
    case 19:
        while (p > 0 && le->buf[p - 1] == ' ')
            p--;
        while (p > 0 && le->buf[p - 1] != ' ')
            p--;
        erase(le, p, le->pos);
        break;
    default:
        break;
    }
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
    if (cmd_show(le)) /* kept while the word is typed: le_command_rested shows it */
        go(le, le->pos);
}

void le_no_command(le_line *le)
{
    if (!le->cmd_state)
        return;
    le->cmd_state = 0; /* a program's line, not the shell's: no command colour */
    if (cmd_show(le))
        go(le, le->pos);
}

int le_command_rested(le_line *le, long waited_us)
{
    if (!rest_down(&le->cmd_rest_us, waited_us))
        return 0;
    if (cmd_show(le))
        go(le, le->pos);
    return 1;
}

void le_command_now(le_line *le)
{
    le->cmd_rest_us = 0;
    if (cmd_show(le))
        go(le, le->pos);
}

void le_replace_word(le_line *le, int from, const unsigned char *s, int n)
{
    if (from < 0 || from > le->pos || le->len - (le->pos - from) + n > LE_MAX - 2)
        return;
    start(le); /* a line put into a fresh prompt (ACTION_FORCE) begins here */
    push_undo(le);
    le->typing = 0;
    le->cmd_rest_us = 0; /* put in, not typed: its colour at once */
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

/* The names under the line in `per` columns `colw` wide, a name longer
 * than `cut` shown as its first cut - 3 bytes and "..." (cut 0: whole);
 * then the prompt and the line again below them. */
static void show_columns(le_line *le, const char *names, int len, int colw, int per, int cut)
{
    int k, col, i;
    unsigned char prompt[256];
    int pl = prompt_from_screen(le, prompt, sizeof(prompt));
    if (per < 1)
        per = 1;
    le->pos = le->len;
    go(le, le->len);
    out(le, "\r\n", 2);
    for (k = 0, col = 0; k < len; col++) {
        int n = (int)strlen(names + k), shown = n;
        if (col == per) {
            out(le, "\r\n", 2);
            col = 0;
        }
        if (cut && n > cut) {
            out(le, names + k, cut - 3);
            out(le, "...", 3);
            shown = cut;
        } else {
            out(le, names + k, n);
        }
        for (i = shown; i < colw && col < per - 1; i++)
            out(le, " ", 1);
        k += n + 1;
    }
    out(le, "\r\n", 2);
    out(le, prompt, pl);
    le->started = 0;
    start(le);
    le->shown = 0;
    le->cmd_drawn = 0; /* the line is drawn anew below the list */
    redraw_from(le, 0);
}

void le_show_list(le_line *le, const char *names, int len)
{
    int widest = 0, k = 0;
    while (k < len) {
        int n = (int)strlen(names + k);
        if (n > widest)
            widest = n;
        k += n + 1;
    }
    widest += 2;
    show_columns(le, names, len, widest, vt_cols(le->t) / widest, 0);
}

void le_kc_show_list(le_line *le, const char *names, int len)
{
    /* KingCON: 19 a column, (XMax + 1) / 19 of them, 15 + "..." past 18 */
    show_columns(le, names, len, 19, vt_cols(le->t) / 19, 18);
}

/* ---- a list to choose from (W30: /theme) -------------------------------------- */

const char *le_menu_name(const le_menu *m, int i)
{
    const char *s = m->names;
    for (; i > 0 && i < m->n; i--)
        s += strlen(s) + 1;
    return s;
}

static int menu_to(le_menu *m, int to, int wrap)
{
    int was = m->sel;
    if (wrap)
        to = (to % m->n + m->n) % m->n;
    else if (to < 0)
        to = 0;
    else if (to > m->n - 1)
        to = m->n - 1;
    m->sel = to;
    if (to == was)
        return LE_MENU_NONE;
    m->rest_us = LE_MENU_REST_US;
    return LE_MENU_MOVED;
}

int le_menu_rested(le_menu *m, long waited_us)
{
    return m->open && rest_down(&m->rest_us, waited_us);
}

int le_menu_key(le_menu *m, long key, int mods, const unsigned char *b, int nb)
{
    int page = (mods & VT_MOD_SHIFT) != 0, i;
    if (!m->open || m->n < 1)
        return LE_MENU_NONE;
    if (key == VT_KEY_RETURN || key == VT_KEY_KP_ENTER || (nb == 1 && (b[0] == '\r' || b[0] == '\n')))
        return LE_MENU_TAKE;
    if (key == VT_KEY_ESCAPE || (nb == 1 && (b[0] == 0x1B || b[0] == 0x07)))
        return LE_MENU_CANCEL; /* Escape, or Ctrl-G as in the line's search */
    switch (key) {
    case VT_KEY_UP:        return menu_to(m, m->sel - (page ? m->rows : 1), !page);
    case VT_KEY_DOWN:      return menu_to(m, m->sel + (page ? m->rows : 1), !page);
    case VT_KEY_PAGE_UP:   return menu_to(m, m->sel - m->rows, 0);
    case VT_KEY_PAGE_DOWN: return menu_to(m, m->sel + m->rows, 0);
    case VT_KEY_HOME:      return menu_to(m, 0, 0);
    case VT_KEY_END:       return menu_to(m, m->n - 1, 0);
    default: break;
    }
    if (nb == 1 && b[0] > 0x20 && b[0] < 0x7F && !(mods & (VT_MOD_CTRL | VT_MOD_ALT))) {
        /* a letter: the next name that starts with it, round to the top */
        int want = b[0] >= 'A' && b[0] <= 'Z' ? b[0] - 'A' + 'a' : b[0];
        for (i = 1; i <= m->n; i++) {
            int k = (m->sel + i) % m->n, c = (unsigned char)le_menu_name(m, k)[0];
            if (c >= 'A' && c <= 'Z')
                c = c - 'A' + 'a';
            if (c == want)
                return menu_to(m, k, 0);
        }
    }
    return LE_MENU_NONE;
}

/* the bar's width: the widest name, cut to the window */
static int menu_width(const le_line *le, const le_menu *m)
{
    int cols = vt_cols(le->t), k, l, w = 0;
    const char *s = m->names;
    for (k = 0; k < m->n; k++, s += l + 1)
        if ((l = (int)strlen(s)) > w)
            w = l;
    if (w > cols - 3)
        w = cols - 3;
    return w < 0 ? 0 : w;
}

/* Row i of the list (i == m->rows: "k of n" and the keys) at screen row y. */
static void menu_row(le_line *le, le_menu *m, long y, int i)
{
    int cols = vt_cols(le->t), k = m->top + i, l;
    char help[96];
    cup(le, y < 0 ? 0 : y, 0);
    out(le, "\033[2K", 4);
    if (i == m->rows) {
        /* where the list is, and the keys */
        int n = 0;
        const char *s = ": Up/Down choose, Enter applies, Escape cancels";
        num(help, &n, m->sel + 1);
        memcpy(help + n, " of ", 4);
        n += 4;
        num(help, &n, m->n);
        l = (int)strlen(s);
        memcpy(help + n, s, (size_t)l);
        n += l;
        out(le, "\033[2m", 4);
        out(le, help, n < cols - 1 ? n : cols - 1);
        out(le, "\033[22m", 5);
    } else if (k < m->n) {
        const char *name = le_menu_name(m, k);
        l = (int)strlen(name);
        if (l > m->w)
            l = m->w;
        if (k == m->sel)
            out(le, "\033[7m", 4);
        out(le, k == m->mark ? "* " : "  ", 2);
        out(le, name, l);
        if (k == m->sel) {
            for (; l < m->w; l++)
                out(le, " ", 1); /* a bar as wide as the widest name */
            out(le, " \033[27m", 6);
        }
    }
}

/* Nothing on grid row y but default blanks (a row never written may still
 * count its cells as used: they are looked at). */
static int row_blank(vt_term *t, int y)
{
    int n, i, used = vt_row_used(t, y);
    const vt_cell *c = vt_row(t, y, &n);
    for (i = 0; c && i < used; i++)
        if (c[i].ch != ' ' || c[i].bg != VT_COLOR_DEFAULT || c[i].attr || c[i].pad)
            return 0;
    return 1;
}

/* CSI n final */
static void csi_n(le_line *le, int n, char final)
{
    char b[16];
    int k = 0;
    b[k++] = 0x1B;
    b[k++] = '[';
    num(b, &k, n);
    b[k++] = final;
    out(le, b, k);
}

void le_menu_draw(le_line *le, le_menu *m)
{
    int rows = vt_rows(le->t), i, d, from = 0, to = 0, whole;
    long top = m->at - vt_lines_scrolled(le->t), help;
    if (!m->open)
        return;
    if (m->sel < m->top)
        m->top = m->sel;
    if (m->sel >= m->top + m->rows)
        m->top = m->sel - m->rows + 1;
    help = top + m->rows;
    d = m->top - m->drawn_top;
    whole = m->drawn_top < 0 || top < 0 || help >= rows || d >= m->rows || -d >= m->rows;
    for (i = (int)help + 1; d && !whole && i < rows; i++)
        whole = !row_blank(le->t, i); /* DL / IL would move it */
    if (whole) {
        m->w = menu_width(le, m);
        for (i = 0; i <= m->rows; i++)
            menu_row(le, m, top + i, i);
    } else {
        if (d > 0) {
            /* the names move up d rows (one blit), the last d come in */
            cup(le, top, 0);
            csi_n(le, d, 'M');
            from = m->rows - d;
            to = m->rows;
        } else if (d < 0) {
            /* down: the first -d come in; the help row was pushed under */
            cup(le, top, 0);
            csi_n(le, -d, 'L');
            to = -d;
        }
        for (i = from; i < to; i++)
            menu_row(le, m, top + i, i);
        i = m->drawn_sel - m->top; /* the row the bar left, if still shown */
        if (m->drawn_sel != m->sel && i >= 0 && i < m->rows && (i < from || i >= to))
            menu_row(le, m, top + i, i);
        i = m->sel - m->top;       /* the bar */
        if (i < from || i >= to)
            menu_row(le, m, top + i, i);
        menu_row(le, m, help, m->rows); /* "k of n" */
        if (d < 0 && help + 1 < rows) {
            cup(le, help + 1, 0);
            out(le, "\033[J", 3); /* what IL pushed under the help row: it was blank */
        }
    }
    m->drawn_top = m->top;
    m->drawn_sel = m->sel;
    cup(le, top + (m->sel - m->top) < 0 ? 0 : top + (m->sel - m->top), 0);
}

void le_menu_open(le_line *le, le_menu *m, const char *names, int n, int sel, int mark)
{
    int x, y, i;
    m->names = names;
    m->n = n;
    m->sel = sel >= 0 && sel < n ? sel : 0;
    m->mark = mark;
    m->top = 0;
    m->drawn_top = m->drawn_sel = -1; /* the first draw is whole */
    m->rest_us = 0;
    m->rows = n < LE_MENU_ROWS ? n : LE_MENU_ROWS;
    if (m->rows > vt_rows(le->t) - 2)
        m->rows = vt_rows(le->t) - 2;
    if (m->rows < 1)
        m->rows = 1;
    m->open = n > 0;
    if (!m->open)
        return;
    /* room for the rows and the help under them: the screen scrolls up
     * when the cursor is near the bottom */
    out(le, "\r", 1);
    for (i = 0; i < m->rows; i++)
        out(le, "\r\n", 2);
    vt_cursor(le->t, &x, &y);
    m->at = y - m->rows + vt_lines_scrolled(le->t);
    le_menu_draw(le, m);
}

void le_menu_close(le_line *le, le_menu *m)
{
    long top;
    if (!m->open)
        return;
    top = m->at - vt_lines_scrolled(le->t);
    cup(le, top < 0 ? 0 : top, 0);
    out(le, "\033[J", 3);
    m->open = 0;
    m->rest_us = 0;
}

/* ---- W21: ghost text from completion candidates ------------------------- */

int le_ghost_want(le_line *le, int *from, int *kind, unsigned long *key)
{
    int i, j, start, first = 1;
    unsigned long k;
    if (!le->suggest || le->searching || !le->len || le->pos != le->len || hist_suggestion(le))
        return 0;
    k = line_key(le);
    if (le->ghost_asked == k || (le->ghost_n && le->ghost_key == k))
        return 0;
    for (i = 0; i < le->len; i++)
        if (le->buf[i] == '"' || le->buf[i] == '\'' || le->buf[i] == '`')
            return 0; /* a quoted line: the word's end is the shell's to say */
    for (start = le->len; start > 0 && le->buf[start - 1] != ' '; start--)
        ;
    if (start == le->len)
        return 0; /* the line ends in a blank: no word yet */
    for (j = 0; j < start; j++)
        if (le->buf[j] != ' ')
            first = 0;
    if (le->buf[start] && strchr("$-<>|&;(", le->buf[start]))
        return 0; /* a variable, an option, a redirect */
    if (first && le->buf[start] == '/')
        return 0; /* UP-Term's own slash commands */
    *kind = first ? 1 : 2;
    for (i = start; i < le->len; i++) {
        if (le->buf[i] == '#' || le->buf[i] == '?' || le->buf[i] == '*')
            return 0;
        if (le->buf[i] == '/' || le->buf[i] == ':')
            *kind = 2; /* a path, even as the first word: files */
    }
    *from = start;
    *key = k;
    le->ghost_asked = k;
    return 1;
}

void le_ghost_offer(le_line *le, unsigned long key, const unsigned char *tail, int n)
{
    int i, k = 0;
    if (!n || !le->suggest || le->searching || le->pos != le->len || line_key(le) != key ||
        hist_suggestion(le))
        return;
    for (i = 0; i < n && k < (int)sizeof(le->ghost) - 3; i++) {
        unsigned char ch = tail[i];
        if (ch < 0x20 || ch == 0x7F)
            break; /* a control character in a file name is not shown */
        if (le->utf8 && ch >= 0x80) {
            le->ghost[k++] = (unsigned char)(0xC0 | (ch >> 6));
            le->ghost[k++] = (unsigned char)(0x80 | (ch & 0x3F));
        } else
            le->ghost[k++] = ch;
    }
    if (!k)
        return;
    le->ghost[k] = 0;
    le->ghost_n = k;
    le->ghost_key = key;
    redraw_from(le, le->pos);
}

void le_ghost_clear(le_line *le)
{
    le->ghost_n = 0;
    le->ghost_key = 0;
    le->ghost_asked = 0;
}
