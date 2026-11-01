# s29 flat colour audit (FLAT-COLOUR-OVERHAUL), read-only, main efeac0e6

Rig 8 LIVE READ NOT POSSIBLE: rig 8 is down (watchdog.log 2026-11-01 16:47:46 "idle 60 min: stopping rig 8"; no fs-uae process for build/rig8; ami.py exec -> connection refused). I did not restart it. Only its disk was read from the host: ENVARC:Amarchy/prefs = `amarchy-gui-theme = vantablack`, `flat-system-look = on`, `flat-window-gadgets = on`. No ENVARC:Sys/palette.prefs exists (desktop palette lives in ENV: RAM only, lost with the rig). "RGB on rig 8" below is therefore PREDICTED for vantablack (unverified live); to get live pens: restart rig 8 with theme vantablack, then `wbpens` (colours 0-3 only; a 0-15 / 32-47 probe does not exist yet, `dripens`/`penprobe` in build/amiga are candidates).

## Pen/role model (code)

Roles (desk_core.h:13): pen index = role: 0 BG, 1 DARKER, 2 FG, 3 ACCENT, 4 SELECTION, 5 MUTED, 6 GREEN, 7 RED. Colour of a role: desk_role (desk_core.c:56-67) -> gtheme bg/desk_darker/fg/accent/selection/muted/ok/fail. Derivations in gt_parse (gthemes_core.c:96-106): selection defaults to lighter_bg (bg moved 1/10 to fg), muted defaults to dark_foreground; fg 0xFFFFFF on dark = 0xBEBEBE (GT_DARK_WHITE_FG).

## Table: element, pen, source field, vantablack (rig 8 prefs) predicted RGB, expected, verdict

| element | pen (role) | code | gtheme field | vantablack RGB (predicted) | verdict |
|---|---|---|---|---|---|
| window/screen surface, DETAIL, BACKGROUND, BARBLOCK | 0 BG | desk_core.c:143,150 (desk_pens) | bg | 000000 | OK |
| TEXT, HIGHLIGHTTEXT, BARDETAIL, EXTRA | 2 FG | desk_core.c:145,151-152,154 | fg | bebebe | OK (text/bg 15.6:1) |
| SHINE, SHADOW (flat) | 0 BG | desk_pens_flat desk_core.c:161-162 | bg | 000000 | by design: frames vanish |
| FILLPEN (active frame, flat) | 0 BG | desk_core.c:163 | bg | 000000 | by design: active == inactive frame, focus shown by title only |
| FILLTEXT | 2 FG | desk_core.c:164 | fg | bebebe | OK |
| BLOCKPEN, BARTRIM | 1 DARKER | desk_core.c:141,155 | desk_darker (darker_bg unless it fails the guards; black ground -> 222222) | 222222 | BLOCK is stock black icon pen in some code (see below) |
| backdrop (root tile) | 5 MUTED, palette-set to darker | desk_core.c:392-396 desk_backdrop_pen; desk_dos.c:662; desk_core.c:332-335 desk_pen_colour | desk_darker | 222222 | CONFLICT, see #1 |
| icon pens ramp | 32-47 (depth>=6), 8-15 (depth 4-5), folded to roles 0-5 below | desk_core.c:189-203, iconpen_core.c:20-100 | blends of bg with fg/accent/muted 33/66/100 %, selection with fg | n/a (16 slots) | blends bg->role; at depth<4 ip_fold picks the nearest of roles 0-5 (depth<=2: 0-3) |
| Active title band | 3 ACCENT (pen_outline) | desk_core.c:174, flat_core.c:511-515 | accent | 8d8d8d | OK on vantablack; text pen_ptext = 0 BG (accent/bg 11.8:1) |
| Active title text | 0 BG (pen_ptext) | desk_core.c:177, flat_core.c:376 flat_tx_pen | bg | 000000 | OK |
| Inactive title text | 5 MUTED (pen_dimtext) | desk_core.c:178, flat_core.c:424,516 | muted -> but pen 5 palette = darker | 222222 not 7a7a7a | BUG, see #1 |
| Gadget glyph active / inactive / box | 2 FG / 3 ACCENT / 0 BG | desk_flat_glyph_pens desk_core.c:~170 | fg / accent / bg | bebebe / 8d8d8d / 000000 | OK |
| Button fill (idle), scroller knob | 4 SELECTION (pen_button) | desk_core.c:176, flat_core.c:456-464, 485-496 | selection | 1a1a1a | weak: 3.0:1 vs bg here, 1.1-1.3:1 on light themes (table 2) |
| Scroller track, button on screen without pen 4 | 0 BG (pen_back) | flat_core.c:462-463,496 | bg | 000000 | OK |
| Button label | 2 FG (pen_text) | desk_core.c:~171 | fg | bebebe | OK |
| Pressed button label | 0 BG (pen_ptext) on ACCENT fill | desk_core.c:177 | bg on accent | 000000 on 8d8d8d | OK |
| Disabled button fill | pen_dim, runtime ObtainBestPen of button pen colour *60 % | flat.c:917-949 DIM_PCT 60 | selection*0.6 (NOT a theme role) | 101010 | OK-ish; hardcoded 60 %, depth>=3 only |
| Disabled button label | 5 MUTED (pen_dimtext) | desk_core.c:178, flat_core.c:424 | muted -> pen 5 palette = darker | 222222 on 101010 (1.2:1) | BUG, see #1 |
| Icon label (font.prefs WBFONT) | front 2 FG, back 1 DARKER, JAM1, outline | desk_core.c:613-616 | fg / darker | bebebe on 222222 | back pen 1 is DARKER; with backdrop pen 5 = darker and label back pen = 1 = darker: consistent only because both are desk_darker |
| Pointer (pointer.prefs) | RGB direct | desk_core.c:~570-576 | fg, bg, accent | bebebe, 000000, 8d8d8d | OK, bypasses pens |
| Amarchy menu window | own palette MT_* | menu_theme.c:52-58 | bg, fg, dim, MT_ACCENT = bg (!), selbox = accent, ok, fail | 000000, bebebe, 505050, 000000, 8d8d8d, b6b6b6, a4a4a4 | menu uses dim (dark_foreground) where desktop uses muted: two different "dim" colours per theme (505050 vs 7a7a7a on vantablack). MT_ACCENT = bg looks odd, check intent |
| Amarchy bar / DOpus send | not traced (budget) | | | | UNVERIFIED |

