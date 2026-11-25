/* The settings table (prefs/settings_def.c) against the two things it stands for: the controls of the Prefs
 * window (pg_items) and the stores (prefs_fields through the file, am_prefs through amconf). Every count is
 * taken from walking the tables, none is written here. */
#include <stdio.h>
#include <string.h>
#include "../prefs/settings_def.h"
#include "../prefs/prefs_core.h"
#include "../config/amconf_core.h"
#include "../patina/menu_core.h"
#include "harness.h"

/* M2: sd_row_label gives "Label: Value" for every kind; the label has no Prefs "_"; a value too wide for the
 * buffer is cut and the label is not. */
static void label_is(int id, const void *store, const char *want)
{
    char buf[160];
    sd_row_label(&sd_settings[id], store, buf, (int)sizeof buf);
    if (strcmp(buf, want))
        printf("row %d: got \"%s\", want \"%s\"\n", id, buf, want);
    CHECK(!strcmp(buf, want));
}

static void test_settings_row_label(void)
{
    prefs_fields f;
    am_prefs a;
    char buf[40];
    int i, seen[16], len;
    prefs_defaults(&f);
    amconf_defaults(&a);
    memset(seen, 0, sizeof seen);
    /* every row of the table gives a label: its plain label, a colon, a value */
    for (i = 0; i < sd_nsettings; i++) {
        const sd_setting *s = &sd_settings[i];
        char plain[80], want[200], got[200];
        sd_label_plain(s, plain, sizeof plain);
        CHECK(strchr(plain, '_') == 0);
        sd_row_label(s, s->store == SD_PATINA ? (void *)&a : (void *)&f, got, sizeof got);
        strcpy(want, plain);
        strcat(want, ": ");
        CHECK(!strncmp(got, want, strlen(want)));
        CHECK(got[strlen(want)] != 0);
        seen[s->kind]++;
    }
    for (i = 0; i <= SD_PROFILE; i++)
        CHECK(seen[i] > 0);                 /* the walk covered every kind */
    /* one value per kind */
    f.blink = 1;
    label_is(SDI_BLINK, &f, "Cursor blinks: On");
    f.blink = 0;
    label_is(SDI_BLINK, &f, "Cursor blinks: Off");
    f.cursor = 1;
    label_is(SDI_CURSOR, &f, "Cursor: Underline");
    strcpy(f.sb, "5000");
    label_is(SDI_SCROLLBACK, &f, "Scrollback: 5000");
    strcpy(f.term, "xterm");
    label_is(SDI_TERM, &f, "Terminal type: xterm");
    f.term[0] = 0;
    label_is(SDI_TERM, &f, "Terminal type: vtcon");   /* empty means vtcon */
    strcpy(f.fg, "A0B0C0");
    label_is(SDI_FG, &f, "Text colour: #A0B0C0");
    label_is(SDI_BG, &f, "Background: default");
    strcpy(f.font, "topaz.font:8");
    label_is(SDI_FONT, &f, "Font: topaz.font:8");
    strcpy(f.screenmode, "0x00029000");
    label_is(SDI_SCREEN_MODE, &f, "Screen mode: 0x00029000");
    label_is(SDI_PALETTE, &f, "Palette: ...");
    label_is(SDI_THEME, &f, "Theme...: ...");
    label_is(SDI_PROFILE, &f, "Profile: ...");
    strcpy(a.val[AM_GTHEME], "nord");
    label_is(SDI_AM_GTHEME, &a, "Menu theme: nord");
    /* too wide: the value is cut, the label stays whole, the buffer is never overrun */
    strcpy(f.font, "a-very-long-font-name-indeed.font:12");
    len = sd_row_label(&sd_settings[SDI_FONT], &f, buf, 16);
    CHECK_INT(len, 15);
    CHECK(!strcmp(buf, "Font: a-very-.."));
    /* a window too narrow for the whole row: the label is cut (".."), the value stays whole */
    strcpy(f.font, "topaz.font:8");
    len = sd_row_label(&sd_settings[SDI_FONT], &f, buf, 18);        /* "Font: topaz.font:8" needs 19 */
    CHECK_INT(len, 17);
    CHECK_STR(buf, "F..: topaz.font:8");
    strcpy(f.fallback, "topaz.font:8");
    len = sd_row_label(&sd_settings[SDI_FALLBACK], &f, buf, 24);
    CHECK_STR(buf, "Fallbac..: topaz.font:8");
    CHECK_INT(len, 23);
    len = sd_row_label(&sd_settings[SDI_FONT], &f, buf, 19);        /* it just fits: nothing is cut */
    CHECK_STR(buf, "Font: topaz.font:8");
    /* an item's label holds the longest label and value a row can show (the menu's rows were 32 bytes) */
    CHECK((int)sizeof ((mn_item *)0)->label >= 20 + 2 + 48);   /* the longest label plus a 48-byte value; it was 32 */
    len = sd_row_label(&sd_settings[SDI_FALLBACK], &f, buf, 12);    /* label alone does not fit: kept whole as far as the buffer goes */
    CHECK(!strncmp(buf, "Fallback fon", 11) || !strncmp(buf, "Fallback fo", 11));
    CHECK_INT((int)strlen(buf) < 12, 1);
}

void suite_settings_def_patina(void)
{
    test_settings_row_label();
}
