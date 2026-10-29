# P1a prefs probe (rig 7, OS 3.2.2, Kickstart 47.115, Workbench 47.4)

Status: R1.1a and R1.1f done; R1.1b partial; R1.1c, R1.1d, R1.1e NOT probed (budget ran out). Rig prefs restored.

## Rig facts
- Workbench screen 640x256, depth 8 (256 colours), title "Workbench Screen".
- Start state: ENV:Sys has pointer.prefs, screenmode.prefs only. ENVARC:Sys has def_*.info, WBConfig.prefs (54 bytes,
  old WBCF chunk, not WBNC), Pointer.prefs (6012 bytes, 41 PNTR chunks of 137 bytes), screenmode.prefs.
  No palette, wbpattern, font or workbench prefs exist on a fresh rig.

## R1.1a palette.prefs (verified)
Golden: tests/data/desk/palette-gui-save.prefs (434 bytes, md5 8b977a3adafe1d45f2b8ab95fd70f653), saved by
SYS:Prefs/Palette Save. ENV:Sys and ENVARC:Sys copies are identical.
- FORM PREF containing PRHD (6 bytes, all zero) and PALT (400 bytes). No other chunk.
- PALT = NDK PalettePrefs exactly: 16 reserved zero bytes, pap_4ColorPens UWORD[32], pap_8ColorPens UWORD[32],
  pap_Colors ColorSpec[32] (8 bytes each: WORD index, UWORD R, G, B at 16 bit).
- Only 8 colour entries (index 0-7) written even on a depth-8 screen, then index 0xFFFF terminator.
- Both pen lists: 0,1,1,2,1,3,1,0,2,1,2,1,2 then 0xFFFF (13 pens, 3.2 default 3D pens).
- Default colours: 1=000000, 2=ffff, 3=6688bb, 4=ee44444 style (61166,17476,17476), 5=(21845,56797,21845),
  6=(0,17476,56797), 7=(61166,39321,0). 8-bit values are byte-replicated to 16 bit.
- Not made: separate 4-colour-only golden (Multicolor Settings not toggled).

## R1.1b what applies live (partial)
- A plain file write to ENV:Sys/palette.prefs does NOT change live colours (waited 4 s). C:IPrefs by hand returns rc 20.
- `SYS:Prefs/Palette FROM <file> USE` applies colours 0-3 live (read back 11 22 33 / 44 55 66 / 77 88 99 / aa bb cc).
  Colours 4-7 from pap_Colors did NOT change live on this depth-8 screen. Sprite colours 17-19 unchanged.
- USE makes IPrefs open a requester "Intuition is attempting to reset the Workbench screen. Please close the following
  windows: DefIcons" (Retry|Cancel). Cancel keeps the colours. Retry clicked once with the DefIcons and Workbench windows
  still open: the requester stayed and dri_Pens did not change (pens still 0,1,1,2,1,3,1,0,2,1,2,1,2).
- UNVERIFIED: whether pens apply after the windows really close. Close gadget clicks at (0,11) had no effect.
- Consequence for OD3: colours 0-3 go live with a Use-style apply; pens need the screen reset; file write alone applies
  nothing. The apply path is "run the Prefs editor with USE" or equivalent, not a notify on the file.

## R1.1f readback tool (done)
wasabi/ami expose no Workbench colours. Tool: tests/amiga/wbpens.c (Makefile target build/amiga/wbpens, __stack 8192 plus
$STACK cookie). Prints DEPTH, RGB idx rr gg bb for 0-7 and 17-19 (GetRGB32 high byte), PEN i v (dri_Pens), TITLE.
Baseline: RGB0 13a907 (edited), 1=000000, 2=ffffff, 3=6688bb, 4=0000ff, 5=ff00ff, 6=00ffff, 7=ffffff, 17=e04040,
18=000000, 19=e0e0c0; pens 0..12 = 0,1,1,2,1,3,1,0,2,1,2,1,2.

## Open for the next probe session
R1.1b pens after real screen reset; R1.1c WBPattern; R1.1d Pointer colour diff and whether pap_Colors 17-19 move the
pointer; R1.1e font.prefs and workbench.prefs. OD3 / OD10 / OD11 must be re-asked after those; only OD3 has probe facts now.
