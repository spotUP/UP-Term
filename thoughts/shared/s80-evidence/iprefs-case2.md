# IPREFS case 2 (Shell open) hunt, S81

Rig 8 (os 32, --no-dopus, main 07b1e6a0 build/amiga/Patina 15:55). No code change; branch s81-iprefs-case2 created and removed (tip = main 07b1e6a0).

## Cause (proven by bisect on the rig)
With a Shell open, ANY stock-editor USE of an IPrefs-watched file raises the IPrefs System Request (Retry|Cancel), whatever the content:
| command (Shell open, identical-or-new bytes) | requester after 10 s |
|---|---|
| SYS:Prefs/WBPattern FROM T:patina-desk/wbpattern.prefs USE (Patina's file) | YES |
| SYS:Prefs/Pointer FROM T:patina-desk/pointer.prefs USE (Patina's file) | YES |
| Pointer / WBPattern / Font / Palette FROM ENV:Sys/<same>.prefs USE (unchanged bytes) | YES, each |
Control: same picks with no foreign window (case 1, MeasureShell closed): DESKMASK 15, 4 writes, no requester.
So IPrefs resets (CloseWorkBench) on every notification of these files; the Shell blocks it.
The deferred path (desk_dos.c dk_apply ~516-527) drops only DT_FONT, and DT_PALETTE only for DESK_DEFER_SKIP. In case 2 DESKMASK=6 = WBPATTERN + POINTER: both still written (dk_put at ~555 and ~566) -> requester. The defer treats only "pen map change" as a reset trigger; it is any write.

## Fix (not implemented, owner decision needed)
When dk_foreign_windows() > 0, no IPrefs-watched write may happen: clear mask bits PALETTE, WBPATTERN, POINTER, FONT (not just font/palette-for-map), return 0 with the pick remembered, and apply when the windows are gone (needs a retry trigger: today "the next apply" only). Alternative: live-only colours via screen LoadRGB32 for the palette part and defer the rest. Gate on foreign count BEFORE the pen-reset test (the foreign test is also valid when no map change is needed: any USE resets).
Unit test: desk_defer_map/ a new mask-gating helper in desk_core.c (fail-before: foreign>0 leaves WBPATTERN|POINTER in the mask).
Cleanup in same fix commit: remove PATINA_FOREIGN probe (owner answer S81).

## Why pens and font.prefs do not change
- Pens: palette.prefs written by Patina holds 8-colour pens 0 253 2 2 253 3 0 0 2 2 0 253; live Workbench pens are 0 1 2 2 253 3 0 0 2 2 0 253. Only BLOCKPEN (index 1) differs: file 253 (0xFEFD), live 1, before and after every pick, and after a plain Palette USE with no Shell (Palette 47.21, wait 15 s, pens and screen address unchanged). So desk_pens_reset() stays true forever: every pick closes windows, waits for a reset that never changes pen 1, and with a foreign window always defers. Cause of IPrefs/Intuition not adopting BLOCKPEN 253 on 3.2: NOT determined (unverified whether the screen resets at all; address 10010630 constant).
- font.prefs: 518 bytes is the fixed file size, so length never shows a change. ENV font.prefs names are topaz.font x3 after picks; no themes/*.yaml mentions a font, so desk_font_name changes nothing for these themes (themes tried: tokyo-night, vantablack, white). Unverified for a theme that defines a font.

## Scratch
Rig 8 stopped. Deleted build/rig-saved/6/20261114-143637 (8.9 MB) and my rig-8 save (18.8 MB). Scripts in session scratchpad: fp.py bis.py bis2.py c1.py pal.py pen.py.
