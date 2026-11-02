# wasabid MUI group buttons (fork 562672a8): rebase and rig test, session s50

Result: NO PR opened. No MUI program on the OS 3.1 rig has a framed group used as a button, so the change cannot be shown to do anything there.

- Rebased on creep-ltx/wasabi main (cfdd89d1 level) as branch pr/mui-group-buttons; the rebased patch is saved as wasabi-mui-group-buttons.patch (applies on upstream main).
- Conflicts: CHANGELOG (kept upstream bullet, added ours below it) and wasabid.c. Upstream already has `mui_short_help()` and `#define MUIA_ShortHelp 0x80428fe3`; the fork commit defined it as 0x80420b17, which is the wrong value (the fork commit would have read a different attribute). The rebase drops the second define and uses `mui_short_help()`.
- PR build md5 574747978f4447a3efe23870ca8e2ae3 (116416 bytes), built with the make wasabi flags. Pinned build/wasabi/wasabid md5 27f53fe0b351f058dfe845cfa0f058ac.
- Rig 60 (fs-uae, OS 3.1, 060). Pinned daemon on 1293, PR daemon RAM:wasabid-pr on 1294, key rigwasabi, `--plain`.
- Programs tried: SYS:Prefs/OpenURL, MUI:MUI (MUI Preferences), MUI:PSI. `wasabi ui list` on 1293 and on 1294 gave 212 lines each, diff empty: no clicked Group.mui in any of them (all buttons are Text.mui or bare Area.mui). Nothing to compare.
- Not found in the repo or rig: any MUI app using group buttons (grep of dist/, build/rig60 and thoughts/ for muimaster users found none besides the above).
- To finish: need an MUI program with image+Text group buttons (for example a MUI app from Aminet) on the rig, then `ui list` before/after and PASS = the group is listed as button/toggle with a label.
