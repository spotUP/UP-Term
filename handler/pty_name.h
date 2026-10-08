/* The names PTY: opens and whether an open may go ahead (pty-handler),
 * apart from AmigaOS so the host tests them (tests/test_pty_name.c).
 *
 *   PTY:<id>/m  a pair's master (once per id), PTY:<id>/s one of its slaves
 *   PTY:<id>/w  a pipe's write end, PTY:<id>/r its read end
 *   "*", "CONSOLE:"  the pair whose port the open arrived at
 *
 * A pipe is a pair without the line discipline: what its writers write
 * its readers read, a Read answers with what the pipe holds (at least one
 * byte, or 0 when every writer has closed) -- PIPE: (Queue-Handler) holds
 * the Read until the whole length is there, and a coproc running cat never
 * got its line (vsh's pipes, shell/vsh.c os_pipe). MODE_NEWFILE on /w makes
 * the pipe and is refused for a taken id (the opener tries the next one). */
#ifndef PTY_NAME_H
#define PTY_NAME_H

#define PN_ID_MAX 16 /* id characters + 1 */

#define PN_BAD -1       /* not a PTY: name */
#define PN_PORT 0       /* "*" or CONSOLE: */
#define PN_MASTER 1
#define PN_SLAVE 2
#define PN_PIPE_W 3
#define PN_PIPE_R 4

#define PN_OPEN 0       /* the existing pair or pipe */
#define PN_NEW 1        /* a new pair or pipe under that id */
#define PN_IN_USE 2     /* ERROR_OBJECT_IN_USE */
#define PN_NOT_FOUND 3  /* ERROR_OBJECT_NOT_FOUND */

static int pn_id_eq(const char *a, const char *b)
{
    for (; *a && *b; a++, b++) {
        char x = *a, y = *b;
        if (x >= 'A' && x <= 'Z')
            x += 32;
        if (y >= 'A' && y <= 'Z')
            y += 32;
        if (x != y)
            return 0;
    }
    return *a == *b;
}

/* The kind of a name given as a BCPL string (length byte first); the id
 * goes to id (PN_ID_MAX bytes) for the kinds that have one. */
static int pn_parse(const unsigned char *b, char *id)
{
    char name[256];
    int i, n = b ? b[0] : 0, colon = -1, slash = -1, start, len;
    for (i = 0; i < n && i < 255; i++) {
        name[i] = (char)b[i + 1];
        if (colon < 0 && name[i] == ':')
            colon = i;
    }
    name[i] = 0;
    if ((name[0] == '*' && !name[1]) || pn_id_eq(name, "CONSOLE:"))
        return PN_PORT;
    start = colon + 1;
    for (i = start; name[i]; i++)
        if (name[i] == '/')
            slash = i;
    if (slash < 0 || !name[slash + 1] || name[slash + 2])
        return PN_BAD;
    len = slash - start;
    if (len < 1 || len >= PN_ID_MAX)
        return PN_BAD;
    for (i = 0; i < len; i++) {
        id[i] = name[start + i];
        if (id[i] == '/')
            return PN_BAD;
    }
    id[len] = 0;
    switch (name[slash + 1] | 32) {
    case 'm':
        return PN_MASTER;
    case 's':
        return PN_SLAVE;
    case 'w':
        return PN_PIPE_W;
    case 'r':
        return PN_PIPE_R;
    }
    return PN_BAD;
}

/* An open of kind under an id that exists (a pipe or not, hung up or not);
 * create: MODE_NEWFILE (ACTION_FINDOUTPUT). */
static int pn_open_rule(int kind, int exists, int is_pipe, int hung_up, int create)
{
    switch (kind) {
    case PN_MASTER:
        return exists ? PN_IN_USE : PN_NEW;
    case PN_SLAVE:
        return exists && !is_pipe && !hung_up ? PN_OPEN : PN_NOT_FOUND;
    case PN_PIPE_W:
        if (create)
            return exists ? PN_IN_USE : PN_NEW;
        return exists && is_pipe ? PN_OPEN : PN_NOT_FOUND;
    case PN_PIPE_R:
        return exists && is_pipe ? PN_OPEN : PN_NOT_FOUND;
    }
    return PN_NOT_FOUND;
}

#endif
