---
date: 2026-11-12
topic: hidden test failures on main behind `make test` stop-at-first-failure
status: partial (no suite was run: the machine stayed busy with `make dist` and a DEViLBOX vitest run the whole time; verdicts below come from reading source and git history)
---

# Hidden failures triage (10 suites)

Machine note: `python3` on PATH is /Library/Frameworks/Python.framework/Versions/3.9 (Python 3.9.10, Unicode 13.0.0).
Homebrew has 3.10-3.14 in /opt/homebrew/bin. This alone explains test_unorm and may affect others.

## test_amarchy_menu_stamp (tests/)
- Failing assertion: `act.c act_menu_open() no longer matches \bms_open\(&ops` and `... \bmo_srv_ops\(&ops`.
- Cause: dd01f589 (2026-10-29, bar core / anchored menus) moved the body of act_menu_open into
  `menu_open_req` (amarchy/act.c:198-221); act_menu_open (act.c:223) and act_menu_open_at now only forward.
  The chain still reaches ms_open -> ms_binary_changed (act.c:218-219, menusrv_core.c:31).
- Verdict: STALE TEST.
- Fix: in CHAIN, add `('act.c', 'act_menu_open', r'\bmenu_open_req\(')` and move the two ms_open/mo_srv_ops links to `menu_open_req`.
- Risk to v1.1 users: none (behaviour intact).

## test_amarchy_gui_rebind (tests/)
- Failing assertion: `gui.c gui_show() no longer matches \bmg_layout\(`.
- Cause: 6aad428b (2026-11-02, viewer text pitch) put layout behind `do_layout` (amarchy/gui.c:742, mg_layout at :747);
  gui_show calls `do_layout(...)` at gui.c:1752. Every other NEED/ONLY_IN_PAIR link and the unbind order
  (pens_release :1836 < mt_close :1839 < UnlockPubScreen :1842) still hold by grep.
- Verdict: STALE TEST.
- Fix: pattern for gui_show becomes `\bdo_layout\(` (optionally add `('do_layout', r'\bmg_layout\(')`).
- Risk: none.

## test_rig_browse_base (tools/)
- Failing assertion: `b.base_stale` -> AttributeError on module upkg_browse_rig (first check, line 28); the src check at
  line 37 reads tools/rig/upkg_browse_rig.py, which no longer has `if base_stale(installed, ...)`.
- Cause: 2fdfa52d (2026-10-31, apps add-on through upkg) moved base_stale and step 0 into tools/rig/upkg_rig.py
  (def base_stale :486, gate at :585); upkg_browse_rig.py only imports upkg_rig.
- Verdict: STALE TEST.
- Fix: `import upkg_rig as b` and read `rig/upkg_rig.py` for the step-0 source check.
- Risk: none (rig tooling only, not shipped).

## test_rig_browse_free (tools/)
- Failing assertion: `b.info_free_blocks` -> AttributeError (line 36).
- Cause: same move, 2fdfa52d: info_free_blocks :501, tree_bytes :512, free_problem :520 now in tools/rig/upkg_rig.py.
- Verdict: STALE TEST.
- Fix: `import upkg_rig as b`. Unverified after the fix: free_problem's message wording ("63.5 MB free", "requester").
- Risk: none (rig tooling).

## test_unorm (tests/)
- Failing assertion: `test_version: unidata_version '13.0.0' != '16.0.0'`; test_every_code_point / test_random_sequences
  then compare against Unicode 13 tables and differ wherever 14-16 changed NFKC.
- Cause: Makefile runs `python3 -I tests/test_unorm.py`; python3 on PATH is 3.9.10 (Unicode 13). Unicode 16.0.0 needs Python 3.14.
  build/unorm_drv is present (2026-11-01).
- Verdict: HARNESS.
- Fix: run it with a pinned interpreter (e.g. `PY_UNORM ?= python3.14` in the Makefile, skip with [SKIP] when absent) rather than bare python3.
- Risk: none to users (claude/unorm.c itself is not implicated by this failure; unverified until run under 3.14).

## test_gen_models_builtin (no such file)
- There is no tests/test_gen_models_builtin.py. The Makefile `test:` line is
  `ONLY=pi_llm` -> `python3 -I tools/gen_models_builtin.py --check` (Makefile:307).
- Inputs and outputs (tools/pi_models_dump.json, llm/models_builtin.json, llm/models_builtin.h) are clean and last changed
  together in 39701138 (2026-10-14); nothing in the check depends on Python version that I can see.
- Verdict: HARNESS (name mismatch: `make test ONLY=gen_models_builtin` runs nothing; the gate is `ONLY=pi_llm`). Not run; the --check itself is expected to pass (unverified).
- Fix: none in code; call it as ONLY=pi_llm.
- Risk: none.

## test_rig_ami (tools/)
- Makefile:290 runs it only when build/wasabi/src/tests/mock-wasabid.py exists (it does here) or ONLY is set.
- Not run (machine busy). Last test change edc4ef68 (2026-11-04).
- Verdict: NOT REACHED (unverified). Next step: run `ONLY=rig_ami` when idle and read the first FAIL line.
- Risk: rig tooling only.

## test_rig_fsuae_log (tools/)
- Static checks (logs_dir line rig.py:1578, uaem_write_flags rig.py:1583, cap_fsuae_log in rig.py and ami.py:520,
  no log_* options) all hold by grep.
- The second half calls `rig.start()` with stubs. Since the test's last edit (e54a8d54, 2026-11-03) start gained
  `sys_hdf_skip_blocks` (a8e44d73, 2026-11-08; a no-op without --no-solidwin/--no-amarchy-block, so harmless) and
  calls `boot_device_check()` (8aaaaecf, 2026-11-02), which is not stubbed and reads `CFG`, a module constant
  bound at import to the REAL rig dir (rig.py:101) while the test only rebinds `rig.RIG`. Outcome then depends on
  whether this machine's rig config exists: likely `sys.exit("rig: not starting: config ... is missing")`.
  Same import-time binding means START_ARGS / OWNER_FILE may be written into the real rig folder (unverified).
- Verdict: NOT REACHED (unverified); leading hypothesis STALE TEST (stub list behind start()).
- Fix: stub `boot_device_check` in host_steps; rebind CFG/START_ARGS/OWNER_FILE to the temp dir.
- Risk: none to users; possible side effect on a developer's live rig dir when the test runs.

## test_rig_upterm (tools/)
- ~40 checks on rig.GO, stage_upterm, clean_stale_icons, migrate_upterm_drawer, paths.UPTERM_BIN.
- Not run, not statically resolved within budget.
- Verdict: NOT REACHED (unverified).
- Risk: rig tooling only.

## test_rig_kit (tools/)
- Simulates rig.GO line by line against a fake SYS:/kit; last test change d0ec00e3 (2026-11-03).
- Not run, not statically resolved within budget.
- Verdict: NOT REACHED (unverified).
- Risk: rig tooling only.

## Summary
No REAL BUG found in shipped code. Two Amarchy guards are stale after refactors that kept behaviour; two browse
suites point at a module whose functions moved; unorm and gen_models_builtin are harness issues. Four rig suites
(rig_ami, rig_fsuae_log, rig_upterm, rig_kit) still need one idle run each, one at a time.
