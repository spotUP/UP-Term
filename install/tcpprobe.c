/* tcpprobe: is a bsdsocket.library available right now, and what is it?
 *
 * The Install's AmiNetXDuo part (dist/common/tcpstack.installer) runs it before it offers a TCP/IP
 * stack: a stack that is already running, or an emulator's own bsdsocket.library (FS-UAE with
 * bsdsocket_library = 1 puts one in memory, with no file in LIBS:), shows only to OpenLibrary.
 * Prints the library's id string (e.g. "bsdsocket.library 4.364 (UAE)") on stdout and returns 0;
 * returns 5 and prints nothing when the library cannot be opened. Opening a stack that starts on
 * demand (AmiTCP) may start it, which is harmless: it is installed. Not a user command. */
#include <exec/libraries.h>
#include <proto/exec.h>
#include <stdio.h>

int main(void)
{
    struct Library *b = OpenLibrary((STRPTR)"bsdsocket.library", 0);
    if (!b)
        return 5;
    printf("%s\n", b->lib_IdString ? (const char *)b->lib_IdString : "bsdsocket.library");
    CloseLibrary(b);
    return 0;
}
