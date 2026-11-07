# Installer dialog borders / cut-off: rig 93 measure (measure only, no product code changed)

Rig 93, purpose "up-term-49: Installer dialog borders + cut-off measure", sys.hdf clone, tokyo-night, screen 800x600 (wasabi "depth=24",
IntuitionBase screen BitMap depth 4). DOpus 5.104 Installer run by hand (`Execute VTCX:dopus_run.script`, pages driven like dopus_rig.drive
up to "Modify Startup-Sequence File" 88%). Probe: throwaway C `winprobe` (scratchpad, not committed): Window fields, ReadPixel, GetRGB32.
Pixel values below are host `ami.grab` RGB (truth of the display) unless "pen".

## Setup finding that matters
On a fresh boot of this rig `amarchy-flat STATUS` = `flat=absent` (Amarchy runs, bar shows, flat is not started until a prefs apply
starts it: amarchy/desk_dos.c:580 pl_start only when prefs flat/gad is on). Measured BOTH states.

## Check 1: window geometry and edge pixels
Installer main window: Flags 0x08023006 (active) / 0x08021006 (inactive), L152 T195 W496 H210, BorderLeft4 Top14 Right4 Bottom2,
screen 800x600. Last window row = 195+210-1 = 404, fully on screen (screen height 600). Not BORDERLESS, no size gadget.
Pens (dri_Pens): DETAIL0 BLOCK1 TEXT2 SHINE0 SHADOW0 FILL3 FILLTEXT0 BACKGROUND0 BARDETAIL2 BARBLOCK0 BARTRIM2. Pen0 1a1b26, pen3 7aa2f7.

A) flat=absent when the window opened (first run):
  row 402/403 at x=300: 1a1b26; row 404 (last row): 1a1b26 (NOT accent); row 405 (below): 000000; left col x=153: 7aa2f7 (stock FILL)
  down to row 403 only; x=152 and x=647: 1a1b26; x=151/648: 000000. => bottom border row = body colour, side lines end one row early,
  window "bottom missing", 000000 under it. Matches the owner's shot (side borders stop 1-2 px early, no bottom row, 000000 below).
B) flat=on before the window opened (second run, all 6 pages, H stays 210):
  row 404: 7aa2f7, row 403: 1a1b26, row 405: 000000; x=152 and x=647: 7aa2f7; no cut-off.
The 000000 below the window is the Workbench desktop window / unallocated colour (ReadPixel gives pen 0x10 there, outside the 4-bitplane
range), NOT 0E0E14 and not outside the display: the window ends inside the 600 px screen.

## Check 2: amarchy-flat STATUS
Fresh flat start: `rectfill calls=64 hits=0 shits=0`, `frame=hooked calls=0 flat=0`.
Installer open (page 1 welcome): rectfill hits=14, frame calls=4 flat=1.
Per page (rig run B): page1 rhits 55 shits 36 frame 56/14 -> page 6 (88%) rhits 55 shits 72 frame 292/73.
=> ring RectFill hits only at window open (hits 14 -> 55, then flat); frame calls rise every page, but only ~1 in 4 is replaced (flat/calls = 73/292).

## Check 3: which call draws what (page 6, gadget boxes from UITREE, pixels from grab)
Gadgets 90, 91, 100 (Proceed/Abort style, 238x16): 1 px accent 7aa2f7 outline, body fill: DRAWN (frameiclass path works here).
Gadgets 97/98/99 (scroller + arrows): accent outline: drawn.
Gadget 92 (string, 472x12) and 96 (text box, 448x97): NO frame at all (all 1a1b26 at edges, interior, bottom). These are not frameiclass
boxes that flat replaced; the Installer draws them with bevel/border lines in SHINE/SHADOW = pen 0 (invisible).
Owner's "buttons have no borders" is NOT reproduced for the 238x16 buttons on this rig (flat on). frame calls DO rise (frameiclass hooked),
so the guessed "buttons are not frameiclass" is false for them; the part of the frame calls that are not replaced (flat/calls 25 %) are the
unreplaced bevels (string box, text box, and others). Not traced to a library call (no SnoopDOS-class tool on the rig; calls 292 vs flat 73
is the evidence, library offsets not read).

## Check 4: inactive window
pen_inbar = pen 11 (STATUS inbar=11), RGB 545a77 vs pen 0 1a1b26: different.
Installer made inactive (Workbench clicked): row 404 / x=152 / x=647 / top row 195 / title bar y=200 = 545a77; outside (y=194, x=151) 000000.
=> inactive ring is visible and differs from the body; inactive edge pixel vs the pixel just outside: 545a77 vs 000000. Not invisible.
Caveat: contrast against the black desktop is fine, against the body 1a1b26 low (545a77 vs 1a1b26).

## Root-cause hypotheses
- Bottom row / window cut off: reproduced only with flat=absent at window-open time (A). Stock bottom border pen is SHADOW = BACKGROUND =
  pen 0, so with no ring patch the last row is body colour and the 000000 desktop shows right below: looks "written over by bg".
  The ring is drawn solely by the RectFill patch (amarchy/flat.c:1229-1250, strips flat_glyph.c:525-531 outer_strip, flat_outer_edge_fill
  flat_glyph.c:573). A window whose frame was painted while flat was absent/late (or a frame paint that bypasses the ROM-caller gate,
  flat.c:1229) keeps the stock look until RefreshWindowFrame. Owner's rig 94 is therefore most likely "flat started after the Installer
  window opened or not at all" (UNVERIFIED on 94: not touched). Owner's 418-px window height was not reproduced (210 here): second variable.
- Dialog frame (window border): same cause as above for the stock case; with flat on the border ring is there (A vs B).
- Buttons: with flat on, 238x16 buttons are framed (accent). No frame: gadgets 92/96 (string/textbox) = unhooked bevel path, invisible
  because SHINE=SHADOW=BACKGROUND (desk_core.c desk_pens_flat); with flat absent EVERY bevel is invisible (SHINE=SHADOW=pen 0),
  which gives "buttons have no borders" exactly (the frameiclass replacement c8_skip flat.c:555, flat_glyph.c:408-436 does not run).
- Inactive edges: not the same cause. Ring exists with flat on (545a77 vs body 1a1b26, flat_inactive_pen flat_core.c:662); it is only
  low contrast against the body. With flat absent an inactive window has no ring at all (stock FILL is the body colour), i.e. same
  "flat not running" root as the others, but a separate low-contrast issue remains if the owner means contrast.

## Single root candidate for the owner's symptoms
amarchy-flat not running (or started after the Installer window opened) on the owner's rig 94: all three symptoms follow from
SHINE=SHADOW=BACKGROUND with no patch active. Next check (needs rig 94 owner): `amarchy-flat STATUS` on 94 at the failing moment.

State: rig 93 stopped, build/rig93 removed, scratchpad files (winprobe, drv.py) small.
