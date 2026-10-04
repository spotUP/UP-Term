/* hl_style -- themes and SGR (hl_style.h). */
#include <string.h>
#include "hl_style.h"

const char *const hl_class_names[HL_NCLASS] = {
    "plain", "comment", "keyword", "type", "builtin", "string", "escape",
    "number", "preproc", "function", "label", "variable", "key", "section",
    "tag", "attr", "heading", "emphasis", "link", "code", "meta", "added",
    "removed",
    "lineno",
    "h1", "h2", "h3", "h4", "h5", "h6", "quote", "bullet", "rule",
    "codespan", "codeblock", "url", "image", "table", "th", "task"
};

/* xterm's 16 colours: what 16-colour output is measured against */
static const unsigned char base16[16][3] = {
    { 0, 0, 0 }, { 205, 0, 0 }, { 0, 205, 0 }, { 205, 205, 0 },
    { 0, 0, 238 }, { 205, 0, 205 }, { 0, 205, 205 }, { 229, 229, 229 },
    { 127, 127, 127 }, { 255, 0, 0 }, { 0, 255, 0 }, { 255, 255, 0 },
    { 92, 92, 255 }, { 255, 0, 255 }, { 0, 255, 255 }, { 255, 255, 255 }
};

void hl_index_rgb(int i, int *r, int *g, int *b)
{
    static const int lv[6] = { 0, 95, 135, 175, 215, 255 };
    if (i < 16) {
        *r = base16[i][0];
        *g = base16[i][1];
        *b = base16[i][2];
    } else if (i < 232) {
        i -= 16;
        *r = lv[i / 36];
        *g = lv[(i / 6) % 6];
        *b = lv[i % 6];
    } else {
        *r = *g = *b = 8 + (i - 232) * 10;
    }
}

static long dist(int r, int g, int b, int i)
{
    int r2, g2, b2;
    long dr, dg, db;
    hl_index_rgb(i, &r2, &g2, &b2);
    dr = r - r2;
    dg = g - g2;
    db = b - b2;
    return dr * dr * 3 + dg * dg * 4 + db * db * 2;
}

/* By hue, not by distance: the 16 colours are far apart, and the nearest
 * by distance turns most mid-tone colours grey (a purple keyword became
 * bright black). A colour keeps its hue sector; greys keep their light. */
int hl_rgb_to_16(int r, int g, int b)
{
    static const int sector[6] = { 1, 3, 2, 6, 4, 5 }; /* red yellow green cyan blue magenta */
    int mx = r > g ? (r > b ? r : b) : (g > b ? g : b);
    int mn = r < g ? (r < b ? r : b) : (g < b ? g : b);
    long h, d = mx - mn;
    if (d < 48) {
        int lum = (r + g + b) / 3;
        return lum < 48 ? 0 : lum < 160 ? 8 : lum < 224 ? 7 : 15;
    }
    if (mx == r)
        h = 60L * (g - b) / d;
    else if (mx == g)
        h = 120 + 60L * (b - r) / d;
    else
        h = 240 + 60L * (r - g) / d;
    if (h < 0)
        h += 360;
    return sector[((h + 30) / 60) % 6] + (mx >= 200 ? 8 : 0);
}

static int cube_level(int v)
{
    return v < 48 ? 0 : v < 115 ? 1 : (v - 35) / 40;
}

int hl_rgb_to_256(int r, int g, int b)
{
    int ci = 16 + 36 * cube_level(r) + 6 * cube_level(g) + cube_level(b);
    int avg = (r + g + b) / 3;
    int gi = avg > 238 ? 255 : avg < 8 ? 232 : 232 + (avg - 3) / 10;
    if (gi > 255)
        gi = 255;
    return dist(r, g, b, gi) < dist(r, g, b, ci) ? gi : ci;
}

static char *put_num(char *p, long v)
{
    char t[12];
    int n = 0;
    if (v == 0)
        t[n++] = '0';
    while (v > 0) {
        t[n++] = (char)('0' + v % 10);
        v /= 10;
    }
    while (n)
        *p++ = t[--n];
    return p;
}

