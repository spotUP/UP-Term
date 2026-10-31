---
title: NVIM-HEAP-CORRUPT recheck on a fresh rig (vtcon-05 s38)
date: 2026-10-31
---
HEAD 351a0382, rig 4 (build/rig4 wiped, fresh clone, `start --max`). nvim a3aa3137 (build/dist/up-nvim/Files/nvim) in VTC:nvimd, config build copy of neovim-amiga/build/v012/userconf/home in VTC:uhome, `SetEnv HOME /VTCX/uhome` (ENV: is lost on every rig start).
Libs: LIBS: first entry VTCX:ixp6; rig ixemul a8a7bf4c / ixnet 9247e6d4 == build/dist/up-term/Files/libs (also SYS:Libs/ixemul same size 201256). Start printed no drift WARN (only the Directory Opus WARN).
Host: other rigs 1, 5, 8 running at 100%+ CPU each, Amiga-View 40%; no vitest.
Runs (`nvim --headless [+startuptime] +q` via ami.py exec):
- A1/A2/A3 (first boot, HOME=uhome, --startuptime RAM:..., one with -u NONE): FS-UAE process exited ~1-2 s after nvim started, rig down, no crash report, log cut mid-line. 3 times.
- normal x3 right after (HOME=uhome): same, rig down in 0-2 s.
- control: -u NONE, HOME unset: PASS rc 0; -u NONE HOME=RAM:h PASS; -u NONE HOME=/VTCX/uhome PASS (rig not restarted since).
- normal config (HOME=uhome) run 1: "the Amiga stopped answering while nvim was running (25 s)", FS-UAE alive at 55% CPU = HANG, no requester readable (wasabi dead). Run 2: connection timeout, rig down.
No "free: start of block corrupt!" text seen (no wasabi to read requesters once dead). Resident libs match the build, so stale libs are not the cause.

## Songs A/B (s38)
Rig 6, fresh `start --max`, nvim a3aa3137 copied to VTC:nvimd, uhome from neovim-amiga/build/v012/userconf/home, `SetEnv HOME /VTCX/uhome`, `nvim --headless +q`. VTC: = host build/rig6/vtc (hard_drive_1, label VTCX).
- "cannot be represented in ISO-8859-1" comes from Emulators/fs-uae/src/od-fs/filesys_host.cpp:130 (my_opendir): it only write_log()s and `continue`s; no exit. The 3 files are Songs/deflemask/AceMan - Protège Moi.dmf, Songs/behringer-td-3/Phuture U+200E- Acid Traxx.seq, witõka.src. The lines appear only when a directory holding them is first listed. Surviving rigs (build/rig, rig8) carry the same 3 lines in their logs, so they are not fatal.
- Rig 6 boot log had 0 such lines (Songs not scanned at boot). First run (Songs present) died and the log then had the 3 lines at the cut point.
- As-is (Songs present), 3 runs: DEAD, DEAD, ALIVE-but-no-rc (hang, wasabi silent). The 2 DEAD runs have scan=0 new "cannot be represented" lines, log cut mid-line.
- Songs moved out of VTC: (build/rig6/Songs.aside, restored), 3 runs: DEAD, DEAD, DEAD (wasabi "connection closed by the Amiga"), scan=0.
- Verdict: NOT SONGS. The exit happens with Songs absent 3/3 and without any scan line in 2/3 as-is runs. No macOS crash report today (last fs-uae .ips is 26 Oct), log cut mid-line: process vanishes without a host crash report. Cause stays in-guest (nvim/ixemul, NVIM-HEAP-CORRUPT) triggering an FS-UAE exit; exact FS-UAE exit path not found (no exit in filesys_host.cpp's opendir).
- Caveat: first attempt batch (restart race) gave 6 void runs (ami.py timeouts after a restart that never answered wasabi); only the ab3 results above count.
