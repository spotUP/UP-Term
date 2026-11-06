# TE-7b.2 Amarchy half: the Shell Font reaches the terminal carrier file

## Handler grammar (read only, handler/ untouched)
- handler/vtcon_handler.c:1658-1666: `v = upconf_str(c->conf, p, "font", 0); if (v && !c->w.fontname[0]) { copy; ':' -> ' '; parse_font(f, c->w.fontname, ..., &c->w.fontsize); }`
- handler/vtcon_handler.c:1158-1163: `parse_font(rest,...)` calls `winspec_font(rest, name, max, &sz)`, which splits a trailing run of digits after a blank as the size and keeps the rest as the name (blanks allowed, ".font" kept).
- So NAME is written WITH ".font", as the look slot stores it ("name.font/size"): `font = XEN.font:9` becomes "XEN.font 9", name "XEN.font", size 9.

## Written line
`font = NAME:SIZE` + newline, appended to the `[profile <name>]` section after the `palette = ...` line, in amarchy/gthemes_core.c gt_term_text.
- Source: model parser (gt_model_attach on the local gtheme) then gt_look_font(GT_FONT_SHELL); no second parser. Model freed with gt_free.
- No Shell Font, size missing or 0: no line (decision: the handler must never get NAME:0).
- Worst case: 39-char name + ":999" adds under 50 bytes; test asserts it fits text[1024] (gthemes_dos.c:196; the other caller gt_load_term passes the caller's cap).

## Tests
tests/test_gthemes.c `test_gthemes_term_font`: (a) exactly one `font = XEN.font:9` line, after the palette line, read back through upconf under the profile name; (b) no Shell Font = no font line; (c) the old lines stay byte for byte; size 0 or no size = no line; (d) a 39-character name with size 999 fits 1024 bytes.
Red/green and the m68k build were run by the main session at merge time (the agent ended before recording them); results are in the merge commit message.

## Handler half (not in this change)
TE-7b.2-HANDLER (UP-Term session): the live reload path makes open tabs follow the `font` key. Precedence agreed: built-in < profile < Amarchy theme < window spec / slash font.

## Test rewrite and proof (final)
- First test version appended `[profile look]` to the v1 gruvbox.theme: `FAIL tests/test_gthemes.c:155: p != 0` (then a null strstr SEGV at :159). Cause: without `amarchy-theme = 2` gt_model_parse takes the v1 path (gt_model_from_v1) and ignores the look section, so a v1 file never has a Shell Font. The test now builds a v2 text (term_font_text).
- Second run: `FAIL :189 strstr(out, ":999\n") != 0`. look.font is char[40] ("name.font/size"), so a 39-char name cannot keep "/999"; worst case is a 35-char name + "/999".
- GREEN: `RUNTEST gthemes rc=0 [OK] gthemes: 6397 checks, 0 failed`
- RED (fix disabled by `if (0 && ...)`): `RUNTEST gthemes rc=2` with `FAIL tests/test_gthemes.c:164: cnt = 0, want 1`, `:166: p != 0`, `:172: len = 219, want 237`, then the run aborted (ASAN) on the font read; terminated, no hang.
- m68k compile proof: done in the first commit acfbfb69's session? Open: not re-run in this follow-up (core unchanged).
