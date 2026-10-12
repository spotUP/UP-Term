/* theme -- see theme.h. Colours are the 16 ANSI ones only: 30-37 / 90-97
 * foregrounds, 40-47 backgrounds. Light themes trade the bright yellow and
 * cyan (unreadable on white) for their dark halves; the colour-blind ones
 * draw a diff in blue and orange-ish yellow instead of green and red. */
#include <string.h>
#include "theme.h"
#include "json.h"
#include "util.h"
#include "../view/hl_style.h"

const cl_theme cl_themes[THEME_COUNT] = {
    { "dark", "Dark mode", "\033[90m", "\033[33m", "\033[36m", "\033[33m", "\033[35m", "\033[36m", "\033[95m",
      "\033[94m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m", "ansi" },
    { "light", "Light mode", "\033[90m", "\033[31m", "\033[34m", "\033[31m", "\033[35m", "\033[34m", "\033[35m",
      "\033[34m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m", "ansi" },
    { "dark-daltonized", "Dark mode (colorblind-friendly)", "\033[90m", "\033[33m", "\033[36m", "\033[33m",
      "\033[35m", "\033[36m", "\033[95m", "\033[94m", "\033[34m", "\033[33m", "\033[97;44m", "\033[30;43m",
      "\033[2m", "ansi" },
    { "light-daltonized", "Light mode (colorblind-friendly)", "\033[90m", "\033[34m", "\033[34m", "\033[34m",
      "\033[35m", "\033[34m", "\033[35m", "\033[34m", "\033[34m", "\033[33m", "\033[97;44m", "\033[30;43m",
      "\033[2m", "ansi" },
    { "dark-ansi", "Dark mode (ANSI colors only)", "\033[90m", "\033[33m", "\033[36m", "\033[33m", "\033[35m",
      "\033[36m", "\033[95m", "\033[94m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m",
      "ansi" },
    { "light-ansi", "Light mode (ANSI colors only)", "\033[90m", "\033[31m", "\033[34m", "\033[31m", "\033[35m",
      "\033[34m", "\033[35m", "\033[34m", "\033[32m", "\033[31m", "\033[30;42m", "\033[97;41m", "\033[2m",
      "ansi" },
    { "monochrome", "Monochrome (no colour: 2- and 4-colour screens)", "", "\033[1m", "\033[1m", "\033[1m",
      "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1m", "\033[1;4m", "\033[7m", "\033[2m",
      "mono" }
};

const cl_theme *theme_get(const char *name)
{
    int i;
    for (i = 0; name && i < THEME_COUNT; i++)
        if (!strcmp(cl_themes[i].name, name))
            return &cl_themes[i];
    return &cl_themes[0];
}

const char *const theme_named_names[THEME_NAMED] = { "red", "blue", "green", "yellow", "purple", "orange", "pink",
                                                    "cyan" };
/* orange and pink have no ANSI colour of their own: the 256-colour cube's */
static const char *const named_sgr[THEME_NAMED] = { "\033[31m", "\033[34m", "\033[32m", "\033[33m", "\033[35m",
                                                   "\033[38;5;208m", "\033[38;5;205m", "\033[36m" };

const char *theme_named(const char *name)
{
    int i, k;
    for (i = 0; name && i < THEME_NAMED; i++) {
        for (k = 0; name[k] && (name[k] | 0x20) == theme_named_names[i][k]; k++)
            ;
        if (!name[k] && !theme_named_names[i][k])
            return named_sgr[i];
    }
    return 0;
}

int theme_index(const cl_theme *t)
{
    int i;
    for (i = 0; i < THEME_COUNT; i++)
        if (&cl_themes[i] == t)
            return i;
    return 0;
}

/* ---- pi's themes (theme.h) ---- */

