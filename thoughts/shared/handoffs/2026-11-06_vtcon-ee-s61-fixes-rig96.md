---
date: 2026-11-06
session: vtcon-ee s61
status: in progress (one rig proof agent running at handoff time)
---

# vtcon-ee s61: completion lock fixes, deleted-theme fallback, rig 96 rebuild

## Done (on origin/main)
- 1bd9220b: completion worker use-after-free of the opener's cli_CommandDir list (rig 96 hard reset on XCON open). Worker DupLocks a refcounted private copy while the opener is blocked in Open. Proof: rig 162, 20/20 XCON opens under JIT, MuForce clean (s61-evidence/reset-hunt-s61.md, ccfix-s61.md).
- 9b2caff8: deleting ENV:up-term/theme-active now drops the theme from open windows (watch_msg theme_changed flag, upconf_theme_report keep/take/gone, gone case calls profile_switch for the window's profile). Host test only: tests/test_upconf.c:803.
- d5294c47: completion CurrentDir race. DupLock of pr_CurrentDir only while the opener's DoPkt Read is held (cc_cd_blocked, cc_cd_read_waits, answer_read). Host test: tests/test_complete.c:196 (610 checks).
- Font fallback TE-7b.2 (owner: built-in font, apply theme anyway); font checks 1-4 PASS rig 163 (s61-evidence/font-checks-s61.md).
- Rig 96 rebuilt fresh full install from 52d2fc3a (s61-evidence/rig96-rebuild-s61.md). Worktree ../vtcon-s61-rig96 (branch s61-rig96) is kept: rig 96 runs from it.

## Open
- S61-SHARED-RIG-PROOF DONE (rig 164, main d5294c47): A, B (colours), C PASS; s61-evidence/shared-proof-s61.md. Font fallback after delete not measured.
- RIG96-REINSTALL-PENS4: reinstall rig 96 ONCE when vtcon-c1 sends the AMARCHY READY sha (pen record is now Amarchy.IconPens.5; full install + reboot, never RAM swaps).
- Owner eye checks on rig 96 (live): Shift+Left Option+Space opens the Amarchy menu (owner tested, DONE); F12 opens the FS-UAE menu; theme editor width; boot reaches the "Welcome to Amarchy" window.
- Not covered by d5294c47: a non-ixemul async reader waiting on its own pr_MsgPort would look blocked (unverified, unlikely).
- Uncommitted s5x/s6x evidence files and test-register rows in the main checkout (thoughts/shared); commit from a worktree.

- TAB-RETURN-ACCEPT DONE a994d26e (host test only). THEME-RECREATE-NO-TAKE: opus hunt found no handler bug; decisive rig check queued. Next rig run: S62-SHARED-RIG-RUN in todos.md.
- TAB-SINGLE-INLINE (owner 2026-11-06): single-match Tab in vsh inserts inline. Sonnet agent running at handoff time (worktree ../vtcon-s61-tab1); grep todos.md for its result. Rig proof rides S62-SHARED-RIG-RUN.
