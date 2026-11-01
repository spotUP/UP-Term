/* Link stubs for render/vtwin.c in the render host binary (tests/test_render.c: the vtwin mouse tests).
 * vtwin.c's non-mouse halves reach exec, dos, timer, console, diskfont and the clipboard; the mouse
 * tests never call them. No NDK header here: the names are all the linker asks for. Anything the tests
 * did reach would return 0 and fail its check, never pass by accident. */
long AbortIO(void) { return 0; }
long BeginRefresh(void) { return 0; }
long ChangeWindowBox(void) { return 0; }
long CheckIO(void) { return 0; }
long clip_read(void) { return 0; }
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
