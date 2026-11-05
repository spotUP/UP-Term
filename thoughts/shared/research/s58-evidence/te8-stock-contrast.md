---
date: 2026-11-05
topic: TE-8-STOCK-CONTRAST - which of the 13 contrast pairs fall below the floor in each shipped theme
verdict: 23 themes (22 other files + tokyo-night, which is both the GT_DEFAULT name and a shipped file). Every theme fails 2 to 4 pairs. Pair 8 (button-face on requester-background) fails in all 23 at exactly 100 (identical colour by construction). Pair 1 (inactive title text on inactive bar) fails in all 23 at 101-192. Both are pair-definition or group-default problems, not per-theme palette problems.
---

# TE-8 stock theme contrast

Source of the theme list: `dist/amarchy-themes/*.theme` filtered through `gt_name_of_file` (same directory scan as `test_gthemes_roles` in tests/test_gthemes.c, which asserts 23 files), plus `GT_DEFAULT` = "tokyo-night" (amarchy/gthemes_core.h:19). `gt_preset_model` reads the shipped file first, so the built-in fallback path is not exercised (tokyo-night has a file). Count: 23 distinct themes. Probe: host C file (not in repo) calling `gt_preset_model` then `gt_contrast_pairs`, read function maps any path to dist/amarchy-themes/<basename>. 24 loads x 13 pairs = 312 rows, no load failure. Floors: text 220, muted 220, selection 150.

## Failing pairs per theme (contrast / floor)

| Theme | P1 title-text-inactive / title-bar-inactive (220) | P4 menu-selected-text / menu-selection (220) | P8 button-face / requester-background (150) | P9 scroll-knob / scroll-track (150) | Fails |
|---|---|---|---|---|---|
| catppuccin | 120 / 220 | ok | 100 / 150 | ok | 2 |
| catppuccin-latte | 103 / 220 | 168 / 220 | 100 / 150 | 121 / 150 | 4 |
| ethereal | 119 / 220 | ok | 100 / 150 | ok | 2 |
| everforest | 141 / 220 | 211 / 220 | 100 / 150 | ok | 3 |
| flexoki-light | 122 / 220 | ok | 100 / 150 | 123 / 150 | 3 |
| gruvbox | 113 / 220 | ok | 100 / 150 | ok | 2 |
| hackerman | 192 / 220 | ok | 100 / 150 | ok | 2 |
| jade-horse | 101 / 220 | ok | 100 / 150 | ok | 2 |
| kanagawa | 126 / 220 | ok | 100 / 150 | ok | 2 |
| last-horizon | 140 / 220 | ok | 100 / 150 | ok | 2 |
| lumon | 163 / 220 | ok | 100 / 150 | ok | 2 |
| lupine | 103 / 220 | ok | 100 / 150 | 137 / 150 | 3 |
| matte-black | 168 / 220 | ok | 100 / 150 | ok | 2 |
| miasma | 101 / 220 | ok | 100 / 150 | ok | 2 |
| nord | 142 / 220 | ok | 100 / 150 | ok | 2 |
| retro-82 | 122 / 220 | ok | 100 / 150 | ok | 2 |
| ristretto | 110 / 220 | ok | 100 / 150 | ok | 2 |
| rose-pine | 115 / 220 | 167 / 220 | 100 / 150 | 107 / 150 | 4 |
| solitude | 126 / 220 | ok | 100 / 150 | ok | 2 |
| starship | 112 / 220 | ok | 100 / 150 | ok | 2 |
| tokyo-night | 125 / 220 | ok | 100 / 150 | ok | 2 |
| vantablack | 137 / 220 | ok | 100 / 150 | ok | 2 |
| white | 109 / 220 | ok | 100 / 150 | ok | 2 |

Pairs 0, 2, 3, 5, 6, 7, 10, 11, 12 pass in all 23 themes. Fail count per theme: min 2, max 4 (catppuccin-latte and rose-pine have 4).

## (a) Fail in ALL themes (23 of 23)

- P8 button-face (normal) on requester-background (normal): 100 against 150, identical in every theme. Both elements default to group GTG_SURFACE (gthemes_core.c:541-542, group "surface" default = "background" swatch), so the two colours are the same value unless a theme overrides one. Contrast 100 means equal. The pair can only pass if a theme gives the button its own fill.
- P1 title-text-inactive on title-bar-inactive: 101..192 against 220. Text element is group GTG_MUTED_TEXT (line 528), ground is group GTG_INACTIVE (line 526); group defaults "muted" and "inactive" swatches. Both swatches are low-contrast tones by design (e.g. nord muted #4c566a), so the palette is the cause, with the floor 220 chosen from text-on-ground pairs.

## (b) Fail in most themes (more than half)

None. Next most frequent: P9 in 4 of 23 (catppuccin-latte 121, flexoki-light 123, lupine 137, rose-pine 107), P4 in 3 of 23 (catppuccin-latte 168, everforest 211, rose-pine 167).

## (c) Fail in single or few themes

- P4 menu-selected-text (GTG_TEXT) on menu-selection (GTG_SELECTION): catppuccin-latte 168, everforest 211, rose-pine 167 against 220.
- P9 scroll-knob (GTG_MUTED_TEXT) on scroll-track (GTG_BACKGROUND): catppuccin-latte 121, flexoki-light 123, lupine 137, rose-pine 107 against 150. Light themes: the header comment of GT_FLOOR_SEL already says shipped light themes sit at 110-130.

## Recommendations (mine, not applied)

- (a) P8: remove the pair or redefine it against a ground that can differ (button face vs menu/window background). As defined it is a tautology in every stock theme, so it produces a permanent warning with nothing the user can fix by editing a palette.
- (a) P1: decide whether the inactive title is meant to be legible text. If yes, fix the inactive-group default (palette), if it is deliberately dimmed, give it a lower floor (about 100, since the minimum measured is 101) rather than 220.
- (c) P4 and P9: leave the pairs; they flag real light-theme or rose-pine weaknesses. If they should stay quiet for stock themes, lower GT_FLOOR_SEL for P9 to about 100 (min measured 107) and accept P4 as a true warning on three themes.

## P8 removed

Removed the `{ GTE_BUTTON_FACE, NORMAL, GTE_REQUESTER_BACKGROUND, NORMAL, GTC_SEL }` entry from `gt_cpair_def[]` in amarchy/gthemes_core.c (table now 12 pairs; GT_NCPAIR derives from the size, GT_CPAIRS 16 unchanged). No index-coupled consumer: info_text and gt_contrast_warnings use the `first` index returned by the same call; the only other users are tests/test_gthemes.c, which assert by element, not by number.
Tests (tests/test_gthemes.c): new `test_gthemes_te8_button_face_equals_requester_gets_no_warning`; nord now measures 1 warning (was 2); te8_clean no longer needs to blacken the requester background.
- RED (pair present, tests updated): `[FAIL] gthemes: 6146 checks, 11 failed`
- GREEN (pair removed): `RUNTEST gthemes rc=0 [OK] gthemes: 6146 checks, 0 failed`
- m68k: make of Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs: rc=0
- Per-theme failing count: no survey tool exists; each of the 23 stock themes drops by exactly 1 (P8 failed in all 23).
