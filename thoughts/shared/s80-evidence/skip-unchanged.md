---
date: 2026-11-14
topic: Patina start apply and the IPrefs Workbench-reset requester
branch: s82-skip-unchanged (commit c3de962e, worktree ../vtcon-s82-skip-unchanged, not merged, not pushed)
---

# Skip unchanged prefs writes (S82)

## Root cause

The start planner already skips files whose bytes equal ENV:Sys
(desk_start_plan / desk_wbpattern_same, desk_start_palette, desk_start_prefs,
patina/desk_core.c about lines 1569-1625). The requester still comes on every
boot because start apply writes ENV: only (prefs editor USE). ENVARC: is written
only on a confirmed pick (desk_dos_confirmed) and for icontrol. ENV: is rebuilt
from ENVARC: at every boot, so ENV:Sys holds the stock prefs again on each boot,
the theme bytes really differ, and the plan is PLAN=22 (PATTERN 2 + PALETTE 4 +
FONT 16) every boot.

## Measurement (rig 7, main 34c32c44 build, DOpus open, ENVARC untouched)

- Boot 1: IPrefs requester (Retry | Cancel), PLAN=22, WRITES=3.
  After: ENV:Sys WBPattern.prefs 410, font.prefs 518, palette.prefs 434.
  ENVARC:Sys: WBPattern.prefs 314 (stock), font.prefs 518 (differs at byte 51),
  no palette.prefs.
  font and palette in ENV: equal Patina's work copy; WBPattern does not (the
  editor rewrites it, first difference at byte 184; the planner handles that
  through desk_wbpattern_same).
- Boot 2 (rig.py reboot, no wipe): requester at 22 s and 55 s, PLAN=22, WRITES=3.
- Boot 3: not run (expected same as boot 2, nothing persists between boots).
- Fixed build not run on the rig: the Amiga build waited behind 10 queued builds
  and was stopped. Compile of patina/desk_dos.c is unverified.

## Change on the branch

- patina/desk_dos.c dk_put: after writing the work copy, compare it with
  ENV:Sys/<basename>; equal: count desk_dos_put_skips, sentinel
  PATINA_DESK_PUT_SKIPS, return 1 without running the editor. Covers every
  dk_put caller (start apply, dk_apply, palette, flat pens, pointer, font,
  workbench.prefs). Tag S82-SKIP-UNCHANGED.
- tests/test_patina_put_skips_unchanged.py (source-structure test), wired into
  the Makefile (TEST_PY_ONLY put_skips). PASS with the fix, FAIL with the fix
  stashed.

This change is a guard for later applies within one boot (theme re-pick of the
same theme, flat pen sync). It cannot remove the boot requester.

## Owner decision needed for the boot requester

1. Start apply persists the files it wrote to ENVARC:Sys (as desk_dos_confirmed
   does). Boot 2 then finds equal bytes and plans 0. Changes the R1.10 rule
   "persist only on Keep".
2. Apply before DOpus opens its windows (ordering in the kit Startup), so the
   Workbench reset finds no window to close. Keeps the persist rule.

## Open risks

- WBPattern byte compare never matches (the editor rewrites the file), so dk_put
  does not skip WBPattern; the planner's desk_wbpattern_same still does.
- BLOCKPEN (branch s81-blockpen) is separate: this fix does not depend on it,
  but a BLOCKPEN mismatch makes dk_flat_pens rewrite palette.prefs.

## S82 ENVARC fix

Verdict: UNKNOWN on the rig (rig check not run, agent budget spent). PASS on host tests and Amiga build.

Design (owner choice): Patina's start apply of an already-Kept theme also writes the files it applied to ENVARC:Sys. Files whose bytes already match (dk_files_equal) are skipped. It reuses the Keep path (desk_persist_pairs + dk_stream_copy). Interactive Use/Try stays ENV:-only. Keep is unchanged. It is skipped while a confirm is pending.

