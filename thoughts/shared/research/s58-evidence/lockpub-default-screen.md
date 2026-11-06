# LOCKPUB-DEFAULT-SCREEN: every LockPubScreen(0) site in amarchy/*.c

Counted by: `grep -n "LockPubScreen" amarchy/*.c`, minus Unlock lines and comments. Result: exactly three calls with a 0 name
(gui.c:1768, leaf.c:434, strut_patch.c:126). Every other call passes through `ipd_lock_pub_screen` (iconpen_dos.c:137, the only
direct named call). `LockPubScreenList` users (bar.c:361, iconpen_dos.c:134/207/320, wbreset_patch.c:37) are list reads, not locks.
tests/test_wb_lock_guard.py:16 exempts `LockPubScreen(0|NULL)` by regex (comment at line 14), so the test does NOT cover these three.
Also related: handler/frontscr.c:43 `lock_front_screen` locks by the front screen's listed name, no list-state check (not NULL, out of scope; it
reads the node name but not ip_pub_lockable; unverified whether the front screen can be a private WB node in the window).

## Existing guard (not used by the three sites)
iconpen_dos.c:119-141 `ipd_lock_pub_screen(name)`: takes LockPubScreenList, finds the node, calls LockPubScreen only if
`ip_pub_lockable(listed, has_screen, flags)` (iconpen_core.c:251: listed && screen && !PSNF_PRIVATE), then unlocks the list.
Returns 0 for a NULL/empty name. Fixes BOOT-31-WB-DEADLOCK and S57-FREEZE-2IN9. Linked in Amarchy, amarchy-menu, amarchy-gui,
amarchy-flat, amarchy-icons (Makefile:957, 1023, 1037, 1057, 1068). Not read: start-pointer-hang.md cause sections (that file is about the
Pointer prefs hang; the guard text is in the iconpen_dos.c comment and test_wb_lock_guard.py docstring).

## Table

| Site | Function | Programs (Makefile) | Call path | Verdict | Reason |
|---|---|---|---|---|---|
| amarchy/gui.c:1768 | screen_bind (1764) | amarchy-gui (Makefile:1037) | (a) startup: main -> screen_bind gui.c:1870 (user/hotkey started). (b) WB reset: desk_dos.c:257 dk_windows_close -> desk_dos_own_close = gui_hide (gui.c:1938, 1695); desk_dos.c:270 dk_windows_open -> desk_dos_own_open = gui_show (gui.c:1939, 1709) -> screen_bind gui.c:1715. Reached only when lock_front_screen() (frontscr.c:38) returns 0, i.e. the front screen is not in the public list. | AT RISK | Path (b) runs inside the WB close/reopen sequence (same sequence as the boot RTG switch). If the front screen is not a listed public screen at that moment, the unguarded LockPubScreen(0) takes Intuition's reopen path for a closed or private default WB. Path (a) is user-started and low risk. Unverified: whether dk_windows_open runs strictly after dk_wait_reset (desk_dos.c:283) finished, I did not read the caller of dk_windows_open. |
| amarchy/leaf.c:434 | leaf item enter, SD_FONT / SD_SCREENMODE branch (leaf.c:415-440) | amarchy-menu (Makefile:1023), amarchy-gui (1037) | User picks a font or screen-mode setting entry; ui->win null (no window) and lock_front_screen() (leaf.c:432) gave 0. Triggered only by a menu click, no timer or boot job found. | SAFE (low risk) | Only a user click reaches it, and the ASL requester it feeds opens on that same default screen anyway. Not reachable by a boot job or tick (grep of callers: only the leaf item handler). Could still hit a closed WB if the user clicks mid-reset: unverified, no test. |
| amarchy/strut_patch.c:126 | on_bar_screen (113) | Amarchy (Makefile:957 only) | OpenWindow / OpenWindowTagList patch hooks: tags_hook strut_patch.c:230 and the NewWindow hook strut_patch.c:262, run in the context of ANY task that opens a window with no screen, name or Workbench flag. Guard before it: `p->strip > 0` (set by strut_publish, bar.c:517; cleared by strut_clear, bar.c:527 in bar_close, which dk_windows_close triggers via BAR OFF desk_dos.c:260-262). | AT RISK (narrow) | While the bar is open strip is > 0 and the hook calls LockPubScreen(0) from arbitrary tasks. When Amarchy closes the bar first (desk_dos reset path) strip is 0 and the site is skipped, so the amarchy-driven reset is covered. It is exposed when the WB closes without bar_close running first (IPrefs or a screen-mode change not routed through desk_dos): strip stays > 0 with a stale screen and any task's default-screen OpenWindow runs the unguarded lock. The sibling branches at 122-125 already use ipd_lock_wb_screen / ipd_lock_pub_screen; only the default branch is bare. |

