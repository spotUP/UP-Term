---
date: 2026-11-01
session: vtcon-05 s46
status: phase done, one item open
---

# vtcon-05 session 46 handoff

## Done (all pushed to origin/main unless noted)
- LIBS-ASSIGN-AUDIT: 9a72dc6b. `paths.rig_only(what)` in tools/rig/paths.py refuses when AMI_HOST/WASABI_HOST is not localhost; called from `ixpty_rig.use_ixemul` and `install_rig.stock_libs`; test tools/test_rig_libs_rig_only.py (Makefile).
- rig_helpers test fix: 9716735e (`set(ir.HELPERS) <= built`).
- ANSI-MUSIC-SMILE-SFX: 3b6dddea. Owner units: Duration = BASIC SOUND ticks (1/18.2 s), CycleDelay = ms, each pause capped at 2000 ms. Cycles 0 = 1, capped at 1000; frequency outside 37..7904 Hz = silent cycle with timing kept. Sentinel `vt_music_smile_blocks()`. Findings thoughts/spot/term-findings/ansi-music-smile-sfx-impl-s46.md.
- Rig listen of MUSIC_pavt199.ANS on rig 7: PASS by owner ear (ledger row ANSI-MUSIC-RIG-LISTEN-PAVT199).
- Q3-PARK control run: verdict recorded in ledger row Q3-PARK. 1 pane on splits build 0afac5be passes 2/2; 2 panes fail 2/2 with IPrefs "System Request" (Retry/Cancel), splitswin left open on the old screen; main build wbtabs_rig 4/4 OK. Cause = multi-pane path of `wb_tabs_park` on feature/native-splits. Evidence thoughts/spot/term-findings/q3-park-s46-control.md.

## Open
1. Q3-PARK fix (needs owner go): opus root-cause read of `wb_tabs_park` multi-pane case in ../vtcon-splits (feature/native-splits 0afac5be). Then a sonnet fix + 2-pane park/unpark rig check. Requester body text still unread (IntuiText; OCR failed). `pane_count()` read None after a 1-pane park: harness counter, unverified.
2. Owner decision: build/rig6 (stopped, kept) and build/rig7 (stopped) - keep or delete. Never delete without OK.

## Gotchas learned
- `~/.config/upterm/rig-mute` overrides `--sound` for every rig (deleted this session with owner OK).
- CSI M music plays only in a PCANSI window (engine/vtengine.c:5750); spec field at handler/vtcon_handler.c:1507.
- ami.py command is `exec`, not `run`.
- Do not touch tools/rig/rig.py (other session's uncommitted edits), thoughts/shared/research/s28-evidence/flat-visible.md, tools/rig/-.
