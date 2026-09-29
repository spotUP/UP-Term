/* vtshow FILE SECONDS -- write FILE to the console this runs in, then read
 * and drop every byte that arrives until RAM:vtshow.stop appears (deleted
 * again on the way out) or SECONDS pass: the replies a replayed program's
 * queries draw from the terminal (DA, DSR, ...) go nowhere, instead of
 * into the Shell as typed commands. For tools/rig/vttest_rig.py, which
 * screenshots until the window stops changing, then writes the stop file. */
#include <stdlib.h>
#include <proto/dos.h>
#include <proto/exec.h>

int main(int argc, char **argv)
{
    static char buf[4096];
    BPTR in, fh;
    LONG n, secs, t;
    char c;
    if (argc < 3)
        return 20;
    secs = atol(argv[2]);
    in = Open((STRPTR)argv[1], MODE_OLDFILE);
    fh = Open((STRPTR)"*", MODE_OLDFILE);
    if (!in || !fh)
        return 20;
    SetMode(fh, 1); /* raw: the replies are read as they come, not as lines */
    while ((n = Read(in, buf, sizeof(buf))) > 0)
        Write(fh, buf, n);
    Close(in);
    for (t = 0; t < secs * 10; t++) {
        BPTR stop = Lock((STRPTR)"RAM:vtshow.stop", ACCESS_READ);
        if (stop) {
            UnLock(stop);
            DeleteFile((STRPTR)"RAM:vtshow.stop");
            break;
        }
        while (WaitForChar(fh, 100000))
            if (Read(fh, &c, 1) != 1)
                break;
    }
    SetMode(fh, 0);
    Close(fh);
    return 0;
}
