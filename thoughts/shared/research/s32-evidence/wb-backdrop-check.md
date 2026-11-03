---
date: 2026-11-03
topic: S32-WB-BACKDROP check i - Workbench Backdrop default state, frame, storage
status: partial (no rig has both real Workbench and Amarchy; see Caveats)
---

# Workbench Backdrop check

## Results
- Rig 74 default image (OS 3.1 ROM 40.68 / WB 40.42, Amarchy running): there is NO Workbench task. Directory Opus 5 replaces it and owns the root window. DOpus menu Opus/Backdrop is [checked] by default; its window is `[backdrop]`, 0,14 800x573, frameless (pixel rows 14.. are content at x=2..790). Unticking it (ui menu Opus/Backdrop) gives a framed window with title bar at y=14..27 (white line at y=14, title colour 102,136,187). DOpus state is DOpus-internal: ENVARC:Sys/wbconfig.prefs md5 unchanged (7f7a31d806763b25bd32d2ec757b371b).
- Rig 74 `--os 32` (Kickstart 47.115, WB 47.2, real Workbench task, Amarchy NOT running: 0 amarchy tasks): menu `Workbench/Backdrop?` is NOT ticked by default; the root window "Workbench" 640x490 at 0,22 has a title bar and border (screenshot rows: title colour (102,136,187) at y=24-36, black border line y=42, doubled-row grab). Ticked (via menu): window reported `[backdrop]`, title "" and the pixel columns x=3/200/630 are uniform (170,170,170) from y=24 to 54 - no title bar, no border. So Backdrop TICKED = frameless; unticked = framed. This confirms the ledger reading.
- Storage: toggling Backdrop did NOT change ENV: or ENVARC:Sys/wbconfig.prefs (54 bytes, FORM PREF / PRHD / WBCF chunk, md5 7f7a31d8..., identical before and after, and identical on the 3.1 and 3.2 images). The WBCF payload (20 bytes `00000000 00000000 0000000b 02800185 00000000`) is the old workbench control prefs; NDK 3.2 prefs/workbench.h defines only WBNC (struct WorkbenchPrefs, has wbp_Borderless = icon borders, not the window frame) and has no backdrop flag. Where Workbench persists the Backdrop flag was NOT found; not measured (not in wbconfig.prefs, not visible in SYS:.backdrop which does not exist). Open.
- Setting without clicks: on 3.2, ARexx `MENU INVOKE WORKBENCH.BACKDROP` to port WORKBENCH (wasabi `arexx WORKBENCH "MENU INVOKE WORKBENCH.BACKDROP"`) returned rc 0 and toggled the item from checked to unchecked (measured through ui list). It is a toggle, not a set, so read the state first (ui list / menu checked, or the window's backdrop flag). `WB.BACKDROP?`, `WORKBENCH.BACKDROP?` and `.TOGGLE` forms return rc 10 (205). Not tested on 3.1 (no Workbench on the rig image; 3.1 Workbench has no ARexx MENU command in this form - unverified).

## Caveats
- Amarchy default effect on Backdrop could not be measured: the only rig with Amarchy has DOpus instead of Workbench; the only rig with Workbench has no Amarchy. Nothing in this check shows Amarchy touching the tick (consistent with the ledger grep).
- Screenshots: ~/Desktop/upterm-shots/wbbackdrop-dopus-{ticked,unticked}.png, wbbackdrop-wb32-{ticked,unticked}.png.
