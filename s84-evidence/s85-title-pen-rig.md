# S85 title pen rig check (rig 84, OS 3.1) - UNDECIDED

Method: wasabi (ami.grab = RGB of front screen, read_ui, T:dripens palette, `patina-flat STATUS` copied to RAM:). No pen-index readback tool: pen = RGB matched against the dripens palette.
Hotkey note: F9 (raw 0x58) on this rig applied a Tokyo-night style theme (bg 1A1B26), NOT vantablack; the starting theme was starship (bg 1C1C1E). Vantablack was never reached.

## Before (starship) / after F9
- STATUS pens-held: `inbar=1 knob=1 dim=13` -> `inbar=1 knob=1 dim=5`; `pensel=5 selrgb=3a3a3c` -> `pensel=1 selrgb=3e4254`. inbar pen NUMBER unchanged (1); sel and dim pen numbers moved.
- Palette after: 1A1B26 414868 A9B1D6 7AA2F7 3E4254 222222 ...; pen 1 = 414868.
- Inactive lister 0x103d4574 (486,154 196x173) title band (x506..587, y154-166):
  before 222222 (palette pen 5), text 1C1C1E (pen 0); after 414868 (= pen 1 = new inbar colour), text 1A1B26 (pen 0).
  Band was repainted in the new theme (not stale, not black).
- Gadget boxes (custom gadgets x588..680) and close box (486..505): 7A7A7A before and after (DOpus-drawn, unchanged); bevel pixels 000000.
- Active lister title: none active at measurement time (all dopus_lister windows active=False): NOT measured.

## Stop
Next hotkey (F10, raw 0x59) left the Amiga daemon unresponsive: "daemon did not answer for 15 s", then TCP connect timeouts for 5+ minutes. fs-uae pid 23825 alive. Not restarted (brief forbids unless unavoidable).

## Verdict
UNDECIDED. The one transition observed (starship -> tokyo-night via F9) repainted the band in the new inbar colour while the inbar pen NUMBER stayed 1, so it did not reproduce the stale-pen black band. The hypothesis needs inbar's pen number to change; that did not occur. Vantablack transition untested.

## Run 2 (rig 84 rebooted, purpose "vtcon S85: inactive title pen check")
- F10 wedge: NOT reproduced. fs-uae logs saved: /private/tmp/claude-501/-Users-spot-Code-UP-Term/cda7ad25-0200-4e00-a785-1e9e049a4226/scratchpad/f10-wedge-tail.log, /private/tmp/claude-501/-Users-spot-Code-UP-Term/cda7ad25-0200-4e00-a785-1e9e049a4226/scratchpad/rig84-wedge-full.log (only bsdsocket thread spam and clipboard lines; no cause). rig.py reboot cleared it.
- Assign at boot: UP-Term: present (Rushhours:Tools/UP-Term); Themes dir lists vantablack and starship; guard_upterm_assign had nothing to do. A leftover IPrefs "System Request" (Retry/Cancel) sat on screen at boot (Cancel clicked); one reappeared after the vantablack apply (cause not identified).
- Hotkey finding: this boot's ENV:up-term/keymap has NO F9/F10 gtheme rows (search "gtheme" empty), so raw F9/F10 did nothing (km-hits 0, theme-active unchanged). I wrote ENV:up-term/keymap.user ("super+ctrl+f10 = gtheme:starship", "super+ctrl+f9 = gtheme:vantablack"), RELOAD (rows 72 -> 74); F9 then applied vantablack.
- STARSHIP (theme-active starship, two Device List listers, both inactive): pens-held "4 missing 12 13 14 15 refs=1 dim=0 knob=12 inbar=1", pensel=12 selrgb=3a3a3c; palette 1C1C1E 8E8E93 F5F5F7 FF9F0A 3A3A3C ...; band y54-65 = 8E8E93 (pen 1 = inbar), title text 1C1C1E (pen 0).
- VANTABLACK after F9 + Keep: palette 000000 7A7A7A BEBEBE 8D8D8D 1A1A1A 222222 ... (bg 000000 confirmed); pens-held "1 missing 8 9 10 11 13 14 15 refs=1 dim=15 knob=15 inbar=1", pensel=15. inbar NUMBER unchanged (1); knob 12->15, dim 0->15, sel 12->15. Pen 1 = 7A7A7A.
- Band RGB after: NOT reliable. The grab is a mixed row (x211-260 3A3A3C, x300 7A7A7A, x330-405 8E8E93 = old starship values) because the desktop-theme apply closed/reopened the DOpus screen (Workbench Screen -> Directory Opus) and the grab did not repaint. Active-lister title text not read.
- Verdict: UNDECIDED, leaning REFUTED: inbar stays pen 1 across starship -> vantablack and pen 1 is 7A7A7A (grey, not black). A stale-index black band needs the inbar number to change; it did not. No clean band/text sample after settle.

## Run 3 (rig 84 as left by Run 2, vantablack, two stacked "Device List" listers at 211,53, UPTERM_RIG=84)
- Screen: ONE screen, 0x100298d8, depth 24 (truecolour), 800x600, title "Directory Opus ..." (DOpus renamed it; `VTC:dripens Workbench` reports the same scr 100298D8, so it IS the Workbench screen; dripens "Directory Opus" fails rc 20). Palette first 16: 000000 7A7A7A BEBEBE 8D8D8D 1A1A1A 222222 FF9F0A 222222 4473FF 7A7A7A 505050 EFAF4F 222222 222222 B6B6B6 222222. dripens pens: text2 line3 back0 outline3 button4 ptext0.
- Pixels (rig grab and host screencapture of the FS-UAE window agree): BEFORE any click the inactive band was NOT black: 8E8E93 band, 1C1C1E text, 3A3A3C beside it (STARSHIP values, none in the vantablack palette) = stale pixels from before the theme apply. The screen is 24-bit, so pixels are literal RGB, not pen-indexed, and stale ones survive a theme change.
- Discriminating test: click lister title (296,59) -> active band 8D8D8D (pen 3), text 000000. Click backdrop (600,400) -> inactive band 7A7A7A (pen 1 = pen_inbar), text 000000 (pen 0). A redraw gives GREY 7A7A7A + black text, correct; the hook does not pick black on a draw.
- Owner's black block NOT reproduced in any capture. Candidates: the other (underneath) lister, or the System Request ("Intuition is attempting to reset the Workbench screen", Retry/Cancel) still covering the title's left part.
- `patina-flat STATUS`: {'hits': 2, 'rows': 74, 'enabled': 1, 'active': 0, 'wins': 0, 'snaps': 0, 'bar': 1, 'wbpens': 0, 'wbrefused': 0, 'shots': 0, 'notes': 0, 'ticks': 905}; dopus pens-held "1 missing 8 9 10 11 13 14 15 refs=1 screens=0 applies=0 dim=15 knob=15 inbar=1".
- Left state: the clicked lister (top of the stack) is redrawn grey/inactive; the second lister was not activated.
