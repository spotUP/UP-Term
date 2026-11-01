---
date: 2026-10-27
topic: Amarchy terminal menu (amarchy-menu) open latency on the real A1200
---
# Where the seconds go when amarchy-menu opens (A1200, measured)

## How it opens
Left Amiga+Space = keymap row `super+space = menu:root` (dist/keymap:37). The commodity (UP-Term:bin/Amarchy) runs
do_row -> act_menu_kind (amarchy/amarchy.c:97) -> cx_menu_cmd (cxdesc.c:53, style text -> "amarchy-menu") ->
keymap_load + mn_osd_size_t (amarchy.c:118-122) -> act_in_window (act.c:83): Open of a NEW borderless window
`XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS 37x12/SCREEN Workbench`, then SystemTags(async) of
`Rushhours:UP-Term/bin/amarchy-menu menu root`. Not inside the UP-Term window. Esc closed it each time (XCON count back to 1).

## Marks (1/50 s), three opens
| open | hotkey | start | terminal | ready | hotkey->start | start->terminal | terminal->ready |
|---|---|---|---|---|---|---|---|
| 1 | 415129 | 415209 | 415215 | 415221 | 80 = 1.60 s | 0.12 s | 0.12 s |
| 2 | 416271 | 416356 | 416372 | 416378 | 85 = 1.70 s | 0.32 s | 0.12 s |
| 3 | 421989 | 422066 | 422072 | 422078 | 77 = 1.54 s | 0.12 s | 0.12 s |
Total about 1.8-2.1 s. hotkey->start is 85-90 % of it. No mark after "ready", so ready->rows is not separable.

## Snoop of open 2 (--ignore-wasabi, wall clock, A1200 time)
```
21.147 UP-Term:bin/Amarchy  GetVar("AMARCHY_TIMING")            (mark hotkey)
21.151 Amarchy  Open("ENV:up-term/keymap", read) = ok  (+ keymap.user fails, FindPort DOPUS.1)   -> commodity work 21.147..21.156 = ~10 ms
21.156 Amarchy  Open("XCON:0/0/0/0/Amarchy/NOBORDER/CENTRE/FRONT/CLOSEINACTIVE/CELLS 37x12/SCREEN Workbench", create) = ok
21.162 Amarchy  SystemTagList(""Rushhours:UP-Term/bin/amarchy-menu" menu root") = rc 0
21.162 Background CLI  Lock("Rushhours:UP-Term/bin/amarchy-menu", read) = ok
        ... 1.6 s with no call from the new process; only other tasks' FindTask polling
        (TCP/IP Control: 243+ identical FindTask("(null)"), AmiAgent, PoPo, dopus_clock)
22.769 Background CLI  LoadSeg("Rushhours:UP-Term/bin/amarchy-menu") = ok
22.769 amarchy-menu  OpenLibrary bsdsocket v3 / usergroup v1 / ixnet v44 / ixemul v47, timer.device   (ixemul startup)
22.810 amarchy-menu  Open("DEVS:Internet/users"), etc:passwd fail (218 Device not mounted)
22.874 amarchy-menu  Open("DEVS:Internet/groups")
22.875 amarchy-menu  GetVar("AMARCHY_TIMING")                    (mark start)   = 22.875 - 21.147 = 1.73 s after hotkey
22.939 amarchy-menu  OpenLibrary keymap.library; Open("ENV:up-term/keymap")  (index/keymap load 22.94)
23.101 ... first draw / ready ~0.2 s later
```
The commodity side (keymap_load, mn_osd_size_t, window open, SystemTags) is ~15 ms. The gap is entirely between
Lock and LoadSeg of amarchy-menu = 1.607 s (21.162 -> 22.769), then ixemul/ixnet/usergroup init 0.1 s.

## Cross checks
- amarchy-menu is 449140 bytes (879 blocks). `Copy` of it to RAM: takes 0.49 s wall for wasabi incl. ~0.2 s round trip
  (Version = 0.2 s), so a (probably cached) read is ~0.3 s; amarchy-gui (460116 bytes) the same. So raw file read does not
  explain 1.6 s on its own; the load runs while the CPU is shared with busy-polling tasks, and LoadSeg also relocates
  (hunk relocs of a gcc/ixemul binary) on a 68k.
- Running `amarchy-menu` bare from a shell (usage + exit, so LoadSeg + ixemul init + exit): 1.30-1.33 s wall minus
  0.2 s = about 1.1 s every time. `amarchy-gui` the same way: 3.9 s. So the cost is program load and startup, not menu logic.

## Drawn menu
Not measured. The text style is the current setting, so Left Amiga+Space opened the text menu. `othermenu:root` is
`super+alt+space`; wasabi key press lamiga+lalt+space and lamiga+ralt+space opened nothing in 6-12 s (no mark file written).
Only datum: `amarchy-gui` exits with usage after 3.9 s, so its load+startup is about 2.5x the text menu's.

## Source lines in the gap
Nothing of ours: the gap is DOS LoadSeg of amarchy-menu (invoked by act.c:100 SystemTags) plus ixemul.library startup
(menu.c main before the "start" mark). Our code starts at the "start" mark (menu.c main). Candidate levers (not tried):
a smaller binary or fewer hunks/relocs, resident (`Resident`/pure flag) or keep amarchy-menu loaded, avoid ixemul/ixnet/
usergroup init (bsdsocket/usergroup/passwd lookups cost ~0.1 s), find the CPU hog (TCP/IP Control FindTask loop).

State left: AMARCHY_TIMING unset, no menu open, snoop ended (--for 14). Raw snoop log:
/private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/507873e1-4856-478f-9521-fa6545b8c386/scratchpad/snoop.log