const char *const pi_role_names[PR_COUNT] = {
    "accent", "border", "borderAccent", "borderMuted", "success", "error", "warning", "muted", "dim", "text",
    "thinkingText", "selectedBg", "scrollbarTrack", "scrollbarThumb", "searchMatchBg", "searchMatchText",
    "userMessageBg", "userMessageText", "customMessageBg", "customMessageText", "customMessageLabel",
    "toolPendingBg", "toolSuccessBg", "toolErrorBg", "toolTitle", "toolOutput", "mdHeading", "mdLink", "mdLinkUrl",
    "mdCode", "mdCodeBlock", "mdCodeBlockBorder", "mdQuote", "mdQuoteBorder", "mdHr", "mdListBullet",
    "toolDiffAdded", "toolDiffRemoved", "toolDiffContext", "syntaxComment", "syntaxKeyword", "syntaxFunction",
    "syntaxVariable", "syntaxString", "syntaxNumber", "syntaxType", "syntaxOperator", "syntaxPunctuation",
    "thinkingOff", "thinkingMinimal", "thinkingLow", "thinkingMedium", "thinkingHigh", "thinkingXhigh",
    "thinkingMax", "bashMode"
};

int pitheme_role(const char *name)
{
    int i;
    for (i = 0; i < PR_COUNT; i++)
        if (!strcmp(name, pi_role_names[i]))
            return i;
    return -1;
}

/* panels: a background role (the others are foregrounds) */
static int is_bg(int r)
{
    return r == PR_SELECTED_BG || r == PR_SEARCH_MATCH_BG || r == PR_USER_MESSAGE_BG || r == PR_CUSTOM_MESSAGE_BG ||
           r == PR_TOOL_PENDING_BG || r == PR_TOOL_SUCCESS_BG || r == PR_TOOL_ERROR_BG;
}

/* system-theme.js TOKEN_FAMILIES and FAMILIES' ANSI slots (pi f10993bc, 1.0.4's dist copy); -1: a neutral role.
 * Order of pi_role_names. */
static const signed char system_slot[PR_COUNT] = {
    5, 4, 5, -1, 2, 1, 3, -1, -1, -1,           /* accent border borderAccent borderMuted success error warning muted dim text */
    -1, 4, -1, -1, 3, -1, 4, -1, 5, -1,         /* thinkingText selectedBg scrollbar* searchMatch* userMessage* customMessageBg/Text */
    5, -1, 2, 1, -1, -1, 3, 4, -1, 5,           /* customMessageLabel toolPendingBg toolSuccessBg toolErrorBg toolTitle toolOutput mdHeading mdLink mdLinkUrl mdCode */
    2, -1, -1, -1, -1, 5, 2, 1, -1, -1,         /* mdCodeBlock mdCodeBlockBorder mdQuote mdQuoteBorder mdHr mdListBullet diffAdded diffRemoved diffContext syntaxComment */
    4, 3, 6, 2, 5, 5, -1, -1, -1, 4,            /* keyword function variable string number type operator punctuation thinkingOff thinkingMinimal */
    4, 6, 5, 13, 1, 2                           /* thinkingLow Medium High Xhigh Max bashMode */
};

void pitheme_system(cl_pitheme *t)
{
    int i;
    memset(t, 0, sizeof(*t));
    cl_copy(t->name, "system", sizeof(t->name));
    t->depth = 16;
    for (i = 0; i < PR_COUNT; i++) {
        t->color[i] = system_slot[i] >= 0 && !is_bg(i) ? system_slot[i] : HL_DEFAULT;
        /* neutral roles below the body text are faint instead of bright black (system-theme.js indexedColors) */
        t->dim[i] = system_slot[i] < 0 && !is_bg(i) && i != PR_TEXT && i != PR_USER_MESSAGE_TEXT && i != PR_TOOL_TITLE;
    }
}

static int hexd(int c)
{
    return c >= '0' && c <= '9' ? c - '0' : c >= 'a' && c <= 'f' ? c - 'a' + 10 : c >= 'A' && c <= 'F' ? c - 'A' + 10 : -1;
}