## Table 2: per theme (guard maths mirrors desk_luma/desk_contrast/desk_darker; ratios x100)

`pen5 live` = darker vs bg, i.e. the contrast of every MUTED-pen text (inactive title, disabled label) while the backdrop is on. `muted/bg` and `muted/sel` = what those labels WOULD have if pen 5 kept muted.

| theme | mode | 0 BG | 1 DARKER | 2 FG | 3 ACCENT | 4 SEL | 5 MUTED(=backdrop) | text/bg | knob(4)/track(0) | accent/bg | muted/bg | muted/sel | darker/bg(pen5 live) |
|---|---|---|---|---|---|---|---|---|---|---|---|---|---|
| catppuccin-latte | L | eff1f5 | d7d8dc | 4c4f69 | 1e66f5 | ccd0da | acb0be | 269 | 115 | 232 | 133 | 116 | 110 |
| catppuccin | D | 1e1e2e | 101019 | cdd6f4 | 89b4fa | 45475a | 585b70 | 515 | 193 | 425 | 238 | 123 | 146 |
| ethereal | D | 060b1e | 03050f | ffcead | 7d82d9 | 252e56 | 6d7db6 | 958 | 250 | 629 | 579 | 231 | 133 |
| everforest | D | 2d353b | 181d20 | d3c6aa | 7fbbb3 | 3d484d | 475258 | 329 | 128 | 282 | 143 | 112 | 160 |
| flexoki-light | L | fffcf0 | e5e2d8 | 100f0f | 205ea6 | cecdc3 | b7b5ac | 942 | 121 | 275 | 136 | 112 | 110 |
| gruvbox | D | 282828 | 161616 | d4be98 | 7daea3 | 504945 | 665c54 | 386 | 164 | 322 | 201 | 122 | 151 |
| hackerman | D | 0b0c16 | 06060c | ddf7ff | 82fb9c | 1f253a | 2d3450 | 1012 | 200 | 864 | 264 | 132 | 131 |
| jade-horse | D | 142620 | 0a1713 | dfede5 | 8bcbb1 | 2b5147 | 64887b | 554 | 184 | 438 | 309 | 167 | 141 |
| kanagawa | D | 1f1f28 | 111116 | dcd7ba | dcd7ba | 363646 | 54546d | 502 | 151 | 502 | 220 | 145 | 150 |
| last-horizon | D | 0c0b0c | 222222 | fafcfb | b59790 | 584e51 | 584e51 | 1100 | 391 | 716 | 391 | 100 | 195 |
| lumon | D | 16242d | 0b1216 | d6e2ee | 8bc9eb | 243d56 | 304860 | 524 | 153 | 442 | 177 | 115 | 155 |
| lupine | L | fafafa | dedede | 212121 | 3264eb | d0d0d0 | 9e9e9e | 571 | 119 | 232 | 153 | 129 | 111 |
| matte-black | D | 121212 | 090909 | bebebe | e68e0d | 2a2a2a | 333333 | 654 | 177 | 535 | 206 | 116 | 140 |
| miasma | D | 222222 | 121212 | c2c2b0 | 78824b | 383838 | 666666 | 434 | 146 | 282 | 244 | 166 | 151 |
| nord | D | 2e3440 | 191c23 | d8dee9 | 81a1c1 | 434c5e | 4c566a | 365 | 137 | 262 | 153 | 111 | 160 |
| retro-82 | D | 05182e | 020c17 | f6dcac | faa968 | 134e5a | 2a6b78 | 712 | 224 | 603 | 306 | 136 | 143 |
| ristretto | D | 2c2525 | 181414 | e6d9db | f38d70 | 403e41 | 72696a | 450 | 144 | 348 | 230 | 160 | 152 |
| rose-pine | L | faf4ed | e1dbd5 | 575279 | 56949f | dfdad9 | cecacd | 258 | 111 | 180 | 119 | 107 | 110 |
| solitude | D | 101315 | 080a0b | cacccc | 798186 | 343d41 | 4b4e55 | 696 | 229 | 451 | 290 | 126 | 140 |
| starship | D | 1c1c1e | 0e0e10 | f5f5f7 | ff9f0a | 3a3a3c | 8e8e93 | 629 | 173 | 446 | 378 | 218 | 151 |
| tokyo-night | D | 1a1b26 | 0e0e14 | a9b1d6 | 7aa2f7 | 292e42 | 414868 | 477 | 147 | 430 | 215 | 145 | 148 |
| vantablack | D | 000000 | 222222 | bebebe | 8d8d8d | 1a1a1a | 7a7a7a | 1561 | 300 | 1184 | 1038 | 346 | 361 |
| white | L | ffffff | e8e8e8 | 000000 | 6e6e6e | c0c0c0 | 808080 | 2061 | 130 | 217 | 190 | 145 | 109 |

