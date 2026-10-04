/* hl_lex -- a small table-driven lexer for syntax highlighting (hl, and
 * mdv's code blocks). A language is a table (hl_langs.c): keyword lists,
 * comment and string delimiters, flags for the rules a family shares, and
 * a mode for the line-shaped formats (diff, markup, Markdown, 68k asm).
 * Text goes in a line at a time; what spans lines (block comments, long
 * strings, heredocs, an open tag, a diff hunk) lives in hl_state. No
 * regular expressions, no allocation per line: a keyword table per
 * language is hashed once. Portable C89. */
#ifndef HL_LEX_H
#define HL_LEX_H

#include "hl_style.h"

/* modes */
enum { HLM_CODE, HLM_ASM, HLM_DIFF, HLM_MARKUP, HLM_MARKDOWN };

/* flags */
#define HLF_NOCASE    0x00000001UL  /* keywords ignore case */
#define HLF_NEST1     0x00000002UL  /* the first block comment pair nests */
#define HLF_NEST2     0x00000004UL  /* the second one does */
#define HLF_CPP       0x00000008UL  /* '#' first on a line: a directive */
#define HLF_TRIPLE    0x00000010UL  /* """ and ''' strings (over lines) */
#define HLF_DOLLAR    0x00000020UL  /* $name ${...} $(...) $1 $@: variables */
#define HLF_FUNC      0x00000040UL  /* name( : a function */
#define HLF_HASHWORD  0x00000080UL  /* a '#' comment starts a word only */
#define HLF_STRPREFIX 0x00000100UL  /* r"" b'' f"" (Python), r"" (Rust) */
#define HLF_CHARLIT   0x00000200UL  /* 'x' a character, 'a a lifetime/label */
#define HLF_KEYSTR    0x00000400UL  /* "string": a key (JSON) */
#define HLF_LUALONG   0x00000800UL  /* [[ ]] [==[ ]==] strings, --[[ ]] comments */
#define HLF_HEXDOLLAR 0x00001000UL  /* $ff %0101 numbers (68k, AmigaE) */
#define HLF_AMIGADOS  0x00002000UL  /* <arg> {arg} $var, .KEY lines */
#define HLF_ATSIGN    0x00004000UL  /* @name: an annotation/decorator */
#define HLF_REGEX     0x00008000UL  /* /re/ after an operator (JavaScript) */
#define HLF_SECTION   0x00010000UL  /* [section] lines (ini, toml) */
#define HLF_KEYLINE   0x00020000UL  /* key = value / key: value lines */
#define HLF_MAKE      0x00040000UL  /* target: and NAME = lines */
#define HLF_QQ        0x00080000UL  /* a doubled quote is an escape */
#define HLF_MACROBANG 0x00100000UL  /* name! : a macro (Rust) */
#define HLF_CSS       0x00200000UL  /* property: inside braces, #fff, .class */
#define HLF_HEREDOC   0x00400000UL  /* <<WORD ... WORD */
#define HLF_YAML      0x00800000UL  /* "- " items, key: needs a blank after */
#define HLF_STARCMT   0x01000000UL  /* '*' first on a line: a comment */

typedef struct hl_lang {
    const char *name;       /* for -l and --list */
    const char *names;      /* other names (-l, Markdown fences), blank-separated */
    const char *exts;       /* file name extensions, lower case */
    const char *files;      /* whole file names, lower case */
    const char *interp;     /* #! interpreters */
    const char *kw[6];      /* keywords, keywords, types, types, builtins,
                             * builtins: blank-separated (two strings for
                             * each: C89 caps a literal at 509 bytes) */
    const char *lcomment[2];
    const char *bopen[2], *bclose[2];
    const char *quotes;     /* string delimiters */
    const char *mlquotes;   /* those that run over lines */
    const char *rawquotes;  /* those without escapes */
    char esc;               /* the escape character, 0 none */
    const char *identx;     /* characters names may hold beyond A-Z a-z 0-9 _ */
    const char *keysep;     /* HLF_KEYLINE separators */
    unsigned long flags;
    int mode;
} hl_lang;

typedef struct hl_state {
    const hl_lang *lang;
    int ctx;                /* what a line starts inside (hl_lex.c CTX_*) */
    int depth;              /* nested block comment depth */
    int aux, aux2;          /* quote / level / hunk counts */
    int braces;             /* CSS: inside a rule */
    int hdpend, hdstrip;    /* a heredoc starts on the next line; <<- */
    char hd[32];            /* its end word */
} hl_state;

typedef void (*hl_emit)(void *u, int cls, const char *s, long n);

/* Every language, in --list order; hl_nlangs of them. */
extern const hl_lang hl_langs[];
extern const int hl_nlangs;

void hl_begin(hl_state *st, const hl_lang *lang);
/* One line without its newline: emit gets tokens in order, adjacent text
 * of one class as one token, every byte exactly once. */
void hl_line(hl_state *st, const char *s, long n, hl_emit emit, void *u);
/* By name or other name, ignoring case ("c++", "py", "sh"); 0 unknown. */
const hl_lang *hl_find(const char *name, long len);
/* By file name (extension or whole name, a path is fine), else by the
 * first line (#! interpreter, <?xml, .KEY, diff); 0 when nothing fits. */
const hl_lang *hl_detect(const char *filename, const char *first, long firstlen);
/* frees the keyword tables (built on first use) */
void hl_cleanup(void);

#endif
