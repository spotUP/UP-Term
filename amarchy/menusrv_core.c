/* The warm menu server's open decision: see menusrv_core.h. */
#include "menusrv_core.h"

int mo_verdict(int found, int alive)
{
    if (!found)
        return MO_NONE;
    return alive ? MO_SIGNAL : MO_TAKE_OVER;
}

int ms_route(int found, int alive, int outstanding)
{
    return (found && alive && !outstanding) ? MS_SEND : MS_SPAWN_RESTART;
}

int ms_binary_changed(long started_stamp, long disk_stamp)
{
    return started_stamp != 0 && disk_stamp != 0 && started_stamp != disk_stamp;
}

int ms_open(const ms_ops *ops, const ms_req *r)
{
    int found = 0, alive = 0, outstanding = 0, route;
    ops->find(ops->ctx, &found, &alive, &outstanding);
    if (mo_verdict(found, alive) == MO_TAKE_OVER && ops->take_over)
        ops->take_over(ops->ctx);
    route = ms_route(found, alive, outstanding);
    /* A live idle server running an older binary than the file on disk: this press opens a menu of its own
     * (loaded from disk, so new code), the old server ends and a new one starts from the new file. When it
     * will not end (busy) it is left, and the next press tries again. */
    if (route == MS_SEND && ops->stamp && ops->quit && ms_binary_changed(ops->st->bin_stamp, ops->stamp(ops->ctx))) {
        ops->spawn(ops->ctx, r);
        if (ops->quit(ops->ctx)) {
            ops->start_server(ops->ctx);
            ops->st->started = 1;
            ops->st->last_start = ops->now(ops->ctx);
            ops->st->bin_stamp = ops->stamp(ops->ctx);
        }
        return MS_SPAWN_RESTART;
    }
    if (route == MS_SEND) {
        ops->send(ops->ctx, r);
        return MS_SEND;
    }
    ops->spawn(ops->ctx, r);
    /* A wedged but live server is left alone (a second one would refuse the port); a missing or dead one
     * is restarted, throttled. */
    if (!(found && alive)) {
        long t = ops->now(ops->ctx);
        if (!ops->st->started || t - ops->st->last_start >= MS_RESTART_SECS) {
            ops->start_server(ops->ctx);
            ops->st->started = 1;
            ops->st->last_start = t;
            if (ops->stamp)
                ops->st->bin_stamp = ops->stamp(ops->ctx);
        }
    }
    return MS_SPAWN_RESTART;
}

void ms_server_names(int text, const char **port, const char **task)
{
    *port = text ? MS_PORT : GS_PORT;
    *task = text ? MS_TASK_NAME : GS_TASK_NAME;
}