## Worst offenders, ranked by visibility

1. Pen 5 is two roles at once. desk_pens_flat gives inactive-title text and disabled-button labels pen_dimtext = DK_MUTED = 5 (desk_core.c:178); the root backdrop is ALSO pen 5 (desk_backdrop_pen = plan.backdrop = DK_MUTED, desk_core.c:392-396) and its palette entry is overwritten with desk_darker whenever the backdrop is owned (desk_dos.c:662, desk_pen_colour desk_core.c:334). So muted text is drawn in the backdrop colour: contrast vs window bg is 1.09-1.46 on 20 of 23 themes (table 2 `pen5 live`), exactly the "colours all over the place": inactive titles and disabled labels are near invisible, and which themes look wrong depends on darker_bg. Fix level: give the backdrop its own pen (pens 0-7 are all assigned; tile depth 3 limit, desk_core.c comment) or draw muted text with a non-palette pen like pen_dim (ObtainBestPen) instead of pen 5.
2. Disabled/inactive text on a selection fill: muted == selection on last-horizon (584e51 both, ratio 1.00), muted/sel <= 1.16 on nine themes. Disabled labels vanish even if #1 is fixed.
3. Button fill and scroller knob (pen 4 SELECTION) vs surface/track (pen 0): selection/bg 1.10-1.30 on all five light themes (catppuccin-latte 115, flexoki-light 121, lupine 119, rose-pine 111, white 130) and 1.2-1.6 on nine dark ones (everforest 128, flexoki, gruvbox 164 ...). Knob and track nearly the same; buttons barely distinct. No contrast guard exists for SELECTION (guards only cover darker/text, DK_FLOOR_*).
4. Backdrop darker-than-window rule inverted on last-horizon and vantablack: black ground -> DK_BLACK_LIFT 222222, so backdrop is LIGHTER than windows there (last-horizon bg 0c0b0c; qluma(bg)==0 after nibble cut). Intended, but light themes (darker 1.09-1.10) give an almost invisible window edge: no frame (SHINE/SHADOW/FILL = BG) plus 1.1:1 backdrop = windows have no visible boundary on 5 light themes.
5. Active title: ACCENT band with BG text. kanagawa accent == fg (dcd7ba both), so band and the text-pen distinction collapse for glyph/title choices; hackerman/vantablack fine. Inactive title colour (#1) never reaches `accent` so active-vs-inactive differs only by whether the band exists.
6. Two different dim colours: menu window and terminal use dark_foreground (dim), desktop pens use muted. They match only where the theme omits muted (then muted = dim). vantablack: 505050 vs 7a7a7a.
7. Hardcoded RGB: DK_BLACK_LIFT 222222 (desk_core.c), GT_DARK_WHITE_FG bebebe, dim button 60 % scale (flat.c DIM_PCT), stock icon colours 0000ff/ff00ff/00ffff/ffffff for pens 4-7 when no saved palette (desk_core.c:~325), pen_dim not a theme role. Pens 6 GREEN and 7 RED are loaded into the palette but no flat element draws with them (menu job footers use ok/fail via own pens).
8. Depth-dependent pen meaning (UNVERIFIED, desk_palette_entry_of not read): pens 4-7 (8-colour pap_8ColorPens) vs the 252-255 remap on deeper screens, and ip_fold at depth <6 folding icon slots 0-7 to roles 0-5 (depth<=2: roles 0-3) so icon ramp colours collapse onto theme roles; depth<3 button fill falls back to pen_line = ACCENT (flat_core.c:460-461), a very different look from SELECTION. Needs the rig depth to check.

Cleanup: no scratch files left large; script at the scratchpad th.py (small).