/* ";38;..." or ";48;..." for colour c at depth */
static char *put_color(char *p, long c, int fg, int depth)
{
    int r, g, b;
    if (c == HL_DEFAULT)
        return p;
    if (HL_IS_RGB(c)) {
        r = (int)((c >> 16) & 255);
        g = (int)((c >> 8) & 255);
        b = (int)(c & 255);
        if (depth == 24) {
            *p++ = ';';
            p = put_num(p, fg ? 38 : 48);
            *p++ = ';';
            *p++ = '2';
            *p++ = ';';
            p = put_num(p, r);
            *p++ = ';';
            p = put_num(p, g);
            *p++ = ';';
            p = put_num(p, b);
            return p;
        }
        c = depth == 256 ? hl_rgb_to_256(r, g, b) : hl_rgb_to_16(r, g, b);
    }
    if (c >= 16 && depth != 256 && depth != 24) {
        hl_index_rgb((int)c, &r, &g, &b);
        c = hl_rgb_to_16(r, g, b);
    }
    *p++ = ';';
    if (c < 8) {
        p = put_num(p, (fg ? 30 : 40) + c);
    } else if (c < 16) {
        p = put_num(p, (fg ? 90 : 100) + c - 8);
    } else {
        p = put_num(p, fg ? 38 : 48);
        *p++ = ';';
        *p++ = '5';
        *p++ = ';';
        p = put_num(p, c);
    }
    return p;
}

int hl_sgr(char *out, const hl_sty *s, int depth)
{
    char *p = out;
    *p++ = 27;
    *p++ = '[';
    *p++ = '0';
    if (s->attr & HL_A_BOLD) { *p++ = ';'; *p++ = '1'; }
    if (s->attr & HL_A_DIM) { *p++ = ';'; *p++ = '2'; }
    if (s->attr & HL_A_ITALIC) { *p++ = ';'; *p++ = '3'; }
    if (s->attr & HL_A_UNDER) { *p++ = ';'; *p++ = '4'; }
    if (s->attr & HL_A_REVERSE) { *p++ = ';'; *p++ = '7'; }
    if (s->attr & HL_A_STRIKE) { *p++ = ';'; *p++ = '9'; }
    if (depth > 0) {
        p = put_color(p, s->fg, 1, depth);
        p = put_color(p, s->bg, 0, depth);
    }
    *p++ = 'm';
    return (int)(p - out);
}

static int has(const char *s, const char *w)
{
    return s && strstr(s, w) != 0;
}

int hl_depth_from_env(const char *term, const char *colorterm)
{
    if (has(colorterm, "truecolor") || has(colorterm, "24bit"))
        return 24;
    if (has(term, "256") || has(term, "vtcon") || has(term, "direct"))
        return 256;
    return 16;
}

/* ---- themes -------------------------------------------------------------- */

static void set(hl_theme *t, int cls, long fg, unsigned attr)
{
    t->s[cls].fg = fg;
    t->s[cls].bg = HL_DEFAULT;
    t->s[cls].attr = attr;
}

static void clear(hl_theme *t)
{
    int i;
    for (i = 0; i < HL_NCLASS; i++)
        set(t, i, HL_DEFAULT, 0);
}

