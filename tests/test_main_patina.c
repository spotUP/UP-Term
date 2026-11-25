/* Runs every Patina host suite, or only the one named by argv[1] (patina_host; vtcon's are in test_main.c). */
#include "harness.h"
void suite_clips(void);
void suite_shot_core(void);
void suite_keymap_patina(void);
void suite_amconf_patina(void);
void suite_settings_def_patina(void);
void suite_gthemes_patina(void);
void suite_pkgs(void);
void suite_job(void);
void suite_bar(void);
void suite_tick(void);
void suite_demo_core(void);
void suite_wbmode(void);
void suite_status_core(void);
void suite_barconf(void);
void suite_note_core(void);
void suite_flat_core_patina(void);
void suite_flat_role_core(void);
void suite_iclabel(void);
void suite_richaa(void);
void suite_iconfile(void);
void suite_iconapply(void);
void suite_dopusenv(void);
void suite_dopustb_bank(void);
void suite_dopustb(void);
void suite_menu_fontspec(void);
void suite_wbreset(void);
void suite_borderblank(void);
void suite_inst(void);
void suite_help_core(void);
void suite_themeprev(void);
void suite_menusrv(void);
void suite_menu_text(void);
void suite_tile(void);

static const h_suite suites[] = {
    { "clips", suite_clips },
    { "shot_core", suite_shot_core },
    { "keymap_patina", suite_keymap_patina },
    { "amconf_patina", suite_amconf_patina },
    { "settings_def_patina", suite_settings_def_patina },
    { "gthemes_patina", suite_gthemes_patina },
    { "pkgs", suite_pkgs },
    { "job", suite_job },
    { "bar", suite_bar },
    { "tick", suite_tick },
    { "demo_core", suite_demo_core },
    { "wbmode", suite_wbmode },
    { "status_core", suite_status_core },
    { "barconf", suite_barconf },
    { "note_core", suite_note_core },
    { "flat_core_patina", suite_flat_core_patina },
    { "flat_role_core", suite_flat_role_core },
    { "iclabel", suite_iclabel },
    { "richaa", suite_richaa },
    { "iconfile", suite_iconfile },
    { "iconapply", suite_iconapply },
    { "dopusenv", suite_dopusenv },
    { "dopustb_bank", suite_dopustb_bank },
    { "dopustb", suite_dopustb },
    { "menu_fontspec", suite_menu_fontspec },
    { "wbreset", suite_wbreset },
    { "borderblank", suite_borderblank },
    { "inst", suite_inst },
    { "help_core", suite_help_core },
    { "themeprev", suite_themeprev },
    { "menusrv", suite_menusrv },
    { "menu_text", suite_menu_text },
    { "tile", suite_tile },
    { 0, 0 }
};

int main(int argc, char **argv)
{
    int i, ran = 0;
    /* --has NAME: exit 0 when NAME is a suite here, 1 when not (make test ONLY= asks first) */
    if (argc > 2 && !strcmp(argv[1], "--has")) {
        for (i = 0; suites[i].name; i++)
            if (!strcmp(argv[2], suites[i].name))
                return 0;
        return 1;
    }
    for (i = 0; suites[i].name; i++) {
        int f0 = h_failures, c0 = h_checks;
        if (argc > 1 && argv[1][0] && strcmp(argv[1], suites[i].name))
            continue;
        suites[i].fn();
        printf("[%s] %s: %d checks, %d failed\n", h_failures == f0 ? "OK" : "FAIL",
               suites[i].name, h_checks - c0, h_failures - f0);
        ran++;
    }
    if (!ran) {
        printf("[ERROR] no suite named %s\n", argc > 1 ? argv[1] : "");
        return 2;
    }
    return h_failures ? 1 : 0;
}
