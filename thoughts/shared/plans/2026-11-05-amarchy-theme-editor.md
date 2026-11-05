---
date: 2026-11-05
topic: AMARCHY-THEME-EDITOR
status: plan, waiting on owner decisions (last section)
ledger: thoughts/spot/todos.md section "## AMARCHY-THEME-EDITOR" (main checkout, line ~3289)
---

# Amarchy theme editor: implementation plan

The theme editor is a page with subpages inside amarchy-gui. It edits every part of a theme: colours, type, shape, state
colours, desktop and meta (presets, import and export, contrast warning, pen budget meter). Simple mode assigns swatches
per group. Advanced mode assigns swatches per element, and an element override wins over its group. The swatch palette
and the element-to-swatch map are data in the theme file; the role table that `gt_roles_of` builds in code today becomes
a lookup over that data.

Decisions already made by the owner (not reopened here): page with subpages in amarchy-gui, not a separate app; Simple and
Advanced modes; swatches and map are theme file data; scope is "as flexible as possible"; reset-bound settings show in a
live preview pane and reach the real desktop on a prompted Workbench reset; Amarchy has no size budget; reuse, never
rebuild.

Rules the builder follows: RULES.md 1 (portable C89 engine code: no `//` comments, declarations at block start,
`VT_MALLOC`), RULES 4 (one reachability test on the rig), RULES 6 (every Amiga program keeps `__stack` and the `$STACK`
cookie; `make test` runs the stack check), RULES 7-9 (rig etiquette; grep `thoughts/shared/research/test-register.md`
before any rig run and append a row after it). Reachability per `~/.claude/REACHABILITY_PROTOCOL.md`: one rig test drives
amarchy-gui, proves the new code ran through a sentinel counter, and asserts the numbers this plan states.

## 1. Reuse: what was searched and what each part builds on

Searched: `amarchy/gthemes_core.[ch]`, `gthemes_dos.h`, `menu_theme.h`, `themewin.[ch]`, `desk_core.h`,
`iconpen_core.h`, `flat_core.h`, `roundaa_core.h`, `iclabel_core.h`, `wbreset_core.h`, `desk_confirm_dos.h`,
`amconf_core.h`, `leaf.h` + `leaf_core.c`, `prefs/settings_def.[ch]`, `prefs/prefs_ui.[ch]`, `amarchy/gui.c`
(views, `pens_get`, `theme_live`, `preview_sync`), `upconf` caps, `dist/keymap` `[menu]` lines 194-206,
`dist/amarchy-themes/*.theme` (23 files), `Makefile` (`AMARCHY_GUI_SRC`, `test_amarchy_stack`), host tests
`tests/test_gthemes.c`, `test_desk.c`, `test_flat_core.c`, `test_iconpen.c`, `test_iclabel.c`, `test_keymap.c`,
`test_amconf.c`, `test_wbreset.c`, rig scripts `tools/rig/amarchy_theme_save_rig.py`, `amarchy_gui_rig.py`,
`amarchy_desk_rig.py`, plans `2026-10-26-amarchy-prefs-inline.md`, `2026-10-26-amarchy-gui-themes.md`,
`2026-10-29-amarchy-p1a-native-theme.md`, `2026-10-26-amarchy-flat-ui-patch.md` (headings).

Nothing that edits a theme exists yet. Everything else exists and is reused:

