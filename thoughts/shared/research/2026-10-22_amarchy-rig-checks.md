# Amarchy rig 4 checks, 2026-10-22 (recorded from agent report)

- 8adbb48e (pushed): CxFilter rejects numeric codes in description string -> rows=0. Fix: plain 'rawkey lcommand' filter + SetFilterIX for code/qualifier; cx_ixspec replaces cx_describe. Revert proof test_keymap.c:395; 1051/0. Rig rows=9 after fix.
- Check 1 heap: estimate only. Full dist keymap ~6.2-6.4 KB heap; total ~32.0-32.1 KB vs limit 32768 (600-700 B margin). Second Run refused, no leak.
- Check 5 STATUS/RELOAD PASS (hits=0 rows=8 enabled=1 active=0). Notify auto-reload works. RUN <n> untested.
- amarchy_rig.py 5/7 (r1_commodity, r3_terminal fail). keymap_rig (B5) 1/12. All failures: hotkeys never fire under wasabi key injection (Left Amiga chords). Suspect injection/focus, not Amarchy; unproven.
- Not run: checks 4, 6 (amarchy_menu_rig.py), 7; A6 meta_alt_rig.py, B6, D1. No E1 clash or D1 timing script exists.
- rig.py install does not install Amarchy/amarchy-menu (used ami.py put).
- dist/keymap header does not say user rows need [bind] section.

## Run 2 (3afd0bbb)

Rig 4, `rig.py start --max`, handler/Amarchy built from 3afd0bbb. No source edits, no commits.
Setup note: on rig 4 `UP-Term:` is `Rushhours:Tools/UP-Term` (a kit drawer), not VTC:Apps, so `rig.py install` (which stages to VTC:Apps/bin) leaves UP-Term:bin with no Amarchy/amarchy-menu and `UP-Term:bin/Amarchy` (used by amarchy_rig.py:68 and amarchy_menu_rig.py:64) fails. I copied both into UP-Term:bin on the rig disk by hand; System:C/Amarchy is an older stray copy.

- Heap (method: AvailMem via `Avail FLUSH`, before/after `Run >NIL: Amarchy`; differential dist keymap vs a `[bind]`-only keymap; ARexx STATUS does not report memory):
  - total drop at start with full dist keymap: 66152 bytes (includes 16384 stack, 25344 file+BSS from `make amarchy-size`, ports/broker).
  - same start with an empty keymap: 48208 bytes. Keymap-attributable heap (parsed table + 8 CxFilter chains): 17944 bytes (auto-reload to dist from empty: 24136, which also holds +6072 for the RAM: file copy; 24136-6072 = 18064, consistent).
  - Plan definition file+BSS+keymap heap = 25344 + 17944 = 43288 bytes vs limit 32768: FAIL by 10520 bytes. The 6.2-6.4 KB estimate was wrong by about 11.6 KB. Even file+BSS+heap without stack exceeds the limit.
  - QUIT returns memory: before start 1075819688, after QUIT 1075819624 (64 bytes, no leak).
- Check 4 (bad edit then RELOAD): PARTIAL/FAIL. Only a newer `[keymap] version` is refused: file with `version = 99` appended, RELOAD -> `rows=8` kept (PASS for that case). Garbage rows mixed into the dist file: bad rows skipped, good rows kept, rows=8. A junk-only file (bytes 1..199 x3): RELOAD swaps to `rows=0` (all hotkeys lost), contradicting "a bad edit leaves the old set active". Cause: config/keymap.c:load (about line 595-612) returns 1 for any text, only `version > KM_VERSION` returns 0 (line 633-638); config/keymap_load.c:keymap_reload swaps whenever load returns 1. Auto-notify reload behaves the same as RELOAD. Restoring the good file gave rows=8 again.
- Check 6 (amarchy_menu_rig.py --fresh): FAIL 2 of 6 (start, esc_up pass). open_timing 10.06 s (timeout), type_th, enter_slash (km-hits +0), viewer_rows (0 lines) fail; all start from the Left Amiga+space / +k chord, the known injection issue. Menu content itself not verified. One UP-Term window was left open by the run.
- Check 7 (window close frees memory, handler per-window keymap included): PASS. 6 rounds of `NewCLI XCON:.../CLOSE FROM script` (open then EndCLI): avail 1075178960 before, 1074277632 open (-901328), 1075178960 after close, identical every round.
- ARexx `RUN <n>`: PASS. With dist + keymap.user sentinel row (rows=9): `RUN 8` creates T:amarchy.sentinel (rc 0 of List), RUN 0..7 do not; hits +1 per valid row; `RUN 9` and `RUN 99` do not move hits (no such row). Result strings are empty; rc not inspected from the ARexx reply.
- Cleanup: Amarchy QUIT, ENV/ENVARC keymap files I created removed.
