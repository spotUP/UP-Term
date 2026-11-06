# boot-console-live (ab5c8512) - NOT PROVEN, blocked

`UPTERM_RIG=150 rig.py start --max --purpose ...` from worktree agent-a3c9d7b3511515a17 (build -> main build via symlink):
exit 1, before any emulator boot. rig.py found 24 stale programs (fresh worktree mtimes) and `make` failed linking
build/amiga/amarchy-index: undefined _gte_nrows, _gte_row, _gte_row_visible.
The worktree base (5161d224) lacks main's newer commit 20e21eee, and the symbol source is not in the worktree.
PASS A / window check / PASS B: not run. Rig 150 stopped + cleared, symlink removed.

## Run 2 (rig 150, branch head 73b73887, 2026-11-05)
- Clean start: exit 0, "up", boot.console exists but is 0 bytes (no error lines). Window list (ami.py uitree): "Amarchy bar", a 39x15 untitled window, the active Workbench backdrop window; no boot shell/CLI window.
- Negative: appended to DH0:S/User-Startup (outside the UP-Term block) `C:List NOSUCHVOL:` -> DOS requester blocks boot, console empty; then `C:Type RAM:nosuchfile`, `Echo "BOOTMARK before"`, `Execute RAM:nosuchscript` -> start exit 0, boot.console still 0 bytes, `Type S:User-Startup` shows the lines present. Even the Echo is not captured. PASS B FAILED: the redirect `C:Execute DH0:S/Startup-Sequence >BOOTX:boot.console` does not receive the Startup-Sequence/User-Startup output on this rig.
