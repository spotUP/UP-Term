/* RIP-7b handler glue. Step 1: the engine gives back the bytes that are not its own
 * (vt_feed_stop / vt_fed). The bytes after a level-9 block are protocol, not terminal output. */
#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include "harness.h"
#include "../engine/vtengine.h"

static int blocks, do_stop = 1;

static void on_block(void *u, int mode, int proto, int type, const char *name)
{
    vt_term **tp = (vt_term **)u;
    (void)mode;
    (void)proto;
    (void)type;
    (void)name;
    blocks++;
    if (do_stop)
        vt_feed_stop(*tp);
}

static int grid_has(vt_term *t, const char *s)
{
    static char scr[8192];
    vt_screen_text(t, scr, (long)sizeof scr);
    return strstr(scr, s) != 0;
}

/* rip: vt_set_rip argument; prefix: bytes before the `!|9` line (RIP on needs ESC [ 2 ! in mode 1) */
static void stop_case(int rip, const char *prefix)
{
    static const char proto[] = "**\030B00000000000000\r\x8a\x11" "garbage-after-block";
    char buf[512];
    long cmd_end, total;
    vt_callbacks cb;
    vt_term *t = 0;
    memset(&cb, 0, sizeof cb);
    cb.rip_block = on_block;
    t = vt_new(80, 25, 100, &cb, &t);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, rip);
    blocks = 0;
    strcpy(buf, prefix);
    strcat(buf, "!|9\033" "0" "5" "00" "0000" "X.ICN" "\r\n");
    cmd_end = (long)strlen(buf);
    memcpy(buf + cmd_end, proto, sizeof proto - 1);
    total = cmd_end + (long)sizeof proto - 1;
    vt_feed(t, (const vt_u8 *)buf, total);
    CHECK_INT(blocks, 1);
    CHECK_INT(vt_rip_blocks(t), 1);
    CHECK(vt_fed(t) == cmd_end - 1 || vt_fed(t) == cmd_end); /* after the CR (LF may remain) */
    CHECK(vt_fed(t) < total);
    CHECK(!grid_has(t, "B00"));
    CHECK(!grid_has(t, "garbage"));
    /* the same buffer with no stop: the remainder is drawn (the test can tell) */
    do_stop = 0;
    vt_free(t);
    t = vt_new(80, 25, 100, &cb, &t);
    vt_set_cell_pixels(t, 8, 14);
    vt_set_rip(t, rip);
    vt_feed(t, (const vt_u8 *)buf, total);
    CHECK_INT(vt_fed(t), total);
    CHECK(grid_has(t, "garbage"));
    do_stop = 1;
    vt_free(t);
}

void suite_ripglue(void)
{
    stop_case(1, "\033[2!");
    stop_case(2, "");
}
