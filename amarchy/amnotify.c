/* amnotify: post a notice to Amarchy (roadmap 2026-10-29-amarchy-p4-capture-notify-power.md R4.10).
 *
 *   amnotify TITLE [BODY] [SECONDS n] [BODYFILE f]       amnotify -t 3 -f T:out.txt Done "build ok"
 *
 * Both the AmigaDOS keywords and the Unix options are read by note_cli_parse (note_core.c, host-tested). The line goes
 * to the port AMARCHY.NOTIFY through rexx_send. SECONDS 0 keeps the notice until it is clicked. BODYFILE puts the
 * file's first lines (newlines as spaces) in the body.
 * Return code: 0 sent, 5 Amarchy is not running, 10 bad arguments. `amnotify ?` (or -h, --help) prints this. */
#include <string.h>
#include <dos/dos.h>
#include <proto/dos.h>
#include "../tty/qmark.h"
#include "note.h"
#include "note_core.h"
#include "rexx_host.h"

static const char ver[] = "$VER: amnotify 1.0 (29.10.2026) (UP-Term)";

/* RULES.md rule 6: the stack the program asks for, and the cookie the Shell and Workbench read. */
long __stack = 8192;
const char amnotify_stack_cookie[] = "$STACK: 8192";

static int is_help(const char *a)
{
    while (*a == ' ' || *a == '\t')
        a++;
    return qmark_line(a) || (a[0] == '-' && ((a[1] == 'h' && (a[2] == 0 || a[2] == ' ' || a[2] == '\n')) || (a[1] == '-' && a[2] == 'h')));
}

int main(void)
{
    char title[NOTE_TITLE], body[NOTE_BODY], file[NOTE_ARG_BODYFILE], line[2 * NOTE_TITLE + 2 * NOTE_BODY + 16], reply[REXX_HOST_OUT];
    const char *args = (const char *)GetArgStr();
    long secs, rc;
    int perr;
    (void)ver;
    if (is_help(args)) {
        PutStr((STRPTR)"amnotify TITLE [BODY] [SECONDS n] [BODYFILE f]: show a notice from Amarchy (also -t n, -f f)\n"
                       "SECONDS 0 keeps it until clicked. Returns 0 sent, 5 Amarchy is not running, 10 bad arguments\n");
        return 0;
    }
    perr = note_cli_parse(args, title, body, &secs, file);
    if (perr != NOTE_OK) {
        PutStr((STRPTR)(perr == NOTE_ERR_SECS ? "amnotify: SECONDS is a number from 0 to 3600\n"
                                              : "amnotify: usage: amnotify TITLE [BODY] [SECONDS n] [BODYFILE f]\n"));
        return 10;
    }
    if (file[0]) {
        BPTR f = Open((STRPTR)file, MODE_OLDFILE);
        if (!f) {
            PutStr((STRPTR)"amnotify: cannot read the BODYFILE\n");
            return 10;
        }
        if (!body[0]) {
            LONG n = Read(f, body, NOTE_BODY - 1), i;
            if (n < 0)
                n = 0;
            body[n] = 0;
            for (i = 0; i < n; i++)
                if (body[i] == '\n' || body[i] == '\r' || body[i] == '\t')
                    body[i] = ' ';
        }
        Close(f);
    }
    strcpy(line, "NOTIFY ");
    if (note_line(title, body, secs, line + 7, (int)sizeof line - 7) < 0)
        return 10;
    if (!rexx_port_exists(NOTE_PORT)) {
        PutStr((STRPTR)"amnotify: Amarchy is not running (no port AMARCHY.NOTIFY)\n");
        return 5;
    }
    rc = rexx_send(NOTE_PORT, line, reply, sizeof reply);
    if (rc != 0) {
        PutStr((STRPTR)(rc < 0 ? "amnotify: Amarchy is not running (no port AMARCHY.NOTIFY)\n" : "amnotify: Amarchy refused the notice\n"));
        return rc < 0 ? 5 : 10;
    }
    return 0;
}
