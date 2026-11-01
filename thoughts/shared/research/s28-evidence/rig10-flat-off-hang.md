---
date: 2026-11-01
topic: s28 flat OFF hang, rig 10 (060, OS 3.2), build de007867
status: open - cause not yet named, no fix
---

# Rig 10: Amarchy stops answering after a flat prefs write

## Setup
- Rig 10 `--max --os 32`, purpose "Amarchy s28: flat OFF hang hunt".
- RAM:Amarchy = build/amiga/Amarchy from de007867 (reload() calls desk_dos_flat_sync).
- UP-Term:bin/amarchy-flat md5 18d0667922958fbae7265ee377007c20.

## Stack 262144 probe (mandatory first probe): the hang STAYS
- Started with `Stack 262144` then `Run >NIL: RAM:Amarchy`; ps showed STACK 262144.
- Wrote a prefs file with flat-system-look=on and flat-window-gadgets=on to ENV:Amarchy/prefs.
- amarchy-flat started: STATUS `flat=on calls=0 chained=0`.
- Right after this ON write Amarchy stopped answering: `rexx AMARCHY STATUS` gave no reply in 30 s.
- The task stays in state `ready` over repeated ps samples (busy loop, not a Wait). FREE 233502, so SP is about 28.6 KB deep.
- Conclusion: the stack size is not the cause, and the hang starts with the ON sync. The reported OFF symptom
  (STATUS stays flat=on) follows: Amarchy is already dead, so the OFF write is never handled.

## Where it spins
- 15 s snoop on `*Amarchy*`: zero DOS calls. Pure CPU loop.
- taskdump of the saved context: sp 102d4fae, stack 1029bf90-102dbf90. Saved PC/first register moved
  1000deba -> 1000deb4 between samples. 0x1000dexx is OUTSIDE the Amarchy code hunk
  (0x1010406c..0x1013f420), so the loop runs in other code in low fast RAM (a library or patch loaded early).
- Above the registers the stack holds a linear incrementing pattern (`000033e4 002133e4 102133e4 ...`) over about 24 KB,
  plus 1379 longs of 0x20000000. No Amarchy return addresses in that 24 KB.
- Amarchy return addresses only near the stack top: code offsets +30948, +30950 (x4), +30994 (x2), +309a4 (x2).
  The link map (vlink -M, relinked binary byte-identical to the build) puts them in `desk_contrast`
  (amarchy/desk_core.c:14, entry 0x30948, desk_luma at 0x30900). These are pure functions, so they are most likely
  stale frames from theme computation, not the loop itself.
- amarchy-flat task: state wait, stack 101e4630-101ec630.

## Open
- Name the owner of 0x1000deb4..0x1000deba (which library/patch) and the caller chain into it. Candidates in the
  ON path: keymap_reload_compact / cx_install after desk_dos_flat_sync (amarchy/amarchy.c:326-335), or a draw hook
  patched by amarchy-flat running on Amarchy's task (win_refresh_frames, amarchy/win_refresh.c:31, RefreshWindowFrame).
- A third taskdeep sample returned no output in 40 s (not explained).
