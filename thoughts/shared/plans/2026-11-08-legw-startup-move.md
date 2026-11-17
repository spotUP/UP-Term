---
date: 2026-11-08
topic: Leg W, Amarchy start order (Startup-Sequence + WBStartup)
status: W1-W7 done (6284b707), W8 KS 3.1 PASS 22/22 (c205a252); KS 3.2 and 3.2+P96 open
research: thoughts/shared/research/2026-11-08-amiga-startup-order.md ("Owner decisions")
touchpoints: thoughts/shared/s64-evidence/legw-touchpoints.md (A-E, file:line)
---

# Leg W: move the Amarchy set start out of S:User-Startup

## Owner decisions (fixed, do not re-litigate)
1. amarchy-flat starts from S:Startup-Sequence, BEFORE IPrefs (between AddDataTypes and IPrefs). Uninstall removes it.
2. Amarchy and amarchy-index start from SYS:WBStartup icons with DONOTWAIT; Amarchy has the higher STARTPRI; the
   indexer gets a low TOOLPRI. The three User-Startup blocks go away; Install/Update strips old ones.

## Implementation defaults chosen by the main session (s64)
- D1 Marker: the flat entry is a marked block, not a bare line, exactly like the RemLib precedent
  (`;BEGIN UP-Term Amarchy-Flat` ... `;END UP-Term Amarchy-Flat`), so `unstartup.sh "" S:Startup-Sequence` (Uninstall,
  already there for RemLib) and `unstartup.sh "Amarchy-Flat" S:Startup-Sequence` strip it with no new strip mode.
  The name gains the "UP-Term " prefix so the move strip (install.dos ~829-837) and Uninstall's catch-all cover it.
- D2 Guard: UP-Term: is assigned in User-Startup, which runs AFTER this block. The block names the absolute install
  path (`$updest`) and is guarded `If EXISTS <abs>/bin/amarchy-flat` ... `Run >NIL: <abs>/bin/amarchy-flat BOOT`.
  reassign.sh (Move) today rewrites only the User-Startup `Assign UP-Term:` line; it gains the same rewrite for the
  flat block's two path lines in S:Startup-Sequence (second file argument, same marker walk).
- D3 Inserter: generalise dist/iconlib/instremlib.sh into one helper `dist/instssblock.sh <name> <before-command>
  <body lines...>` (case-insensitive command match, optional `C:` prefix, first match, no-op when the block exists or
  the command is absent). RemLib (before SetPatch) and Flat (before IPrefs) both call it. No second inserter.
- D4 Icons: PROJECT icons in SYS:WBStartup, default tool `UP-Term:bin/Amarchy` / `UP-Term:bin/amarchy-index` (no binary
  copy, so an Update never leaves a stale twin). Made by tools/mkicon.py (`--tooltype` DONOTWAIT, STARTPRI=..,
  TOOLPRI=..); kit rows in dist/parts.txt + Makefile like the existing .info rows. UP-Term: exists by then (User-
  Startup ran before LoadWB). Values: Amarchy STARTPRI=10, amarchy-index STARTPRI=-10 TOOLPRI=-5.
- D5 Screen wait: Amarchy keeps desk_start_gate (amarchy/amarchy.c:285-306, polls the public screen list on the 1 s
  tick). It meets the owner's "waits for the Workbench screen itself". Only the stale comment at :285 changes.
- D6 WBStartup message: Amarchy and amarchy-index link with vbcc's default startup, which replies the Workbench
  message. Unverified until the rig boot (W8) shows both running and no lost-message hang.

## Checklist (IDs; tick in the open with running count)
- [x] W1 dist/instssblock.sh (D3) + instremlib.sh callers moved to it; tests/test_dist_iconlib.py:80-92 extended to
      cover both names (insert before IPrefs on the 3.1 and 3.2.2 Startup-Sequence fixtures, idempotent, no-op without
      IPrefs).
