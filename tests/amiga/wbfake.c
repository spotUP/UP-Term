/* wbfake -- a fake app for the Amarchy app index rig case (plan 2026-10-23-amarchy-app-index.md, X9).
 * Started as Workbench starts a tool (no console): writes RAM:x9.<name>.started, waits two seconds,
 * writes RAM:x9.<name>.ended and ends. <name> is the program's own file name, so one binary serves as
 * several fake apps. Plain vbcc, no ixemul. */
#include <dos/dos.h>
#include <dos/dosextens.h>
#include <workbench/startup.h>
#include <proto/dos.h>
#include <string.h>

static void mark(const char *name, const char *what)
{
    char path[120];
    BPTR f;
    strcpy(path, "RAM:x9.");
    strcat(path, name);
    strcat(path, what);
    if ((f = Open((STRPTR)path, MODE_NEWFILE))) {
        Write(f, (APTR)"run\n", 4);
        Close(f);
    }
}

int main(int argc, char **argv)
{
    const char *name = "app";
    if (argc == 0 && argv) {                 /* from Workbench: argv is the WBStartup */
        struct WBStartup *wb = (struct WBStartup *)argv;
        if (wb->sm_NumArgs > 0 && wb->sm_ArgList[0].wa_Name)
            name = (const char *)wb->sm_ArgList[0].wa_Name;
    } else if (argc > 1)
        name = argv[1];
    mark(name, ".started");
    Delay(100);
    mark(name, ".ended");
    return 0;
}
