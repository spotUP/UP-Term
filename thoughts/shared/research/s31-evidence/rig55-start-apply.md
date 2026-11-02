# rig 55 start-apply check (read-only, 2026-11-02)
Rig 55 wasabi: port 1288, key rigwasabi (python3 build/wasabi/wasabi --host 127.0.0.1 --port 1288 --key rigwasabi run "...").
ENV:Sys/ holds only pointer.prefs (6012) and screenmode.prefs (62, Today 14:40:02). No Workbench.prefs, wbpattern.prefs, Palette.prefs, Icontrol.prefs.
`get ENV:Sys/Workbench.prefs` -> Error 205 Object not found.
ENV:Amarchy/prefs (177 bytes, 14:43:50): desktop-theme on, flat-system-look on, flat-window-gadgets on.
Status: Amarchy (VTCX:Apps/bin/Amarchy) and UP-Term:bin/amarchy-flat running. No Amarchy log file found.
Source:
- Only caller of desk_dos_apply: amarchy/gthemes_dos.c:225 (gt_dos_select = a theme pick). dk_wbborder (icon border, desk_dos.c:830) is called only from desk_dos_apply (desk_dos.c:1046). The wbpattern write is in dk_apply (desk_dos.c:423) and dk_flat_pens (desk_dos.c:714), also reached only via desk_dos_apply.
- Start path in amarchy.c main: desk_dos_flat_pens_refresh (amarchy.c:582, desk_dos.c:1011) and desk_dos_marker_recover (amarchy.c:583). The refresh only runs when flat && palette.prefs exists and is stale (desk_flat_start_refresh, desk_core.c:430, requires curlen > 0). With no Palette.prefs it returns 0 and does nothing. It never touches Workbench.prefs or wbpattern.prefs.
- reload (amarchy.c:386) calls desk_dos_flat_sync (flat state to amarchy-flat only) and borderblank_follow; neither writes the border or backdrop.

# S31-START-APPLY fix (2026-11-02, rig 56, OS 3.2)
Invariant: whenever Amarchy runs, the ENV: desk prefs match the Amarchy prefs. Before, only a theme pick (desk_dos_apply) wrote them.
Fix: desk_core.c desk_start_plan (pure: which of Workbench.prefs border/gauge and wbpattern.prefs differ from what the prefs want)
and desk_dos.c desk_dos_start_apply, called from amarchy.c main after desk_dos_marker_recover. No confirm requester. Sentinels
AMARCHY_DESK_START_PLAN (plan bits: 1 = Workbench.prefs, 2 = backdrop pattern) and AMARCHY_DESK_START_WRITES.

Host: `make test ONLY=desk_start_apply` [OK] 11 checks, 0 failed. With desk_start_plan stubbed to 0: [FAIL] 3 of 11.
`make test ONLY=desk` [OK] 2218 checks, 0 failed.

Rig 56 run 1 (ENV:Sys had only screenmode.prefs + Pointer.prefs, ENV:Amarchy absent; prefs written with the 3 keys on):
- AMARCHY_DESK_START_PLAN=3, AMARCHY_DESK_START_WRITES=2.
- ENV:Sys gained Workbench.prefs (84 bytes, WBNC borderless word at 0x36 = 0001), WBPattern.prefs (410), Palette.prefs (434).
- ENVARC:Amarchy/desk-original got ENV-Sys-Workbench.prefs.absent and ENV-Sys-wbpattern.prefs.absent (OFF restores "no file").
- ENVARC:Sys untouched.
- Finding: IPrefs "System Request": "Intuition is attempting to reset the Workbench screen. Please close the ..." with the
  "Amarchy bar" window still open. The start writes ran with our windows open; dk_apply closes them around a reset.
  Fix: desk_dos_start_apply now closes our windows (dk_windows_close, the main-process hooks) around the writes when plan != 0,
  waits for IPrefs (dk_wait_iprefs) and reopens them. Plan 0 (the steady state) touches no window.

Rig 56 run 2 (fresh rig, same prefs, window close fix):
- No "System Request"; the bar reopens after the reset. AMARCHY_DESK_START_PLAN=3, WRITES=2.
- Shots (raw grab 640x256): ~/Desktop/upterm-shots/s31-startapply-before.png / -after.png.
  Root backdrop region x 520-630: before 2 colours, grey 170 plus 380 white dot pixels (the pattern); after no white dots,
  solid pen 7 fill (238,153,0) plus window grey. SYS: window left border strip x 29-44: before 3 colours (the volume gauge:
  blue 102,136,187 / white / black); after 1 colour (grey 170): gauge gone. Icon frames gone on the drawer icons.
- Second finding: restart with the run-2 binary gave PLAN=2 every start (a backdrop write and a Workbench reset per start).
  The WBPattern editor stores the all-pen-0 DRAWER and SCREEN chunks with depth 1, desk_wbpattern writes depth 3 / 0,
  so the byte compare never matched. Fix: desk_wbpattern_same (same header, which/flags and pen per pixel) in desk_start_plan.
  Regression check in test_desk_start_prefs_on_writes_border_gauge_backdrop: byte compare stubbed back -> [FAIL] 1 of 14.
- Rig 56 run 3 (desk_wbpattern_same binary, ENV: as the editor left it): PLAN=0, WRITES=0, WBPattern.prefs and
  Workbench.prefs times unchanged, bar stays open.
Open: the solid backdrop shows pen 7 orange on the 8-bit rig Workbench, not a theme tone; dk_apply clamps depth to 3 the
same way, so a theme pick gives the same pen. Palette.prefs was rewritten at 15:42 while Amarchy ran, not by the start path.
