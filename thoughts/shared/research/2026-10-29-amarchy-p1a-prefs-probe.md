# P1a prefs probe (rig 7, OS 3.2.2, Kickstart 47.115, Workbench 47.4)

Status: R1.1a, R1.1b, R1.1c, R1.1d, R1.1f done; R1.1e partial (font.prefs/workbench.prefs NOT probed, see Part 2). Rig prefs restored (md5 below).

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

## Part 2 (rig 7, --os 322, session s23)
Method: hand-built IFF files (python) fed to `SYS:Prefs/<Editor> FROM <file> USE`, ENV:Sys file read back with ami.py get,
live state read with wbpens. Files used are in the scratchpad only; palette changes used tests/data/desk/palette-gui-save.prefs
as base (pens changed to 1,2,2,1,2,3,2,1,0,2,0,2,0 in both lists; colours 0-3 = 112233/445566/778899/aabbcc, 4-7 and 17-19 changed too).

### R1.1b pens after a real reset (done)
- With every Workbench window closed first (ARexx `ADDRESS WORKBENCH; WINDOW ALL CLOSE` worked, rc 0), `Palette FROM x USE`
  showed NO reset requester and applied at once. Earlier "requester stays" was caused by the open windows.
- Live after USE: colours 0-3 changed; colours 4-7 NOT changed (stay 0000ff/ff00ff/00ffff/ffffff); sprite 17-19 NOT changed
  (e04040/000000/e0e0c0). Reason is consistent with wbe_LockPens (workbench.prefs locks pens 4-7): UNVERIFIED as cause.
- dri_Pens DID change: requested 1,2,2,1,2,3,2,1,0,2,0,2,0 -> read back 0,1,2,1,2,3,2,1,0,2,0,2,0. Pens 2..12 applied;
  pens 0 (DETAILPEN) and 1 (BLOCKPEN) kept 0,1 (not applied; cause not probed; the golden defaults already are 0,1).
- Screen title unchanged by USE ("Workbench Screen"), screen not reopened visibly (depth stays 8).
- OD3 evidence: apply path = close windows (or accept the requester) + `Palette USE`; pens 2..12 and colours 0-3 apply live;
  colours 4-7 and 17-19 cannot be set through pap_Colors on this screen. Pens 0/1 not changeable.

### R1.1c wbpattern.prefs (done)
Real struct (differs from the plan table, NDK wbpattern.h): PTRN body = wbp_Reserved[4] (16 zero bytes), UWORD wbp_Which
(0 ROOT, 1 DRAWER, 2 SCREEN), UWORD wbp_Flags, BYTE wbp_Revision (0), BYTE wbp_Depth, UWORD wbp_DataLength, data.
- Flags WBPF_PATTERN 0x1: data = 16x16 pattern, planar, plane-major, 32 bytes per plane. The editor normalises to depth 3,
  96 bytes (a depth-2 input kept plane 0/1, plane 2 zero). Flags 0: data = NUL-terminated picture path, DataLength includes the NUL.
  WBPF_NOREMAP 0x10 survives (ROOT picture written with flags 0x10 came back 0x10; SCREEN picture with flags 0 came back 0).
- `Wbpattern USE` always writes all three chunks ROOT, DRAWER, SCREEN, in that order; a chunk missing from the input is
  written as flags 0x1, depth 0, 96 zero bytes (empty pattern). Total sizes 410 (3 patterns) / 298 (2 pictures + 1 empty).
- Picture paths used: SYS:Prefs/Presets/Backdrops/pattern.iff (also default_pal.iff, background_*.png exist there).
- UNVERIFIED: whether a plain file write redraws the backdrop (not tested; palette precedent says no); on-screen look of each variant.

### R1.1d pointer.prefs (done)
- PNTR body: 16 reserved, pp_Which, pp_Size, Width, Height, Depth, YSize, hotspot X, Y (32 bytes), then (1<<Depth)-1 = 3 RGB
  byte triples (9 bytes), then 96 bytes of bitmap: 137 per chunk. Colour table starts at chunk-body offset 32.
- Shipped file (md5 4da47600949a47055d5c699404da3683, same in ENV: and ENVARC:): 41 chunks, pp_Which 40 down to 0, all 16x24,
  depth 2, Size 5, ALL 41 colour tables identical e04040 / 000000 / e0e0c0 = the sprite colours 17-19 readback.
- Setting all 41 tables to ff0000/00ff00/0000ff and `Pointer FROM x USE`: live GetRGB32 17-19 became ff0000/00ff00/0000ff;
  the editor rewrote ENV:Sys/pointer.prefs (6012 bytes) keeping my colours in all 41 chunks; only 7 bytes differ from the input
  (reserved-field garbage and one hotspot byte of the first chunk). So the colours of every chunk must be changed.
- pap_Colors 17-19 in palette.prefs do NOT move the pointer colours (stayed e04040/000000/e0e0c0 after Palette USE).
  Sprite colours are driven by pointer.prefs only.
- OD11 evidence: pointer colours are themeable only via pointer.prefs (all 41 chunks) + `Pointer USE`, live at once, no reset needed.
  Bitmaps stay untouched by a colour-only rewrite.

### R1.1e font.prefs / workbench.prefs (NOT done)
- `SYS:Prefs/Font FROM <hand-built font.prefs> USE` wrote no ENV:Sys/font.prefs (error 205 on read). Cause not found; the editor
  may not take FROM like the others, or the hand-built FONT chunk (156 bytes: 12 reserved, UWORD reserved2, UWORD type,
  4 pens/modes, TextAttr 8, name 128) was rejected. Font Preferences GUI has buttons "Select Workbench Icon Text...",
  "Select System Default Text...", "Select Screen Text..." (ASL font requesters; the pen/draw-mode fields are there).
- Remaining: drive the ASL requester for the WB icon text (front/back pen, draw mode, any outline/shadow choice) then Use
  and diff ENV:Sys/font.prefs; Workbench prefs editor toggles and WBTF format codes. OD10 has no probe evidence yet.
- Hazard: a failed `Pointer FROM <bad path>` leaves a "Program Error" requester that makes later ami.py exec calls hang or
  return nothing; clear it before the next command.

### Rig restore
Rig 7 ENV:Sys/pointer.prefs restored byte-identical (md5 4da47600949a47055d5c699404da3683). palette/wbpattern/font prefs
files deleted from ENV:Sys; live palette and sprite colours on rig 7 were NOT reset (private OS copy; use --fresh next time).
