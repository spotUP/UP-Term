# Native splits phase 0 findings (2026-10-19)

Status: NS0.2 INCONCLUSIVE (leaning fails), NS1.2 rig half NOT DONE, NS0.1 NOT DONE.
Tool-call budget (~50) ran out. Open items stay open in the plan.

## NS0.2 spike: two tab processes, disjoint rects, ONE window, no hide/show

Spike worktree vtcon-spike (branch spike/ns0.2), never merged, now removed and
the branch deleted. Diffs kept in the session scratchpad:
ns0-spike.diff (variant A, clip_sync active) and ns0-spike-variantB.diff
(variant B, clip_sync returns early, no layer clip).

Setup: host binds with vr_set_rect on the left half; the tab (own_rp, foreign_window)
gets vr_set_rect on the right half in bind() and in TM_REGISTER; vtwin_show(0)
is a no-op. Host ran a continuous Echo loop, tab was opened with Right Amiga T.
Screenshots analysed as pixel dumps (800x600).

Evidence:
- Variant A: host pane (left) draws its sentinel text cleanly and continuously.
  Right pane shows stale backdrop pixels, no tab text.
- Variant B (no InstallClipRegion at all): right pane still shows no tab text.
  So the missing tab output is NOT explained by the clip alone.
- Typing `echo alive >RAM:tabproof` into the tab produced no file. `Status` listed
  only one UP-Term:bin/vsh process, so the tab process may never have started
  or has died. Not isolated.
- No guru and no crash seen. Two hangs (ami.reboot timeout 240 s; later `Version`
  stuck 60 s) happened while the host Echo loop ran. Cause unknown: CPU flood
  of the 68k versus a layer-lock deadlock. Not distinguished.
- Not checked: leak outside rect by the tab, cursor corruption (tab drew nothing).

Design hazard (code reading, unconfirmed by evidence):
render/amiga_render.c clip_sync calls InstallClipRegion(r->win->WLayer, rg).
The clip is per LAYER, not per RastPort. Two panes in one window overwrite
each other's clip, last installer wins. The plan's "per pane RastPort clip"
assumption looks wrong. Per-pane clipping needs either per-draw
install/restore under LockLayer, or no layer clip with strict rect-bounded
drawing in the renderer. Needs a decisive test before NS1.

Next step for NS0.2: add VTCON_DEBUG serial logging in tab_register, vtwin_show
and bind to see whether the tab process starts and what r.off/hidden/cols/rows are.

## NS1.2 rig half: not done
Rig 4 could not be driven further (jammed, see above). Build of this branch not installed.

## NS0.1 baseline: not done
No numbers. tools/rig/tmux_rig.py needs the patched ixemul, PTY:, the kit NOT
installed and VTC:tmux from tmux-amiga; presence of that build is unverified.
engbench is built via tools/prof68k.py (run ONLY w PERS p) or the Makefile.

## Attempt 2 (2026-10-19, rig 4, HEAD fdba16a7 + ns0-spike.diff, SERIAL=1 build)

Verdict: two handler processes DO draw into one XCON window at the same time; no guru and no
hang seen. Pane isolation is NOT proven clean (see Corruption). Diff with the serial logs:
scratchpad ns0-spike2.diff (worktree and branch spike/ns0.2b removed).

Why the tab did not draw in attempt 1: not reproduced. With a fresh handler (install + full
stop/start of the rig) the serial trace reads: tab_spawn -> tab_spawn_go -> tab_register ->
tab_reg_ok (ok=1, inset=15) -> TM_SHOW with off=1 -> vtwin_show -> off=0, rect_x=312 (bind put the
tab on the right half). The tab draws (vsh banner, prompt) and `echo alive >RAM:tabproof` made
the file. Status shows the tab's vsh (the host runs the AmigaDOS Shell, so "one vsh" was the tab's
own, not a missing one). Most likely attempt 1 ran a stale handler (rig.py install needs a
reboot) or the pre-fdba16a7 clip variant. Product path from spawn to show has no bug at HEAD.

Rig gotcha: FS-UAE cannot open serial_port when the file does not exist (fs-uae.log:
"SERIAL: Could not open device"), so no serial.log is written. `touch build/rig4/serial.log`
before start. rig.py does not do it for rig 2+ (not checked for rig 1).

Concurrent draw: host `List SYS: ALL` (left, 39 columns) and tab `List SYS: ALL` (right,
38 columns) streamed together and finished; wasabid still answered; cursors both at their
pane's prompt row (host `6.BOOTX:>` left, tab `SYS: %` right).

Corruption (screenshot, unverified by pixel diff): row 1 of the scrolled output shows host text
running to x=490, i.e. past its 312 px pane into the tab's; `emppty` / `emptyy` double letters
where the two panes' wrapped lines meet. Not isolated: could be a host-grid width of 39 versus
the tab's 38 plus a scroll that blits full-width rows. Needs a pixel-level test (clean run with
only one pane streaming, then check columns >= 312 for host-only output) before NS1.
Run length was well under 2 minutes, so the 2-minute no-hang bar is NOT shown.