- Code: `desk_start_persist_mask()` (patina/desk_core.c, tag S82-START-PERSIST) and `dk_start_persist()` (patina/desk_dos.c), called after `dk_windows_open(was)` in `desk_dos_start_apply`. Sentinel PATINA_DESK_START_PERSISTS is set when at least one file was copied.
- R1.10 lives in thoughts/shared/plans/2026-10-26-patina-roadmap.md line 80 (not RULES.md). It now has a one-line S82 exception.
- Host test: `make test ONLY=desk` gives "[FAIL] desk: 2396 checks, 3 failed" with the mask stubbed to 0, and "[OK] desk: 2396 checks, 0 failed" after the fix.
- Structure test tests/test_patina_put_skips_unchanged.py: FAIL before (desk_dos.c stashed), PASS after.
- Commits on branch s82-skip-unchanged: c3de962e (dk_put skip unchanged), e402aeb8 (Persist Patina's start apply to ENVARC:Sys).
- Amiga build: `make amiga` through buildq, exit 0, VTCON_BUILD=e402aeb8. build/amiga/Patina is 412268 bytes, md5 b8a700c6105ef87b337425d70a5fa0f5.

Per-boot lines: not run. Planned check on rig 7: `rig.py dopus`, `rig.py install`, `rig.py start --max`. Then read PATINA_DESK_START_PLAN / _WRITES / _PERSISTS, `ami.py requesters` and build/rig7/retry.log on boot 1 and on boots 2-3 after `rig.py reboot`. Expected: boot 1 may show the requester once. Boots 2-3 should show PLAN=0 and no requester. One manual theme pick should still apply.

Open risk: the editor rewrites the WBPattern ENV bytes, so the ENVARC copy is the editor's rewrite. The planner compares with desk_wbpattern_same, so boot 2 should still plan 0. This is unverified until the rig check runs.

Cleanup: no rig started. Scratch in the session scratchpad s82/ holds small logs only (under 1 MB). No large scratch was made.

## S82 ENVARC rig check

Rig 7, Patina md5 b8a700c6105ef87b337425d70a5fa0f5 (VTCON_BUILD e402aeb8), `rig.py dopus` + `rig.py install` redone after the earlier agent was stopped mid-run. Sentinels read with `ami.py exec "Type ENV:<name>"`, requester with `ami.py requesters` (text only, no screenshots).

| Boot | START_PLAN | START_PERSISTS | PUT_SKIPS | System Request | Verdict |
|------|-----------|----------------|-----------|----------------|---------|
| 1 (fresh sys.hdf) | 22 (pattern, palette, font) | 3 files copied to ENVARC:Sys | not set (0) | yes, IPrefs Retry/Cancel open (not clicked) | EXPECTED on a fresh ENVARC; persist wrote 3 files |
| 2 (`rig.py reboot`) | 4 (palette only) | 0 | 1 | no | PARTIAL: requester gone, but PLAN is 4, not the expected 0 |
| 3 (`rig.py reboot`) | 4 | 0 | 1 | no | PARTIAL: same as boot 2 |

Reading: the persist works (boot 1 writes, boots 2-3 persist nothing and dk_put skipped one byte-equal write) and no System Request appears on boots 2-3, which was the bug. PLAN stays 4 (DESK_START_PALETTE) every boot: the planner still sees the palette as different while the put finds the bytes equal, consistent with the BLOCKPEN / dk_flat_pens mismatch in Open risks above. Follow-up: find why desk_start_prefs(DT_PALETTE) plans a write whose bytes already match.

Step 5 (manual theme pick): NOT VERIFIED. `patina_theme_save_rig.py --selects 1-1` stopped at "the Patina commodity is not running": its `Status` name check does not match, although the PATINA ARexx port is present and ENV:PATINA_THEME = tokyo-night. No pick was made.

## S82 theme pick check

Rig 7, build of s82-themepick (main 50471aac), 2026-11-15. Result: PASS (the manual pick applies and persists after the ENVARC start-persist fix).

- Tool bug: `g.process_running('Patina')` matched the Status listing by name; the commodity's process is not listed under that name, so patina_theme_save_rig.py, patina_prefs_rig.py and five more rigs aborted with "Patina commodity is not running" while the PATINA port existed. Fix: `patina_gui_rig.patina_up()` reuses `patina_rig.port_up()` (ARexx port probe); all `process_running('Patina')` callers use it.
- Boot 1: ENV:PATINA_THEME tokyo-night, PATINA_DESK_START_PLAN=22, PERSISTS=3, PUT_SKIPS unset.
- Pick (patina_theme_save_rig.py --selects 2-2, Keep clicked): ENV:PATINA_THEME=starship. ENV:Sys/palette.prefs and ENVARC:Sys/palette.prefs both changed 5603c3438f -> df7b1ce4cb (identical); font, WBPattern, screenmode prefs unchanged and equal in ENV and ENVARC.
- Reboot (rig.py reboot, DH0 kept): ENV:PATINA_THEME=starship, palette md5 df7b1ce4cb in both ENV:Sys and ENVARC:Sys, no requester open. PATINA_DESK_START_PLAN=4, PATINA_DESK_START_PERSISTS=0 (nothing left to persist), PATINA_DESK_PUT_SKIPS=1.
- Caveats: (1) with DirectoryOpus running, the pick raises IPrefs "Intuition is attempting to reset the Workbench screen. Please close all windows, except drawers" (System Request, Retry|Cancel; a DOpus lister and the Patina bar are open); the tool reports it as a FAIL. DOpus had to be quit (its quit asks a confirm) before the pick went through. (2) The second pick's pen readback "Workbench colours 0-3 []" came back empty after DOpus quit, so the tool's own verdict was FAIL; the prefs-file and ENV evidence above is the basis of this PASS. Not investigated further.