| Need | Reused from | New code |
|------|-------------|----------|
| Parse and write theme text | `upconf` (default section + `[profile X]` sections), `gt_parse`, `gt_save_text` | v2 sections read and written by the same functions |
| Role derivation (darker, selection, muted, mix, floors) | `gt_darker`, `gt_selection`, `gt_muted`, `gt_mix`, `GT_FLOOR_*`, `GT_STEP_DARK`, `GT_INACTIVE_PCT` | derived colour refs call these, never copies |
| Role views | `menu_theme_roles_gui`, `desk_pens*`, `ip_colours`, `FLAT_ROLE_*`, `tw_pen` | they keep reading `gt_roles`; only new elements add readers |
| Save + live apply | `gt_dos_select` (ENV + ENVARC, `wb_pens_apply`, DOpus, term bridge `GT_TERM_FILE`), `tw_tick`, `ip_recolour`, `desk_live_follow`, `desk_live_role_pens` | a re-apply of the same theme name after an edit |
| Preview without saving | `gt_dos_preview_send`, `gt_dos_desk_restore`, `gt_preview_term`, gui.c `preview_sync` | preview takes an in-memory theme |
| Settings rows | `sd_setting` table, `SD_COLOUR`, `SD_CYCLE`, `SD_INT`, `SD_FONT`, `SD_CHECK`, `LF_SETTING` + `lf_setting_*`, colour picker (16 entries + "Custom...") | `SD_SWATCH`, `SD_SWATCHREF`, `SD_ELEMENT` kinds, `PG_PAGE_THEME*` pages |
| Menu tree pages | `[menu]` lines `settings.<name> = Label \| settings:<name>`, `sd_page_by_name` | `settings.theme.*` nodes |
| Drawing the preview | gui.c `fill`, `fill_round`, `put`, `draw_row`, `pens_get` / `pens_release`; `roundaa_core` corners | one new view `theme_preview_view` |
| Reset-bound apply | WB `font.prefs` path, IControl path, `AM_FLATSIZE`, `wbreset_core` ("Amarchy.WBReset" clients), `desk_confirm_dos` (`dc_begin`) | one prompt after Save when reset-bound fields changed |
| Shared flat state | `flat_state` "Amarchy.Flat" (`FLAT_VERSION` 4, 36 bytes, `pad[2]` at 34) | version 5 with shape fields and new element pens |
| Rig test | `amarchy_theme_save_rig.py` (stable-box waits, both prefs files read back), `amarchy_gui_rig.py` helpers | `tools/rig/amarchy_theme_editor_rig.py` |

## 2. Theme file format v2

### 2.1 The cap that shapes the format

`upconf` holds at most `UC_MAX_PROFILES` 8 sections and `UC_MAX_KEYS` 40 keys per section, values up to `UC_MAX_VALUE`
160 bytes, files up to `UC_MAX_FILE` 16384 bytes; anything over sets the overflow flag. A theme already has about 30 keys
in the default section. So v2 data goes into its own sections, one key per swatch, group or element, and state colours
ride inside the value. Five sections are used, three stay free. The caps are not raised (shared code; struct size in
every program that links upconf).

### 2.2 Layout

```
# default section: every v1 key stays, written from the resolved groups, so an older Amarchy reads a v2 file as v1
name = Tokyo Night
background = #1a1b26
foreground = #c0caf5
accent = #7aa2f7
...                                  (the ~30 v1 keys, unchanged names)
amarchy-theme = 2                    (absent = v1)
base = tokyo-night                   (theme this one was duplicated from; Reset to Base reads it)

[profile swatches]                   (up to GT_SWATCH_MAX 32 keys; key = swatch name, value = colour)
night = #1a1b26
ink = #c0caf5
sky = #7aa2f7

[profile groups]                     (one key per group; value = normal,hover,pressed,disabled refs)
background = night
accent = sky,mix(sky;ink;30),darker(sky),muted(sky)
text = ink

[profile elements]                   (sparse: only overrides; same value form; Advanced mode writes these)
title-bar-inactive = darker(night)
scroll-knob = sky,mix(sky;ink;30)

[profile look]                       (type, shape, states, desktop; one key each, under 40 in all)
title-font = topaz.font/8
corner-radius = 3
title-bar-height = thin
ghost-style = dim
pointer = #ff5555,#000000,#ffffff
```

Colour ref grammar (one value per state, comma separated, missing trailing states fall back to normal):
`<swatch name>` | `#rrggbb` | `darker(<ref>)` | `selection(<ref>)` | `muted(<ref>)` | `mix(<ref>;<ref>;<percent>)` |
`group:<group key>`. Each derived form calls the existing `gt_darker`, `gt_selection`, `gt_muted`, `gt_mix`. Nesting depth
is capped at 4; a cycle (`group:` back to itself) resolves to the group's default and sets a parse warning.

### 2.3 Loading old themes

No `amarchy-theme` key = v1. The loader builds the v2 model from the v1 keys: one swatch per v1 colour key (named after the
key), each group set to the ref that reproduces today's `gt_roles_of` assignment, no element overrides, look keys at their
current defaults (taken from the `AM_*` prefs the user has now, so loading changes nothing). Proof: a golden test resolves
all 23 shipped themes through the new path and compares every role byte with the old `gt_roles_of` output.

## 3. Data model (amarchy/gthemes_core.h, pure C89)

