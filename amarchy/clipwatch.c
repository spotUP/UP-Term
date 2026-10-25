/* The commodity's clipboard history (amarchy/clipwatch.h). The hook runs on clipboard.device's task and may
 * only Signal; the reading happens in the commodity's own task when its signal comes. */
#include <string.h>
#include <exec/types.h>
#include <exec/memory.h>
#include <exec/tasks.h>
#include <devices/clipboard.h>
#include <utility/hooks.h>
#include <proto/exec.h>
#include <proto/dos.h>
#include "clipwatch.h"
#include "clips_core.h"
#include "../handler/clip.h"

static struct MsgPort *cw_port;
static struct IOClipReq *cw_io;
static struct Hook cw_hook;
static struct Task *cw_task;
static LONG cw_bit = -1;
static clip_ring cw_ring;
static int cw_loaded;

/* called by clipboard.device with a0 = hook, a2 = object, a1 = message; h_Data is the task to signal */
static ULONG cw_entry(__reg("a0") struct Hook *h, __reg("a2") void *obj, __reg("a1") void *msg)
{
    struct Task *t = (struct Task *)h->h_Data;
    (void)obj;
    (void)msg;
    if (t && cw_bit >= 0)
        Signal(t, 1UL << cw_bit);
    return 0;
}

int cw_start(void)
{
    cw_task = FindTask(0);
    cw_bit = AllocSignal(-1);
    if (cw_bit < 0)
        return 0;
    cw_port = CreateMsgPort();
    cw_io = cw_port ? (struct IOClipReq *)CreateIORequest(cw_port, sizeof(struct IOClipReq)) : 0;
    if (!cw_io || OpenDevice((STRPTR)"clipboard.device", PRIMARY_CLIP, (struct IORequest *)cw_io, 0)) {
        if (cw_io)
            DeleteIORequest((struct IORequest *)cw_io);
        cw_io = 0;
        cw_stop();
        return 0;
    }
    memset(&cw_hook, 0, sizeof cw_hook);
    cw_hook.h_Entry = (HOOKFUNC)cw_entry;
    cw_hook.h_Data = cw_task;
    cw_io->io_Command = CBD_CHANGEHOOK;
    cw_io->io_Data = (STRPTR)&cw_hook;
    cw_io->io_Length = 1;                       /* 1 adds the hook, 0 removes it */
    DoIO((struct IORequest *)cw_io);
    if (cw_io->io_Error) {
        CloseDevice((struct IORequest *)cw_io);
        DeleteIORequest((struct IORequest *)cw_io);
        cw_io = 0;
        cw_stop();
        return 0;
    }
    return 1;
}

unsigned long cw_sigmask(void)
{
    return cw_io && cw_bit >= 0 ? 1UL << cw_bit : 0;
}

int cw_changed(void)
{
    long len = 0;
    char *text;
    int changed = 0;
    if (!cw_loaded) {
        UnLock(CreateDir((STRPTR)"ENV:Amarchy"));  /* fails when the drawer exists, which is fine */
        clips_load_file(&cw_ring, CLIPS_PATH);
        cw_loaded = 1;
    }
    text = clip_read(&len);
    if (!text)
        return 0;
    if (clips_prepend(&cw_ring, text, len))
        changed = clips_save_file(&cw_ring, CLIPS_PATH);
    FreeVec(text);
    return changed;
}

void cw_stop(void)
{
    if (cw_io) {
        cw_io->io_Command = CBD_CHANGEHOOK;
        cw_io->io_Data = (STRPTR)&cw_hook;
        cw_io->io_Length = 0;
        DoIO((struct IORequest *)cw_io);
        CloseDevice((struct IORequest *)cw_io);
        DeleteIORequest((struct IORequest *)cw_io);
        cw_io = 0;
    }
    if (cw_port) {
        DeleteMsgPort(cw_port);
        cw_port = 0;
    }
    if (cw_bit >= 0) {
        FreeSignal(cw_bit);
        cw_bit = -1;
    }
    clips_free(&cw_ring);
    cw_loaded = 0;
}
