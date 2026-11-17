# FLAT-UNLOAD-INFLIGHT (branch flat-unload-inflight, worktree vtcon-flat-unload-inflight)

Design as built: flat_rf.s / flat_tx.s are now templates (labels _flat_*_tpl, _c1, _c2, _end): addq.l #1,(abs).l ; same pushes ; jsr (handler).l ; lea ; subq.l #1,(abs).l ; rts.
flat.c gate_make() AllocMem(MEMF_PUBLIC|MEMF_CLEAR) block [count.l][copy], patches the two counter operands (label+2), CacheClearU(). SetFunction slot -> block code.
gate_wait(): while count>0 Delay(1); no timeout. tx_remove/rf_remove: restore, then gate_wait, then clear state. Block never freed (about 70 bytes per load; kept in r->tgate/rgate across remove/install of the same record).
Gate: test_flat_budget.py jsr regex accepts "jsr (sym).l"; stub_bytes unchanged in value (rf 28, tx 16): flat-budget rf headroom 64, tx 72, c2_reg 68, d5 80, c2_stk 76 (same as baseline).

Reused: flat_swap_install/remove (ours = block code), CacheClearU already used in dopus_unpatch/dopus_scan, AllocVec record pattern. No existing persistent allocation to reuse (flatshared is FreeVec'd in drop_record, so the block is separate).

Sites:
- tx_remove (flat.c Delay(5) ~1397): covered (graphics Text stub).
- rf_remove (~1941): covered (RectFill stub).
- hook_remove (~2386, sysiclass/frameiclass dispatchers c2/c8_disp_*) and dopus_unpatch (~2418, d4_drawbox/fieldbox, body trampolines, d5_disp_reg): NOT covered. These are C functions in flat.c (stack-convention variants take arguments on the stack, so a wrapper in a block must re-push them, +16 bytes, failing the headroom gate on c2_disp_stk; reg variants would fit +4). Still carry the fixed Delay: same defect remains there. Needs a separate design.

Test: tests/test_flat_unload_inflight.py, wired Makefile (test ONLY=flat_unload_inflight, TEST_PY_ONLY). Fail-before on d83e72ea: 4 of 4 fail, e.g. "AssertionError: Regex matched: 'Delay(5)' ... in tx_remove"; stubs: "(0, 0, 1, 1) != (1, 1, 1, 1)". After: OK.
Results: make amarchy-flat rc=0; flat-budget rc=0; flat_clone_stack OK; flat_hook_locks OK; new test OK. flat_core host test NOT run: make test ONLY=flat_core stops on missing python fontTools (menu_icons.inc); flat_core.c/h untouched.

Not verified: any rig behaviour (unload under a blocked draw), 68060 cache coherence (CacheClearU used; copyback), vasm output checked by bytes only (52b9/53b9 abs.l, 4eb9 abs.l with reloc).
Residual risks: a task preempted exactly between the library JMP and the stub's first addq is not counted; a permanently blocked caller makes unload wait forever (by design); ~70 bytes leaked per load.

## Closed-flag gates, all four sites (s71 agent 16)

Commit c2a00bf3 on flat-unload-inflight (on top of 288c14f8). Not pushed.

Gate template (flat_rf.s / flat_tx.s / new flat_dg.s; gate_make in flat.c patches magic operands in the copy:
$11111111 counter, $22222222 closed, $33333333 original, $44444444 handler):

    addq.l #1,count        ; count in FIRST
    tst.l  closed          ; no register
    bne.s  pass
    <push args, jsr handler, pop>   (dg: just jsr handler)
    subq.l #1,count
    rts
  pass:
    subq.l #1,count
    move.l orig,-(a7)      ; rts jumps to the original; caller return stays on top; no register touched
    rts

Sites (flat.c, at commit): tx_remove/tx_install, rf_remove/rf_install, hook_remove/hook_install (sysiclass sgate AND
frameiclass fgate: hook_remove unloads both, so dropping Delay(5) without gating fsw would have left it unprotected),
dopus_unpatch + dopus_scan install (d5gate). Unload order everywhere: gate_close, flat_swap_remove, gate_wait; a refused
restore (FLAT_STILL_CHAINED) calls gate_reopen. Reinstall in the same record reuses the block and gate_open sets orig + closed=0.
c2_disp_stk and c8_disp_stk got the leading `void *caller_return` parameter.

Budget (headroom, before -> after): c2_disp_reg 68 -> 64, d5_disp_reg 80 -> 76, c2_disp_stk 76 -> 72, flat_rf_c 64 -> 64,
flat_tx_c 72 -> 72. Gate adds 4 bytes (stub 0 -> 4) to the three dispatcher chains; test_flat_budget.py reads the gate's jsr
from flat_dg.s. Earlier regex change (accepts `jsr (sym).l`): only widens the set of lines recognised as the stub's jsr, nothing
is dropped from the count. c8_disp_reg/stk are not in ENTRIES (never were); not measured.

Fail-before: FLAT_ROOT=<git archive 288c14f8> python3 tests/test_flat_unload_inflight.py: 5 failures + 1 error (no flat_dg.s):
no gate_close, no closed test in flat_rf.s, no gate_open, Delay(5) in hook_remove, no dummy parameter. New tree: 6 tests OK.

Run: make amarchy-flat rc=0; make flat-budget rc=0 PASS; make test ONLY=flat_unload_inflight, flat_hook_locks, flat_clone_stack
rc=0. ONLY=flat_core rc=2: "ModuleNotFoundError: No module named 'fontTools'" from tools/gen_menu_icons.py:21, run by the
Makefile rule for build/amiga/menu_icons.inc (target dependency of the flat_core test); not installed, not worked around.

Not covered / not verified:
- D4 library entries (dsw: d5 DrawBox/FieldBox replacements, body patches) are NOT gated: dopus_unpatch still has Delay(2) before
  CloseLibrary for them. The d5 dispatcher itself is gated.
- Rig: unload under a blocked draw; 68060 copyback cache behaviour of the patched block (CacheClearU only after the copy; closed/
  orig are plain data writes read by tst/move); original-function pass-through under real traffic; frameiclass fgate.
- The machine-gate marker log named in the brief did not exist at the given path; the process check was waited to clear.
Scratch: scratchpad a16-old/ (git archive of amarchy, < 2 MB) left; logs a16-*.log.

## Dopus library gates and c8 budget (s71 agent 17)

Gap 1 closed in amarchy/flat.c (branch flat-unload-inflight, one commit on top of c2a00bf3). The three D4 library entries
(DrawBox, FieldBox, internal bevel body) now go through the dispatcher gate template flat_dg.s, one gate per element
(`r->dgate[el]`, handler remembered in `dgate_h[el]`; `dgate_get` reuses a gate only for the same handler, because FieldBox
uses d4_fieldbox for the LVO swap and d4_fieldbody for the body hook). No new template: every handler keeps the caller's
registers (register convention) or takes one leading `caller_return` parameter for the gate's return address (d4_fieldbody,
d4_bevelbody, same trick as c2_disp_stk). Body hooks: the swap's `ours` is the gate code; gate orig is the BODY itself, not
the trampoline (the trampoline lives in the record, which may be freed after the count is out; the body holds the original
prologue again once restored, and gate_close + restore share one Forbid, so no task sees the closed gate with the hook still in).
LVO swaps: orig = the saved library entry. dopus_unpatch: gate_close, restore, reopen if refused, all inside the Forbid;
gate_wait per removed element after Permit; the Delay(2) is gone.
Tests: tests/test_flat_unload_inflight.py gained test_no_fixed_delay_in_any_unload_path and
test_every_patched_dopus_entry_reaches_our_code_only_through_a_gate. Fail-before (FLAT_ROOT = amarchy/ of c2a00bf3): 3 FAIL
(those two plus test_no_fixed_delay_after_the_restore). After: make test ONLY=flat_unload_inflight rc 0, flat_hook_locks rc 0,
flat_clone_stack rc 0, make amarchy-flat rc 0. NOT run on an Amiga rig (no rig use allowed): the gated D4 path is unexecuted.

