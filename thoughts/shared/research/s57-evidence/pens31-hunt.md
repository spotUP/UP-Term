---
date: 2026-11-04
session: s57 (Amarchy)
topic: 3.1 DrawInfo pens and font specialdrawmode after the desk palette apply
rig: 87, --os 31 (Kickstart 40.68, Palette 40.2, Font 40.2, IPrefs 40.7), build origin/main 31ca8e69
status: answered, no product change
---

# 3.1 DrawInfo pens after the desk apply (rig 89 FAIL on s56-deskrig 1651b99f)

## Verdicts

| item | verdict | deciding evidence |
|---|---|---|
| DrawInfo pens stayed `0 1 2 0 0 0 2 0 2 2 0 2` | (c) wrong expectation / unclean setup | that line is EXACTLY the FLAT pen map's 4-colour list (`desk_expect <theme> palette 8 flat`: 4col `0 0 2 0 0 0 2 0 2 2 0 0`) with slot 1 = 1 and slot 11 = 2 (both 3.1 behaviours below). So on rig 89 the screen DID reset and IPrefs DID apply Amarchy's palette: the flat one, because `amarchy_gui_rig --setup` failed to turn flat off (register row). It is also not stock: stock 3.1 is `0 1 1 2 1 3 1 0 2 1 2 1`. |
| slot 1 (BLOCKPEN) = 1, not the file's pen | (b) 3.1 limitation | stock Palette 40.2 USE of the desk file (slot 1 = 0xFEFD in its 8-colour list, 0 in its 4-colour list): DrawInfo slot 1 = 1. Also with the raw file in ENV: slot 1 = 1. Intuition keeps the screen's BlockPen there. |
| slot 11 (BARTRIM) | (b) Palette 40.2 rewrites it | file in 0 / 5, editor's ENV: file out 2 / 2 |
| slot 1/4 = `5` in the want line | (c) | pen 5 is depth dependent: depth 2 uses the 4-colour list (5 folded to 0 by desk_pen_fold); depth 8 the editor writes PEN_C2 (0xFEFD) and Intuition resolves it to 253 (desk_palette_pen(5, 8) = 253). Never a literal 5 on 3.1. |
| font specialdrawmode expected 1, got 0 | (c) | Amarchy writes amconf_icon_text(prefs); icon-text default is `plain` (0, amconf_core.h:37) and rig 89 ran with empty ENV:Amarchy/prefs. desk_expect hardcodes DK_ICONTEXT_OUTLINE. Not (b): stock Font 40.2 `FROM f USE` with byte 19 = 1 writes ENV:Sys/font.prefs byte-identical (518 bytes, front/back/draw/special 2 5 0 1). NDK 3.2 names byte 19 fp_SpecialDrawMode; in 3.1 it is the pad before fp_TextAttr, round-tripped. Whether WB 40.42 draws outline from it: not measured. |

## Measurements, rig 87 (3.1, Workbench depth 8 on this rig)

1. Baseline: `scr: 10043DB8`, `pens: 0 1 1 2 1 3 1 0 2 1 2 1`, ENV:Sys holds only screenmode + wbconfig.
2. Desk palette (catppuccin-latte, desk_pens, desk_palette 8 colours, cut to the 3.1-safe 12 pens + ffff, the bytes desk_palette_31safe makes):
   file 4col `0 0 2 2 0 3 0 0 2 2 0 0 ffff`, 8col `0 5 2 2 5 3 0 0 2 2 0 5 ffff`.
   `SYS:Prefs/Palette FROM T:p31.prefs USE` (= dk_put / dk_editor, Amarchy's path):
   - screen RESET: `scr: 10043DB8 -> 10002770`, no requester (nothing of ours on the screen)
   - DrawInfo `0 1 2 2 253 3 0 0 2 2 0 2`, rgb of 253 = d7d8dc (the theme colour), pen 5 itself = ff00ff
   - ENV:Sys/palette.prefs as the editor wrote it: 4col `0 0 2 2 0 3 0 0 2 2 0 2 ffff`, 8col `0 fefd 2 2 fefd 3 0 0 2 2 0 2 ffff`
3. Same raw bytes copied straight to ENV:Sys/palette.prefs (no editor; how dk_palette_31_fix writes ENVARC/ENV):
   DrawInfo `0 1 2 2 5 3 0 0 2 2 0 5`, same screen address. Literal pen 5 = ff00ff magenta at depth 8.
4. Font: see table.

## What the script should expect on 3.1

- Build the want from the pen list IPrefs will use: 4-colour list below depth 3, 8-colour list otherwise; map 4-7 with
  desk_palette_pen(pen, depth) (depth 8: 5 -> 253), slot 1 = 1, slot 11 = 2 (Palette 40.2). For desk pens (non-flat):
  depth 2 `0 1 2 2 0 3 0 0 2 2 0 2`; depth 8 `0 1 2 2 253 3 0 0 2 2 0 2`.
- Use desk_pens_flat when flat is on (or make --setup's flat-off a hard precondition).
- Font: pass the run's icon-text value to the font expectation instead of hardcoded outline (or set icon-text outline in the setup).

## Found (outside the question)

- Raw 3.1-safe bytes in ENV:/ENVARC: (dk_palette_31_fix, the start conversion and the ENVARC copy) carry literal pen 5 in the
  8-colour list; IPrefs 40.7 reading them directly (e.g. at boot from ENVARC) puts slots 4 (SHADOW) and 11 (BARTRIM) on pen 5
  = magenta at depth 8 (measured step 3). The editor path does not (it turns 5 into PEN_C2). Boot-time effect on 3.1 not measured.