static void ansi(hl_theme *t)
{
    clear(t);
    set(t, HL_COMMENT, 8, HL_A_ITALIC);
    set(t, HL_KEYWORD, 12, HL_A_BOLD);
    set(t, HL_TYPE, 6, 0);
    set(t, HL_BUILTIN, 13, 0);
    set(t, HL_STRING, 2, 0);
    set(t, HL_ESCAPE, 10, HL_A_BOLD);
    set(t, HL_NUMBER, 3, 0);
    set(t, HL_PREPROC, 5, 0);
    set(t, HL_FUNCTION, 14, 0);
    set(t, HL_LABEL, 11, HL_A_BOLD);
    set(t, HL_VARIABLE, 9, 0);
    set(t, HL_KEY, 12, 0);
    set(t, HL_SECTION, 11, HL_A_BOLD);
    set(t, HL_TAG, 12, 0);
    set(t, HL_ATTR, 6, 0);
    set(t, HL_HEADING, 13, HL_A_BOLD);
    set(t, HL_EMPH, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_LINK, 12, HL_A_UNDER);
    set(t, HL_CODE, 3, 0);
    set(t, HL_META, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_ADDED, 2, 0);
    set(t, HL_REMOVED, 1, 0);
    set(t, HL_LINENO, 8, 0);
    set(t, MD_H1, 13, HL_A_BOLD | HL_A_UNDER);
    set(t, MD_H2, 14, HL_A_BOLD);
    set(t, MD_H3, 11, HL_A_BOLD);
    set(t, MD_H4, 10, HL_A_BOLD);
    set(t, MD_H5, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H6, HL_DEFAULT, HL_A_ITALIC);
    set(t, MD_QUOTE, 8, 0);
    set(t, MD_BULLET, 3, 0);
    set(t, MD_RULE, 8, 0);
    set(t, MD_CODESPAN, 3, 0);
    set(t, MD_CODEBLOCK, HL_DEFAULT, 0);
    set(t, MD_URL, 8, 0);
    set(t, MD_IMAGE, 5, 0);
    set(t, MD_TABLE, 8, 0);
    set(t, MD_TH, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_TASK, 2, 0);
}

static void mono(hl_theme *t)
{
    clear(t);
    set(t, HL_COMMENT, HL_DEFAULT, HL_A_DIM);
    set(t, HL_KEYWORD, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_PREPROC, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_LABEL, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_SECTION, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_HEADING, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_EMPH, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_LINK, HL_DEFAULT, HL_A_UNDER);
    set(t, HL_META, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_REMOVED, HL_DEFAULT, HL_A_DIM);
    set(t, HL_LINENO, HL_DEFAULT, HL_A_DIM);
    set(t, MD_H1, HL_DEFAULT, HL_A_BOLD | HL_A_UNDER);
    set(t, MD_H2, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H3, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H4, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H5, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H6, HL_DEFAULT, HL_A_ITALIC);
    set(t, MD_QUOTE, HL_DEFAULT, HL_A_DIM);
    set(t, MD_RULE, HL_DEFAULT, HL_A_DIM);
    set(t, MD_URL, HL_DEFAULT, HL_A_DIM);
    set(t, MD_TABLE, HL_DEFAULT, HL_A_DIM);
    set(t, MD_TH, HL_DEFAULT, HL_A_BOLD);
}