/* one colour spelling to a pitheme colour: 0 and *out, -1 invalid. vars: the theme's "vars" object (J_BAD none) */
static int pi_color(jv v, jv vars, pitheme_ext ext, long *out, int depth, char *bad, int badcap)
{
    char s[96];
    int r, g, b, k;
    if (json_type(v) == J_NUM) {
        long n = json_long(v, -1);
        if (n < 0 || n > 255) {
            cl_copy(bad, "ANSI color index must be an integer from 0 to 255", badcap);
            return -1;
        }
        *out = n;
        return 0;
    }
    if (json_type(v) != J_STR) {
        cl_copy(bad, "expected a string or a number", badcap);
        return -1;
    }
    json_str(v, s, sizeof(s));
    if (!s[0]) {
        *out = HL_DEFAULT;
        return 0;
    }
    if (s[0] == '#') {
        int d[6], n = (int)strlen(s) - 1;
        if (n != 3 && n != 6) {
            cl_copy(bad, "Invalid color value: ", badcap);
            cl_cat(bad, s, badcap);
            return -1;
        }
        for (k = 0; k < n; k++)
            if ((d[k] = hexd(s[1 + k])) < 0) {
                cl_copy(bad, "Invalid color value: ", badcap);
                cl_cat(bad, s, badcap);
                return -1;
            }
        if (n == 3) {
            r = d[0] * 17;
            g = d[1] * 17;
            b = d[2] * 17;
        } else {
            r = d[0] * 16 + d[1];
            g = d[2] * 16 + d[3];
            b = d[4] * 16 + d[5];
        }
        *out = HL_RGB(r, g, b);
        return 0;
    }
    if (ext && ext(s, &r, &g, &b) == 1) {
        *out = HL_RGB(r, g, b);
        return 0;
    }
    if (depth >= 8 || json_type(vars) != J_OBJ) {
        cl_copy(bad, depth >= 8 ? "Circular variable reference detected: " : "Variable reference not found: ", badcap);
        cl_cat(bad, s, badcap);
        return -1;
    }
    {
        jv ref;
        if (!json_get(vars, s, &ref)) {
            cl_copy(bad, "Variable reference not found: ", badcap);
            cl_cat(bad, s, badcap);
            return -1;
        }
        return pi_color(ref, vars, ext, out, depth + 1, bad, badcap);
    }
}

static const int pi_optional[][2] = {   /* role, the role it takes when omitted (themes.md) */
    { PR_SCROLLBAR_TRACK, PR_MUTED }, { PR_SCROLLBAR_THUMB, PR_TEXT }, { PR_THINKING_MAX, PR_THINKING_XHIGH },
    { PR_SEARCH_MATCH_BG, PR_SELECTED_BG }, { PR_SEARCH_MATCH_TEXT, PR_TEXT }
};

