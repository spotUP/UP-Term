/* amarchy-flat core: see flat_core.h. */
#include <string.h>
#include <stdio.h>
#include "flat_core.h"

void flat_init(flat_state *s)
{
    memset(s, 0, sizeof *s);
    s->version = FLAT_VERSION;
    s->size = FLAT_STATE_SIZE;
}

int flat_check(const void *p, size_t avail)
{
    const flat_state *s = (const flat_state *)p;
    if (!p || avail < 4)
        return FLAT_ERR_SIZE;
    if (s->version != FLAT_VERSION)
        return FLAT_ERR_VERSION;
    if (s->size != FLAT_STATE_SIZE || avail < FLAT_STATE_SIZE)
        return FLAT_ERR_SIZE;
    return FLAT_OK;
}

const char *flat_status_word(const flat_state *s, int found)
{
    if (!found || !s)
        return "absent";
    if (s->held)
        return "held";
    return s->enabled ? "on" : "off";
}

int flat_status_line(const void *p, size_t avail, int found, char *out, size_t cap)
{
    int rc = found ? flat_check(p, avail) : FLAT_OK, n;
    const flat_state *s = (const flat_state *)p;
    if (cap < 48)
        return 0;
    if (!found || rc != FLAT_OK)
        n = snprintf(out, cap, "flat=absent%s", !found ? "" : rc == FLAT_ERR_VERSION ? " bad=version" : " bad=size");
    else
        n = snprintf(out, cap, "flat=%s calls=%lu chained=%d", flat_status_word(s, 1), (unsigned long)s->calls, s->still_chained ? 1 : 0);
    return n > 0 && (size_t)n < cap ? n : 0;
}

int flat_can_exit(const flat_state *s)
{
    return !s || !s->still_chained;
}

void flat_swap_init(flat_swap *w)
{
    memset(w, 0, sizeof *w);
}

int flat_swap_install(flat_swap *w, uintptr_t *slot, uintptr_t ours)
{
    if (!w || !slot || !ours)
        return FLAT_ERR_ARG;
    if (w->installed || *slot == ours)
        return FLAT_ERR_DOUBLE;
    w->saved = *slot;
    w->ours = ours;
    *slot = ours;
    w->installed = 1;
    w->inert = 0;
    return FLAT_OK;
}

int flat_swap_remove(flat_swap *w, uintptr_t *slot, flat_state *st)
{
    if (!w || !slot || !w->installed)
        return FLAT_NOT_INSTALLED;
    if (*slot != w->ours) {                 /* a foreign hook sits on top: never write our saved value over it */
        if (st)
            st->still_chained = 1;
        w->inert = 1;
        return FLAT_STILL_CHAINED;
    }
    *slot = w->saved;
    w->installed = 0;
    w->inert = 0;
    if (st)
        st->still_chained = 0;
    return FLAT_REMOVED;
}

static void put32(unsigned char *p, uint32_t v)
{
    p[0] = (unsigned char)(v >> 24);
    p[1] = (unsigned char)(v >> 16);
    p[2] = (unsigned char)(v >> 8);
    p[3] = (unsigned char)v;
}

void flat_tramp_build(unsigned char *out, uint32_t counter_addr, uint32_t saved_addr)
{
    static const unsigned char code[16] = {
        0x2F, 0x08,             /*  0 move.l a0,-(sp) */
        0x20, 0x7A, 0x00, 0x0C, /*  2 movea.l 16(pc),a0   (ext word at 4, data at 16) */
        0x52, 0x90,             /*  6 addq.l #1,(a0) */
        0x20, 0x5F,             /*  8 movea.l (sp)+,a0 */
        0x2F, 0x3A, 0x00, 0x08, /* 10 move.l 20(pc),-(sp) (ext word at 12, data at 20) */
        0x4E, 0x75              /* 14 rts: jumps to the saved routine, returns to the original caller */
    };
    memcpy(out, code, sizeof code);
    put32(out + 16, counter_addr);
    put32(out + 20, saved_addr);
}

int flat_conv(unsigned intuition_version)
{
    return intuition_version < 44 ? FLAT_CONV_STACK : FLAT_CONV_REG;
}

/* ---- FLAT-D4 ---- */
static const flat_dopus_build dopus_table[] = {
    { 73, 8, { 0x780fd46bu, 0xba2cf172u } },   /* DOpus 5.104 (LIB_VERSION 73, LIB_REVISION 8), measured on rig 5 (OS 3.2) by STATUS "sums=" (flat_sum_reloc, same at two load addresses) */
};

