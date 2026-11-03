# RL8 FLAT-COLOUR-OVERHAUL: sizing (read-only, 2026-11-03)

Method: grep for SetAPen/SetBPen/pen_ fields/RGB literals and DK_/MT_/IP_ role use per file under amarchy/, then ranges read. Counts are grep counts (spelling), not a proof of completeness: a second pass from the builders (desk_pens_*, ip_colours, menu_theme_roles_gui, flat_glyph_pen) is part of step 1 below.

## 1. Does a role table exist?

Yes, four of them, in parallel, all fed by one struct:

- gtheme (amarchy/gthemes_core.h:20): bg, fg, dim, accent, ok, fail, dark_bg, darker_bg, lighter_bg, selection, muted. 11 colours, derived with fallbacks in gt_parse. This is the source.
- DK_* desktop roles (amarchy/desk_core.h:15): DK_BG, MUTED, FG, ACCENT, SELECTION, DARKER, GREEN, RED (8 pens, pen index = enum value). desk_role (desk_core.c:85-93) maps gtheme to colour. desk_pens/desk_pens_flat (desk_core.c:148-172) assign DrawInfo slots; flat_state pens at desk_core.c:195-204. DK_RED is reused as the backdrop pen (desk_core.c:218), a role name that does not match its use.
- MT_* menu roles (amarchy/menu_theme.h:11-21, menu_theme.c:6-17): BG, TEXT, DIM, ACCENT, SELBOX, OK, FAIL (7). menu_theme_roles_gui (menu_theme.c:51-59) maps gtheme. MT_ACCENT is the background colour (selected row text), MT_SELBOX is the theme accent: names invert the meaning.
- IP_* icon slots (amarchy/iconpen_core.h:16-19): 16 slots = FG/ACCENT/MUTED at 33/66/100 % blended from the ground, plus SELFG, SELBG, 3 spare. ip_colours (iconpen_core.c).
- FLAT_ROLE_BACK/TEXT/LINE/FRAME (amarchy/flat_glyph.h:28), mapped to pens by flat_glyph_pen (flat_glyph.c:323).
- No wb_pens_plan file exists in the tree (searched root and thoughts/shared/plans names). bar_core.c has no colour code at all (0 MT_/DK_/RGB hits); the bar draws through themewin.c tw_pen with MT_ roles.

So the change is unification of 4 role vocabularies into one 5-6 role table plus derived tones, not creating a table.

## 2. Colour picks outside one table

| Element | file:line | What it uses |
|---|---|---|
| frames (window border, active fill) | desk_core.c:165-172 | DK_BG/DK_DARKER/DK_FG assignments per DrawInfo slot; flat variant sets shine=shadow=fill=BG |
| frames | flat.c:234 flat_frame_pen, flat_core.c:542 | depth-dependent fallback pen choice |
| titles | desk_core.c:172 (FILLTEXT=DK_FG), flat.c:439-451 (band_pen/text_pen/bg_pen from flat_tx), flat_core.c:584-588 | DK roles + depth fallbacks, dimtext for inactive |
| text | flat.c:450,461,586,622,636 | fg/bg taken from the caller's RastPort pens (stock), not roles |
| sliders / scroller | flat.c:514-529, flat_core.c:446-538 | pen_knob = g_knob_pen (flat.c:1139-1150), set 0 on fallback (flat.c:1134,1169-1170); DK_SELECTION contrast-pushed (desk_core.c:63-78, DK_FLOOR_SEL 150) |
| buttons | desk_core.c:195-204; flat.c:334, flat_core.c:482 | pen_button=DK_SELECTION, pen_ptext=DK_BG, pen_outline=DK_ACCENT, pen_dimtext=DK_MUTED; pen_dim/pen_knob are amarchy-flat's own obtained pens (flat.c:1134-1160) |
| glyphs | flat_glyph.c:23,256,276-277,323 | FLAT_ROLE_* + frame_pen override |
| icons | iconpen_core.c:~28 IP_ refs, icons.c (3 IP_ refs) | blend ramps from ground (33/66/100) of FG, ACCENT, MUTED; fold to DK pens on shallow screens (iconpen_core.h:33) |
| menu | gui.c: 41 MT_ refs; fallbacks gui.c:1319-1346 (is_bg ? pen 0 : pen 1, pen[fb]); gui.c:445 MT_DIM for level text; gui.c:462,471,497-563 MT_ACCENT/MT_DIM/MT_OK/MT_FAIL | all via g->pen[role], but 7 roles, border and header tied to MT_SELBOX (menu_theme.h:24-32) |
| menu text | menu_text.c:118-152 (1 RGB literal, op_nearest_pen, MT_INK_RGB path) | nearest-pen search on RGB |
| bar | bar.c:153-154 (MT_BG, MT_SELBOX, MT_TEXT); themewin.c:80-189 tw_pen(role) (8 MT_ refs) | menu roles reused for the bar |
| start logo | not found in bar.c/bar_core.c by grep "logo"; icons via menu_icon.c/icon_gfx.c have 0 colour code | unverified: logo is drawn with a text segment in MT_SELBOX if s->accent (bar.c:154) |
| backdrop | desk_core.c:218, desk_dos.c:701 | theme darker tone through DK_RED/pen 7 |
| palette write | desk_dos.c:608 (SetRGB32 loops) | applies the DK table, 1 site |
| other | desk_core.c: 5 RGB literals (floors, DK_BLACK_LIFT 0x222222UL at :29) | constants |

