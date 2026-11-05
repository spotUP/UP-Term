/* The drawn menu's font spec: the theme's Menu Font, else the user's AM_FONT, else the built-in (amarchy/menu_fontspec_core.c, TE-7b.1). */
#include <stdio.h>
#include <string.h>
#include "../amarchy/menu_fontspec_core.h"
#include "harness.h"

static gt_model mdl;

static void theme_with(gtheme *t, const char *menu_font)
{
    memset(t, 0, sizeof *t);
    memset(&mdl, 0, sizeof mdl);
    strncpy(mdl.look.font[GT_FONT_MENU], menu_font, sizeof mdl.look.font[GT_FONT_MENU] - 1);
    t->model = &mdl;
}

/* mt_font_spec_load through a fake reader serving real theme text: the model loader must carry the Menu Font in */
static const char *file_body;

static long fake_theme_read(void *ctx, const char *path, char *buf, long cap)
{
    (void)ctx;
    (void)path;
    if (!file_body || (long)strlen(file_body) > cap)
        return -1;
    memcpy(buf, file_body, strlen(file_body));
    return (long)strlen(file_body);
}

static void test_fontspec_load(void)
{
    char out[64];
    int used;

    file_body = "amarchy-theme = 2\nname = \"mf\"\n[profile look]\nmenu-font = \"topaz.font/8\"\n";
    used = mt_font_spec_load(fake_theme_read, 0, "mf", "JetBrainsMono.font:16", out, (int)sizeof out);
    CHECK_INT(used, 1);
    CHECK_STR(out, "topaz.font:8");

    file_body = "amarchy-theme = 2\nname = \"nf\"\n[profile look]\ntitle-font = \"topaz.font\"\n";
    used = mt_font_spec_load(fake_theme_read, 0, "nf", "JetBrainsMono.font:16", out, (int)sizeof out);
    CHECK_INT(used, 0);
    CHECK_STR(out, "JetBrainsMono.font:16");

    file_body = 0;   /* every path missing */
    used = mt_font_spec_load(fake_theme_read, 0, "gone", "JetBrainsMono.font:16", out, (int)sizeof out);
    CHECK_INT(used, 0);
    CHECK_STR(out, "JetBrainsMono.font:16");
}

void suite_menu_fontspec(void)
{
    gtheme t;
    char out[64], tiny[8];
    int used;

    theme_with(&t, "topaz.font/8");
    used = mt_font_spec(&t, "JetBrainsMono.font:16", out, (int)sizeof out);
    CHECK_INT(used, 1);
    CHECK_STR(out, "topaz.font:8");   /* theme names a Menu Font: it wins over AM_FONT, in the form prefs_font_parse reads */

    theme_with(&t, "");
    used = mt_font_spec(&t, "JetBrainsMono.font:16", out, (int)sizeof out);
    CHECK_INT(used, 0);
    CHECK_STR(out, "JetBrainsMono.font:16");   /* no theme font: AM_FONT unchanged */

    used = mt_font_spec(&t, "", out, (int)sizeof out);
    CHECK_INT(used, 0);
    CHECK_STR(out, "");   /* both empty: the built-in font */
    mt_font_spec(0, 0, out, (int)sizeof out);
    CHECK_STR(out, "");

    theme_with(&t, "Hack.font/12");
    mt_font_spec(&t, "", out, (int)sizeof out);
    CHECK_STR(out, "Hack.font:12");

    /* size missing or 0: the bare name, mt_open reads size 0 */
    theme_with(&t, "Hack.font");
    mt_font_spec(&t, "x.font:9", out, (int)sizeof out);
    CHECK_STR(out, "Hack.font");
    theme_with(&t, "Hack.font/0");
    mt_font_spec(&t, "x.font:9", out, (int)sizeof out);
    CHECK_STR(out, "Hack.font");

    /* the model's field holds 39 characters: a name that fills it is used whole, no overflow of the spec buffer */
    theme_with(&t, "AVeryLongFontNameThatDoesNotFitTheModelsNameField.font/16");
    used = mt_font_spec(&t, "topaz.font:8", out, (int)sizeof out);
    CHECK_INT(used, 1);
    CHECK_STR(out, "AVeryLongFontNameThatDoesNotFitTheModel");

    /* over-long output: cut, terminated, nothing written past outsize */
    memset(tiny, 'Z', sizeof tiny);
    theme_with(&t, "LongFontName.font/16");
    mt_font_spec(&t, "", tiny, 6);
    CHECK_INT((int)strlen(tiny), 5);
    CHECK(tiny[6] == 'Z' && tiny[7] == 'Z');
    memset(tiny, 'Z', sizeof tiny);
    theme_with(&t, "");
    mt_font_spec(&t, "AAAAAAAAAAAAAAAA", tiny, 6);
    CHECK_INT((int)strlen(tiny), 5);
    CHECK(tiny[6] == 'Z');

    test_fontspec_load();
}
