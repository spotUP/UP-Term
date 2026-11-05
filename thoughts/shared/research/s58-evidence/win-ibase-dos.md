# AMARCHY-WIN-IBASE-DOS

Violated invariant: under LockIBase a program only reads IntuitionBase lists; DOS calls and lock-taking or blocking Intuition
calls run outside it (amarchy/win.c header). Root-cause level: the verb dispatcher win_exec, which loaded the config inside the
lock. Fix level: gather/load first, lock, copy, unlock, apply.

## Lock spans in win.c (7 pairs)
- 98 win_ticket_take: reads ActiveWindow, member(), geometry copy. Plain reads. OK.
- 120 win_target_rect: ticket_valid/usable, geometry copy. Plain reads. OK.
- 141 win_active_terminal: reads, pure win_core predicate. OK.
- 191 do_screen: FirstScreen walk only; ScreenToFront etc. run after unlock. OK.
- 219 do_title: reads Flags only; ShowTitle after unlock. OK.
- 253 win_exec main span: reads + win_box_of/bar_usable (pure list reads) + win_core + ChangeWindowBox/WindowToFront/WindowToBack/ZipWindow
  (queue-only, documented K6 design, kept under the lock so the window cannot close between check and call).
  VIOLATION: amconf_load(&p) at line 273 (WV_SNAP), a DOS file read. FIXED: loaded before the lock, tile gap held in a local.
- 329 snap re-lock: member(sw) then win_apply_box (queue-only). OK; this is the pattern reused (re-check membership under a fresh
  lock before acting on a pointer held across an unlock).

Other files scanned by the guard: no further violations (borderblank, flat, desk_dos, strut_patch, tile, shot, win_refresh).

## Guard
tests/test_ibase_lock_guard.py (Makefile ONLY=ibase_lock_guard). Fail before fix: `win.c:273 amconf_load()`. Pass after fix.
The queue-only calls ChangeWindowBox/WindowToFront/WindowToBack/ZipWindow are deliberately not forbidden (K6 design; tile.c
does the same); moving them out would lose the atomic membership check.

## Open
Rig check: WIN hotkey snap (WIN verb snap left/right) on a rig, window moves and tile gap still honoured. Not run (no rig).
