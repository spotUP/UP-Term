# B4 Amarchy demo tour

Entry: the menu row `learn.amarchydemo` (`rexx:address AMARCHY DEMO`) and the CLI `Amarchy demo [stop|status|<n>]`
both end in the one verb `DEMO` (rexx_cmd -> demo_port). The CLI is a thin sender (`demo_cli` in amarchy.c, before
ReadArgs): it sends the verb through `rexx_send`, prints the reply, returns 5 with "Amarchy is not running" when the port is
absent. `demo_port` only arms state; every action runs from `demo_tick` (1 s commodity tick, main task).

## Scenes (amarchy/demo_core.c, total 116 s)
Windows move/size/zip 12; Snap 12; Focus and cycle 10; Tiling layouts 14; Themes 14; Desktop look 10; Status bar 12;
Notices and clips 10; Capture 10; Launcher and packages 12. After the last scene it wraps (loops+1) until a key or click.
Scene bodies call win_port, AmTile's port verbs, bar_port (injected with demo_set_bar), note_run, shot_screen,
gt_dos_preview_send. The demo opens two windows titled "Amarchy demo A/B: ..." so the window scenes work on an empty desktop.

## Snapshot (static g_snap, applied by stop_demo)
Box of every user window (win_box_of), active window, front screen, theme name (gt_dos_current), AmTile STATUS
(on/off, layout, nmaster, ratio), bar_is_open(). Stop order: input watch off, AmTile put back, bar put back, desktop
colours restored (gt_dos_desk_restore + gt_dos_term_send of the saved theme), demo windows closed, boxes re-applied to windows still
present, active window and front screen back, sentinel AMARCHY_DEMO_DONE=loops. Stop triggers: STOP verb, key down or left/right
click (cx_watch_input; key repeat, key-up and mouse move ignored), Amarchy QUIT (demo_shutdown after the main loop), a snapshot window gone.

## Themes: what preview covers
`gt_dos_preview_send` changes the Workbench pens/colours and the console tab colours, in memory and in ENV:, nothing in ENVARC;
`gt_dos_desk_restore` + `gt_dos_term_send(saved)` put them back. NOT previewable: flat frames, icon styles, backdrops,
fonts (they persist through desk_dos_apply, which the demo never calls). Gap: the "Desktop look" scene shows more theme pens/colours only, not the
flat/icon changes.

## Deviations
- demo_core.c holds names and seconds only (DOS-free, host tested); the scene functions are a parallel array in demo.c.
- AmTile LAYOUT/RATIO/NMASTER write through to ENVARC (amconf); the demo restores the values at stop, so the final file content equals the start. The demo never sets GAP.
- Capture scene writes one IFF file into the shot dir and deletes it (DeleteFile(shot_last())) in the same tick.
- Launcher scene shows notices only; opening the menu viewer is skipped (cannot be closed reliably from a tick).
- End-on-input latency: the CxCustom callback sets a flag; the tick polls it, so the demo ends within one 1 s tick.
- rig test records the window boxes (not titles) after the list is stable: a start-up window (Welcome) that closes by itself ends the demo by design (snapshot window gone).
- Extra sentinel AMARCHY_DEMO_WHY: 1000 + key code or 2000 + mouse code for an input stop, 2 for a snapshot window gone. DV_STATUS counts an armed start as running.
- cx watch filters: CxFilter takes a ParseIX description, so the names are "rawkey" and "rawmouse"; an arbitrary name leaves CxObjError set and the watch never fires.
- Rig start waits for AMARCHY_DESK_START_WRITES: Amarchy's own start apply runs on the main task about 30 s after start (no Workbench on the rig, DESK_WB_READY_TICKS timeout) and blocks the loop up to 20 s. Found with sentinels and a wasabi snoop (SYS:Prefs/Palette run at tick 30). The stall is not from the demo; a demo started inside it is late.

## Latency
Key or click to stop: one tick (about 1 s); rig key_stops checks DONE within 3 s.

## Results
Rig 96 (UPTERM_RIG=96 python3 tools/rig/amarchy_demo_rig.py --fresh):
    PASS start: port up True, boxes before [(0, 0, 378, 110), (0, 11, 640, 235), (0, 20, 600, 200), (197, 11, 443, 235), (556, 0, 84, 256)]
    PASS tour: scene '3' after the polls
    PASS status: status 'demo running=1 scene=3 loops=0' (scene '3', why '')
    PASS key_stops: DONE '0' status 'demo running=0 scene=0 loops=0' why '1064'
    PASS boxes: before [(0, 0, 378, 110), (0, 11, 640, 235), (0, 20, 600, 200), (197, 11, 443, 235), (556, 0, 84, 256)] now [(0, 0, 378, 110), (0, 11, 640, 235), (0, 20, 600, 200), (197, 11, 443, 235), (556, 0, 84, 256)]
    amarchy_demo_rig: passed 5 of 5
Host: [OK] demo_core: 74 checks, 0 failed; [OK] keymap: 21574 checks, 0 failed; tests.test_rig_only_list OK.
Learn-row table hits (learn.amarchydemo added): 3 (dist/keymap, dist/menu-icons, tests/test_keymap.c count 4 to 5).

## git diff --stat
     Makefile                                        |  11 +-
     amarchy/amarchy.c                               |  42 +-
     amarchy/cx.c                                    |  73 ++++
     amarchy/cx.h                                    |   5 +
     amarchy/demo.c                                  | 526 ++++++++++++++++++++++++
     amarchy/demo.h                                  |  15 +
     amarchy/demo_core.c                             | 145 +++++++
     amarchy/demo_core.h                             |  34 ++
     dist/keymap                                     |   1 +
     dist/menu-icons                                 |   1 +
     tests/test_demo_core.c                          |  83 ++++
     tests/test_keymap.c                             |   7 +-
     tests/test_main.c                               |   2 +
     tests/test_rig_only_list.py                     |   2 +-
     thoughts/shared/s61-evidence/b4-amarchy-demo.md |  53 +++
     tools/rig/amarchy_demo_rig.py                   | 132 ++++++
     16 files changed, 1123 insertions(+), 9 deletions(-)
