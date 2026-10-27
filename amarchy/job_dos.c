/* job_dos: see job_dos.h. */
#include <stdio.h>
#include <string.h>
#define timeval ndk_timeval            /* ixemul's <sys/time.h> owns these names */
#define timerequest ndk_timerequest
#include <dos/dos.h>
#include <dos/dostags.h>
#include <exec/ports.h>
#include <exec/io.h>
#include <devices/timer.h>
#include <proto/dos.h>
#include <proto/exec.h>
#undef timeval
#undef timerequest
#include "job_dos.h"
#include "leaf.h"
#include "proclaunch.h"
#define UPASSIGN_DOS
#include "../config/upassign.h"

#define JB_NAME "amarchy-job"
#define JB_TICK_MICROS 200000UL
#define JB_CHUNK 1024
#define JB_MARKER "; echo @JOBRC $?"

static struct {
    int active;
    char file[48], title[96];
    BPTR rd;                           /* the reader of the output file */
    struct MsgPort *port;
    struct ndk_timerequest *tr;
    int dev, busy;                     /* timer.device open, tick request in flight */
} jb;

static void tick_stop(void)
{
    if (jb.busy) {
        AbortIO((struct IORequest *)jb.tr);
        WaitIO((struct IORequest *)jb.tr);
        jb.busy = 0;
    }
}

static void tick_start(void)
{
    tick_stop();
    if (!jb.tr)
        return;
    jb.tr->tr_node.io_Command = TR_ADDREQUEST;
    jb.tr->tr_time.tv_secs = JB_TICK_MICROS / 1000000;
    jb.tr->tr_time.tv_micro = JB_TICK_MICROS % 1000000;
    SendIO((struct IORequest *)jb.tr);
    jb.busy = 1;
}

static void tick_close(void)
{
    tick_stop();
    if (jb.dev)
        CloseDevice((struct IORequest *)jb.tr);
    if (jb.tr)
        DeleteIORequest((struct IORequest *)jb.tr);
    if (jb.port)
        DeleteMsgPort(jb.port);
    jb.dev = 0;
    jb.tr = 0;
    jb.port = 0;
}

static int tick_open(void)
{
    jb.port = CreateMsgPort();
    if (jb.port)
        jb.tr = (struct ndk_timerequest *)CreateIORequest(jb.port, sizeof(struct ndk_timerequest));
    if (jb.tr && !OpenDevice((STRPTR)"timer.device", UNIT_VBLANK, (struct IORequest *)jb.tr, 0))
        jb.dev = 1;
    if (!jb.dev) {
        tick_close();
        return 0;
    }
    return 1;
}

int jb_start(jc *j, const char *cmd, const char *prog, const char *title)
{
    char full[512], line[640], pbuf[256];
    const char *vsh;
    BPTR in, out, path;
    LONG rc;
    if (jb.active || !cmd || !*cmd || strlen(cmd) + sizeof JB_MARKER >= sizeof full)
        return 0;
    jc_init(j);
    if (!(vsh = upassign_vsh()))
        return 0;                      /* the end marker needs vsh's $? */
    snprintf(full, sizeof full, "%s%s", cmd, JB_MARKER);
    if (!lf_shell_command(line, sizeof line, vsh, full))
        return 0;
    snprintf(jb.file, sizeof jb.file, "T:amarchy-job.%lx", (unsigned long)FindTask(0));
    snprintf(jb.title, sizeof jb.title, "%s", title ? title : "");
    DeleteFile((STRPTR)jb.file);
    /* MODE_READWRITE takes a shared lock: the reader below can open the file while the job writes */
    out = Open((STRPTR)jb.file, MODE_READWRITE);
    in = Open((STRPTR)"NIL:", MODE_OLDFILE);
    jb.rd = out ? Open((STRPTR)jb.file, MODE_OLDFILE) : 0;
    if (!out || !in || !jb.rd || !tick_open()) {
        if (jb.rd)
            Close(jb.rd);
        if (in)
            Close(in);
        if (out)
            Close(out);
        jb.rd = 0;
        DeleteFile((STRPTR)jb.file);
        return 0;
    }
    path = pl_dup_path();
    rc = SystemTags((STRPTR)line, SYS_Input, in, SYS_Output, out, SYS_Asynch, TRUE,
                    NP_Name, (ULONG)JB_NAME,
                    NP_StackSize, pl_stack(prog ? prog : (lf_command_program(line, pbuf, (int)sizeof pbuf) ? pbuf : 0),
                                           65536),
                    path ? NP_Path : TAG_IGNORE, path, TAG_END);
    if (rc == -1) {                    /* not started: the files and path are still ours */
        pl_free_path(path);
        Close(in);
        Close(out);
        Close(jb.rd);
        jb.rd = 0;
        tick_close();
        DeleteFile((STRPTR)jb.file);
        return 0;
    }
    jb.active = 1;
    tick_start();
    return 1;
}

ULONG jb_sigmask(void)
{
    return jb.active && jb.port ? 1UL << jb.port->mp_SigBit : 0;
}

void jb_poll(jc *j)
{
    char buf[JB_CHUNK];
    LONG n;
    if (!jb.active)
        return;
    if (jb.busy && GetMsg(jb.port)) {  /* the tick came: take it and arm the next */
        jb.busy = 0;
        tick_start();
    }
    while ((n = Read(jb.rd, buf, sizeof buf)) > 0)   /* the file offset of jb.rd is the position already read */
        jc_feed(j, buf, n);
}

void jb_cancel(jc *j)
{
    struct Task *t;
    if (!jb.active)
        return;
    Forbid();
    t = FindTask((STRPTR)JB_NAME);
    if (t)
        Signal(t, SIGBREAKF_CTRL_C);
    Permit();
    jc_cancel(j);
}

void jb_end(void)
{
    if (!jb.active)
        return;
    tick_close();
    if (jb.rd)
        Close(jb.rd);
    jb.rd = 0;
    DeleteFile((STRPTR)jb.file);
    jb.active = 0;
}

const char *jb_title(void)
{
    return jb.title;
}

/* Publishes n as the global variable name (a rig case reads it after the run). */
static void sentinel(const char *name, long n)
{
    char b[16];
    int i = 15;
    b[i] = 0;
    do {
        b[--i] = (char)('0' + n % 10);
        n /= 10;
    } while (n && i > 0);
    SetVar((STRPTR)name, (STRPTR)(b + i), -1, GVF_GLOBAL_ONLY);
}

static long lines_seen;   /* sentinel AMARCHY_JOB_LINES: lines appended in all jobs of this run */

void jb_poll_publish(jc *j)
{
    long total = j->total;
    int ended = j->ended;
    jb_poll(j);
    if (j->total != total) {
        lines_seen += j->total - total;
        sentinel("AMARCHY_JOB_LINES", lines_seen);
    }
    if (j->ended && !ended)
        sentinel("AMARCHY_JOB_DONE", j->rc + 1);
}