Count: 3 sites. 2 AT RISK, 1 SAFE (low risk), 0 GUARDED.

## Smallest fix per AT RISK site

- strut_patch.c:126 (default branch of on_bar_screen): resolve the default name without locking (GetDefaultPubScreen into a
  MAXPUBSCREENNAME buffer; V36+ call, unverified that it is safe in the window, it only copies a name) and pass it to the existing
  `ipd_lock_pub_screen` as the named branch on line 124 already does. A not-yet-lockable default then reads as "not on the bar screen"
  (return 0), which is the desired answer. Fix the exemption comment in tests/test_wb_lock_guard.py:14-16 accordingly.
- gui.c:1768 (screen_bind fallback): same replacement, a shared helper `ipd_lock_default_pub_screen()` next to
  `ipd_lock_wb_screen` in iconpen_dos.c (reuses ip_pub_lockable), called from gui.c, strut_patch.c and also leaf.c:434 so the test
  regex can drop the 0/NULL exemption entirely. gui_show must then tolerate a 0 return and retry on the next tick (screen_bind
  already returns "no screen" and gui_show returns early at gui.c:1715).

## Fix

New `ipd_lock_default_pub_screen()` (amarchy/iconpen_dos.c, declared in iconpen_dos.h): `GetDefaultPubScreen(buf)` with a
MAXPUBSCREENNAME+1 buffer, then `ipd_lock_pub_screen(buf)`. No second list walk. The three bare `LockPubScreen(0)` calls now use it.
tests/test_wb_lock_guard.py has no 0/NULL exemption: any `LockPubScreen(` in amarchy/*.c outside iconpen_dos.c fails. Docstring and
the Makefile ONLY=wb_lock_guard comment updated.

Callers on a 0 return:
- gui.c screen_bind: `if (!g->scr) return "amarchy-gui: no screen\n"` right after (unchanged path); gui_show returns early on it
  and the next tick binds again.
- leaf.c (SD_FONT / SD_SCREENMODE entry): `where.scr` stays 0, the same state as when no front screen is listed and ui->win is null;
  the `if (where.scr) UnlockPubScreen` after lf_setting_pick already guards it. The requester opens with no screen given.
- strut_patch.c on_bar_screen: `if (!s) return 0` ("not on the bar's screen", window left alone), the same 0 path the
  named and Workbench branches already use.
- Safety in the OpenWindow patch (any task): ipd_lock_pub_screen only does LockPubScreenList / LockPubScreen of a screen found
  in the list / UnlockPubScreenList, the same calls the two sibling branches in that function already made from the same hook;
  GetDefaultPubScreen only copies a name under Intuition's own lock. It never opens a screen.

Link lists: `make -n -B` for Amarchy (strut_patch.c), amarchy-gui (gui.c, leaf.c), amarchy-menu (leaf.c) all already link
amarchy/iconpen_dos.c; wbrun, jobprobe and amarchy-index link none of gui.c, leaf.c, strut_patch.c; amarchy-flat and amarchy-icons
link iconpen_dos.c and none of the three. No Makefile link change needed.
