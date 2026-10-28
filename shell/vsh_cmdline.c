/* vsh's argv -> AmigaDOS command line. Pure C (no Amiga headers) so a host test drives it. */
#include <stdlib.h>
#include <string.h>
#include "vsh_cmdline.h"
#include "vsh_quote.h"

/* Length of the leading keyword of `a` when it is KEY=...: non-empty, only letters, digits, `_`, `-`
 * and `/` (AmigaDOS template names, `-D=1`, `--opt=val`); 0 when `a` has no `=` or its prefix is not one. */
static long keyword_len(const char *a)
{
    long n = 0;
    for (; a[n] && a[n] != '='; n++) {
        char c = a[n];
        if (!((c >= 'a' && c <= 'z') || (c >= 'A' && c <= 'Z') || (c >= '0' && c <= '9') || c == '_' || c == '-' ||
              c == '/'))
            return 0;
    }
    return a[n] == '=' ? n : 0;
}

/* argv as an AmigaDOS command line: arguments with spaces, quotes,
 * semicolons, asterisks or newlines in double quotes, " and * escaped with * and a
 * newline written *N (AmigaDOS: ReadItem fails a raw newline inside quotes,
 * and an unquoted one ends the line, so `sed 'p<newline>p'` reached a native
 * or ixemul command cut after its first line).
 * ReadArgs matches a keyword only on an UNQUOTED item, so KEY=value keeps `KEY=` bare and quotes only
 * the value, the way AmigaShell sends it (`aget TO=T:t1 URL="https://x?a=1"`): quoting the whole word
 * made `"TO=T:t1"` a plain value and the command never saw its keyword. A word whose prefix before the
 * first `=` is no keyword (`a b=c`, `=x`) is one quoted item, `=` included. */
char *vsh_command_line(char **argv)
{
    long len = 1;
    int i;
    char *s, *o;
    for (i = 0; argv[i]; i++)
        len += 2 * (long)strlen(argv[i]) + 5;
    s = (char *)malloc(len);
    if (!s)
        return 0;
    o = s;
    for (i = 0; argv[i]; i++) {
        const char *a = argv[i];
        long kl = keyword_len(a);
        int quote;
        if (i)
            *o++ = ' ';
        if (kl) {
            memcpy(o, a, kl + 1);
            o += kl + 1;
            a += kl + 1;
        }
        quote = !*a || strpbrk(a, " \t\";*=\n") != 0;
        o = vsh_put_word(o, a, quote);
    }
    *o = 0;
    return s;
}