- [x] W2 install.dos: drop the three User-Startup writers (547-603) but keep stripping old blocks on Update (Amarchy,
      Amarchy index, Amarchy-Flat by name); add the Startup-Sequence flat block (backup .before-UP-Term, vsh helper,
      copy back) guarded by D2.
- [x] W3 install.dos + Install.installer (and dist/common/*.installer if they mirror it): copy the two WBStartup icons
      into SYS:WBStartup.
- [x] W4 Uninstall: delete SYS:WBStartup/Amarchy.info and amarchy-index.info; Startup-Sequence strip already covers
      the flat block (check); User-Startup strip of old blocks stays.
- [x] W5 reassign.sh: rewrite the flat block's absolute path on Move; test.
- [x] W6 Icons: mkicon rows, parts.txt, Makefile; a test reads the generated .info tooltypes and default tool.
- [x] W7 Tests/docs: tests/test_flat_startup_block.py, tests/test_dist_installer.py:955-964, tests/test_amarchy_docs.py:
      85-92, tests/test_rig_a500_user_startup.py fixture, README.txt 63/732/999, amarchy.c:285 comment,
      tools/rig/rig.py 339-351/386-402 (shadow block anchor), 1052-1056 (--no-amarchy-block -> remove icons),
      amarchy_gui_rig.py:57/1307 FLAT_BOOT text.
- [ ] W8 Reachability (the one test): tools/rig/install_rig.py drives the real installer on a clean image, reboots,
      asserts: block sits after AddDataTypes and before IPrefs; icons in SYS:WBStartup with the tooltypes; after boot
      processes amarchy-flat, Amarchy, amarchy-index all run (sentinel: Status lists them); flat patch installed before
      IPrefs (sentinel: amarchy-flat's install timestamp/order vs IPrefs task in Status, or flat's own boot log);
      Uninstall leaves S:Startup-Sequence byte-identical (existing check :884/:1365, comment :674 updated).
      Run on KS 3.1, KS 3.2, and 3.2 + P96 RTG.

## Gotchas
- One test run at a time, nice 19, 2 workers; never kill other sessions' builds (wait).
- Startup-Sequence edits are system-file edits: always backup S:Startup-Sequence.before-UP-Term first (existing guard).
- KS 3.1 IPrefs stack (8192 bytes, research R-facts): the patch now installs before IPrefs; recheck clone_render stack
  headroom on KS 3.1 in W8.

## W8 state (s64, c205a252, evidence thoughts/shared/s64-evidence/legw-w8.md)
- KS 3.1: `install_rig.py --only legw` PASS 22/22 on a fresh rig (a-e, D6 confirmed: AMARCHY port + ENVARC:Amarchy/apps).
- Open W8a: KS 3.2 and KS 3.2 + P96. `--os32` lacks the tree; use `rig.py start --os 32 --max` (worked for the DT-6 diagnosis agent). Fresh rig per run (re-run on the same rig fails "too many ARexx messages").
- Open W8b: empty SYS:UP-Term/bin left after Uninstall after a reboot, cause unknown; the Uninstall Ctrl-C to amarchy-flat did not cure it. base_uninstall(rebooted=True) skips the SYS:UP-Term listing compare and so hides it: the skip must go once the cause is fixed.
- W8a run (s64, evidence thoughts/shared/s64-evidence/legw-w8a.md): KS 3.2 and 3.2 (uaegfx) both FAIL before (a)-(e).
  Install aborts at `vsh unstartup.sh Amarchy` (install.dos ~569): "vsh: rm: not found", "vsh: grep: not found", rc 127;
  guest vsh $PATH printed empty. grep missing because the agent's stub ports kit lacked it (slim.py drops grep from base);
  rm exists in SYS:UP-Term/bin, so the empty PATH is the real question. Unknown why KS 3.1 passed with the same recipe.
  Next: build the kit from the real upterm-ports (not stubs), rerun on 3.2; if PATH still empty, fix install.dos /
  unstartup.sh so it does not depend on PATH, and check whether unstartup.sh needs grep the slim base lacks.
