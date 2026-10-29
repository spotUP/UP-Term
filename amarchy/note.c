/* note: the AMARCHY.NOTIFY port and the one queue instance (note.h). Every verb goes through note_verb, whether
 * the sender is the port, the NOTES verb on AMARCHY or later a menu row. */
#include "note.h"
#include "note_core.h"
#include "tick.h"
#include "rexx_host.h"

static struct rexx_host host;
static note_q queue;
static int inited;

/* Seconds since the Amiga epoch, the clock note_core is given (the commodity tick's clock). */
static long now_secs(void)
{
    return tick_epoch();
}

/* The commodity tick hook (tick_hook): notices whose time is up go. */
void note_expire(long tick_secs)
{
    (void)tick_secs;
    if (inited)
        note_tick(&queue, now_secs());
}

long note_run(const char *cmd, char *out)
{
    if (!inited) {
        note_init(&queue);
        inited = 1;
    }
    return note_verb(&queue, cmd, now_secs(), out, REXX_HOST_OUT);
}

static long port_cmd(void *ud, const char *cmd, char *out)
{
    (void)ud;
    return note_run(cmd, out);
}

void note_open(void)
{
    char scratch[REXX_HOST_OUT];
    note_run("COUNT", scratch);                 /* the queue exists before the first message */
    rexx_host_open(&host, NOTE_PORT);
}

unsigned long note_sigmask(void)
{
    return rexx_host_sigmask(&host);
}

void note_serve(void)
{
    rexx_host_serve(&host, port_cmd, 0);
}

void note_close(void)
{
    rexx_host_close(&host, port_cmd, 0);
}

long note_posted(void)
{
    return inited ? queue.posted : 0;
}