Gap 2: `_c8_disp_reg` and `_c8_disp_stk` added to ENTRIES (stubs via the DG gate). make flat-budget now FAILS, as the numbers say:
  c2_disp_reg 320 (headroom 64), d5_disp_reg 308 (76), c2_disp_stk 312 (72), flat_rf_c 320 (64), flat_tx_c 312 (72): unchanged.
  c8_disp_reg deepest chain 796 (headroom -412), c8_disp_stk 780 (-396); limit CHAIN_MAX 384, HEADROOM_MIN 64.
  Also reported by the same run: frames flat.s:l1213 = 580 and flat.s:l1414 = 116 exceed HELPER_MAX 80 (both reachable from c8).
Limits not raised. The c8 path (frameiclass draw) needs a real fix: its drawing frames run on the caller's stack (cf. the S63
clone stack, c2_clone / flat_on_stack) or must be cut down. The d4_* handlers are static, so they are not measurable by label
and stay outside ENTRIES (not measured).

## c8 frame draw on the private stack (s71 agent 19)

- Commit on branch flat-unload-inflight (see git log). c8_skip split: early tests stay; draw = c8_draw, entered via c8_draw_on -> flat_on_stack(c8_run) on cl_stk/cl_sss under cl_ss (shared with c2_clone).
- Re-entry: stack pointer (address of a local) inside cl_stk -> c8_run called directly, no swap, no semaphore. Lock order: AttemptSemaphore only, never waits; busy -> return 0 (ROM frame).
- flat.s l1414 = c8_draw (frame 112, was 116), l1213 = its 580-byte callee chain member (role/border/knob helpers); both now run only on the private stack.
- Budget after: c2_disp_reg 320/hr 64, d5_disp_reg 308/76, c2_disp_stk 312/72, c8_disp_reg 72/312, c8_disp_stk 56/328, flat_rf_c 320/64, flat_tx_c 312/72. PASS. No tool/limit change (c8_run is a non-const pointer so the call is indirect like c2's clone_on_private).
- Not charged by the tool: flat_on_stack's own frame (about 20 bytes saved regs + args) and StackSwap itself, same as c2.
- rc: amarchy-flat 0, flat-budget 0, flat_clone_stack 0, flat_hook_locks 0, flat_unload_inflight 0, flat_core 2 (build/amiga/menu_icons.inc needs python fontTools, missing; unrelated).
- Process slip: the first make amarchy-flat started while another repo's pptx-kit build was above 10 percent CPU; later runs waited.
- Not verified: no rig run; the stack depth of c8_draw on cl_stk (16384) not measured; busy fallback not seen on device.

## Rig 68 proof 6414d25b (s71 agent 24)

Binary: build/amiga/amarchy-flat from the worktree, rebuilt (the existing one was 11 s older than flat.c's mtime), 227256 bytes, md5 c43dfefb4668e98b3b9cf0285dd6e779 (identical before and after the rebuild). Installed as RAM:amarchy-flat-ui after Break C of the old resident build. Logs: scratchpad a24-*.log.

1. Start/STATUS PASS: resident in 1.4 s, `flat=on`, `loop beat=1 stage=wait refused=none refusals=0`, `dopus=73.8 patched=7`, `frame=hooked`.
2. 10 unload/reload cycles with `dopus refresh all` + Wait 1 before and after each: PASS. Start 1.1-2.0 s, unload 0.2-1.3 s (the longest 1.3 s, below the 2 s mark), process gone every time, no refused restore seen (each Break ended the process), no guru, `Version` answers after cycle 10.
3. Break C mid-loop (20 refreshes with Wait 1 running on the Amiga, Break at ~9 s): PASS. Patcher gone in 0.8 s, `flat=absent`, DOpus process still there, loop script finished (marker file written), rig answered 30 s later. Stock look not checked by pixels.
4. Pass-through, 67 s of refresh + Wait 1 (60 refreshes): PASS. beat 1 -> 61, stage=wait, refusals=0, dopus calls 16 -> 3347 (flat = calls), no freeze.
5. c8 on the private stack: PARTIAL / NOT MEASURED. Opened SYS:Prefs/Time (GadTools buttons) with the patcher resident: `frame=hooked calls=0 flat=0` before, `calls=429 flat=114` after (a second run: 2223 / 608 cumulative with DOpus traffic). calls minus flat counts every c8_skip return of 0 (non-SKIP decisions, early exits, AND the busy fallback), so the busy fallback cannot be told apart: a counter in c8_draw_on's `!AttemptSemaphore` / `!cl_stk` branch is needed. The pixel check failed to run: the Time window was not located (uitree read errors "too many gadgets" with DOpus on screen; diff-against-baseline was swamped by lister redraw noise). No verdict on flat versus stock frame pixels.
6. After all: rig answers, DOpus process (Process 11) alive. Listers count not re-read (none closed by this run).

Notes: rig stayed stable; no reset loop seen. Requester watch of ami.py was off (UPTERM_REQUESTER_WATCH=0) because its thread errors on the same "too many gadgets" read. Final state: no amarchy-flat resident (`flat=absent`), SYS:Prefs/Time closed.

## Busy-fallback counters (s71 agent 26)

Commit 8fff45bb on flat-unload-inflight. Two counters, fbusy and fnostk, were added to flatshared (amarchy/flat_shared.h, next to fcalls/fflat). c8_draw_on in amarchy/flat.c now increments fnostk at line 916 (cl_stk is NULL) and fbusy at line 920 (AttemptSemaphore(&cl_ss) failed), as plain increments through g_r like fcalls/fflat; no new lock, no wait, no stack use. The STATUS read-out copies them in the same spot as fcalls/fflat, and flat_frame_status_line (amarchy/flat_core.c) prints: frame=hooked calls=<n> flat=<n> busy=<n> nostk=<n> (frame=absent unchanged). The host test tests/test_flat_core.c checks the new text.

Binary: build/amiga/amarchy-flat in the worktree, 227344 bytes, md5 a5788edafc3ef5b027bd439168bcb8d0. make flat-budget rc=0, PASS; smallest headroom 64 (_c2_disp_reg and _flat_rf_c), then 72 (_c2_disp_stk, _flat_tx_c); c8 paths 312 and 328.

Tests: flat_hook_locks OK, flat_clone_stack OK (nice 19, one at a time). flat_core NOT run: make fails on missing python fontTools (menu_icons.inc); only a compiler syntax check of tests/test_flat_core.c was clean. Not rig-tested: nothing shows yet whether busy or nostk ever rise on the Amiga.

## Rig 68 proof 5ade172e (s71 agent 31)

Binary build/amiga/amarchy-flat (worktree), 227472 bytes, md5 f9f26612e91d7882479a15272ccbe72f; Amiga List shows 227472. Old resident Break 12 C (0.8 s), new started from RAM:amarchy-flat in 1.3 s.
1. STATUS: flat=on, patched=7, refused=none refusals=0, `frame=hooked calls=0 flat=0 busy=0 nostk=0`. PASS.
2. Q1 (SYS:Prefs/Time opened, 5 s): frame before `calls=0 flat=0 busy=0 nostk=0`, after `calls=153 flat=38 busy=0 nostk=0` (stable 3 s later). busy=0 and nostk=0: fallback never taken. PASS.
3. Q2 pixels (grab of the Time window box 0,14,332x203): 9 distinct colours only, all theme (1A1B26, 7AA2F7, 3E4254, A9B1D6 and antialias mixes); no grey bevel pair (B3B3B3/E9E9E9 etc). Frames flat. PASS. Window closed by `Break <pid> C` (the Cancel click via ami.click_px did not close it).
4. Q3: before `dopus calls=0 face=0 nopix=0`; after refresh 1: calls=38 face=38 nopix=0; after refresh 2: calls=76 face=76 nopix=0 (nopix stayed 0, did not grow). Pixels: rear lister info row y14..26: no horizontal accent run (only glyph-sized clusters, y12 = title bar); front lister info row and both path fields: no accent outline (the accent rows at y566..569 are the horizontal scroller). Layout in the grab: front-lister region showed no accent at all. PASS.
5. Q4: Break C with DOpus open: patcher gone in 0.2 s, flat=absent; `dopus refresh all` ok; Prefs/Time opened and closed; `Version` answers, DirectoryOpus process present, no guru, no amarchy-flat process. Restarted RAM:amarchy-flat in 1.2 s: flat=on, patched=7, frame=hooked busy=0 nostk=0. PASS.
Unverified: lister count after the run (`lister query source` via rx returned only RESULT); ami.py ping hung (uitree read, known) so liveness was checked through exec. Screenshots deleted, no scratch over 10 MB.
