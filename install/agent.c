/* Agent [TASK]: open the AI agent in a new tab of this UP-Term window (Omarchy's "ask AI").
 *
 * The agent is the ai-agent preference's (C:Claude or C:amiga-pi, /agent sets it); TASK is its first
 * message, so the tab starts working on it. Without TASK the agent opens empty, as Super + L does.
 * It sends "/ask TASK" to the window the program runs in -- the route C:UPTerm uses (handler/upcmd.h),
 * no new packet; the handler's /ask opens the tab with the same code as the menu item.
 * Return codes: 0 done, 5 the window refused it, 10 not an UP-Term window, 20 bad arguments. */
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <dos/rdargs.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "../handler/upcmd.h"
#include "../tty/qmark.h"
#include "upterm_version.h"

static const char vers[] = "$VER: Agent " UPTERM_VERSION " (" UPTERM_DATE ") UP-Term";

/* vbcc's startup runs main on a stack of this size whatever the starter gives it; the cookie says the
 * same to AmigaOS 3.2 and to vsh (RULES.md rule 6). */
long __stack = 16384;
const char agent_stack_cookie[] = "$STACK: 16384";

int main(void)
{
    LONG args[1] = { 0 };
    static const char tmpl[] = "TASK/F";
    struct RDArgs *rd;
    static char line[256], ans[512];
    LONG r;
    if (qmark_line((const char *)GetArgStr())) {
        PutStr((STRPTR)tmpl);
        PutStr((STRPTR)"\n");
        return RETURN_OK;
    }
    rd = ReadArgs((STRPTR)tmpl, args, 0);
    if (!rd) {
        PrintFault(IoErr(), (STRPTR)"Agent");
        return 20;
    }
    strcpy(line, "/ask");
    if (args[0] && *(const char *)args[0]) {
        strcat(line, " ");
        strncat(line, (const char *)args[0], sizeof(line) - 6);
    }
    FreeArgs(rd);
    r = upterm_window_command(line, ans, sizeof(ans));
    if (r == 0) {
        PutStr((STRPTR)(IoErr() == ERROR_ACTION_NOT_KNOWN ? "Agent: not an UP-Term window\n" : "Agent: the window refused it\n"));
        return IoErr() == ERROR_ACTION_NOT_KNOWN ? 10 : 5;
    }
    PutStr((STRPTR)ans);
    return r == 1 ? RETURN_OK : 5;
}
