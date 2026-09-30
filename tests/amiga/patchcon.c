/* patchcon -- D4.3 of the console.device plan, test only: SetFunction the ROM
 * console.device's RawKeyConvert (-48) to a stub, so UPConsole DEVICE ON has
 * a patch to refuse (DD21); Ctrl-C puts the ROM's vector back and ends.
 * While it runs, keys typed into console windows convert to nothing. */
#include <stdio.h>
#include <exec/execbase.h>
#include <exec/libraries.h>
#include <dos/dos.h>
#include <proto/exec.h>

extern struct ExecBase *SysBase;

static LONG stub(void)
{
    return 0;
}

int main(void)
{
    struct Library *con;
    APTR old;
    Forbid();
    con = (struct Library *)FindName(&SysBase->DeviceList, (STRPTR)"console.device");
    Permit();
    if (!con)
        return 20;
    old = SetFunction(con, -48, (APTR)stub);
    printf("patched RawKeyConvert, waiting for Ctrl-C\n");
    fflush(stdout);
    Wait(SIGBREAKF_CTRL_C);
    SetFunction(con, -48, old);
    printf("restored\n");
    return 0;
}
