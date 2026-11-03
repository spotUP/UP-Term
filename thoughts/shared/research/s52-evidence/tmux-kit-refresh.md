---
date: 2026-11-02
topic: TMUX-KIT-REFRESH
---
# Kit ships a current tmux

Before: tmux-amiga/build/tmux-bin dated 17 Oct, md5 2ad318db679882a3a6d2d29d4f674806, lacks tmux-amiga 4beb716 (pane console fix). dist-base copied it unchecked. Rebuild failed: Makefile.amiga NCURSES pointed at vtcon/build/rig/vtc/pkgs/ncurses-5.5-1-p-bin-m68k, which no longer exists.

## tmux-amiga (branch s52-ncurses, commit 99e6377)
- Makefile.amiga extracts vtcon/build/pkgs/ncurses-5.5-1-bin-m68k.lha (`lha xqf`, same tool the vtcon Makefile uses for its archives) into build/ncurses/ and points NCURSES there; object files depend order-only on the stamp. Overridable by NCURSES= / NCURSES_LHA=.
- `make -f Makefile.amiga build/tmux-bin` in the worktree tmux-amiga-s52 at 4beb716 plus the fix: rc 0, 1095256 bytes.
- New tmux-bin md5 cab20eccab3222d5c933a03e7e4b1148, copied to the main checkout's build/tmux-bin (main checkout clean, at origin/main 4beb716 before the copy).
- 4beb716 proof: the shipped binary is stripped (no symbols), so the same objects relinked without -s show `amiga_console_from_fd` (nm count 1; added by 4beb716 in amiga/console.c). Old binary was 1095068 bytes; new 1095256.

## vtcon (branch s52-tmuxkit)
- tools/tmuxfresh.py: dist-base runs it before `cp $(TMUX_BIN) $(KIT)/Files/tmux`. It fails when the binary is missing, older than the last commit touching Makefile.amiga/amiga/tmux/libevent, or those inputs have uncommitted changes.
- tests/test_tmux_kit_fresh.py (5 tests, unittest). Before: no tools/tmuxfresh.py on main, so the import fails (ModuleNotFoundError). After: `python3 -m unittest tests.test_tmux_kit_fresh` Ran 5, OK. The real script returns exit 1 with "stale tmux" on an old binary and 0 on a fresh one.
- Real files: `tools/tmuxfresh.py tmux-amiga tmux-amiga/build/tmux-bin` exits 0 with the new binary.
