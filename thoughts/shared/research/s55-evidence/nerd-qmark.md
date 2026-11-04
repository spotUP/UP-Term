---
date: 2026-11-04
topic: R3C-UD6-NERD-QMARK, UPDemo UD6 draws Nerd icons as '?' on a fresh 3.2 install
status: fixed on host; rig not re-checked
---

# R3C-UD6-NERD-QMARK: Nerd icons as '?' on a fresh install

## Symptom

On rig 96 (AmigaOS 3.2, fresh install), UPDemo scene UD6 draws 19 of 20 Nerd Font icons as '?'. It also
draws the Powerline separators (U+E0B0, U+E0B2, U+E0A0) as '?' and U+F0001 as a hollow box.
FONTS:SymbolsNerdFontMono.ttf is present.

## Root cause

The fault is in `handler/vtcon_handler.c`, function `apply_profile`, at HEAD 766108d7:

- Lines 1535-1543 return early when there is no config, or when the config has neither the window's
  profile nor `default`.
- Line 1591 resolves `font-fallback` through `upconf_font_fallback`. When the key is unset, this call
  makes it SymbolsNerdFontMono if FONTS:SymbolsNerdFontMono.ttf exists (NERD-FALLBACK-DEFAULT). Line
  1591 comes after the early return.

A fresh install meets that early return. `dist/install.dos` (lines 374-378) copies the shipped sample
`dist/up-term.conf` to ENVARC:up-term/up-term. Every `[profile ...]` line in that sample is a comment,
so the parsed config has zero profiles. `apply_profile` then returns early, and `c->w.fallback` stays
"".

The effects on rendering:

- `vtwin` outline_sync opens no outline font, because `vo_open` runs only when the fallback is
  non-empty.
- `render/glyphmap.c fallback()` falls through to Unifont and then to `vt_map_glyph`.
- `vt_map_glyph` gives '?' for BMP code points with no glyph, and VT_GLYPH_MISSING (the box) for code
  points above U+FFFF. This matches the symptom exactly.
- The one icon that drew correctly is presumably in Unifont or the bitmap font (unverified).

## Fix

- New pure function `upconf_window_profile(c, want)` in `config/upconf.c` and `config/upconf.h`. It
  returns `want` when the file has that profile, else "default" when the file has that, else 0. It is
  null-safe.
- `upconf_font_fallback` accepts a 0 config, so a missing file also gets the default.
- `apply_profile` picks its profile through `upconf_window_profile` and resolves the font fallback
  before any return. The old call at line 1591 is removed.
- `setup_text` reuses `upconf_window_profile` instead of repeating the selection logic.

The fix lives at the profile-resolution level. A window with no profile now gets every built-in
default, the font fallback included.

## Tests

- `tests/test_upconf.c` `fallback_without_profile`: a comment-only sample, and no config at all, both
  give SymbolsNerdFontMono. With the default font absent, the result is "".
- `tests/test_handler_fallback_order.py`, run by `make test ONLY=make_hygiene`: in `apply_profile`, the
  `upconf_font_fallback` call must come before the first `return`. With the handler checked out at HEAD
  (fix reverted), the test fails. With the fix in place, it passes. The handler is Amiga-only, so this
  is a source-structure test.

## Unverified

- Rig 96's actual ENV:up-term/up-term was not read. The "no profile" state is inferred from
  install.dos and the shipped sample. A user file with `[profile default]` but no `font-fallback`
  already got the default before this fix.
- No rig run. UD6 still needs an eye check on the owner's rig after an install from main.