static void rich(hl_theme *t)
{
    clear(t);
    set(t, HL_COMMENT, HL_RGB(0x7f, 0x84, 0x8e), HL_A_ITALIC);
    set(t, HL_KEYWORD, HL_RGB(0xc6, 0x78, 0xdd), HL_A_BOLD);
    set(t, HL_TYPE, HL_RGB(0xe5, 0xc0, 0x7b), 0);
    set(t, HL_BUILTIN, HL_RGB(0x56, 0xb6, 0xc2), 0);
    set(t, HL_STRING, HL_RGB(0x98, 0xc3, 0x79), 0);
    set(t, HL_ESCAPE, HL_RGB(0x56, 0xb6, 0xc2), HL_A_BOLD);
    set(t, HL_NUMBER, HL_RGB(0xd1, 0x9a, 0x66), 0);
    set(t, HL_PREPROC, HL_RGB(0xc6, 0x78, 0xdd), 0);
    set(t, HL_FUNCTION, HL_RGB(0x61, 0xaf, 0xef), 0);
    set(t, HL_LABEL, HL_RGB(0xe5, 0xc0, 0x7b), HL_A_BOLD);
    set(t, HL_VARIABLE, HL_RGB(0xe0, 0x6c, 0x75), 0);
    set(t, HL_KEY, HL_RGB(0xe0, 0x6c, 0x75), 0);
    set(t, HL_SECTION, HL_RGB(0xe5, 0xc0, 0x7b), HL_A_BOLD);
    set(t, HL_TAG, HL_RGB(0xe0, 0x6c, 0x75), 0);
    set(t, HL_ATTR, HL_RGB(0xd1, 0x9a, 0x66), 0);
    set(t, HL_HEADING, HL_RGB(0x61, 0xaf, 0xef), HL_A_BOLD);
    set(t, HL_EMPH, HL_DEFAULT, HL_A_BOLD);
    set(t, HL_LINK, HL_RGB(0x61, 0xaf, 0xef), HL_A_UNDER);
    set(t, HL_CODE, HL_RGB(0x98, 0xc3, 0x79), 0);
    set(t, HL_META, HL_RGB(0xab, 0xb2, 0xbf), HL_A_BOLD);
    set(t, HL_ADDED, HL_RGB(0x98, 0xc3, 0x79), 0);
    set(t, HL_REMOVED, HL_RGB(0xe0, 0x6c, 0x75), 0);
    set(t, HL_LINENO, HL_RGB(0x5c, 0x63, 0x70), 0);
    set(t, MD_H1, HL_RGB(0x61, 0xaf, 0xef), HL_A_BOLD | HL_A_UNDER);
    set(t, MD_H2, HL_RGB(0x56, 0xb6, 0xc2), HL_A_BOLD);
    set(t, MD_H3, HL_RGB(0xe5, 0xc0, 0x7b), HL_A_BOLD);
    set(t, MD_H4, HL_RGB(0x98, 0xc3, 0x79), HL_A_BOLD);
    set(t, MD_H5, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_H6, HL_DEFAULT, HL_A_ITALIC);
    set(t, MD_QUOTE, HL_RGB(0x5c, 0x63, 0x70), 0);
    set(t, MD_BULLET, HL_RGB(0xd1, 0x9a, 0x66), 0);
    set(t, MD_RULE, HL_RGB(0x5c, 0x63, 0x70), 0);
    set(t, MD_CODESPAN, HL_RGB(0xe5, 0xc0, 0x7b), 0);
    set(t, MD_URL, HL_RGB(0x5c, 0x63, 0x70), 0);
    set(t, MD_IMAGE, HL_RGB(0xc6, 0x78, 0xdd), 0);
    set(t, MD_TABLE, HL_RGB(0x5c, 0x63, 0x70), 0);
    set(t, MD_TH, HL_DEFAULT, HL_A_BOLD);
    set(t, MD_TASK, HL_RGB(0x98, 0xc3, 0x79), 0);
}

const char *hl_theme_names(void)
{
    return "ansi mono rich";
}

int hl_theme_builtin(hl_theme *t, const char *name)
{
    if (!strcmp(name, "ansi"))
        ansi(t);
    else if (!strcmp(name, "mono"))
        mono(t);
    else if (!strcmp(name, "rich"))
        rich(t);
    else
        return 0;
    return 1;
}

static int lower(int c)
{
    return c >= 'A' && c <= 'Z' ? c + 32 : c;
}

static int word_is(const char *w, int n, const char *s)
{
    int i;
    for (i = 0; i < n; i++)
        if (!s[i] || lower(w[i]) != s[i])
            return 0;
    return s[n] == 0;
}

int hl_class_find(const char *name, int len)
{
    int i;
    for (i = 0; i < HL_NCLASS; i++)
        if (word_is(name, len, hl_class_names[i]))
            return i;
    return -1;
}