```
#define GT_SWATCH_MAX 32
#define GT_SWATCH_NAME 16
#define GT_STATES 4                /* GT_ST_NORMAL, GT_ST_HOVER, GT_ST_PRESSED, GT_ST_DISABLED */
#define GT_REF_DEPTH 4

typedef struct gt_ref {            /* one colour reference */
    unsigned char kind;            /* GT_REF_NONE, SWATCH, RGB, DARKER, SELECTION, MUTED, MIX, GROUP */
    unsigned char a, b, pct;       /* swatch index / group id / nested ref slot, mix percent */
    uc_u32 rgb;                    /* kind RGB */
} gt_ref;

typedef struct gt_look {           /* type, shape, states, desktop */
    char font[5][40];              /* GT_FONT_SCREEN, TITLE, MENU, ICON, SHELL ("name.font/size") */
    unsigned char title_align, title_style;
    unsigned char corner_radius, border_width, bar_height, gadget_size, gadget_pad, scroll_width;
    unsigned char knob_min, knob_shape, glyph_set, gadget_order;
    unsigned char ghost_style, hover_on, inactive_pct;
    uc_u32 pointer[3];
} gt_look;

typedef struct gt_model {
    int version;                   /* 1 or 2 as read */
    int nswatch;
    char swatch_name[GT_SWATCH_MAX][GT_SWATCH_NAME];
    uc_u32 swatch[GT_SWATCH_MAX];
    gt_ref group[GTG_COUNT][GT_STATES];
    gt_ref elem[GTE_COUNT][GT_STATES];   /* kind NONE = follows its group */
    gt_ref nested[64];                   /* pool for nested refs */
    gt_look look;
    char base[32];
    unsigned warn;                       /* GT_WARN_CYCLE, GT_WARN_OVERFLOW, GT_WARN_UNKNOWN_KEY */
} gt_model;
```

Groups `GTG_*` (one per current role so v1 maps 1:1): Background, Surface, Text, Muted Text, Accent, Selection, Inactive,
Dim, Success, Failure. Each group's key, label and v1 default ref live in one table `gt_group_def[]`.

Elements `GTE_*`, one table `gt_elem_def[]` with key, full English label, default group, the states it uses, and the
consumer that reads it (menu role, desk pen, DrawInfo slot, flat role, icon slot, terminal). Catalogue (from the ledger
brainstorm; S32-INACTIVE-BAR and the knob are new): Window Title Bar, Window Title Bar Inactive, Title Text, Title Text
Inactive, Window Border, Window Border Inactive, Gadget Glyph, Gadget Glyph Inactive, Scroll Bar Track, Scroll Knob,
Menu Background, Menu Text, Menu Selection, Menu Selected Text, Menu Header, Menu Separator, Requester Background,
Button Face, Button Text, Text Field, Text Cursor, Desktop Backdrop, Icon Label Text, Icon Label Shadow, Icon Label
Outline, Icon Foreground, Icon Accent, Icon Muted, Icon Selection Plate, Helper Window Background, Helper Window Text,
Terminal Background, Terminal Foreground, Terminal Cursor, Success Text, Failure Text. 36 elements, under the 40-key cap;
a host test fails when `GTE_COUNT > UC_MAX_KEYS`.

Resolution: `gt_resolve(const gt_model *m, int elem, int state)` returns the element ref if set, else the group ref of
`gt_elem_def[elem].group` for that state, else the normal state. `gt_roles_of` becomes `gt_roles_from_model`: every
`GR_*` role is the resolved normal state of its group. `gtheme` gains one `gt_model *model` pointer (allocated with
`VT_MALLOC` by `gt_load`, freed by a new `gt_free`), so the stack size of every caller stays as it is; measure `sizeof`
before and after and keep `test_amarchy_stack` green.

## 4. Editor pages (amarchy-gui, full English labels)

New `[menu]` lines in `dist/keymap` (one parent node, nine children):

```
settings.theme          = Theme Editor          | settings:theme
settings.theme.swatches = Swatches              | settings:theme-swatches
settings.theme.groups   = Groups                | settings:theme-groups
settings.theme.elements = Elements              | settings:theme-elements
settings.theme.type     = Type                  | settings:theme-type
settings.theme.shape    = Shape                 | settings:theme-shape
settings.theme.states   = States                | settings:theme-states
settings.theme.desktop  = Desktop               | settings:theme-desktop
settings.theme.files    = Presets and Files     | settings:theme-files
```