const flat_dopus_build *flat_dopus_table(int *n)
{
    if (n)
        *n = (int)(sizeof dopus_table / sizeof dopus_table[0]);
    return dopus_table;
}

uint32_t flat_sum(const void *p, size_t len)
{
    const unsigned char *b = (const unsigned char *)p;
    uint32_t sum = 0;
    size_t i;
    for (i = 0; i + 4 <= len; i += 4)
        sum += ((uint32_t)b[i] << 24) | ((uint32_t)b[i + 1] << 16) | ((uint32_t)b[i + 2] << 8) | b[i + 3];
    return sum ? sum : 1;
}

/* A routine's bytes carry absolute addresses (relocations) that differ with every load address, so a plain sum of the
 * first bytes changes from boot to boot. Skip the long that follows an opcode word with an absolute-long operand
 * (source EA 7/1, or the destination EA of a MOVE), and fingerprint the rest. */
uint32_t flat_sum_reloc(const void *p, size_t len)
{
    const unsigned char *b = (const unsigned char *)p;
    uint32_t sum = 0;
    size_t i = 0;
    while (i + 2 <= len) {
        unsigned op = ((unsigned)b[i] << 8) | b[i + 1];
        sum = (sum << 5 | sum >> 27) ^ op;
        i += 2;
        if ((op & 0x3f) == 0x39 || (op < 0x4000 && ((op >> 6) & 7) == 7 && ((op >> 9) & 7) == 1))
            i += 4;                     /* absolute-long operand: relocated, not part of the fingerprint */
    }
    return sum ? sum : 1;
}

uint32_t flat_jmp_target(const unsigned char *e)
{
    if (!e || e[0] != 0x4E || e[1] != 0xF9)
        return 0;
    return ((uint32_t)e[2] << 24) | ((uint32_t)e[3] << 16) | ((uint32_t)e[4] << 8) | e[5];
}

int flat_dopus_gate(const flat_dopus_build *tab, int n, unsigned version, unsigned revision,
                    const uint32_t sums[FLAT_EL_COUNT], int *reason)
{
    int i, el, mask = 0, measured = 0;
    int why = FLAT_GATE_UNKNOWN;
    for (i = 0; tab && i < n; i++) {
        if (tab[i].version != version || tab[i].revision != revision)
            continue;
        why = FLAT_GATE_UNMEASURED;
        for (el = 0; el < FLAT_EL_COUNT; el++) {
            if (!tab[i].sum[el])
                continue;
            measured = 1;
            if (sums && sums[el] == tab[i].sum[el])
                mask |= 1 << el;
        }
        if (measured)
            why = mask ? FLAT_GATE_OK : FLAT_GATE_SUM;
        break;
    }
    if (reason)
        *reason = why;
    return mask;
}

int flat_dopus_decide(const flat_state *s, int element, int recessed, int pen_shine, int pen_back)
{
    (void)recessed;                         /* a recessed box swaps the two halves; the shadow half is the L either way */
    if (!s || element < 0 || element >= FLAT_EL_COUNT)
        return FLAT_DRAW_STOCK;
    if (!s->enabled || s->held)
        return FLAT_DRAW_STOCK;
    return pen_shine == pen_back ? FLAT_DRAW_SKIP : FLAT_DRAW_STOCK;
}

int flat_dopus_status_line(int found, unsigned version, unsigned revision, int reason, int mask, int installed,
                           unsigned long calls, unsigned long flat, const uint32_t sums[FLAT_EL_COUNT], char *out, size_t cap)
{
    int n, k;
    if (cap < 96)
        return 0;
    if (!found)
        n = snprintf(out, cap, "dopus=absent");
    else if (!installed)
        n = snprintf(out, cap, "dopus=%u.%u inert=%s", version, revision,
                     reason == FLAT_GATE_UNKNOWN ? "unknown" : reason == FLAT_GATE_UNMEASURED ? "unmeasured" : "sum");
    else
        n = snprintf(out, cap, "dopus=%u.%u patched=%d calls=%lu flat=%lu", version, revision, mask, calls, flat);
    if (found && sums && n > 0 && (size_t)n < cap) {        /* the measured routine sums: how the table is filled */
        k = snprintf(out + n, cap - (size_t)n, " sums=%08lx,%08lx", (unsigned long)sums[0], (unsigned long)sums[1]);
        n = k > 0 ? n + k : -1;
    }
    return n > 0 && (size_t)n < cap ? n : 0;
}
