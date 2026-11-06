# TE-7b.2 follow-ups (S61)
- Invariant: the window font is a pure function of (spec FONT, theme Shell Font, profile font, built-in); removing a layer must fall back to the next one, never keep the removed layer's font.
- Level: font_live (handler) treats "target font empty" as "keep the old one"; vtwin_set_font(w, "", 0) already means the built-in (GfxBase->DefaultFont, same as a window opened with empty fontname). Both theme_font_live and profile_switch go through font_live: one fix.
- Fix: pure decision upconf_font_change(oldname, oldsize, newname, newsize, font_spec) in config/upconf.c (host-testable; the handler has no host harness for font_live); font_live calls vtwin_set_font(&w, "", 0) when it says "built-in". font_spec windows keep their font.
- (2): a missing ENV:up-term/up-term gives an EMPTY table (read_conf returns early, conf stays zeroed), so apply_profile already reaches the theme branch there; only conf==NULL (out of memory) returned early. Early returns in apply_profile and theme_font_live removed; upconf helpers already take a null conf (tests/test_upconf.c, upconf_font_source(0,0,&t)).
- Test: tests/test_upconf.c upconf_font_change cases.
