/* medshell -- DP5 of the console.device plan, the Shell's side: what the
 * V47 Shell writes back after a medium-mode report. mediumprobe showed the
 * V47 con-handler (KS 47.115) answer TAB after "ab" with the bytes
 * CSI "12;2;3U" "ab" and then take no input and no writes until the reader
 * answers in a way it did not find. Here the ROM Shell runs on a PTY: slave
 * (PTY: accepts SetMode 2, as raw), this program is the master: it feeds
 * the Shell the report bytes a con-handler would send and hex-dumps every
 * byte the Shell writes (lines "SHELL n: hex | text"). Cases: the report for
 * "ab" as measured, then "dir RAM:T" with the numbers scaled the same way
 * (length;length+1). Output also in RAM:medshell.log. */
#include <stdio.h>
#include <string.h>
#include <exec/types.h>
#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "probeout.h"

static void drain(BPTR m, int seconds, const char *tag)
{
    char buf[64], hex[3 * 64 + 1], text[65];
    LONG n, i;
    int idle = 0;
    while (idle < seconds * 4) {
        if (!WaitForChar(m, 250000)) {
            idle++;
            continue;
        }
        n = Read(m, buf, sizeof(buf));
        if (n <= 0)
            break;
        for (i = 0; i < n; i++) {
            sprintf(hex + 3 * i, "%02x ", (unsigned char)buf[i]);
            text[i] = (buf[i] >= 32 && buf[i] < 127) ? buf[i] : '.';
        }
        text[n] = 0;
        po_line("SHELL %s %ld: %s| %s\n", tag, n, hex, text);
    }
}

static void report(BPTR m, const char *line, const char *tag)
{
    char r[80];
    int len = (int)strlen(line);
    sprintf(r, "\x9b" "12;%d;%dU%s", len, len + 1, line);
    po_line("SENT %s: CSI 12;%d;%dU%s\n", tag, len, len + 1, line);
    Write(m, r, (LONG)strlen(r));
    drain(m, 3, tag);
}

int main(void)
{
    BPTR m, nil;
    po_start("RAM:medshell.log");
    m = Open((STRPTR)"PTY:md/m", MODE_READWRITE);
    if (!po_check(m != 0, "PTY:md/m master", 0))
        return po_end();
    nil = Open((STRPTR)"NIL:", MODE_NEWFILE);
    po_check(SystemTags((STRPTR)"NewShell PTY:md/s", SYS_Input, nil, SYS_Output, 0, SYS_Asynch, TRUE,
                        TAG_DONE) == 0, "NewShell on PTY:md/s", 0);
    drain(m, 4, "start");
    report(m, "ab", "tab-ab");
    report(m, "dir RAM:T", "tab-dir");
    Write(m, "endcli\r", 7);
    drain(m, 2, "end");
    Close(m);
    return po_end();
}
