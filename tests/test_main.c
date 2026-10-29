/* Runs every host suite, or only the one named by argv[1]. */
#include "harness.h"

void suite_xterm(void);
void suite_keys(void);
void suite_amiga(void);
void suite_reflow(void);
void suite_sixel(void);
void suite_kitty(void);
void suite_pcansi(void);
void suite_glyph(void);
void suite_mirror(void);
void suite_lineedit(void);
void suite_sh_parse(void);
void suite_sh_expand(void);
void suite_sh_exec(void);
void suite_ldisc(void);
void suite_waitset(void);
void suite_brk(void);
void suite_sh_pipe(void);
void suite_sh_ixargv(void);
void suite_sh_own(void);
void suite_histfile(void);
void suite_pens(void);
void suite_proc(void);
void suite_pty_name(void);
void suite_bmsg(void);
void suite_qmark(void);
void suite_ttyown(void);
void suite_upassign(void);
void suite_bigvol(void);
void suite_fssm(void);
void suite_clips(void);
void suite_shot_core(void);
void suite_pi_llm(void);
void suite_upcon(void);
void suite_upconf(void);
void suite_keymap(void);
void suite_prefs(void);
void suite_amconf(void);
void suite_settings_def(void);
void suite_gthemes(void);
void suite_desk(void);
void suite_pkgs(void);
void suite_job(void);
void suite_bar(void);
void suite_tick(void);
void suite_status_core(void);
void suite_note_core(void);
void suite_menusrv(void);
void suite_menu_text(void);
void suite_iconspec(void);
void suite_updiff(void);
void suite_zmodem(void);
void suite_otag(void);
void suite_ttf(void);
void suite_slash(void);
void suite_winspec(void);
void suite_screenreq(void);
void suite_cmdline(void);
void suite_win(void);
void suite_tile(void);
void suite_fontpair(void);
void suite_updemo(void);
void suite_tabprog(void);
void suite_pace(void);
void suite_painter(void);
void suite_text(void);
void suite_clip(void);
void suite_input(void);
void suite_protocol(void);
void suite_sbar(void);
void suite_unifont(void);
void suite_emoji(void);
void suite_telnet(void);
void suite_upagent(void);
void suite_complete(void);
void suite_winmem(void);
void suite_sbpack(void);
void suite_hl(void);
void suite_md(void);
void suite_claude_http(void);
void suite_claude_json(void);
void suite_claude_stream(void);
void suite_claude_tools(void);
void suite_claude_match(void);
void suite_claude_repl(void);
void suite_claude_cli(void);
void suite_claude_tui(void);
void suite_claude_config(void);

static const h_suite suites[] = {
    { "xterm", suite_xterm },
    { "keys", suite_keys },
    { "amiga", suite_amiga },
    { "reflow", suite_reflow },
    { "sixel", suite_sixel },
    { "kitty", suite_kitty },
    { "pcansi", suite_pcansi },
    { "glyph", suite_glyph },
    { "mirror", suite_mirror },
    { "lineedit", suite_lineedit },
    { "sh_parse", suite_sh_parse },
    { "sh_expand", suite_sh_expand },
    { "sh_exec", suite_sh_exec },
    { "ldisc", suite_ldisc },
    { "waitset", suite_waitset },
    { "brk", suite_brk },
    { "sh_pipe", suite_sh_pipe },
    { "sh_ixargv", suite_sh_ixargv },
    { "sh_own", suite_sh_own },
    { "histfile", suite_histfile },
    { "pens", suite_pens },
    { "proc", suite_proc },
    { "pty_name", suite_pty_name },
    { "bmsg", suite_bmsg },
    { "qmark", suite_qmark },
    { "ttyown", suite_ttyown },
    { "upassign", suite_upassign },
    { "bigvol", suite_bigvol },
    { "fssm", suite_fssm },
    { "clips", suite_clips },
    { "shot_core", suite_shot_core },
    { "upcon", suite_upcon },
    { "upconf", suite_upconf },
    { "keymap", suite_keymap },
    { "prefs", suite_prefs },
    { "amconf", suite_amconf },
    { "settings_def", suite_settings_def },
    { "gthemes", suite_gthemes },
    { "desk", suite_desk },
    { "pkgs", suite_pkgs },
    { "job", suite_job },
    { "bar", suite_bar },
    { "tick", suite_tick },
    { "status_core", suite_status_core },
    { "note_core", suite_note_core },
    { "menusrv", suite_menusrv },
    { "menu_text", suite_menu_text },
    { "iconspec", suite_iconspec },
    { "updiff", suite_updiff },
    { "zmodem", suite_zmodem },
    { "otag", suite_otag },
    { "ttf", suite_ttf },
    { "slash", suite_slash },
    { "winspec", suite_winspec },
    { "screenreq", suite_screenreq },
    { "cmdline", suite_cmdline },
    { "win", suite_win },
    { "tile", suite_tile },
    { "fontpair", suite_fontpair },
    { "updemo", suite_updemo },
    { "tabprog", suite_tabprog },
    { "pace", suite_pace },
    { "painter", suite_painter },
    { "text", suite_text },
    { "clip", suite_clip },
    { "input", suite_input },
    { "protocol", suite_protocol },
    { "sbar", suite_sbar },
    { "unifont", suite_unifont },
    { "emoji", suite_emoji },
    { "telnet", suite_telnet },
    { "upagent", suite_upagent },
    { "complete", suite_complete },
    { "winmem", suite_winmem },
    { "sbpack", suite_sbpack },
    { "hl", suite_hl },
    { "md", suite_md },
    { "claude_http", suite_claude_http },
    { "claude_json", suite_claude_json },
    { "claude_stream", suite_claude_stream },
    { "claude_tools", suite_claude_tools },
    { "claude_match", suite_claude_match },
    { "claude_repl", suite_claude_repl },
    { "claude_cli", suite_claude_cli },
    { "claude_tui", suite_claude_tui },
    { "claude_config", suite_claude_config },
    { "pi_llm", suite_pi_llm },
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
