/* event -- see event.h. */
#include <string.h>
#include "event.h"

void llm_event_init(llm_event *e, int type)
{
    memset(e, 0, sizeof(*e));
    e->type = type;
    e->index = -1;
    e->in = e->out = e->cache_r = e->cache_w = -1;
}
