# PAL31 rig check of 5292ff95 (s56, rig 84)

Golden = tests/data/desk/palette-gui-save.prefs, 434 bytes, md5 172bd3aa9479fab9366e6a41a50a6213. Build = worktree vtcon-s56-pal31 build/amiga/Amarchy, put to VTCX:Apps/bin and run there.

## 3.1 (purpose "Amarchy s56: 3.1 palette safe-form check")
- (a) PASS. Golden in ENVARC:Sys/palette.prefs, Amarchy started, 25 s. ENVARC file md5 6a80b98c2fc42b38fa0632c8c1db03c8 (434 bytes), pen lists 12 pens then ffff in both PALT lists (parsed from the file). ENVARC:Sys/palette.prefs.32bak md5 = golden. ENV:Sys/palette.prefs written (md5 63edfc4c979e45027653cd58d4da5175), also 12/12. `Echo alive` answered, Status lists the processes, no Guru. ENV:AMARCHY_DESK_PAL31_CHECKS = 3, _FIXES = 1.
  Deviation: golden not copied into ENV:Sys by hand. Doing so live on 3.1 crashes the guest (connection reset, rig died twice) because IPrefs applies it; that is the bug under test and it happens before Amarchy can convert. ExecBase at $4 was not read directly; liveness (Echo, Status, wasabid) is the evidence.
- (b) PARTIAL. The start-apply palette write reached IPrefs without a crash: Workbench colours 0-3 became the desk ones (1a1b26 414868 a9b1d6 7aa2f7, T:wbpens). The menu theme pick (amarchy_desk_rig.py) did not run: "open menu: no stable state within 60s" on this 3.1 rig. Open: drive the pick by hand or fix the rig script's 3.1 path.
- (c) INCONCLUSIVE. `ami.py reboot` did not reboot (RAM:g.prefs survived); `rig.py reboot` did, Workbench came up, but the re-clone of DH0: dropped the ENVARC palette file and the .32bak, and the kit Amarchy rewrote ENV:Sys/palette.prefs (same md5 63edfc4c...). The converted ENVARC file was not what booted. Open: boot with the converted file in place (needs an ENVARC that survives rig.py reboot, or an Installer-style image edit).

## 3.2 (purpose "Amarchy s56: 3.2 palette no-change check", Kickstart 47.115, Palette 47.21)
PASS. Golden in ENVARC:Sys, Amarchy started: ENVARC md5 still 172bd3aa..., no .32bak, Echo alive. The PAL31 sentinels do not exist (the 3.1 pass is not entered). ENV:Sys/palette.prefs written by the normal desk start (md5 b44a2999...).

Gate verdict: (a) and the 3.2 no-change row pass; (b) and (c) are not proven. The owner gate should stay until (b) and (c) are run.

## Coordinator question: Workbench reset blocked on rig 84 (NOT ANSWERED)
The message arrived after rig 84 was stopped and cleared (rig process gone, build/rig84 removed), so the requester text, the window list with owners and `EXISTS LIBS:screennotify.library` were not read. No requester was seen during my runs: I read no screen text, only files and command output; Echo alive answered throughout. The run's only reset-related fact: after the start-apply palette write on 3.1 the Workbench colours changed (IPrefs applied the palette), so a reset did happen without a crash. blocker task/window: unknown; screennotify present: unknown; requester text: unknown. To answer, restart rig 84 on 3.1, run Amarchy with the golden in ENVARC, then run `wasabi windows`, `wasabi ps`, `EXISTS LIBS:screennotify.library`, `Version LIBS:screennotify.library` and read the requester while the rig is still up.