int pitheme_load(cl_pitheme *t, const char *json, long n, pitheme_ext ext, char *err, int errlen)
{
    jv root, name, vars, colors, v;
    int i, k, missing = 0;
    char bad[160], lab[64];
    unsigned char have[PR_COUNT];
    if (n >= 3 && (unsigned char)json[0] == 0xEF && (unsigned char)json[1] == 0xBB && (unsigned char)json[2] == 0xBF) {
        json += 3;
        n -= 3;
    }
    memset(have, 0, sizeof(have));
    memset(t, 0, sizeof(*t));
    t->depth = 16;
    if (json_parse(json, n, &root) || json_type(root) != J_OBJ) {
        cl_copy(err, "Failed to parse theme: not a JSON object", errlen);
        return -1;
    }
    lab[0] = 0;
    if (json_get(root, "name", &name) && json_type(name) == J_STR)
        json_str(name, lab, sizeof(lab));
    cl_copy(t->name, lab, sizeof(t->name));
    if (!json_get(root, "colors", &colors) || json_type(colors) != J_OBJ) {
        cl_copy(err, "Invalid theme \"", errlen);
        cl_cat(err, lab, errlen);
        cl_cat(err, "\": expected an object with a \"colors\" map.", errlen);
        return -1;
    }
    if (strchr(lab, '/')) {
        cl_copy(err, "Invalid theme name \"", errlen);
        cl_cat(err, lab, errlen);
        cl_cat(err, "\": theme names cannot contain \"/\" because it is reserved for automatic light/dark theme settings.", errlen);
        return -1;
    }
    if (!json_get(root, "vars", &vars)) {
        vars.p = 0;
        vars.n = 0;
    }
    for (i = 0; i < PR_COUNT; i++) {
        t->color[i] = HL_DEFAULT;
        if (!json_get(colors, pi_role_names[i], &v))
            continue;
        if (pi_color(v, vars, ext, &t->color[i], 0, bad, sizeof(bad))) {
            cl_copy(err, "Invalid theme \"", errlen);
            cl_cat(err, lab, errlen);
            cl_cat(err, "\": ", errlen);
            cl_cat(err, pi_role_names[i], errlen);
            cl_cat(err, ": ", errlen);
            cl_cat(err, bad, errlen);
            return -1;
        }
        have[i] = 1;
    }
    for (k = 0; k < (int)(sizeof(pi_optional) / sizeof(pi_optional[0])); k++)
        if (!have[pi_optional[k][0]]) {
            t->color[pi_optional[k][0]] = t->color[pi_optional[k][1]];
            t->dim[pi_optional[k][0]] = t->dim[pi_optional[k][1]];
            have[pi_optional[k][0]] = 2;
        }
    {
        /* pi lists the missing roles sorted by name */
        int order[PR_COUNT], j;
        for (i = 0; i < PR_COUNT; i++)
            if (!have[i]) {
                for (j = missing; j > 0 && strcmp(pi_role_names[order[j - 1]], pi_role_names[i]) > 0; j--)
                    order[j] = order[j - 1];
                order[j] = i;
                missing++;
            }
        if (missing) {
            cl_copy(err, "Invalid theme \"", errlen);
            cl_cat(err, lab, errlen);
            cl_cat(err, "\":\n\nMissing required color tokens:", errlen);
            for (j = 0; j < missing; j++) {
                cl_cat(err, "\n  - ", errlen);
                cl_cat(err, pi_role_names[order[j]], errlen);
            }
        }
    }
    if (missing) {
        cl_cat(err, "\n\nPlease add these colors to your theme's \"colors\" object.\nSee the built-in themes (dark.json, light.json) for reference values.", errlen);
        return -1;
    }
    return 0;
}

int pitheme_color(const cl_pitheme *t, long c, int bg, char *out)
{
    int n = 0, idx, r, g, b;
    char num[16];
    out[0] = 0;
    if (c == HL_DEFAULT)
        return 0;
    if (HL_IS_RGB(c)) {
        r = (int)((c >> 16) & 255);
        g = (int)((c >> 8) & 255);
        b = (int)(c & 255);
        idx = t->depth >= 256 ? hl_rgb_to_256(r, g, b) : hl_rgb_to_16(r, g, b);
    } else {
        idx = (int)c;
        if (idx >= 16 && t->depth < 256) {      /* an index of the cube on a 16-colour screen: its nearest ANSI colour */
            hl_index_rgb(idx, &r, &g, &b);
            idx = hl_rgb_to_16(r, g, b);
        }
    }
    strcpy(out, "\033[");
    n = 2;
    if (idx < 16) {
        cl_ltoa((idx < 8 ? (bg ? 40 : 30) + idx : (bg ? 100 : 90) + idx - 8), num);
        strcpy(out + n, num);
    } else {
        cl_ltoa(bg ? 48 : 38, num);
        strcpy(out + n, num);
        strcat(out + n, ";5;");
        cl_ltoa(idx, num);
        strcat(out + n, num);
    }
    strcat(out + n, "m");
    return (int)strlen(out);
}

