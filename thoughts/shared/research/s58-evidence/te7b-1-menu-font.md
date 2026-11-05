# TE-7b.1 the drawn menu opens the theme's Menu Font

Pure function: `mt_font_spec(t, am_font, out, outsize)` in amarchy/menu_fontspec_core.[ch] (new). Searched for an existing pure menu file: menu_text.c is AmigaOS-only, menu_font.c AmigaOS-only, menu_core.c has no font logic; gthemes_core.* off limits. Precedence: theme Menu Font (via gt_look_font, "name.font/size" becomes "name.font:size"), else AM_FONT, else "". Size 0 or missing: bare name (mt_open reads size 0). Model field is 40 bytes, so an over-long name cannot exist in the model; output is cut at outsize-1 and terminated.
gui.c pick_font: gt_dos_load_current (same call pens_get(g, 0) uses) feeds mt_font_spec. A Style > Theme preview (pens_get with a name) still opens the saved theme's font; the font is bound once in screen_bind.

## RED (final test, fix disabled by `if (0 && t && gt_look_font`)
RUNTEST menu_fontspec rc=2 [FAIL] menu_fontspec: 16 checks, 8 failed
  FAIL tests/test_menu_fontspec.c:25: used = 0, want 1
## GREEN
RUNTEST menu_fontspec rc=0 [OK] menu_fontspec: 16 checks, 0 failed
Suite `desk` not run: no file it links was touched.
## m68k
make build/amiga/{Amarchy,amarchy-gui,amarchy-menu,amarchy-index,amarchy-icons,amarchy-flat,upprefs}: rc=0. Makefile: menu_fontspec_core.c added to AMARCHY_GUI_SRC (only target linking gui.c) and to the host TESTS list.

## Callers (grep mt_open( and menu_font_open( in amarchy/*.c)
- gui.c:1757 mt_open (pick_font): routed.
- menu_text.c:258 mt_open definition: the consumer, left.
- menu_font.c:60 menu_font_open definition: not called from amarchy/*.c besides its own file, left.
- flat.c, leaf.c, leaf_core.c, menu.c: no mt_open/menu_font_open call; leaf windows draw through gui.c's g->mt, so they agree. leaf_core.c uses prefs_font_parse only for parsing.

## Later rig pixel check
Set Menu Font in the theme editor (or `look` font[GT_FONT_MENU], e.g. `topaz.font/8`) on the active theme, open the drawn menu, measure the row glyph height: topaz 8 gives 8 px glyph rows, against JetBrainsMono 16 (default, 16 px). Then clear Menu Font and confirm the AM_FONT / built-in height returns.
