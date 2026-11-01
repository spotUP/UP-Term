#include <dos/dos.h>
#include <dos/dostags.h>
#include <proto/dos.h>
#include <proto/exec.h>
#include <string.h>
/* leafprobe <variant> <command> : variants a..e, one variable each */
int main(void)
{
    const char *args = (const char *)GetArgStr();
    char v = args[0];
    const char *cmd = args + 2;
    BPTR w, o = 0;
    LONG rc;
    w = Open((STRPTR)"XCON:0/20/640/300/LP/CLOSE", MODE_NEWFILE);
    if (!w) return 20;
    switch (v) {
    case 'a': rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE, NP_StackSize, 65536, TAG_END); break;
    case 'b': o = w;
              rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, o, SYS_Asynch, TRUE, NP_StackSize, 65536, TAG_END); break;
    case 'c': o = w;
              rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, o, SYS_Asynch, TRUE, SYS_UserShell, TRUE, NP_StackSize, 65536, TAG_END); break;
    case 'd': rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Asynch, TRUE, SYS_UserShell, TRUE, NP_StackSize, 65536, TAG_END); break;
    case 'e': rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, w, SYS_Error, w, SYS_Asynch, TRUE, NP_StackSize, 65536, TAG_END); break;
    case 'f': rc = SystemTags((STRPTR)cmd, SYS_Input, w, SYS_Output, 0, SYS_Error, w, SYS_Asynch, TRUE, NP_StackSize, 65536, TAG_END); break;
    default: rc = -1;
    }
    return rc == -1 ? 10 : 0;
}
