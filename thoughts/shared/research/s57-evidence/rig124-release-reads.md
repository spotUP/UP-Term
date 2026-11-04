# Rig 124 release reads (s57, 2026-11-04, OS 3.2, fresh-all from main 9fd26e8c)

Build: make worktree-ready, wasabi, amiga, dist (UPTERM_PORTS=kit-main) rc 0. Install: installer_rig.py ONLY fresh-all, [PASS], 35 checks; its own post-install reboot worked this time (no CalledProcessError; restart fix 9fd26e8c). Window title after reboot: "vtcon rig 124 (060 max, OS 32) - vtcon-ee s57: fresh 3.2 install, release pixel reads".
md5 rig vs build/dist/up-term/Files: L:vtcon-handler 5f62fde07d25825bdf2b2a4d0d7c8916, C:UPDemo 93e2cdb7539dd63e24fb2c8ca6e17ab8, SYS:UP-Term/bin/Amarchy b55a9312feae585b324974731916a678: equal. Version L:vtcon-handler = 1.0.
Rig state at the end: the btnbg rig picked theme vantablack and turned flat on; amarchy_gui_rig --setup then failed ("the earlier amarchy-flat resident unloaded on Break"), so the rig is no longer in its fresh post-install state.

| Row | Check | Measured | Verdict |
|---|---|---|---|
| WB-BACKDROP + S31-START-PALETTE | grab after install reboot (rig124-wb-fresh.png) | backdrop 1a1b26 (26,27,38) = 235274 of 327680 px; bar 0e0e14 | PASS |
| FLAT-PENS-NOT-LIVE | amarchy-flat STATUS right after install | `frame=hooked calls=0 flat=0`; Amarchy STATUS enabled=1 active=1 | FAIL (flat=0 on the fresh install; calls=0 may only mean no frame drawn yet) |
| FLAT-BUTTON-FILL | amarchy_btnbg_rig.py, depth 8, vantablack | idle fill 8d8d8d, held bebebe, background bebebe; corners PASS, press 631 px PASS, "a button is drawn" FAIL | FAIL against owner spec (lighter tint than background): fill is darker; flat_core.c flat_button_pen not yet fixed |
| strut_off (4073) | amarchy_bar_rig.py --only strut_off | off box [100,106,200,150] root 256; on box [100,96,200,150] bar top 246; clamped 1 -> 2 | PASS |
| WELCOME | amarchy_gui_rig.py --welcome | window opened, 1728 painted px inside; click on checkbox did not change pixels | FAIL on click check (geometry PASS earlier on rig 71, 7d247d68) |
| inactive WB knob pens (4125) | amarchy-flat STATUS pens-held | refs=3 dim=240 knob=251 inbar=250; pen 249 not read, no pixel compare | UNVERIFIED |
| S56-LOCALE-SAVE-FILL (4125) | not run | - | UNVERIFIED |
| FLAT-SCROLLER | not run (no helper found) | - | UNVERIFIED |
| R1.17-WBRESET, AM-FLAT-WB, DESK-CHECK-INVERTED | gui_rig --setup then desk_rig | setup FAIL; desk "[FAIL] the Amarchy commodity is not running" | UNVERIFIED (setup failure, not a product verdict; DESK-CHECK-INVERTED and AM-FLAT-WB 3.1 are 3.1 rows) |

Shots: ~/Desktop/upterm-shots/rig124-wb-fresh.png.
