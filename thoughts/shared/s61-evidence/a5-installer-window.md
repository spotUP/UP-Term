# A5 Installer window cut-off: NOT REPRODUCED (partial hunt)
Rig 96, --os 32, start WARN lines verbatim (4, all unrelated):
[WARN] UP-Term:share/updemo/{earth.anim,fjord.png,kanim-stream.bin,photo.png} is missing in the rig's UP-Term: tree; `rig.py install` puts the kit's copy there
State: Installer 43.3 (C:Installer). No ENVARC:sys/font.prefs exists (ENVARC:sys holds only screenmode.prefs + def_*.info) -> stock fonts.
Workbench here is DOpus5's screen/backdrop; Installer opened on "Workbench Screen" 640x256 (x2 rows = 640x512), 8 bit. No Amarchy process in Status.
| variation | window | fits |
| stock, 10-line test script, welcome page | 448x318 at 96,96 (bottom 414 of 512) | yes (haiku grab a5-baseline-testscript.png) |
Not tested: Amarchy running, DOpus script's own pages (askchoice 5% page), font/antialias variations.
Hypotheses 1/2 need Amarchy (proportional AA font, navy screen as in owner screenshot) which is not running on this rig; candidate 3 (old Installer) is ruled out: 43.3 is the current 3.2 version and fits.

## sys.hdf clone measurement (rig 96, no --os, 2026-11-07)
Start printed no [WARN] (only "[BUILD] stale before push: Amarchy", refreshed). Amarchy IS running here (bar window at 0,587 800x13).
- DOpus screen "Directory Opus" 800x600, 24 bit. Installer 43.3. Installer opened on a separate "Workbench Screen" 800x600, not on the DOpus screen.
- ENVARC:sys/font.prefs exists (518 bytes, unlike --os 32): icon font helvetica 11, system font topaz 8, screen font helvetica 11 (parsed from the FONT chunks). DOpus5:Environment holds `default` and `workbench` (5428 bytes each); fonts inside not decoded.
- Test script: 4-line askchoice prompt (scratchpad t2.script). Installer window 496x210 at 152,195 -> bottom 405 of 600. Window size did not change after Proceed.
- Haiku on a5-syshdf.png (welcome page): nothing cut; bottom edge about y=404; Installer text looked like non-antialiased Topaz 8, not helvetica; background very dark navy (~#0d0d1f). After Proceed (a5-syshdf-ask.png) gadgets sit at y 386..402 inside the window; that page was NOT visually read (one-screenshot limit), so the askchoice page itself is unverified.
- Verdict: not reproduced. Installer window did not use the helvetica 11 screen font here and fit its content. The cut-off is not shown to be a property of the master image's DOpus 5.82 screen font; navy background matches the owner's shot, font does not. Open: owner's window may be on the DOpus screen (needs Installer started from DOpus, or a different screen mode/font), not tested.
