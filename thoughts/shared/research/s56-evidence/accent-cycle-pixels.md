---
date: 2026-11-04
topic: S56 accent fill and cycle plain fill, pen-index pixel check
rig: 86 (060 max, AmigaOS 3.2, Workbench 640x256 depth 8), main 37b5e3d2
status: FAIL for S56-PRIMARY-ACCENT-FILL; S56-CYCLE-PLAIN not confirmed
---

Setup: worktree build (amarchy-flat md5 2b5873e2d1e195803d2edf62f60257ca, RAM:amarchy-flat md5 equal), palette-flat-pen8.prefs USE, amarchy-flat, then UP-Term:bin/Amarchy (tokyo-night: pen0 1a1b26, pen2 a9b1d6, pen3 7aa2f7, pen4 3e4254). STATUS: flat=on, pens text=2 line=3 back=0 outline=3 button=4 ptext=0, clone draws>0 fallback=0. Pens read with build/amiga/wbpenprobe (ReadPixel pen indices) per gadget box from the UI tree. Windows opened after flat was running. Screenshots: ~/Desktop/upterm-shots/s56-pix86-*.png.

## 1. S56-PRIMARY-ACCENT-FILL: FAIL
Expected Save/Use fill pen 3 (7aa2f7), label pen 0 (ptext). Measured, identical in Locale, ScreenMode, Input, Serial, Sound prefs:
- Locale Save (12,181 88x14): pen4 1136, pen2 84 (label), pen0 12 (corner cuts). Use (232,181): pen4 1155, pen2 65, pen0 12. Cancel (452,181): pen4 1113, pen2 107, pen0 12.
- ScreenMode Save (12,184), Input Save (12,188), Serial Save (12,178), Sound Save (12,184): the same 1136/84/12.
- Zero pen3 pixels in any Save/Use box. Cancel is not the outlined secondary look either (previous build 386666d6: pen0 933 + outline 299; now pen4 filled 1113, no outline).
Every Save/Use/Cancel is drawn with role PLAIN (pen_button fill, stock text pen 2): the S56-CYCLE-PLAIN role filter (amarchy/flat.c role_of, flat_push_label) takes the GadTools push buttons for non-push gadgets. Suspect: GadTools button GadgetText offsets fail flat_push_label(has_text, lx>=0, ly>=0, lx<w, ly<h). Unverified: GadgetText values were not read on the rig.

## 2. S56-CYCLE-PLAIN: not confirmed
Locale has no cycle gadget; ScreenMode, Input, Serial show none in the UI tree (kind is reported "button" for all). Sound Preferences "Sound Type:" (216,80 180x14), a cycle by position: fill pen 255 (rgb 0e0e14) 2427 px, pen1 (414868) 93 px, not pen4 (3e4254), no accent pixels. The fill is not the pen_button of the Save buttons; identification of the gadget as a GadTools cycle is by label, unverified. Verdict: cannot PASS; pen is neither accent nor pen_button.

## 3. S56-LOCALE-SAVE-FILL: PASS (fill is not pen 0)
Locale Save (12,181 88x14): 1136 px pen 4, 0 px pen 0 apart from 12 corner cuts. Side effect of finding 1: Save is plain pen_button, not accent.

## 4. Image-only / unlabeled buttons
- Scroller arrow buttons, e.g. Locale (150,60 18x9) / (150,69 18x9) / (520,157 18x8), Input (594,157), ScreenMode (278,89): pen4 140, pen2 18 (arrow glyph), pen3 2, pen0 2 (18x9): plain pen_button fill.
- ScreenMode palette swatch box (204,142 92x14): pen0 974, pen3 314 (outline plus swatch), no pen4.
- Sound unlabeled (216,136 180x14): pen0 2136, pen3 384 (waveform display).
- Sound "Test Sound" (112,156 200x14) and "Select Sample..." (30,136 184x14): pen255 fill, pen1 label; Disabled state unverified.