Counts of role-vs-literal hits per file (grep): gui.c MT 41 / literal 0; desk_core.c DK 115 / literal 5; iconpen_core.c IP 28 + DK 4; themewin.c MT 8; menu_text.c MT 5 + literal 1; bar.c MT 3; icons.c IP 3; flat_core.c DK 2; flat.c, flat_glyph.c, bar_core.c, icon_gfx.c, menu_icon.c 0 role symbols (they take pens from flat_state or the caller). Hardcoded pen NUMBERS: flat.c:1134,1139,1169,1170 (pen_dim/pen_knob = 0), gui.c:1322/1328 (pen 0 / pen 1 fallbacks), desk_core.c pen index == enum value (the 0..7 contract). RGB literals outside the theme files: about 6 in total, all in desk_core.c/menu_text.c.

The expected conclusion: very few raw RGB literals; the colour mess is the number of role vocabularies and the per-element derivations (selection push, darker 80 %, icon 33/66 blends, MT_DIM, muted), not hardcoded colours.

## 3. Themes

23 shipped themes in dist/amarchy-themes/*.theme (Omarchy colors.toml format, plus built-in tokyo-night in gt_builtin, gthemes_core.h GT_DEFAULT). User themes: ENVARC:Amarchy/themes. Colour data is in those files only; the derived keys come from gt_parse. (The count of 322 `.theme` files from find includes worktree copies; ignore.) A "same rule in all themes" overhaul changes derivation code, not the 23 files, unless the owner wants their keys trimmed.

## 4. Tests pinning colours

tests/test_desk.c (1719 lines, 220 DK_/colour refs; backdrop-pen and selection tests at :688, :848), tests/test_iconpen.c (543 lines, 100 refs; :53 backdrop), tests/test_gthemes.c (773, 94 refs; asserts MT_HEAD_ROLE carries each theme accent), tests/test_keymap.c (4520, 54 refs; MT_BORDER_ROLE == MT_SELBOX, default palette equals xterm), tests/test_pens.c (152), tests/test_flat_core.c, tests/test_bar.c (5), plus rig tests test_rig_themes_live.py, test_dist_amarchy_themes.py, test_flat_budget.py. Python rig tests check pixels and need a rig.

## 5. Estimate

About 12-15 source files touched (gthemes_core, desk_core, menu_theme, iconpen_core, gui, themewin, bar, flat, flat_core, flat_glyph, menu_text, desk_dos plus headers), roughly 900-1500 changed lines in code and 600-1000 in tests (the pinned numbers in test_desk/test_iconpen/test_gthemes get re-derived). Size: large, three to four agent steps, strictly sequential because every step moves the same table.

Steps:
1. Role table (risk: high). One 5-6 role struct (background, surface, text, muted, accent, plus derived) in gthemes_core with the single derivation rule for all themes; DK_/MT_/IP_/FLAT_ROLE become views of it. Rename inverted names (MT_ACCENT, MT_SELBOX, DK_RED-as-backdrop). Touches gthemes_core, desk_core, menu_theme, iconpen_core and their tests. Risk: pen index == enum contract (0..7) and palette.prefs layouts; a wrong mapping breaks Workbench colours on a real screen. Include the second-pass count from the builders.
2. Frames, titles, text, sliders (risk: high). desk_pens_flat, flat_core frame/title/scroller/button pens, flat.c knob/dim pens, flat_glyph roles. Risk: the flat patcher runs inside other tasks' Intuition calls; depth fallbacks (few-colour screens) change; needs rig pixel checks per the visual-proof rule.
3. Menu and bar (risk: medium). gui.c, menu_text.c, themewin.c, bar.c move to the new roles incl. fallbacks and OK/FAIL footers. Risk: contrast on light themes, the minimal-menu two-colour mode (menu_theme_minimal).
4. Icons and start logo (risk: medium-low). iconpen_core ramps from roles; icon folds on 2-5 plane screens. Risk: anti-alias edge on ground, fold collisions at depth 2-3.

Unverified: start-logo drawing path (grep only), bar_core.c colour (none by grep), wb_pens_plan (no file found).

## 6. Step 1 done (2026-11-03): the role table

### Recount from the builders (before step 1)
Counted from the builder bodies, not grep:
- desk_role (feeds desk_pens / desk_pens_flat / desk_palette / desk_pen_plan): 8 pens from 8 picks (bg, muted, fg, accent, desk_selection, desk_darker, ok, fail), plus the live backdrop override in desk_pen_colour (desk_darker when flat, muted when not): 9 picks.
- ip_colours: 13 slots blended from 5 inputs (ground = the backdrop colour, fg, accent, muted, selection), all via desk_role.
- menu_theme_roles_gui: 7 slots from 6 sources (bg twice, fg, dim, accent, ok, fail). dim is used by no other builder of the four.
- flat_glyph_pen (desk_flat_glyph_pens): 4 roles on pens DK_FG, DK_ACCENT, DK_BG plus frame_pen.
- Distinct theme colours across the four: 9 (bg, fg, dim, muted, accent, selection, darker, ok, fail) = 7 UI colours + 2 status tones. After step 1: 6 UI roles + 2 status tones (dim is gone from the menu).
- Outside the four (found while recounting, NOT moved in step 1): dopus_theme_commands (menu_theme.c:81, DOpus palette bg/fg/dim/accent) and wb_pens_plan (menu_theme.c:204, bg/fg/dim/accent with its own wb_luma/wb_contrast_x100 maths). Both still read t->dim. Pinned by test_keymap.c (th.dim at 3646, 3737).

### The table
gthemes_core.h/.c: `enum { GR_BACKGROUND, GR_SURFACE, GR_TEXT, GR_MUTED, GR_ACCENT, GR_SELECTION, GR_ROLES }`, `gt_roles { c[GR_ROLES]; ok, fail }`, one derivation `gt_roles_of`:
BACKGROUND = gt_darker(t), SURFACE = bg, TEXT = fg, MUTED = muted, ACCENT = accent, SELECTION = gt_selection(t); ok/fail = green/red status tones (not UI roles).
The colour maths moved from desk_core to gthemes_core under gt_ names (gt_luma, gt_contrast, gt_ground_black, gt_darker, gt_selection, GT_FLOOR_TEXT/GT_STEP_DARK/GT_FLOOR_SEL/GT_BLACK_LIFT); DK_FLOOR_FILL stays in desk_core.
Views: desk_role (DK_*) and menu_theme_roles_gui (MT_*) read gt_roles_of; desk_pen_of_role(GR_*) gives the DK pen of a role; desk_flat_glyph_pens (FLAT_ROLE_*) and ip_colours/ip_fold (IP_*, via desk_role) go through it.
Renames: MT_ACCENT -> MT_SELTEXT (it is the background colour), MT_SELBOX -> MT_THEME_ACCENT (it is the accent), DK_RED -> DK_BACKDROP (pen 7 is the Workbench backdrop pen, nothing draws red with it).
Unchanged: pen index == DK_* enum (0..7); palette.prefs bytes (pen 7 still writes the fail tone, pinned in test_gthemes_role_table).

### Changed test expectations
- test_gthemes.c:208 test_gthemes_roles: rgb[MT_DIM] == shipped dim -> == t.muted. Reason: the menu's dim role is GR_MUTED, the same muted every other view uses (one derivation).
- test_gthemes.c:331 knob audit: the knob that moves MT_DIM is u.muted, not u.dim. Same reason.
- No other value changed; DK/IP/flat tests only got the renames (DK_RED, desk_* maths names).

### What step 2 must know
- MT_DIM got darker/lower-contrast on some themes (tokyo-night 565f89 -> 414868 on 1a1b26). Check menu dim-text contrast in step 3 (raise via gt_contrast against GR_SURFACE in gt_roles_of if needed, not per element).
- The backdrop colour is still split: palette.prefs pen 7 = fail; live desk_pen_colour = gt_darker when flat, muted when not; desk_dos.c overwrites the screen backdrop with gt_darker. GR_BACKGROUND is the intended one; changing pen 7's file colour changes palette.prefs bytes (state it in that commit).
- dopus_theme_commands and wb_pens_plan still pick dim directly and wb_pens_plan duplicates the contrast maths; fold them into gt_roles_of / gt_contrast.
