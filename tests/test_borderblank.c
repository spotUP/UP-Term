/* AM-BORDERBLANK (amarchy/borderblank_core.c): which modes get the black border, and the table of the screens
 * Amarchy blanked (Off and quit clear only those). */
#include "../amarchy/borderblank_core.h"
#include "harness.h"

static void test_native_modes(void)
{
    CHECK_INT(bb_native_mode(0x00000000UL), 1);   /* LORES_KEY, default monitor */
    CHECK_INT(bb_native_mode(0x00008004UL), 1);   /* HIRESLACE_KEY */
    CHECK_INT(bb_native_mode(0x00029000UL), 1);   /* PAL hires */
    CHECK_INT(bb_native_mode(0x00031004UL), 1);   /* VGA productivity */
    CHECK_INT(bb_native_mode(0x00039025UL), 1);   /* VGA hires lace etc. */
    CHECK_INT(bb_native_mode(0x00099004UL), 1);   /* DBLNTSC */
    CHECK_INT(bb_native_mode(0x000A9004UL), 1);   /* DBLPAL */
    CHECK_INT(bb_native_mode(0x000B1000UL), 0);   /* past DBLPAL */
    CHECK_INT(bb_native_mode(0x40120000UL), 0);   /* CyberGraphX / P96 RTG */
    CHECK_INT(bb_native_mode(0x50031100UL), 0);   /* P96 RTG */
    CHECK_INT(bb_native_mode(0xFFFFFFFFUL), 0);   /* INVALID_ID */
}

static void test_table(void)
{
    static char s[BB_MAX + 1];
    bb_table t;
    int i;
    bb_init(&t);
    CHECK_INT(bb_add(&t, 0), 0);
    CHECK_INT(bb_add(&t, &s[0]), 1);
    CHECK_INT(bb_add(&t, &s[0]), 0);
    CHECK_INT(bb_has(&t, &s[0]), 1);
    CHECK_INT(bb_drop(&t, &s[1]), 0);
    CHECK_INT(bb_drop(&t, &s[0]), 1);
    CHECK_INT(bb_has(&t, &s[0]), 0);
    CHECK_INT(t.n, 0);
    for (i = 0; i < BB_MAX; i++)
        CHECK_INT(bb_add(&t, &s[i]), 1);
    CHECK_INT(bb_add(&t, &s[BB_MAX]), -1);
    CHECK_INT(bb_drop(&t, &s[3]), 1);             /* the last entry moves into the hole */
    CHECK_INT(bb_has(&t, &s[BB_MAX - 1]), 1);
    CHECK_INT(bb_add(&t, &s[BB_MAX]), 1);
}

void suite_borderblank(void)
{
    test_native_modes();
    test_table();
}
