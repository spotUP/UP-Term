/* Link stubs for render/vtwin.c in the render host binary (tests/test_render.c: the vtwin mouse tests).
 * vtwin.c's non-mouse halves reach exec, dos, timer, console, diskfont and the clipboard; the mouse
 * tests never call them. No NDK header here: the names are all the linker asks for. Anything the tests
 * did reach would return 0 and fail its check, never pass by accident. */
#include <stdlib.h>
#include <string.h>
long AbortIO(void) { return 0; }
long BeginRefresh(void) { return 0; }
long ChangeWindowBox(void) { return 0; }
long CheckIO(void) { return 0; }
/* the clipboard text the paste test sets (0: an empty clipboard); a copy, as clip_read hands one to FreeVec */
const char *stub_clip_text;
char *clip_read(long *n)
{
    long len;
    char *p;
    if (!stub_clip_text)
        return 0;
    len = (long)strlen(stub_clip_text);
    p = (char *)malloc((size_t)len + 1); /* FreeVec is free() in gfx_fake.c */
    memcpy(p, stub_clip_text, (size_t)len + 1);
    *n = len;
    return p;
}
long clip_write(void) { return 0; }
long CloseDevice(void) { return 0; }
long CloseFont(void) { return 0; }
long CopyMem(void) { return 0; }
long CreateIORequest(void) { return 0; }
long CreateMsgPort(void) { return 0; }
long CreateNewProcTags(void) { return 0; }
long DeleteIORequest(void) { return 0; }
long DeleteMsgPort(void) { return 0; }
long DisplayBeep(void) { return 0; }
long DoubleClick(void) { return 0; }
long EndRefresh(void) { return 0; }
long FindTask(void) { return 0; }
long GetDisplayInfoData(void) { return 0; }
long GetMsg(void) { return 0; }
long GetVPModeID(void) { return 0; }
long OpenDevice(void) { return 0; }
long OpenDiskFont(void) { return 0; }
long OpenFont(void) { return 0; }
long PutMsg(void) { return 0; }
long RawKeyConvert(void) { return 0; }
long ReplyMsg(void) { return 0; }
long ReportMouse(void) { return 0; }
long SendIO(void) { return 0; }
long SetWindowTitles(void) { return 0; }
long vo_close(void) { return 0; }
long vo_name(void) { return 0; }
long vo_open(void) { return 0; }
long vo_read(void) { return 0; }
long vo_set_aspect(void) { return 0; }
long WaitIO(void) { return 0; }
long WaitPort(void) { return 0; }
