# Backdrop pen fix (s28): rig 8 steps

Pen chosen: 5 (DK_MUTED). A 3.2 WBPattern tile has 3 bitplanes (PT_DEPTH_MAX), so it can name pens 0-7 only. Pens 0-3 are the stock
icon pens (pen 1 = BLOCKPEN/BARTRIMPEN, icon black), pen 4 is the flat button fill (DK_SELECTION), the flat DrawInfo array uses only pens
0, 1, 2. Pen 5 is in the tile range, not 0-4, in no DrawInfo slot. Pen 1 is no longer touched. On a screen under 8 colours no pen 5
exists: the palette is not recoloured and the tile reads pen (5 & mask) = 1.

Expected RGB: palette entry 5 (PALT pen 5) = desk_darker(active theme) = theme darker_bg when it passes the contrast checks, else
bg scaled down, else 0x111111-class lift for a black ground (DK_BLACK_LIFT). Pen 1 RGB must be identical before and after flat on.
Values per theme: read from the theme's darker_bg in the gtheme table (amarchy/gthemes*), then compare after 12-bit nibble cut.

## Steps (rig 8, 060, 3.2, `rig.py start --max --os 32 --purpose "Amarchy s28: backdrop pen 5 check"`)
1. Delete ENV:Sys/wbpattern.prefs and ENVARC:Sys/wbpattern.prefs. Note pen 1 and pen 5 RGB (GetRGB4 via wasabi).
2. Flat on only (desktop-theme pattern target off). Wait for the Workbench reset.
3. Pick theme A. Measure root backdrop pixel RGB vs window interior RGB: must differ; backdrop = pen 5 = desk_darker(A); pen 1 RGB unchanged from step 1;
   an icon's black still black.
4. Pick theme B: backdrop RGB changes to desk_darker(B), tile file unchanged (ENV:Sys/wbpattern.prefs identical bytes), window interior follows B bg.
5. Open Prefs:WBPattern: the ROOT tile shows a solid pen-5 fill. Cancel.
6. Flat off: palette and wbpattern.prefs return to the step 1 snapshot (file deleted again if it was absent); pen 5 RGB back to the step 1 value.

Binaries (host test desk: 1795 checks, 0 failed; revert to pen 1: 3 failed):
Amarchy e538214c24e0b39fadc1ebfcc0cdd697, amarchy-flat ee819391e053e356f09a241909dc8ec7, amarchy-gui 3d263a51ed8df0604ba128ffbef25913.
