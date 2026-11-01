/* C:UPTerm -- UP-Term's slash commands from a script or another program
 * (ledger C1): "UPTerm cursor bar" does in the window of its output what
 * "/cursor bar" typed at the prompt does, and prints the answer.
 *
 *   UPTerm COMMAND/F
 *   UPTerm help          the list
 *   UPTerm state [KEY]   what the user did in the window (counters; a KEY prints its bare value,
 *                        an unknown one returns 5 with a message): the tutorial's checks
 *   UPTerm screen FILE <path>   (also FILE=<path>) the window's visible rows as text, one line per
 *                        row, trailing blanks cut, into <path>: no selection, no clipboard
 *   UPTerm ACTIVE screen FILE <path>   the same for the focused UP-Term window, asked through its
 *                        request port (upreq.h) from a process that has no window: the rig's reader
 *
 * The command goes to the window of the output, or, when the output is
 * redirected (">NIL:", a file), to the window of the input, and with
 * neither on a window (a command substitution), to the process's console.
 *
 * Return code 0 done, 10 refused (the answer says why), 20 not an UP-Term
 * window or not a command. No ixemul, no C library beyond vbcc's startup. */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "vtcon_packets.h"
#include "../tty/qmark.h"
#include "upterm_version.h"
#include "upreq.h"
#include "upcmd.h"

/* the base version and build day (upterm_version.h, written by the Makefile) */
static const char vers[] = "$VER: UPTerm " UPTERM_VERSION " (" UPTERM_DATE ") UP-Term";

/* vbcc's startup runs main on a stack of this size whatever the starter gives
 * it; the cookie says the same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 16384;
const char upterm_stack_cookie[] = "$STACK: 16384";

static int lower(int ch)
{
    return ch >= 'A' && ch <= 'Z' ? ch + 32 : ch;
}

/* *p at the word `w` (any case) followed by a blank or the end: skip it and the blanks after it */
static int word_is(const char **p, const char *w)
{
    const char *s = *p;
    while (*w && lower(*s) == *w)
        s++, w++;
    if (*w || (*s && *s != ' '))
        return 0;
    while (*s == ' ')
        s++;
    *p = s;
    return 1;
}

/* "FILE <path>" or "FILE=<path>" (the AmigaDOS key form): the path, or 0 when the word is not FILE
 * or names none ("screen workbench" stays the screen setting) */
static const char *file_arg(const char **p)
{
    const char *s = *p;
    if (lower(s[0]) != 'f' || lower(s[1]) != 'i' || lower(s[2]) != 'l' || lower(s[3]) != 'e')
        return 0;
    s += 4;
    if (*s == '=')
        s++;
    else if (*s == ' ')
        while (*s == ' ')
            s++;
    else
        return 0;
    return *s ? s : 0;
}

/* The focused UP-Term window answers through its request port, the quick menu's path (upreq.h).
 * The sender has no UP-Term window of its own, so the focused window heads the list (self_in_mru 0). */
static int send_active(const char *line)
{
    upreq_body r;
    long rc;
    memset(&r, 0, sizeof r);
    r.version = UPREQ_VERSION;
    if (strlen(line) + 7 >= sizeof r.action) {
        PutStr((STRPTR)"UPTerm: line too long\n");
        return RETURN_FAIL;
    }
    strcpy(r.action, "slash:");
    strcat(r.action, line);
    rc = upreq_send(&r, 0);
    if (rc == UPREQ_OK)
        return RETURN_OK;
    PutStr((STRPTR)"UPTerm: ");
    PutStr((STRPTR)(r.answer[0] ? r.answer : rc == UPREQ_NOPORT ? "no UP-Term window to ask" : "refused"));
    PutStr((STRPTR)"\n");
    return rc == UPREQ_NOPORT ? RETURN_FAIL : RETURN_ERROR;
}

int main(void)
{
    LONG args[1] = { 0 };
    static const char tmpl[] = "COMMAND/F";
    struct RDArgs *rd;
    static char line[256], ans[4096];
    LONG r;
    int active;
    const char *a, *file = 0;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"UPTerm");
        return RETURN_FAIL;
    }
    a = args[0] ? (const char *)args[0] : "help";
    while (*a == ' ' || *a == '/')
        a++; /* "UPTerm /cursor bar" works too */
    active = word_is(&a, "active");
    line[0] = '/';
    if (word_is(&a, "screen") && (file = file_arg(&a))) {
        strcpy(line + 1, "screen-text ");
        strncat(line, file, sizeof(line) - 14);
    } else
        strncpy(line + 1, a, sizeof(line) - 2);
    line[sizeof(line) - 1] = 0;
    FreeArgs(rd);
    if (active)
        return send_active(line);
    /* the window of the output; redirected ("UPTerm find x >NIL:"), the
     * window of the input; neither ("x=$(UPTerm state tabs-opened)": vsh
     * gives a command substitution NIL: and a pipe), the console of the
     * process, "*" at pr_ConsoleTask -- the answer still goes to the output */
    r = upterm_window_command(line, ans, sizeof(ans));
    if (r == 0) {
        if (IoErr() == ERROR_ACTION_NOT_KNOWN)
            PutStr((STRPTR)"UPTerm: not an UP-Term window\n");
        else {
            PutStr((STRPTR)"UPTerm: no such command: ");
            PutStr((STRPTR)(line + 1));
            PutStr((STRPTR)" (UPTerm help lists them)\n");
        }
        return RETURN_FAIL;
    }
    PutStr((STRPTR)ans);
    return r == 1 ? RETURN_OK : RETURN_ERROR;
}
