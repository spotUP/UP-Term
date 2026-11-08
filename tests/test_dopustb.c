/* DT-2, DT-3: the Directory Opus toolbar's set pick and brush CMAP (amarchy/dopustb_core.c). */
#include <stdio.h>
#include <string.h>
#include "../amarchy/dopustb_core.h"
#include "../amarchy/iconpen_core.h"
#include "harness.h"

static void theme(gtheme *t, const char *s)
{
    gt_parse(s, (long)strlen(s), t);
}

static const char THEME_A[] = "name = dta\nbackground = #101820\nforeground = #e0e8f0\ndark_foreground = #607080\naccent = #ff8800\n"
                              "selection = #304860\nmuted = #8090a0\n";
static const char THEME_B[] = "name = dtb\nbackground = #f0e8d8\nforeground = #202830\ndark_foreground = #a0a8b0\naccent = #2060c0\n"
                              "selection = #c0d0e0\nmuted = #607080\n";

static unsigned pick(unsigned font, int rx, int ry, int *tall)
{
    dt_set s;
    dt_pick_set(font, rx, ry, &s);
    *tall = s.tall;
    return s.size;
}

static void test_pick_set(void)
{
    int tall;
    char name[8];
    dt_set s;
    /* the owner's band rule: about 2 x the font height, snapped to 16/24/32 */
    CHECK_INT(pick(8, 1, 1, &tall), 16);
    CHECK_INT(pick(9, 1, 1, &tall), 16);
    CHECK_INT(pick(10, 1, 1, &tall), 16);
    CHECK_INT(pick(11, 1, 1, &tall), 24);
    CHECK_INT(pick(12, 1, 1, &tall), 24);
    CHECK_INT(pick(13, 1, 1, &tall), 24);
    CHECK_INT(pick(14, 1, 1, &tall), 24);
    CHECK_INT(pick(15, 1, 1, &tall), 32);
    CHECK_INT(pick(24, 1, 1, &tall), 32);
    CHECK_INT(pick(0, 1, 1, &tall), 24);          /* unknown font */
    /* aspect: tall pixel only at 2:1 (li_pick_variant), unknown aspect is square */
    pick(8, 22, 44, &tall); CHECK_INT(tall, 1);
    pick(8, 44, 44, &tall); CHECK_INT(tall, 0);
    pick(8, 22, 33, &tall); CHECK_INT(tall, 0);
    pick(8, 0, 0, &tall); CHECK_INT(tall, 0);
    dt_pick_set(12, 22, 44, &s);
    CHECK_INT(dt_set_name(&s, name, sizeof name), 3);
    CHECK(strcmp(name, "24t") == 0);
    s.tall = 0; s.size = 32;
    CHECK_INT(dt_set_name(&s, name, sizeof name), 2);
    CHECK(strcmp(name, "32") == 0);
    CHECK_INT(dt_set_name(&s, name, 3), 0);
}

static void ilbm(uint8_t *b, size_t *len)
{
    static const uint8_t head[] = { 'F','O','R','M',0,0,0,0,'I','L','B','M', 'B','M','H','D',0,0,0,2,0,0, 'C','M','A','P',0,0,0,24 };
    memcpy(b, head, sizeof head);
    memset(b + sizeof head, 0x55, 24);
    memcpy(b + sizeof head + 24, "BODY\0\0\0\0", 8);
    *len = sizeof head + 24 + 8;
}

static void test_cmap(void)
{
    gtheme t[2];
    uint8_t rgb[DT_PENS][3], ip[IP_PENS][3], file[128];
    size_t len;
    gt_roles r;
    int k;
    theme(&t[0], THEME_A);
    theme(&t[1], THEME_B);
    for (k = 0; k < 2; k++) {
        dt_cmap(&t[k], rgb);
        gt_roles_of(&t[k], &r);
        ip_colours(&t[k], r.c[GR_SURFACE], ip);            /* the Workbench icons' own function */
        CHECK(memcmp(rgb[0], ip[IP_SPARE12], 3) == 0);
        CHECK(memcmp(rgb[1], ip[IP_FG33], 3) == 0);
        CHECK(memcmp(rgb[2], ip[IP_FG66], 3) == 0);
        CHECK(memcmp(rgb[3], ip[IP_FG100], 3) == 0);
        CHECK(memcmp(rgb[4], ip[IP_ACCENT33], 3) == 0);
        CHECK(memcmp(rgb[5], ip[IP_ACCENT66], 3) == 0);
        CHECK(memcmp(rgb[6], ip[IP_ACCENT100], 3) == 0);
        CHECK(memcmp(rgb[7], rgb[0], 3) == 0);
        /* pen 0 is the lister ground, pen 3 the full text colour */
        CHECK_INT(((long)rgb[0][0] << 16) | (rgb[0][1] << 8) | rgb[0][2], (long)r.c[GR_SURFACE]);
        CHECK(rgb[1][0] != rgb[3][0] || rgb[1][1] != rgb[3][1]);
        /* the patch writes exactly those bytes into the file's CMAP and nothing else */
        ilbm(file, &len);
        CHECK_INT(dt_cmap_patch(file, len, rgb), 1);
        CHECK(memcmp(file + 30, rgb, 24) == 0);
        CHECK(memcmp(file + 12, "BMHD", 4) == 0 && memcmp(file + 54, "BODY", 4) == 0);
        CHECK_INT(dt_cmap_patch(file, len, rgb), 2);     /* already equal */
    }
    dt_cmap(&t[0], rgb);
    ilbm(file, &len);
    file[29] = 12;                                       /* a CMAP of 12 bytes (the old 4-pen brush) is refused, untouched */
    CHECK_INT(dt_cmap_patch(file, len, rgb), 0);
    CHECK_INT(file[30], 0x55);
    CHECK_INT(dt_cmap_patch(file, 5, rgb), 0);
}

void suite_dopustb(void)
{
    test_pick_set();
    test_cmap();
}
