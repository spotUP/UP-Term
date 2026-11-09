/* RIP palette (RIP-3): the 64-colour EGA master set and the 16-entry live
 * palette that the `Q` (set all) and `a` (set one) commands change. Standalone:
 * libc-free. The canvas palette entries 0..15 follow the master colours, so a
 * palette command recolours every pixel already drawn (a live palette).
 * Default indices follow RIP-0 closure: 6 is brown (master 0x14, as SyncTERM and
 * the colour names), not the table in the document. D15: behaviour reference only. */
#include "rip.h"

static const unsigned char default_master[RIP_NPAL] = {
    0, 1, 2, 3, 4, 5, 20, 7, 56, 57, 58, 59, 60, 61, 62, 63
};

unsigned long rip_ega_rgb(int m)
{
    static const unsigned char hi[3] = { 4, 2, 1 }; /* R, G, B high-intensity bits */
    unsigned long rgb = 0;
    int k;
    m &= 63;
    for (k = 0; k < 3; k++) {
        int v = ((m & hi[k]) ? 0xAA : 0) + ((m & (hi[k] << 3)) ? 0x55 : 0);
        rgb = (rgb << 8) | (unsigned long)v;
    }
    return rgb;
}

void rip_pal_default(unsigned char master[RIP_NPAL])
{
    int i;
    for (i = 0; i < RIP_NPAL; i++)
        master[i] = default_master[i];
}

static void apply(struct rip_pal *p, struct cv *c, int i)
{
    if (c)
        cv_set_pal(c, i, rip_ega_rgb(p->master[i]));
}

void rip_pal_reset(struct rip_pal *p, struct cv *c)
{
    int i;
    rip_pal_default(p->master);
    for (i = 0; i < RIP_NPAL; i++)
        apply(p, c, i);
}

int rip_pal_exec(struct rip_pal *p, struct cv *c, const struct rip_cmd *cmd)
{
    int i;
    if (cmd->level != 0)
        return 0;
    if (cmd->ch == 'Q' && cmd->nargs == RIP_NPAL) {
        for (i = 0; i < RIP_NPAL; i++)
            p->master[i] = (unsigned char)(cmd->arg[i] & 63);
        for (i = 0; i < RIP_NPAL; i++)
            apply(p, c, i);
        return 1;
    }
    if (cmd->ch == 'a' && cmd->nargs == 2) {
        i = (int)cmd->arg[0];
        if (i < 0 || i >= RIP_NPAL)
            return 0;
        p->master[i] = (unsigned char)(cmd->arg[1] & 63);
        apply(p, c, i);
        return 1;
    }
    return 0;
}
