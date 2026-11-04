#!/usr/bin/env python3
"""A window with no profile still gets the Nerd Font fallback (R3C-UD6-NERD-QMARK).

A fresh install copies the shipped sample dist/up-term.conf, whose [profile] sections are all comments, so
apply_profile in vtcon_handler.c finds no profile and returns early. The font-fallback resolution
(upconf_font_fallback, which turns an unset key into SymbolsNerdFontMono) sat after that return, so the
window opened no outline font and UD6 drew the Nerd icons as '?'. The handler is Amiga-only, so this
checks the source: in apply_profile the upconf_font_fallback call comes before the first return."""
import os, re

ROOT = os.path.abspath(os.path.join(os.path.dirname(os.path.abspath(__file__)), ".."))
src = open(os.path.join(ROOT, "handler/vtcon_handler.c")).read()
m = re.search(r"\nstatic void apply_profile\(con \*c\)\n\{", src)
assert m, "apply_profile not found"
body = src[m.end():src.find("\n}", m.end())]
call = body.find("upconf_font_fallback(")
ret = re.search(r"\breturn\b[^;\n]*;", body)
assert call >= 0, "apply_profile does not resolve font-fallback"
assert ret, "apply_profile has no return: test needs updating"
assert call < ret.start(), "apply_profile returns before upconf_font_fallback: a window with no profile gets no Nerd fallback"
assert "upconf_window_profile(" in body, "apply_profile does not pick its profile through upconf_window_profile"
