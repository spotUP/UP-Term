/* Runs every host suite, or only the one named by argv[1]. */
#include "harness.h"

void suite_xterm(void);
void suite_keys(void);
void suite_amiga(void);
void suite_reflow(void);
void suite_sixel(void);
void suite_pcansi(void);
void suite_glyph(void);
void suite_mirror(void);
void suite_lineedit(void);
void suite_sh_parse(void);
void suite_sh_expand(void);
void suite_sh_exec(void);
void suite_ldisc(void);
void suite_upcon(void);
void suite_upconf(void);
void suite_prefs(void);
void suite_iconspec(void);
void suite_zmodem(void);
void suite_otag(void);
void suite_slash(void);
void suite_fontpair(void);
void suite_updemo(void);
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
    { "pcansi", suite_pcansi },
    { "glyph", suite_glyph },
    { "mirror", suite_mirror },
    { "lineedit", suite_lineedit },
    { "sh_parse", suite_sh_parse },
    { "sh_expand", suite_sh_expand },
    { "sh_exec", suite_sh_exec },
    { "ldisc", suite_ldisc },
    { "upcon", suite_upcon },
    { "upconf", suite_upconf },
    { "prefs", suite_prefs },
    { "iconspec", suite_iconspec },
    { "zmodem", suite_zmodem },
    { "otag", suite_otag },
    { "slash", suite_slash },
    { "fontpair", suite_fontpair },
    { "updemo", suite_updemo },
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
    { 0, 0 }
};

int main(int argc, char **argv)
{
    int i, ran = 0;
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
