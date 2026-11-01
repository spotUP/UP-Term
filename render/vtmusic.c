/* vtmusic.c: ANSI music through audio.device (a port of DCTelnet's sound.c).
 *
 * One square wave in chip memory, one channel allocated at a low priority
 * (another program's sound wins), and a fixed pool of IOAudio writes that
 * queue on that channel. Nothing here waits for a note: finished writes
 * come back on the port and are taken back on the next play. */
#include "vtmusic.h"
#include <string.h>
#include <exec/memory.h>
#include <exec/io.h>
#include <devices/audio.h>
#include <proto/exec.h>
#include <clib/alib_protos.h>

#define VM_WAVE 16
#define VM_POOL 256
#define VM_VOLUME 48

static BYTE *vm_wave;
static struct MsgPort *vm_port;
static struct IOAudio *vm_master;
static struct IOAudio *vm_pool;
static UBYTE vm_busy[VM_POOL];
static int vm_open, vm_failed;

static void vm_reclaim(void)
{
    struct Message *m;
    while ((m = GetMsg(vm_port)) != NULL) {
        long i = (struct IOAudio *)m - vm_pool;
        if (i >= 0 && i < VM_POOL)
            vm_busy[i] = 0;
    }
}

static int vm_start(void)
{
    static UBYTE chans[4] = { 1, 2, 4, 8 };
    int i;
    if (vm_open)
        return 1;
    if (vm_failed)
        return 0;
    vm_failed = 1; /* one try: no device, no music, no retry per string */
    vm_wave = (BYTE *)AllocMem(VM_WAVE, MEMF_CHIP);
    vm_pool = (struct IOAudio *)AllocMem(sizeof(struct IOAudio) * VM_POOL, MEMF_PUBLIC | MEMF_CLEAR);
    vm_port = CreateMsgPort();
    if (vm_port)
        vm_master = (struct IOAudio *)CreateIORequest(vm_port, sizeof(struct IOAudio));
    if (!vm_wave || !vm_pool || !vm_master)
        goto fail;
    for (i = 0; i < VM_WAVE; i++)
        vm_wave[i] = (BYTE)(i < VM_WAVE / 2 ? 100 : -100);
    vm_master->ioa_Request.io_Message.mn_Node.ln_Pri = -40;
    vm_master->ioa_Request.io_Flags = ADIOF_NOWAIT;
    vm_master->ioa_Data = chans;
    vm_master->ioa_Length = sizeof(chans);
    if (OpenDevice((STRPTR)AUDIONAME, 0, (struct IORequest *)vm_master, 0) != 0)
        goto fail;
    memset(vm_busy, 0, sizeof(vm_busy));
    vm_open = 1;
    vm_failed = 0;
    return 1;
fail:
    if (vm_master)
        DeleteIORequest((struct IORequest *)vm_master);
    if (vm_port)
        DeleteMsgPort(vm_port);
    if (vm_pool)
        FreeMem(vm_pool, sizeof(struct IOAudio) * VM_POOL);
    if (vm_wave)
        FreeMem(vm_wave, VM_WAVE);
    vm_master = NULL;
    vm_port = NULL;
    vm_pool = NULL;
    vm_wave = NULL;
    return 0;
}

/* One write of ms at chz (0: silence at A 440). 0 when the pool is full. */
static int vm_write(vt_u32 chz, vt_u32 ms, int volume)
{
    struct IOAudio *a;
    vt_u32 hz, cycles;
    int i;
    for (i = 0; i < VM_POOL && vm_busy[i]; i++)
        ;
    if (i == VM_POOL)
        return 0;
    if (!chz) {
        chz = 44000;
        volume = 0;
    }
    hz = chz / 100;
    cycles = hz * ms / 1000; /* 3951 * 65535 fits */
    if (cycles == 0)
        cycles = 1; /* 0 would loop for ever */
    if (cycles > 65535UL)
        cycles = 65535UL;
    a = &vm_pool[i];
    *a = *vm_master;
    a->ioa_Request.io_Message.mn_ReplyPort = vm_port;
    a->ioa_Request.io_Command = CMD_WRITE;
    a->ioa_Request.io_Flags = ADIOF_PERVOL;
    a->ioa_Data = (UBYTE *)vm_wave;
    a->ioa_Length = VM_WAVE;
    a->ioa_Period = vt_note_period(chz, VM_WAVE);
    a->ioa_Volume = (UWORD)volume;
    a->ioa_Cycles = (UWORD)cycles;
    vm_busy[i] = 1;
    BeginIO((struct IORequest *)a);
    return 1;
}

void vtmusic_play(const vt_note *notes, int n, int background)
{
    int i;
    /* MF and MB both queue: the console never blocks (the flag only
     * says the sender expects that) */
    if (n <= 0 || background < 0 || !vm_start())
        return;
    vm_reclaim();
    for (i = 0; i < n; i++) {
        const vt_note *nt = &notes[i];
        if (nt->on_ms && !vm_write(nt->chz, nt->on_ms, VM_VOLUME))
            return;
        if (nt->ms > nt->on_ms && !vm_write(0, (vt_u32)nt->ms - nt->on_ms, 0))
            return;
    }
}

void vtmusic_close(void)
{
    int i;
    if (!vm_open)
        return;
    for (i = 0; i < VM_POOL; i++)
        if (vm_busy[i]) {
            AbortIO((struct IORequest *)&vm_pool[i]);
            WaitIO((struct IORequest *)&vm_pool[i]);
            vm_busy[i] = 0;
        }
    CloseDevice((struct IORequest *)vm_master);
    DeleteIORequest((struct IORequest *)vm_master);
    DeleteMsgPort(vm_port);
    FreeMem(vm_pool, sizeof(struct IOAudio) * VM_POOL);
    FreeMem(vm_wave, VM_WAVE);
    vm_master = NULL;
    vm_port = NULL;
    vm_pool = NULL;
    vm_wave = NULL;
    vm_open = 0;
}
