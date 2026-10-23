/* upconf_rows and the line tokeniser it shares with upconf_parse. Its own file so a
 * program that only walks rows (the Amarchy commodity) does not link the profile table. */
#include <string.h>
#include "upconf.h"
#include "upconf_lex.h"

/* 1 when s starts with "profile", case-insensitively. */
int uc_prefix_profile(const char *s)
{
    static const char want[] = "profile";
    int i;
    for (i = 0; want[i]; i++) {
        char c = s[i];
        if (c >= 'A' && c <= 'Z')
            c = (char)(c - 'A' + 'a');
        if (c != want[i])
            return 0;
    }
    return 1;
}

/* Case-insensitive compare of two NUL-terminated strings. */
int uc_ieq(const char *a, const char *b)
{
    while (*a && *b) {
        char ca = *a, cb = *b;
        if (ca >= 'A' && ca <= 'Z')
            ca = (char)(ca - 'A' + 'a');
        if (cb >= 'A' && cb <= 'Z')
            cb = (char)(cb - 'A' + 'a');
        if (ca != cb)
            return 0;
        a++;
        b++;
    }
    return *a == 0 && *b == 0;
}

/* Copy at most n-1 bytes, always NUL-terminating. 1 when src was longer than
 * n-1 (truncated), 0 otherwise. */
int uc_copy(char *dst, const char *src, int n)
{
    int i;
    for (i = 0; i < n - 1 && src[i]; i++)
        dst[i] = src[i];
    dst[i] = 0;
    return src[i] ? 1 : 0;
}

/* The line tokeniser upconf_parse and upconf_rows share. s is one line with
 * its leading blanks and trailing blanks/CR already cut. */

/* A "[...]" line: the section name ("profile name" or "name"), cut in place;
 * 0 when the bracket is not closed. */
char *uc_section_name(char *s)
{
    char *close = strchr(s, ']');
    if (!close)
        return 0;
    *close = 0;
    s++;
    /* "profile name" or "name" */
    if (uc_prefix_profile(s))
        s += 7;
    while (*s == ' ')
        s++;
    return s;
}

/* A "key = value" line: the key (trailing blanks cut), *value at the text
 * after the "=" and its blanks; 0 when there is no "=". */
char *uc_key_value(char *s, char **value)
{
    char *eq = strchr(s, '=');
    char *v;
    if (!eq)
        return 0;
    v = eq + 1;
    *eq = 0;
    while (eq > s && (eq[-1] == ' ' || eq[-1] == '\t'))
        *--eq = 0;
    while (*v == ' ' || *v == '\t')
        v++;
    *value = v;
    return s;
}

int upconf_rows(const char *buf, long len, const char *section, upconf_row_fn fn, void *ud, int *dropped)
{
    char line[256];
    char cur[UC_NAME];
    long i = 0;
    int lineno = 0, rows = 0;
    if (dropped)
        *dropped = 0;
    if (!buf || len <= 0)
        return 0;
    uc_copy(cur, "default", UC_NAME);
    while (i < len) {
        long n = 0;
        char *s = line;
        int whole;
        lineno++;
        while (i < len && buf[i] != '\n' && n < (long)sizeof(line) - 1)
            line[n++] = buf[i++];
        line[n] = 0;
        whole = i >= len || buf[i] == '\n';
        while (i < len && buf[i] != '\n')
            i++; /* a line too long for the buffer is dropped whole, as upconf_parse does */
        if (i < len)
            i++;
        while (*s == ' ' || *s == '\t')
            s++;
        if (!*s || *s == ';' || *s == '#')
            continue;
        if (!whole) {
            if (dropped)
                (*dropped)++;
            continue;
        }
        while (n > 0 && (line[n - 1] == ' ' || line[n - 1] == '\t' || line[n - 1] == '\r'))
            line[--n] = 0;
        if (*s == '[') {
            char *name = uc_section_name(s);
            if (name)
                uc_copy(cur, *name ? name : "default", UC_NAME);
        } else {
            char *value;
            char *key = uc_key_value(s, &value);
            if (key && uc_ieq(cur, section)) {
                fn(ud, lineno, key, value);
                rows++;
            }
        }
    }
    return rows;
}

static int menu_style_row(void *ud, int lineno, const char *key, const char *value)
{
    if (uc_ieq(key, "menu-style")) {
        if (uc_ieq(value, "text"))
            *(int *)ud = UC_MENU_TEXT;
        else
            *(int *)ud = UC_MENU_GRAPHICAL;
    }
    /* never stops the walk (line numbers start at 1); reads lineno because vbcc (-warnings-as-errors) refuses a
     * (void) cast of it and the host build (-Wextra -Werror) refuses an unused parameter */
    return lineno < 0;
}

int upconf_menu_style(const char *buf, long len)
{
    int style = UC_MENU_GRAPHICAL;
    upconf_rows(buf, len, "default", menu_style_row, &style, 0);
    return style;
}
