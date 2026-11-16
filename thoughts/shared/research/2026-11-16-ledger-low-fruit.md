# Ledger low-hanging fruit triage (2026-11-16)

Source: thoughts/spot/todos.md (5151 lines, 551 open `- [ ]` lines). Method: grep of open rows, keyword filters, then reading candidate rows and checking the code/tests. Most open rows are rig-driven, owner-eye or stale narrative; genuinely small host-only rows are rare.

## Done
- line 2190 (PATINA wblaunch comment): patina/wblaunch.c:185 comment claimed dos does not unlock NP_CurrentDir; ledger says measured otherwise. Comment corrected, commit fc4bd6a5. Comment-only, no build needed.

## Qualifies, but already fixed (ledger should tick, no code change)
- line 1919 archive-tool completion rules (lha/lzx/unzip): dist/vshrc:160 `complete -F _archive_complete lha lzx unzip tar gunzip unxz` exists.
- line 2753 RIG-PATINA-CONF-TEST: `python3 tools/test_rig_patina_conf.py` prints "rig patina conf: ok" on current main.
- line 3926 DIST-CHECK README-KEYS: row itself already notes the test passes on main 06ddb135; only the full `make dist-check` remains (not low fruit: long build).
- line 4852 seed_rig_vtc crash when build/rig missing: tools/rig/rig.py:1453 now returns when src is not a dir.

## Looked small but not
- 2996 DESK-CURRENT-MARKER: s58 reconcile says owned by Patina; "(current)" logic in patina/gthemes_core.c:401, cause of stale marker not isolated; needs a rig look.
- 4597 STACK-FRAMES-AMPREFS: ~30 functions, moving 4640-byte structs off the stack in Amiga code; needs size re-measure and rig. Over 50 lines.
- 3431/3385 FLAT-BUTTON-* , 5148 S86-ACTIVE-SCROLL-GREY: touch patina/flat_core.c (excluded).
- 5075, 5133, 5147 (B8c), 5074: DOpus/theme, need rig 84 and owner eye check.
- 4100 RIG-REBOOT-KEEPSYS: needs a rig; cause suspected elsewhere (flat freezes daemon).
- 1385 RIG-SETUP-STALE: needs a fresh rig to prove the behaviour; spec offers two fixes (owner choice).
- 4326 S7c-7 and 3296 TE-WINDOW-REMEASURE: multi-part, rig verification.
- 1372/1373/2475 IXEMUL-*: other repo (ixemul-vtcon).
- 1053 FOLLOW-UP patch 0016: upterm-ports repo.
- 2142+ most "agent running"/"next:" rows are narrative from Oct-Nov 3 and stale; need a ledger reconcile, not code.
