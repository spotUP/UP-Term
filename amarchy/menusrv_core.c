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

int ms_open(const ms_ops *ops, const ms_req *r)
{
    int found = 0, alive = 0, outstanding = 0, route;
    ops->find(ops->ctx, &found, &alive, &outstanding);
    if (mo_verdict(found, alive) == MO_TAKE_OVER && ops->take_over)
        ops->take_over(ops->ctx);
    route = ms_route(found, alive, outstanding);
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
        }
    }
    return MS_SPAWN_RESTART;
}
