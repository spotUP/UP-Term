# wasabid MUI group buttons (fork 562672a8): rebase and rig test, session s50

Result: NO PR opened. No MUI program on the OS 3.1 rig has a framed group used as a button, so the change cannot be shown to do anything there.

- Rebased on creep-ltx/wasabi main (cfdd89d1 level) as branch pr/mui-group-buttons; the rebased patch is saved as wasabi-mui-group-buttons.patch (applies on upstream main).
- Conflicts: CHANGELOG (kept upstream bullet, added ours below it) and wasabid.c. Upstream already has `mui_short_help()` and `#define MUIA_ShortHelp 0x80428fe3`; the fork commit defined it as 0x80420b17, which is the wrong value (the fork commit would have read a different attribute). The rebase drops the second define and uses `mui_short_help()`.
- PR build md5 574747978f4447a3efe23870ca8e2ae3 (116416 bytes), built with the make wasabi flags. Pinned build/wasabi/wasabid md5 27f53fe0b351f058dfe845cfa0f058ac.
- Rig 60 (fs-uae, OS 3.1, 060). Pinned daemon on 1293, PR daemon RAM:wasabid-pr on 1294, key rigwasabi, `--plain`.
- Programs tried: SYS:Prefs/OpenURL, MUI:MUI (MUI Preferences), MUI:PSI. `wasabi ui list` on 1293 and on 1294 gave 212 lines each, diff empty: no clicked Group.mui in any of them (all buttons are Text.mui or bare Area.mui). Nothing to compare.
- Not found in the repo or rig: any MUI app using group buttons (grep of dist/, build/rig60 and thoughts/ for muimaster users found none besides the above).
- To finish: need an MUI program with image+Text group buttons (for example a MUI app from Aminet) on the rig, then `ui list` before/after and PASS = the group is listed as button/toggle with a label.

## 2026-11-02 attempt (vtcon-05): BLOCKED, rig 59 never booted

