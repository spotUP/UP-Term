#ifndef VSH_QUOTE_H
#define VSH_QUOTE_H

/* One word as an AmigaDOS item, the one place the escapes live (vsh_command_line and the AI agent tab's
 * task share it, no malloc): with `quote` it is wrapped in double quotes with " and * escaped by * and a
 * newline written *N; without, it is copied as it is (a newline still *N). Writes at o, returns the
 * end; room for 2 * strlen(a) + 2 bytes. */
static char *vsh_put_word(char *o, const char *a, int quote)
{
    if (quote)
        *o++ = '"';
    for (; *a; a++) {
        if (quote && (*a == '"' || *a == '*' || *a == '\n'))
            *o++ = '*';
        *o++ = *a == '\n' ? 'N' : *a;
    }
    if (quote)
        *o++ = '"';
    return o;
}

#endif