`PG_PAGE_THEME`, `PG_PAGE_THEME_SWATCHES` ... `PG_PAGE_THEME_FILES` go into the `PG_PAGE_*` enum in `prefs/prefs_ui.h`
before `PG_NPAGES`; `sd_page_by_name` maps the names. Rows (all `SD_AMARCHY` store, the store writes into the theme being
edited, not `ENV:Amarchy/prefs`):

- Theme Editor: Theme (`SD_THEME`, picks the theme to edit), Mode (Simple / Advanced), Base Theme (read only), Save,
  Save As..., Revert, Apply to Desktop, Contrast (summary line, e.g. "2 warnings"), Pen Budget (e.g. "Screen 16 colours:
  14 of 16 pens, 3 elements share a colour").
- Swatches: one row per swatch (`SD_SWATCH`: name + colour, edited with the existing colour picker and "Custom..."),
  Add Swatch, Rename Swatch, Remove Swatch (refused while a group or element uses it; the row names the users),
  Terminal Palette (the 16 v1 terminal colours, reused rows).
- Groups (Simple mode, shown in both): per group four rows Normal, Hover, Pressed, Disabled (`SD_SWATCHREF`: a cycle over
  swatch names plus Darker, Selection, Muted, Mix... and Custom...).
- Elements (Advanced mode only; hidden in Simple): per element one row (`SD_ELEMENT`) showing "Follows Accent" or the
  override; Return opens the four state rows; Clear Override.
- Type: Screen Font, Window Title Font, Menu Font, Icon Font, Shell Font (`SD_FONT`, ASL through `lf_setting_pick`),
  Title Alignment (Left / Centre / Right), Title Style (Plain / Bold).
- Shape: Corner Radius, Border Width, Title Bar Height (Stock / Thin, today's `AM_FLATSIZE`), Gadget Size, Gadget
  Padding, Scroll Bar Width, Knob Minimum Length, Knob Shape (Square / Rounded), Gadget Glyph Set, Gadget Order (Left /
  Right).
- States: Ghost Style (Dim / Stipple / Both), Hover Highlight (On / Off), Inactive Window Dimming (percent, today's
  `GT_INACTIVE_PCT` 45 as default).
- Desktop: the existing backdrop rows (`AM_DESK*`) reused, Pointer Colours (three colour rows), Icon Label Style (the
  existing `AM_ICONTEXT` row reused), Icon Label Colour (element shortcut).
- Presets and Files: Duplicate Theme..., Import Theme..., Export Theme..., Reset to Base, Delete User Theme.

Preview pane: a new gui.c view `theme_preview_view` beside the page rows. It draws a mock desktop from the model being
edited: an active and an inactive window (title bar, title text in the chosen title font, gadgets with the chosen glyph
set, order, size and corner radius, border width, scroll bar and knob), a menu strip with a selected item, a requester
button, a text field with a cursor, a disabled item in the ghost style, and one icon with its label. It uses `fill`,
`fill_round`, `put`, `pens_get` and `roundaa_core`, and takes its colours from `gt_resolve`, so reset-bound fields
(fonts, bar height) show before any reset. Placement is an owner decision (section 9, D4).

## 5. Apply path per setting

| Setting | Path | When the desktop changes |
|---------|------|--------------------------|
| Swatch, group, element colours | write the user theme file, `gt_dos_select(name)` re-applies: ENV + ENVARC, `wb_pens_apply`, DOpus, term bridge `GT_TERM_FILE`; `tw_tick` retakes helper pens; amarchy-flat `ip_recolour` + `desk_live_follow` | live (on edit or on Apply, D3) |
| Unsaved preview | `gt_dos_preview_send` with the in-memory model; `gt_dos_desk_restore` on Revert or leaving the page | live, undone on Revert |
| Terminal palette | term bridge `GT_TERM_FILE`; console tabs watch it | live |
| Menu font | Amarchy's own font row (`AM_FONT`) | live on the next menu open |
| Screen font, window title font | WB `font.prefs` | Workbench reset (prompt) |
| Icon font | WB `font.prefs` icon font | Workbench redraws icons on its own on 3.1 and 3.2 (verify on the rig in TE-7; fall back to reset-bound if not) |
| Shell font | the UP-Term font row (`SD_UPTERM`) | next new console window |
| Title bar height | IControl prefs + `AM_FLATSIZE` | Workbench reset (prompt) |
| Corner radius, border width drawn by flat, knob shape and minimum, glyph set, ghost style, hover | `flat_state` v5 fields | live: amarchy-flat refreshes the window frames it draws |
| Gadget size, gadget padding, gadget order, scroll bar width | Intuition owns gadget geometry; flat lays out its gadgets from `flat_state` v5 at window open | Workbench reset (prompt); windows opened after the reset |
| Backdrop, icon label style and colour | existing `AM_DESK*` / `AM_ICONTEXT` paths | live |
| Pointer colours | pens 17-19 through `SetRGB32` (and pointer prefs on save) | live |

Corner radius applies only to what amarchy-flat and Amarchy draw (`roundaa_core`); Intuition window shapes stay square.
Border geometry (BorderTop from the screen font) belongs to Intuition and changes only after a reset.

## 6. Pen budget per screen depth

The meter (`gt_pen_budget(const gt_model *m, int depth, gt_budget *out)`, pure) counts the distinct resolved colours of
all elements and states against the pens the depth gives, and names the elements that will share a colour.

| Screen | Pens the theme gets | What happens to more colours |
|--------|---------------------|------------------------------|
| 8 colours (depth 3) | pens 0-7 = the desktop role pens `DK_*` (pens 0-3 from `palette.prefs`, 4-7 live through `desk_live_role_pens`) | every element folds to the nearest of the 8 role colours (the `ip_fold` rule: nearest, ties to the lower pen, never `DK_GREEN` / `DK_BACKDROP`); icon slots all fold |
| 16 colours (depth 4) | pens 0-7 role pens, 8-15 icon slots 8-15; no free pens for flat extras | flat extras (`pen_dim`, `pen_knob`, `pen_inbar`, `pen_sel`) and new elements take `ObtainBestPen` nearest; the meter lists them |
| 32 / 64 (depth 5, EHB counts as 5) | as depth 4 plus pens 16-31 shared with pointer 17-19 | as depth 4 |
| 256 colours (depth 8, AGA or RTG CLUT) | role pens 0-7, pointer 17-19, icon pens 32-47 (`IP_FIRST`), flat extras, menu and helper pens through `vr_pen_obtain` | exact pens up to the free count of the ColorMap; the meter shows "N of M free pens"; past that, nearest |
| Truecolour (RTG 15/16/24 bit) | colours drawn directly; pens still obtained for code that draws with pens | no shortage; the meter says "exact colours" |

The icon-label rule from 351e9c0e stays: label shade pens are obtained and released inside each `Text` call. The meter
never obtains a pen; it reads depth and counts. On a shallow screen Simple mode shows a line per group when two groups
land on one pen.

## 7. Phases and checklist

Every phase: host test written first and seen failing, then the code, then `make test ONLY=<suite>` green and the
project type-check equivalent (`make test` builds with warnings as errors). The reachability test is one rig script,
`tools/rig/amarchy_theme_editor_rig.py`, built in TE-1 as a skeleton and given one `--case` per phase; each case drives
amarchy-gui through its real entry point (Left Amiga + Space > Settings > Theme Editor), makes the edit by keys, reads
the theme file back from `ENVARC:Amarchy/themes/`, and checks the desktop pixels or pens. Sentinel counters are exposed
the same way `gt_dos_wb_pens` is today (grep its print site).

### TE-1 Format v2 and the model (gthemes_core)

Files: `amarchy/gthemes_core.h`, `amarchy/gthemes_core.c`, `tests/test_gthemes.c`, `tools/rig/amarchy_theme_editor_rig.py`
(skeleton: start, open the page, read sentinels).

- [x] TE-1.1 (d10f1b46) `gt_model`, `gt_ref`, `gt_look`, `gt_group_def[]`, `gt_elem_def[]` as section 3.
- [x] TE-1.2 (d10f1b46) Ref grammar parser and writer (section 2.2), depth cap 4, cycle warning.
- [x] TE-1.3 (d10f1b46) v2 reader for `[profile swatches|groups|elements|look]`; unknown keys set `GT_WARN_UNKNOWN_KEY`, kept on write.
- [x] TE-1.4 (d10f1b46) v1 to model derivation (section 2.3).
- [x] TE-1.5 (d10f1b46) Writer `gt_model_save_text`: v1 default keys from the resolved groups, then the v2 sections.
- [x] TE-1.6 (d10f1b46) `gtheme.model` pointer, `gt_free`; measure `sizeof(gtheme)` before and after. Delta is one pointer (8 bytes host, 4 on m68k), measured by construction, not by a host print.

Host tests (fail before, pass after): golden test over all 23 shipped themes, model roles byte-equal to old `gt_roles_of`;
round trip parse, write, parse equal; v2 file loaded by the v1 reader gives the resolved colours; cycle and depth refusal;
`GTE_COUNT <= UC_MAX_KEYS` and a full v2 theme sets no overflow flag.

### TE-2 Roles from the model

Files: `amarchy/gthemes_core.c`, `menu_theme.c`, `desk_core.c`, `iconpen_core.c`, `flat_core.c`, tests for each.

- [x] TE-2.1 `gt_roles_of` calls `gt_roles_from_model`; old code paths removed (no second source). (3923cdd5)
- [x] TE-2.2 `menu_theme_roles_gui`, `desk_pens`, `ip_colours`, `tw_pen` read element refs where an element exists for
  their role (Menu Header, Icon Label Shadow and so on); the group value stays the fallback. (3923cdd5; Icon Label Shadow, Outline, Text open: no colour slot)
- [x] TE-2.3 New element readers: Window Title Bar Inactive (S32-INACTIVE-BAR), Scroll Knob. (3923cdd5)

Host tests: an element override changes exactly the view slot it names and no other (one case per consumer); with no
overrides every view's output is byte-equal to today's for all 23 themes.

### TE-3 Save, Save As, Duplicate, Import, Export (gthemes_dos)

Files: `amarchy/gthemes_dos.[ch]`, `amarchy/gthemes_core.c`, `tests/test_gthemes.c`.

- [x] TE-3.1 Save writes `ENVARC:Amarchy/themes/<name>.theme` (user dir wins over shipped, as today; D2) and `ENV:` copy.
- [x] TE-3.2 `gt_dos_select` re-applies when the name is the current one and the file changed (today it may skip; check
  and test). Sentinel `gt_dos_edit_applies`.
- [x] TE-3.3 Save As and Duplicate with `gt_name_ok`; Import and Export copy through ASL (`lf_setting_pick`), validated
  by the parser before the copy.
- [x] TE-3.4 Delete User Theme removes the user file only; a shipped theme of that name shows again.

Host tests: save text equals the round trip; name refusal; import of a broken file refused with the parser's message.

### TE-4 Editor pages and rows

Files: `dist/keymap`, `prefs/prefs_ui.[ch]`, `prefs/settings_def.[ch]`, `amarchy/leaf.h`, `amarchy/leaf_core.c`,
`amarchy/gui.c` (row kinds), `tests/test_keymap.c`, settings tests.

- [x] TE-4.1 `[menu]` nodes of section 4; `PG_PAGE_THEME*` and `sd_page_by_name`.
- [x] TE-4.2 `SD_SWATCH`, `SD_SWATCHREF`, `SD_ELEMENT` kinds and their `lf_setting_*` handlers, reusing the colour picker.
- [x] TE-4.3 Edit buffer: the page edits one `gt_model` copy; Revert reloads the file; leaving with changes asks Save /
  Discard / Cancel.
- [x] TE-4.4 Mode row hides the Elements page in Simple mode; an override made in Advanced survives a switch to Simple and
  the Groups page marks the overridden elements.
- [x] TE-4.5 Every row of section 4 present, full English labels.

Host tests: menu tree contains the ten nodes; each new row kind steps and sets through `lf_setting_set`; Remove Swatch
refused while used.

### TE-5 Preview pane

Files: `amarchy/gui.c`, a pure layout helper `amarchy/themeprev_core.[ch]`, `tests/test_themeprev.c`, the render fake
(`tests/render_host/gfx_fake.c`) if the draw calls go through it.

- [ ] TE-5.1 `themeprev_layout` (pure): boxes of the mock desktop from the look (bar height, radius, gadget size, order).
- [ ] TE-5.2 `theme_preview_view` draws it with `fill`, `fill_round`, `put`, pens from `pens_get`; sentinel
  `gui_preview_draws`.
- [ ] TE-5.3 Pane placement per D4.

Host tests: layout boxes for stock and thin bar heights and for radius 0 and 6; colours taken from `gt_resolve`.

### TE-6 Live apply and flat_state v5

Files: `amarchy/flat_core.h`, `amarchy/flat.c`, `amarchy/gthemes_dos.c`, `amarchy/gui.c` (`preview_sync`,
`theme_live`), `tests/test_flat_core.c`.

- [ ] TE-6.1 `FLAT_VERSION` 5: corner radius, border width, knob shape and minimum, glyph set, ghost style, hover, the
  inactive bar and knob colours; readers of version 4 refuse version 5 cleanly (existing version check).
- [ ] TE-6.2 Preview path takes the in-memory model (`gt_dos_preview_send`), Revert calls `gt_dos_desk_restore`.
- [ ] TE-6.3 Live timing per D3.

Host tests: flat_state v5 layout offsets; a v4 reader refuses v5; preview then restore leaves pens as before.

### TE-7 Reset-bound type and shape

Files: `amarchy/gthemes_dos.c`, `amarchy/wbreset_core.[ch]`, `amarchy/desk_confirm_dos.c`, `tests/test_wbreset.c`.

- [ ] TE-7.1 Save compares reset-bound fields (section 5 table) with the running ones; when any changed, write font.prefs
  and IControl prefs and show the prompt per D1 through the existing confirm machinery.
- [ ] TE-7.2 Icon font: check on the rig whether Workbench redraws without reset (3.1 and 3.2); record the answer in the
  test register; set its row live or reset-bound from that.
- [ ] TE-7.3 Sentinel `gt_dos_reset_prompts`.

Host tests: the reset decision function returns reset-bound only for the fields of the table.

### TE-8 Meta: presets, contrast warning, pen budget meter

Files: `amarchy/gthemes_core.[ch]`, `amarchy/gui.c`, `tests/test_gthemes.c`.

- [ ] TE-8.1 Presets: the shipped themes and `gt_builtin` as starting points (Duplicate Theme); Reset to Base.
- [ ] TE-8.2 Contrast warning: reuse the contrast measure behind `GT_FLOOR_TEXT` / `GT_FLOOR_SEL` / `GT_FLOOR_MUTED`
  (grep `GT_FLOOR_TEXT` in gthemes_core.c); each text element against the element it sits on; behaviour per D8.
- [ ] TE-8.3 `gt_pen_budget` per section 6; meter row on the top page and per-group share lines on shallow screens.

Host tests: budget counts for depth 3, 4, 8 and truecolour on two fixed models with stated numbers (tokyo-night with no
overrides: depth 3 = 8 of 8, 0 shared beyond the role fold; the test fixes the exact numbers when written); contrast
pairs below each floor flagged, above not.

### TE-9 The reachability test (one feature, one script)

Files: `tools/rig/amarchy_theme_editor_rig.py`, `RULES.md` `## Commands` row, `thoughts/shared/research/test-register.md`.

- [ ] TE-9.1 Main case: rig up on main with every TE phase (`UPTERM_RIG=<n> rig.py start --max --purpose "<session>:
  theme editor reachability"`); open amarchy-gui > Settings > Theme Editor; Duplicate tokyo-night as `te-test`; Swatches
  > set `sky` to `#ff8800`; Save. Assert: `ENVARC:Amarchy/themes/te-test.theme` holds `sky = #ff8800` and
  `amarchy-theme = 2`; both prefs files name `te-test`; sentinel `gt_dos_edit_applies` went up by 1; the active title bar
  pixels read `#ff8800` (or the nearest pen on a shallow screen, from `gt_pen_budget`).
- [ ] TE-9.2 Advanced case: override Scroll Knob only; assert the knob pixels change and the title bar pixels do not.
- [ ] TE-9.3 Reset-bound case: set Title Bar Height to Thin; assert `gt_dos_reset_prompts` +1 and the preview pane bar
  height changes (`gui_preview_draws` +1) before any reset; after Reset Now, the real bar height matches.
- [ ] TE-9.4 Run on 3.1 (`--max`) and 3.2 (`--os 32`) rigs, an AGA mode and an RTG mode; A1200 last. Append each run to
  the test register; stop and clear own rigs after.
- [ ] TE-9.5 `RULES.md` Commands row for the script.

## 8. Order and size

TE-1 and TE-2 first (format and roles; host only). TE-3, TE-4, TE-5 in that order. TE-6 and TE-7 after TE-4. TE-8 any time
after TE-2. TE-9 cases land with the phase they prove; the full run is the finish line. One sonnet builder per phase,
one phase per brief.

## 9. Owner decisions

D1. Reset-bound changes on Save.
- Prompt "Reset Workbench Now / Later" (Recommended): owner controls when windows close; the change waits if Later.
- Reset at once without asking: fastest, but closes open work without warning.
- Save only, note "takes effect after the next reset": no interruption, easy to forget.

D2. Editing a shipped theme.
- Save writes a user copy under the same name in `ENVARC:Amarchy/themes` (Recommended): matches today's "user wins"; Reset
  to Base gets the shipped one back; a later shipped update is hidden behind the copy.
- Force Save As with a new name: shipped themes never shadowed; one extra step on every first edit.
- Allow writing into `UP-Term:Amarchy/Themes`: simplest; an update of the kit overwrites the owner's edits.

D3. When colour edits reach the real desktop.
- Every edit, live, with Revert (Recommended): what you see is the desktop; more pen rewrites while stepping a value.
- Only on Apply to Desktop; the preview pane is live: no desktop churn; the real desktop lags the editor.

D4. Preview pane placement.
- Right of the rows when the window is wide enough, else a Preview row that toggles a full view (Recommended): always
  visible on a big screen; one key on a small one.
- Always a separate view on a key: simplest layout; never side by side with the rows.
- A strip below the rows: works at any width; short, so the mock desktop is small.

D5. Element catalogue in the first build.
- All 36 elements of section 3 (Recommended): matches "as flexible as possible"; more TE-2 consumer work.
- Only elements something draws today plus the inactive bar and the knob: smaller TE-2; the rest is a later row.

D6. States per element.
- Normal, Hover, Pressed, Disabled for every element, with Active and Inactive as separate window elements (Recommended):
  one grammar for all; many rows in Advanced mode.
- Normal and Disabled only: fewer rows; no hover or pressed colours.

D7. Pen shortage on 8- and 16-colour screens.
- Fold to the nearest pen and show it in the meter (Recommended): any theme loads anywhere; colours may merge on shallow screens.
- Refuse Save when the theme needs more pens than the depth gives: no surprises; blocks themes made on a 256-colour screen.
- Offer to merge the closest swatches: guided; edits the owner's palette for them.

D8. Contrast below the floors.
- Warn on the row and in the summary, Save allowed (Recommended): owner keeps control; unreadable themes can be saved.
- Block Save below the floor: no unreadable theme; blocks deliberate low-contrast looks.
- Offer an automatic fix (move toward the floor like `gt_roles_of` does): one key; changes the chosen colour.

D9. Gadget order (left or right) and gadget size.
- Include, laid out by amarchy-flat at window open after a reset (Recommended): full flexibility; the largest flat change in
  the plan.
- Leave out of this plan, ledger row for later: smaller TE-6; two brainstorm items wait.

D10. Per-screen theme (brainstorm meta item).
- Workbench screen only now, ledger row for per-screen themes (Recommended): this plan stays one theme per desktop; other
  public screens keep the Workbench theme.
- Include a theme per public screen name: matches the brainstorm; needs pen holders per screen and a new prefs key.

### Owner answers (2026-11-05)

- D1: prompt "Reset Workbench Now / Later" (decided earlier in the session, not asked again).
- D2: "A new theme should be created". Saving an edited shipped theme creates a new user theme with its own name (the editor proposes a name such as "<Theme> Custom" and the user can change it). The shipped theme is never shadowed or overwritten. TE-3 implements this instead of the same-name ENVARC copy.
- D3: live on every edit, with Revert.
- D4: preview to the right of the rows when the window is wide enough, else a Preview toggle.
- D5: all 36 elements.
- D6: Normal, Hover, Pressed and Disabled for every element; Active and Inactive are separate window elements.
- D7: fold to the nearest pen and show it in the pen budget meter.
- D8: warn on the row and in the summary; Save stays allowed.
- D9: include gadget order and size, laid out by amarchy-flat at window open after a reset.
- D10: include a theme per public screen name in this plan. This needs pen holders per screen and a new prefs key; the phase list must gain a per-screen phase (TE-10) before TE-9's full run, and TE-9 gets a per-screen case.