static int hexv(int c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 :
           c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* a colour word: 1 with *c set, 0 not a colour */
static int color_word(const char *w, int n, long *c)
{
    static const char *const names[8] = { "black", "red", "green", "yellow", "blue",
                                          "magenta", "cyan", "white" };
    int i, bright = 0;
    if (n == 7 && w[0] == '#') {
        long v = 0;
        for (i = 1; i < 7; i++) {
            if (hexv((unsigned char)w[i]) < 0)
                return 0;
            v = v * 16 + hexv((unsigned char)w[i]);
        }
        *c = 0x1000000L | v;
        return 1;
    }
    if (n > 0 && n <= 3 && w[0] >= '0' && w[0] <= '9') {
        long v = 0;
        for (i = 0; i < n; i++) {
            if (w[i] < '0' || w[i] > '9')
                return 0;
            v = v * 10 + (w[i] - '0');
        }
        if (v > 255)
            return 0;
        *c = v;
        return 1;
    }
    if (word_is(w, n, "default")) {
        *c = HL_DEFAULT;
        return 1;
    }
    if (word_is(w, n, "grey") || word_is(w, n, "gray")) {
        *c = 8;
        return 1;
    }
    if (n > 7 && word_is(w, 7, "bright-")) {
        bright = 8;
        w += 7;
        n -= 7;
    } else if (n > 6 && word_is(w, 6, "bright")) {
        bright = 8;
        w += 6;
        n -= 6;
    }
    for (i = 0; i < 8; i++)
        if (word_is(w, n, names[i])) {
            *c = i + bright;
            return 1;
        }
    return 0;
}

static void copy_err(char *err, int errlen, const char *what, long line)
{
    char num[12];
    char *p;
    int n = (int)strlen(what);
    if (errlen < 32)
        return;
    if (n > errlen - 20)
        n = errlen - 20;
    memcpy(err, what, n);
    p = put_num(num, line);
    *p = 0;
    strcpy(err + n, " on line ");
    strcat(err, num);
}

int hl_theme_parse(hl_theme *t, const char *text, long n, char *err, int errlen)
{
    long i = 0, line = 0;
    while (i < n) {
        long e = i, w;
        int cls, wn, on = 0;
        hl_sty st;
        line++;
        while (e < n && text[e] != '\n')
            e++;
        while (i < e && (text[i] == ' ' || text[i] == '\t' || text[i] == '\r'))
            i++;
        if (i == e || text[i] == ';' || text[i] == '#') {
            i = e + 1;
            continue;
        }
        w = i;
        while (i < e && text[i] != ' ' && text[i] != '\t' && text[i] != '=' && text[i] != ':')
            i++;
        cls = hl_class_find(text + w, (int)(i - w));
        if (cls < 0) {
            copy_err(err, errlen, "unknown class", line);
            return -1;
        }
        st.fg = st.bg = HL_DEFAULT;
        st.attr = 0;
        for (;;) {
            long c;
            while (i < e && (text[i] == ' ' || text[i] == '\t' || text[i] == '=' ||
                             text[i] == ':' || text[i] == ',' || text[i] == '\r'))
                i++;
            if (i >= e)
                break;
            w = i;
            while (i < e && text[i] != ' ' && text[i] != '\t' && text[i] != ',' && text[i] != '\r')
                i++;
            wn = (int)(i - w);
            if (word_is(text + w, wn, "on")) {
                on = 1;
                continue;
            }
            if (color_word(text + w, wn, &c)) {
                if (on)
                    st.bg = c;
                else
                    st.fg = c;
                on = 0;
            } else if (word_is(text + w, wn, "bold")) {
                st.attr |= HL_A_BOLD;
            } else if (word_is(text + w, wn, "dim")) {
                st.attr |= HL_A_DIM;
            } else if (word_is(text + w, wn, "italic")) {
                st.attr |= HL_A_ITALIC;
            } else if (word_is(text + w, wn, "underline")) {
                st.attr |= HL_A_UNDER;
            } else if (word_is(text + w, wn, "reverse")) {
                st.attr |= HL_A_REVERSE;
            } else if (word_is(text + w, wn, "strike")) {
                st.attr |= HL_A_STRIKE;
            } else if (!word_is(text + w, wn, "plain")) {
                copy_err(err, errlen, "unknown word", line);
                return -1;
            }
        }
        t->s[cls] = st;
        i = e + 1;
    }
    return 0;
}