- Candidates downloaded from Aminet (MUIFind, ICookie_MUI, MuiEnv1_6, MUIEmail) but none run: `UPTERM_RIG=59 rig.py start --max` died copying "rig 1's VTC: drawer" (FileNotFoundError build/rig/vtc). build/rig (rig 1, the owner's) no longer exists; build/rig-gc.log shows "build/rig.del 2065345 bytes leftover .del" at 13:04:21, i.e. a rig gc sweep removed rig 1's folder around the time of this start (another session's rig.py start pid 54154 began 13:01 too; cause not isolated).
- No ui list taken; verdict NO-DIFF/unknown. Rig 59 cleared, downloads deleted.
- Still missing: rig 1 restored (or start from a rig that has a vtc drawer), plus a MUI app known to use framed Group buttons (none of the four was confirmed to).

## 2026-11-02 second attempt (vtcon-05): NO-DIFF, rig 59 (OS 3.1, 060, muimaster 19.35 = MUI 3.8)

- App: TheBar 26.2 68k demo1 (Aminet dev/mui/MCC_TheBar_68k.lha, TheBar/TheButton .mcc copied to MUI:Libs/mui, demo run from RAM:tb). Window "TheBar Demo1" has 6 image+label toolbar buttons.
- Pinned wasabid (md5 27f53fe0, port 1292) `ui list`: 117 lines, the six toolbar items already `button "Pread mail." ... (TheButton.mcc)` etc.
- Patched wasabid rebuilt from patch on cfdd89d1 (md5 574747978f4447a3efe23870ca8e2ae3, matches). C:wasabid.new update refused to replace the running daemon ("cannot move the new binary into place"), so it ran as RAM:wasabid-pr on port 1294 (`1294 name pr59 plain`). `ui list`: 117 lines, diff against the pinned output EMPTY.
- Why: these buttons are TheButton.mcc (own class), not Group.mui with InputMode; the patch only touches Group.mui, so the demo cannot exercise it. `ui click "Save mail."` with the patched daemon worked (a new empty-titled 89x57 window opened), but the pinned daemon lists and clicks the same.
- Verdict NO-DIFF. Still needed: an app whose buttons are Group.mui with MUIA_InputMode (framed group of Image + Text) - not TheBar. Not YAM (not on Aminet under guessed names; yam.ch hosts it).
- Side note: `RAM:wasabid-pr ?` on the rig starts a daemon on the default port and blocks the run slot; do not use it for usage.

## 2026-11-02 own test app (vtcon-05): DIFF found, but the patch lists the WRONG group (verdict: not a PASS)

- App: tools/rig/fixtures/mui-group-button/muigroupbtn.c (branch s51-muigroup, commit b4155f67 plus the later fix of the MUI_MakeObject LVO; see the final commit on the branch). Window "MUI Group Button Test": framed HGroup RelVerify (Image + Text "Group Button"), framed VGroup Toggle ("Group Toggle"), SimpleButton "Plain Button". Built with vbcc + MUI 3.8 libraries/mui.h (Aminet dev/mui/mui38dev.lha). Rig 59, OS 3.1, muimaster 19.35. Patched wasabid rebuilt from the patch on cfdd89d1: md5 574747978f4447a3efe23870ca8e2ae3 (matches); ran as RAM:wasabid-pr on 1294; pinned on 1292.
- Pinned ui list (101 lines), lines 7-9:
    label    "Group Button"  at 373,278 64x11  (Text.mui)
    label    "Group Toggle"  at 363,296 74x11  (Text.mui)
    button   "Plain Button" at 360,312 80x15  (Text.mui)
- Patched ui list (99 lines), the same place:
    button   "Group Button"  at 356,273 88x57  (Group.mui)
  Diff: the three lines above became ONE line. 356,273 88x57 is the whole window contents (the root VGroup), not the HGroup (which is about 64+ wide, 11-15 high). The toggle group and Plain Button vanished from the list.
- Click: pinned `ui click "Group Button"` -> no gadget (it is a label); a raw `mouse click 405 283` on the pinned daemon DID print GROUP PRESSED (the fixture works). Patched `ui click "Group Button"` -> "clicked button ..." but NO GROUP PRESSED (it clicks the centre of the root group, a gap between children).
- Verdict: the patch changes the output, but on this app it is a regression: it labels the root group as the button and hides the real children. Needs a fix (only list a Group.mui whose own InputMode is set and that is not an ancestor of other listed gadgets) before any PR.

## 2026-11-02 fix (vtcon-ee): PASS on rig 59 (OS 3.1, 060, muimaster 19.35)

- Fix (local branch pr/mui-group-buttons, commit 0baee77, one commit on cfdd89d1; patch saved over wasabi-mui-group-buttons.patch): a Group.mui is an item only below the window's root group, only with an input mode AND a label (its own Text, else tooltip); it is emitted and its children are still walked, with the group's own Text not listed again; `mui3_group_mode` tells toggle from button (bit 16 of the old rule is set on every group, so it called the toggle a button).
- Found with a debug build (dump of the object words): RelVerify and Toggle group of equal size differ only in bit 0x80 of the word 120 bytes into the object (0x340 vs 0x3c0); it does not change when the toggle is pressed. Evidence is one pair on muimaster 19.35; a Text.mui-based toggle uses the older rule, unchanged.
- Fixed wasabid md5 1c635eb37f9c44f06d533c26e23ec856, ran as RAM:wasabid-fx on 1298 beside the pinned one. Test app built with vbcc + MUI 3.8 SDK (the fixture needs a DoMethod shim, written out of tree).
- Before (patch from s50, 99 lines): `button "Group Button" at 356,273 88x57 (Group.mui)` (root group), Group Toggle and Plain Button gone.
- After `ui list`:
    button   "Group Button"  id 0 at 360,276 80x15  (Group.mui)
    toggle   "Group Toggle"  id 0 at 360,294 80x15  (Group.mui)
    button   "Plain Button"  id 0 at 360,312 80x15  (Text.mui)
- Clicks: `ui click "Group Button"` -> app output file RAM:o.txt reads READY, GROUP PRESSED (one click, one line). `ui click "Group Toggle"` -> `[selected]` appears on the toggle line. `ui click "Plain Button"` clicked (no output expected).

## 2026-11-02 second MUI version (vtcon-ee): NOT-TESTABLE (no second muimaster available)

- Rig 60 on OS 3.2 (`UPTERM_RIG=60 rig.py start --max --os 32`, Kickstart 47.115 / Workbench 47.2): `Version LIBS:muimaster.library`, `Version MUI:Libs/muimaster.library` and `Version muimaster.library` all fail "object not found" (rc 20); `List DH0:MUI/Libs` -> object not found; `List DH0:Libs PAT=#?mui#?` lists nothing. The 3.2 system image has no MUI at all, so there is no 3.2 MUI to test.
- Repo search (dist/, vendor/, build/dist, build/rig-inputs, and build/rig*/ trees; find -iname muimaster*, MUI*.lha, MUI*.lzx): nothing. The only muimaster known is 19.35 from the OS 3.1 rig image (the first PASS). Not downloaded from the web, per the brief.
- Not done as a consequence: fixture shim/commit (an untested, uncommitted domethod.c shim sits untracked in vtcon-s51-muigroup/tools/rig/fixtures/mui-group-button; the branch has no new commit), wasabid rebuild, ui list, clicks, word at object+120.
- To make it testable: supply a second MUI install (for example MUI 3.8 / 4.0 / 5.0 library set) as a rig input, then rerun steps 1, 4 and 5.

## 2026-11-02 MUI 5 check (vtcon-ee): NOT-TESTABLE (MUI 5 never opens the fixture window)

- MUI 5.0-20210831 os3 (github amiga-mui/muidev release, extracted on the host, MUI drawer copied to Work:m5/MUI, `Assign MUI:` + `Assign LIBS: MUI:Libs ADD`) on rig 60 (OS 3.2, Kickstart 47.115): `Version muimaster.library` -> `muimaster.library 21.227` (also `Version MUI:Libs/muimaster.library` -> same).
- Fixture rebuilt with Bebbo gcc (SDK headers from mui38dev.lha; commit 9e61fe46 on s51-muigroup). Fixed wasabid rebuilt with the Makefile's flags: md5 1c635eb37f9c44f06d533c26e23ec856 (identical to the earlier fixed build), run as RAM:wasabid-fx on 1298.
- Result: the app object is created and prints READY, but `get(win, MUIA_Window_Open)` right after `set(win, MUIA_Window_Open, TRUE)` reads 0 (debug build, RAM:o2.txt: `READY open=0`); `ui list` / `windows` show no "MUI Group Button Test" window after 6-15 s, no requester. So no Group Button / Group Toggle / Plain Button exist to list or click; the word at object+120 was not dumped. Not a verdict on the bit rule: the same fixture opens fine on 19.35. Cause of the failed open on MUI 5 is not found (candidates: MUI 5 prefs/screen defaults on this image, missing MUI 5 support files; not investigated further).
- To finish: find why MUI 5 refuses to open the window (MUI 5 Settings / MUI:Presets / a Workbench-screen setting), or test MUI 3.8 user (github amiga-mui/muidev release 3.8, mui38usr.lha).