int pitheme_sgr(const cl_pitheme *t, int role, char *out)
{
    int n = 0;
    out[0] = 0;
    if (t->dim[role]) {
        strcpy(out, "\033[2m");
        n = 4;
    }
    return n + pitheme_color(t, t->color[role], is_bg(role), out + n);
}

static void hl_role(const cl_pitheme *t, struct hl_theme *h, int cls, int role, unsigned attr)
{
    h->s[cls].fg = t->color[role];
    h->s[cls].bg = HL_DEFAULT;
    h->s[cls].attr = attr | (t->dim[role] ? HL_A_DIM : 0);
}

void pitheme_hl(const cl_pitheme *t, struct hl_theme *h)
{
    int i;
    for (i = 0; i < HL_NCLASS; i++) {
        h->s[i].fg = HL_DEFAULT;
        h->s[i].bg = HL_DEFAULT;
        h->s[i].attr = 0;
    }
    /* pi's markdown (modes/interactive/components/markdown) and its code highlighter's roles */
    hl_role(t, h, MD_H1, PR_MD_HEADING, HL_A_BOLD | HL_A_UNDER);
    for (i = MD_H2; i <= MD_H6; i++)
        hl_role(t, h, i, PR_MD_HEADING, HL_A_BOLD);
    hl_role(t, h, MD_QUOTE, PR_MD_QUOTE_BORDER, 0);
    hl_role(t, h, MD_QTEXT, PR_MD_QUOTE, HL_A_ITALIC);
    hl_role(t, h, MD_FENCE, PR_MD_CODE_BLOCK_BORDER, 0);
    hl_role(t, h, MD_BULLET, PR_MD_LIST_BULLET, 0);
    hl_role(t, h, MD_RULE, PR_MD_HR, 0);
    hl_role(t, h, MD_CODESPAN, PR_MD_CODE, 0);
    hl_role(t, h, MD_CODEBLOCK, PR_MD_CODE_BLOCK, 0);
    hl_role(t, h, MD_URL, PR_MD_LINK_URL, 0);
    hl_role(t, h, MD_TABLE, PR_TEXT, 0);
    hl_role(t, h, MD_TH, PR_TEXT, HL_A_BOLD);
    hl_role(t, h, HL_LINK, PR_MD_LINK, HL_A_UNDER);
    hl_role(t, h, HL_EMPH, PR_TEXT, HL_A_BOLD);
    hl_role(t, h, HL_COMMENT, PR_SYNTAX_COMMENT, 0);
    hl_role(t, h, HL_KEYWORD, PR_SYNTAX_KEYWORD, 0);
    hl_role(t, h, HL_PREPROC, PR_SYNTAX_KEYWORD, 0);
    /* highlight.js: C's int and void are keywords for pi, a language's built-ins (print, len) its "type" colour */
    hl_role(t, h, HL_TYPE, PR_SYNTAX_KEYWORD, 0);
    hl_role(t, h, HL_BUILTIN, PR_SYNTAX_TYPE, 0);
    hl_role(t, h, HL_FUNCTION, PR_SYNTAX_FUNCTION, 0);
    hl_role(t, h, HL_STRING, PR_SYNTAX_STRING, 0);
    hl_role(t, h, HL_ESCAPE, PR_SYNTAX_STRING, 0);
    hl_role(t, h, HL_NUMBER, PR_SYNTAX_NUMBER, 0);
    hl_role(t, h, HL_VARIABLE, PR_SYNTAX_VARIABLE, 0);
    hl_role(t, h, HL_ADDED, PR_TOOL_DIFF_ADDED, 0);
    hl_role(t, h, HL_REMOVED, PR_TOOL_DIFF_REMOVED, 0);
}
