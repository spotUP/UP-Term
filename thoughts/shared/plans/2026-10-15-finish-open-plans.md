---
date: 2026-10-15
topic: finish every open plan item except the hardware tests
tags: [plan, open-items, vtcon, ports, rig]
status: draft
---

# Finish all open plan items except the hardware tests

<!-- statusline: no-rollup (this plan re-lists the open items of other plans; the footer counts them there) -->
Evidence base: `thoughts/shared/research/2026-10-15_plan-audit.md` (item by item, with commits). Counts
used here come from the plan files' structure, not a single grep: 80 open boxes in 11 plans on 2026-10-15
(vtcon 28, unix-tool-ports 20, replay-hardware-tests 12, ledger 9, a2-progress 4, amiga-pi 2, five singles).

Done = every item in "In scope" below is ticked in its own plan with a commit and a rig or host test that
drives the product entry point and a sentinel (REACHABILITY_PROTOCOL). A partial run is reported as
"N of M, K open", never "done". Scope cuts are the owner's call.

## Out of scope (the owner's hardware, or a third party): not planned here

Real boards and machines: replay-hardware-tests H2-H14 (12), O6 (real AmiSSL login per provider),
V4 and W3 (A1200 manual checks), W50 (Replay freeze), T1 pre-check 2 and W7 on real hardware,
NL22 (real Amiga), a1 A1.4 hardware part (A1200), a2 1.8 and 5.5 (A1200/030/060 timings on real boards).
Owner's A1200 (Replay) network, 2026-10-17: FIXED. Cause was a loose cable at the server-room end
(owner replugged it); the session 8 handoff's "fault is on the Amiga side" was wrong, as moving the
Pi's cable only tested the Amiga end. wasabid answers on 192.168.0.66:1234 but refuses our client
("bad key") until the owner stored the key in `~/.config/upterm/a1200-wasabi.conf` (mode 600, `key = ...`).
Access CONFIRMED 2026-10-17: `AMI_HOST=192.168.0.66 AMI_PORT=1234 AMI_TOKEN="$(sed -n 's/^key = //p' <conf>)"
tools/rig/ami.py ping` -> wasabid 0.11b2. A1200 is 68060 + 68882, 172M RAM; no UP-Term assign (kit not
installed); SMB share `spotUP:` mounted. Next: install the rebuilt kit, then real-Amiga tests.
Third parties: X2 (ixemul upstream, waits for the maintainer's reply, branch `feature/x2-vtcon-1`),
W54 (tmux over eriQue's auxcon, not released). These stay open in their plans; this plan only keeps the
branches current. Rig 1 is the owner's machine: never used.

## Rules for every phase (decisions already made, not re-litigated)

- At most 2 agents at once; one rig per agent (rigs 2 and 3, `rig.py start --max`, 060); stop the rig at the
  end; no CPU burners; no `git stash`; WIP commits; worktrees `build/wt-<name>` removed in the same task.
- Models: sonnet writes and runs; haiku runs tests and reads screenshots (text answer only, the main session
  never sees an image); opus only for the root-cause hunt (FP5.7) and plan reviews.
- A box is ticked only with the commit hash and the test name in the note. Owner "look" items (G2, G3, D1, P7)
  get an objective criterion (pixel or text read by a haiku subagent). The agent ticks the box only when that
  criterion passes; the owner's eyes are collected in one list at the end of the phase ("owner visual
  sign-off"), not a blocker for the next phase.
- Run only the affected subset (`make test ONLY=<suite>`, `install_rig.py --only <case>`); a full gate only
  to land (kit content changes: `make dist` + `make dist-check`).
- Each fix ships a regression test that fails before and passes after (revert-check it).
- Ledger first: every phase appends its checklist and results to `thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`
  under a new `## Finish run 2026-10-15` heading before the first edit; tick there as items complete.
- After each phase: handoff file `thoughts/shared/handoffs/YYYY-MM-DD_finish-phase-N.md`, then `/clear`.

## Phase 0: evidence and housekeeping (1 agent, sonnet; small)

- [x] FP0.1 X1: find the stray `PI_DEBUG_LOG`/`debug_log` hunk in `~/Code/UP-Term/amiga-pi` worktree `build/wt-u2`
  (`git -C ... status`, `git diff`); if it is not committed work: report it, remove only after the owner says
  (worktree of another session). Ledger line 1137.
- [x] FP0.2 (done 2026-10-16: resize_rig.py 2 of 2 PASS rig 2 --max after tmux rebuild with $STACK cookie) W47: re-run `tools/rig/resize_rig.py` on rig 2 and quote the PASS line
  (tmux 47x10 to 72x19, nvim 47x11 to 72x20). The PASS exists only in commit 54de76b3's message today.
  2026-10-16 session 5: FAILS (3 runs). tmux: ixemul "free: start of block corrupt!" then `ERR: vsh is still running
  after 30 s` (same as build/rig2/fp0.2-notes.txt, amiga-pi progress 1460-1463; last PASS 2026-10-12). nvim: sizes
  right (47x11 -> 72x20) but `truth None` (ixwinch printed nothing in 4 s). Split test running: current vtcon with
  an ixemul-vtcon build from before 2026-10-12 vs the current ixemul (43 vtcon handler/tty/shell commits and many
  ixemul libc merges since). Logs: session-5 scratchpad fp02b_*, fp02c/.
  2026-10-16 session 5: CAUSE FOUND. Not ixemul (old 0d09e12 build fails the same). Rig copied a stale
  tmux-amiga/build/tmux-bin (2026-10-07) without the $STACK: 262144 cookie fix (tmux-amiga 0d30fa7, 2026-10-15):
  tmux overflowed vsh's 16000-byte stack into malloc headers. Rebuilt with owner's OK (`make -f Makefile.amiga
  check`, stack_cookie 0 failed). Re-run: tmux (72,19) truth (72,20) PASS, nvim (72,20) PASS. Guard open: tmux_rig copies a stale tmux-bin
  silently; it should run tmux-amiga tests/test_stack_cookie.py (or refuse a binary older than its sources).
  2026-10-16 session 6: guard DONE. tmux_rig.check_tmux_current runs test_stack_cookie.py before the copy and
  exits with a rebuild message; tools/test_rig_tmux_guard.py (3 cases, 2 fail with the guard removed). Real
  tmux-bin passes. PROF rest sub-counter diff dropped (ReadEClock 41 us noise); kept as
  thoughts/shared/patches/2026-10-16_prof-rest-subcounters.patch.
- [ ] FP0.3 W6: read the S1 race number for main (`tools/rig` race script; CCON 1.2.8b9 was 30.68 s at 5f2dc765),
  run it on rig 2 once, record main's number in the vtcon plan row.
- [x] FP0.4 P12 close step: "cut_cases oracle reproducible" (rides with U3 part 2): run the oracle twice, compare.
- [x] FP0.5 Ledger headings: "Queue run (after P11) 12 of 14" has 19 ticked: recount by box range and fix.
- [ ] FP0.6 Coreutils 3.7: `../upterm-ports/pkgs/coreutils/recipe.mk` (9.12, GPG-verified) is untracked work of
  another session: do not touch; ask the owner once who commits it (collected under Blocked on me).

Verification: quoted rig lines and commit hashes. Gate: none (docs plus rig runs).

## Phase 1: rig checks the agents can run (emulator, no real board) (1 agent, sonnet + haiku screenshot reader)

- [x] FP1.1 (done: 5e24ecc1 amigapi_rig step 13e PASS, last footer cell pixel-equal to the first 'm' of medium, flush right at 63 columns, 2026-10-15) G2: footer right edge `claude-opus-5-5 . medium` not clipped: rig 3, pixel read of the last 8 columns
  of the footer row; criterion: the last glyph column inside the window width (golden `start_w64` family).
- [x] FP1.2 (done: 5e24ecc1 amigapi_rig step 13f PASS, header whole in a 63-column window with outputPad 0, 2026-10-15) G3: `outputPad 0` header fully inside the window: rig 3 run, text read by the haiku reader.
- [x] FP1.3 P2 (done 2026-10-15: host test custom_headers_reach_the_request 24/24, revert-check FAILED without the fixture header line; rig: amigapi_rig step 15 13 of 13 with amiga-pi 7d46d16, run by the Opus agent on rig 2; the P2 crash was the 32 KB stack, not the header path): `SetEnv SAVE ANTHROPIC_CUSTOM_HEADERS` path: test through the top-level entry point against
  `tools/claude_fixture.py` extended to record request headers; criterion: header present, no 400, workspace id
  header absent unless set. Regression test name: `custom_headers_reach_the_request`.
- [x] FP1.4 (fixture half done 2026-10-15, rig 2, main 19f977fb, no code change so no new commit: tools/rig/claude_rig.py 6 of 6 PASS incl. 'Esc drops the unfinished turn' and 'Ctrl+C twice: the Shell runs commands again'; the real-key half stays OWNER's, not ticked) a3 T13: Claude TUI on the rig against `tools/claude_fixture.py` (conversation, Ctrl+C). The "then with
  a real key" half is the owner's (real key): note it, do not tick the real-key half.
- [x] FP1.5 (done 2026-10-15, rig 2, main 19f977fb, no code change: claude_rig.py 6 of 6 PASS plus a 77x20 (640x180) run of the same fixture: 20 rows, widest line 77 cells, hello/edit/startup Read+Glob/Bash tool/Ctrl+C mid-answer 'Stopped. The unfinished answer is not kept.', Ctrl+C x2 back at the Shell; Version has no fixture stream, Bash List SYS: ALL stands for run_command; full-screen half and pixel look of Markdown not read) a2 3.8 and 4.8: conversation on the rig at 77x20 + Ctrl+C; tool checks on the rig (fixture).
- [~] FP1.6 (not ticked: 4 of 6 criteria PASS, 3f8f0c1d tools/rig/hlmd_rig.py on rig 2: 1 hl --list 22 languages, 2 line numbers + 4 colours, 4 mdv heading/links/table whole and re-fitted wider after a resize, 6 ROM CON hl -p 4 colours and mdv table + - | only; 3 timing measured on the 060 only, 100 KB C in 0.3 s, the stock-A1200 number is open; 5 hlp/mdp SKIP, no less in the kit or on the rig; the pixel look of the colours is the owner's) highlight-markdown R1: the manual rig checks listed in its plan, by criterion.
- [x] FP1.7 (done 2026-10-15, rig 2, d5f2c5fc tools/rig/demo_rig.py `key` and `end` PASS 7 of 7 each: Help > Demo tour by menu pick, C:UPDemo runs, 'UP-Term demo' tab window opens, two frames 6 s apart differ, 4+ colours, Esc ends it / it ends itself, tab window gone, shell tab text unchanged; the tour's text cannot be read by Copy; finding: the plan says any key ends the tour, the code ends on Esc, Q, Ctrl-C only, space and Return go to the next scene) D1: demo menu to tab to tour to tab closes, text/pixel criterion.
- [x] FP1.8 (done 2026-10-15, rig 2, 5946f257 tools/rig/uptelnet_rig.py amigados and vsh PASS 8 of 8 each, uptelnetd on 127.0.0.1 with a throwaway password; C:uptelnet of the rig was an older build, 17072 bytes, the script copies the current one; the rig has no ENV:vsh/vshrc, the script puts dist/vshrc there for the run) 5.2 telnet: `C:uptelnet` + vshrc `telnet()` rig run against `tools/uptelnetd.py` locally (rig half of A1.4).
- [ ] FP1.9 W-T8: re-measure the W-T4 numbers on a quiet Mac (`uptime` load < 5, no other sessions' builds;
  schedule when the machine is idle; ask the owner to say "quiet" when ready: this one only needs that word).
- [x] FP1.10 P7: write the objective part of the iTerm2 cell-level comparison (what differs cell by cell, one
  table); the verdict "good enough" stays the owner's. UNBLOCKED 2026-10-16: owner turned on the iTerm2 Python API; module in git-ignored venv build/iterm2-venv (`python3 -m venv build/iterm2-venv && build/iterm2-venv/bin/pip install iterm2`; Homebrew python has none), API probe reads the current session. Method: research/2026-10-15_fp45-fp110.md:84-105. DONE 2026-10-17 (4091749c, research/2026-10-17_fp110-iterm-cells.md; tools/iterm_compare.py + refdiff iterm columns): 1920 cells per scenario. screen 2 splits: text 0, attr 0, colour 2, cursor row differs (iTerm2 row 94 = likely scrollback-based, unverified). tmux 2 panes: text 303, attr 52, colour 250, all in left pane rows 12-22 (engine shows chat log, iTerm2 shows zsh prompt + blank). chat log: text 3, colour 86 (row 0 bg 4 vs default, row 22). Lead CLOSED (cd3f6b9b, 73c916c2): replay pty had ONLCR on, bare LF became CR LF, tmux repaint erased left pane = harness artifact; fixed with stty -opost + pty test. Rerun: tmux text 0 attr 0 colour 77 (blank-cell fg none vs default); chat-log text 0; row-0 colour = iTerm2 API cannot report EL-erased cells. Engine matches on text and attributes in all 3 scenarios. Owner verdict pending. Owner 2026-10-17: more scenarios before verdict, see FP1.10b.
- [x] FP1.10b (owner 2026-10-17) extra iTerm2 cell scenarios before the good-enough verdict: vim (alt screen, scroll region, syntax colour), htop (256 colour, fast repaint), less with colour (man page). Same tools/iterm_compare.py method, one table, no rig. DONE 2026-10-17 (fc04e8e4, research/2026-10-17_fp110-iterm-cells.md section FP1.10b; htop absent, tools/colorsim.py used): vim text 0 attr 0 colour 60 (blank fg none vs default, known class); man/less all 0; colour256 colour 6 = inverse run row 11 cols 37-42 (engine stores fg 27 + inverse flag, iTerm2 reports swapped). Unverified: row 11 col 40 blank bg 27 vs default (API or iTerm draw; needs screenshot); cursor y = iTerm2 scrollback-absolute row (likely). No engine bug found. Owner verdict pending.

Owner visual sign-off list (output of the phase): G2, G3, D1, P7, R1 screenshots saved under
`thoughts/shared/research/` as text descriptions; no image reaches the main session.

## Phase 2: RTG true colour without owning pens (1 agent, sonnet; medium)

- [x] FP2.1 W-T13: draw true colour on RTG with `FillPixelArray` for fills (cause found: 24-bit WB has a 32-entry
  map and 0 free pens, ledger 1293). Files: `render/amiga_render.c` and its host test `tests/test_render.c`
  (pen allocation path vs RTG path, sentinel proves the RTG path ran). (done: b408a22c + 27b3a14c, direct_colours_are_exact_on_a_true_colour_screen_whose_colour_map_is_used_up, make test ONLY=render 420 checks, revert-checked, 2026-10-15)
- [x] FP2.2 W-T11: the split-screen scene whose text scroll wrong colours: rig 2 full demo run after FP2.1;
  criterion: pixel reads of the text rows equal the palette colours (haiku reader, text answer). (done: b408a22c, tools/rig/demo_colours_rig.py --hog rig 2: scenes 6 and 7 PASS 9 of 9 and 3 of 3 exact, main's handler 0 of 9 and 0 of 3, 2026-10-15)

## Phase 3: vtcon.md not-started and in-progress items (1 agent at a time; each its own branch, 1 PR-sized commit set)

Order (cheapest evidence first):
- [x] FP3.1 Q2: Neovim `tui_spec` replay (host test under `tests/`, `git log --grep tui_spec` is empty). (done: 50887859, make test ONLY=xterm neovim_tui_streams_land_where_libvterm_puts_them, make test-ref ONLY=nvim 6 streams 0 failed, 2026-10-15; Q2 rig session part stays open)
- [ ] FP3.2 RT1/W8: phase profile of retro32-term against ibmcon (the plan's own measurement; rig, one table). PARTIAL 2026-10-16 (46cb7c3a, research/2026-10-16_rt1-phase-profile.md): whole-run only, DCTelnet --test-speed 3.7 MB on rig 3 060: Built-in mean 9.05 s vs ibmcon 8.15 s (1.11x slower). Phase split needs timers inside DCTelnet (~/Code/DCTelnet, outside repo: owner call). Unverified: Built-in ran planar path or Text() path.
- [x] FP3.3 (done 2026-10-16: `rig.py start --a500` boots, 68000 confirmed; engbench ONLY 0-6: plain text 33-45 KB/s, newlines 2.4 KB/s, 6.4-8.1x slower than the A1200; thoughts/shared/research/2026-10-16_w7-68000-engbench.md) W7: A500 (68000) rig config and the speed measurement the plan lists (emulator config, not a board).
- [ ] FP3.4 S1: the asm hot-path part (S2 perf is merged at 0912d65); measure before and after on rig 2.
  2026-10-16 session 5: S1's asm part = ASM1 (vtcon plan line 697/1618): renderer and handler hot C paths in asm,
  chosen by tools/prof68k.py instruction counts. Engine part merged 99241c9; painter_68k.s exists (8a48cd1b).
  Step 1: phase split per conbench shape. 060 --max run done (research/2026-10-16_fp34-phase-split.md): handler
  cost tiny except sync-line (rest 91% of non-wait, ~2.56 s); a ~0.2 s "rest" floor on every shape. 060 JIT under
  Mac load 44-116 is not the target: re-run on --exact 020 together with FP3.7 (research/2026-10-16_fp37-exact-direct.md).
  Step 2 --exact 020 DONE (research/2026-10-16_fp37-exact-direct.md): biggest non-wait cost = "rest" (packet/loop
  path): sync-line 2.3 s, sgr-colour 1.3 s, bytewise 1.2 s (2400 one-byte writes = ~0.5 ms a packet), scroll-nl
  0.65 s. Then draw on colour shapes (sgr-colour 2.2 s, sgr-perchar 1.4 s), then feed (sgr-colour 1.12 s,
  plain-lines 0.64 s). So ASM1 target #1 = the per-packet path, not the painter. Next: split "rest" into its
  parts (packet receive, reply, loop, signal wait) with PROF counters before choosing what goes to asm.
  Step 3 rest split DONE (research/2026-10-16_fp34-rest-split.md; PROF sub-counter diff 187+/16- in
  handler/vtcon_handler.c left UNCOMMITTED for review). Findings: (1) PROF=1 SERIAL=1 builds trace every packet
  (DBG "packet" :6065, "reply" :486, ~5.4 ms each): 2174 of sync-line's 2931 ms "rest" is the trace itself, so
  every PROF phase split so far overstates rest. Fix the instrument first: phase output to serial must not turn on
  the per-packet trace. (2) ReadEClock = 41 us a call on --exact 020, so sub-counters under ~0.1 ms are timer
  noise; measure with one timer pair around 100 packets or the emulator cycle counter. (3) Real rest ~1.3 ms a
  WRITE (bytewise/sgr-colour/scroll-nl); only concrete candidate: service_reads runs after the reply (:6233) AND
  again in the loop tail for the same packet. Next: fix instrument, re-measure, then pick ASM1 / FP3.6 row 12.
  2026-10-16 session 6: instrument FIXED. PROF lines print via PROF_LOG (serial writer of their own), not DBG:
  build with `PROF=1` alone, no SERIAL=1, no trace. PROF now in HANDLER_FLAGS (toggling it relinks).
  Test tools/test_handler_prof.py (fails before). Sub-counter diff dropped. NEXT: re-measure rest on rig 2
  with a PROF=1 build (phase_rig.py), then decide ASM1 target and FP3.6 rows 7, 12.
  Re-measured (research/2026-10-16_fp34-rest-clean.md, rig 3 --exact, 0 trace lines): rest 288-1220 ms, ~0.2 ms a
  write net of ReadEClock. ASM1 target #1 now = painter on colour shapes (paint ~1 s), #2 row composition. Cursor
  pass 2.7 ms a frame on sync-line = algorithm question, open. Row 12: no gain left in rest; DROPPED (owner 2026-10-16). Owner 2026-10-16: cursor pass = investigate as algorithm (what it redraws) before ASM1; ASM1 profile = extend prof68k to the painter (not paintbench). Cursor read 2026-10-16 (research/2026-10-16_cursor-pass-algorithm.md, source only): PROF slot 2 = vr_mask_begin+vr_cursor_off and vr_cursor_on+vr_mask_end (vtwin.c:540-558), every frame; on re-renders the glyph (Text), off is a one-cell draw_rows. Cost flat across shapes = fixed per frame. NEXT (rig, after FP4.1 agent): split slot 2 into mask / off / on, count gfx calls per frame; then pick cut (skip unchanged cell; fold off into the flush). Painter profile 2026-10-17 (ba9f4d1a, research/2026-10-16_prof68k-painter.md; prof68k build-paint/run-paint): vp_asm_plane 70-76% of painter instructions in all 3 shapes (loop 90-108 B, fits 256 B cache), vp_span_fast C glue 21-25%; sgr-perchar 558 instr/cell vs 121 sgr-colour (per-call glue dominates short runs); pen-1-on-0 glyph-pointer line painter.c:60 ~20%. Row composition (painter_run) not profiled: needs graphics.library. ASM1 candidates: per-call glue for short runs, then glyph pointer; plane loop already asm. Cursor split 2026-10-17 (dfc8f936, research/2026-10-17_cursor-pass-split.md; PROF slots mask/off/on + gfx counters, render test added): rig 2 on 060 (--max), sync-line 200 frames: mask 0.00, off 0.03, on 0.04 ms/frame; off = 1 RectFill + 1 pen + 1 draw_rows, on = 1 Text + 1 RectFill + 2 pens; 0/200 frames with cursor cell unchanged. Earlier 2.7 ms was stock 020 + os32, not comparable; 020 re-measure blocked: OS3.2 tree not found on this Mac (conbench_rig.py default flags; override UPTERM_RIG_FLAGS added). On 060 the pass is ~1% of sync-line; favoured cut = fold off into the flush (max ~0.03 ms/frame on 060). Skip-unchanged does not apply. OWNER: re-measure on 020 before any cut, or park. PARKED (owner 2026-10-17): 020 re-measure waits for an OS3.2 tree; no cut until then.
- [x] FP3.5 (done 2026-10-15: (a) 84b339f8, (c) 87aac22e test_slash 292 + test_terminfo_amiga 4, (b) c13c27d7 test_prefs 6808, (d) 604ef8be tools/rig/slashenv_rig.py 4 of 4 PASS rig 2) W46: remaining commands of "setup as / commands" (468f1b03 lists what stays open).
- [ ] FP3.6 W27: CCON techniques rows 5, 7, 10, 12 (research doc exists).
  2026-10-16 session 5: row 5 DONE in code (b1a181a9: write mask = planes the pass drew, `r->mask`; painter.c
  vp_span/vp_span_fast skip planes outside it). Row 10 DONE (b1a181a9, handler/vtcon_handler.c:6645 skips the
  port walk when only a DOS packet woke the loop). Open: row 7 (one-plane pen-1-on-0 path: vp_asm_plane still
  runs its pattern XOR) and row 12 (ACTION_WRITE replied via dos ReplyPkt, vtcon_handler.c:451). Both are hot
  paths of FP3.4/ASM1: decide from one prof68k profile of renderer + handler, not separately.
- [x] FP3.7 (done 2026-10-16: DIRECT stays opt-in, no gain on --exact 020; research/2026-10-16_fp37-exact-direct.md) P3: planar fast path cycle-exact comparison (`7ecdb88`); emulator cycle counter as oracle.
  2026-10-16 session 5: question = handler DIRECT=0 (RastPort) vs DIRECT=1 (planar, now with painter_68k.s
  8a48cd1b) on `rig.py start --exact` (020 cycle-exact; emulated time counts cycles, so Mac load does not
  matter). R1 (vtcon plan line 93) measured planar SLOWER (6.6 vs 3.4 s drawing) before the asm painter, which is
  why DIRECT is opt-in. The result decides whether DIRECT becomes the default.
  Result 2026-10-16: draw ms rastport vs direct within 5% on all 8 shapes (sync-line 1824/1749, sgr-colour
  2322/2223, plain-lines 837/826); single run each. The default build already draws every run through
  painter_68k.s ("0 Text runs"), so DIRECT=1 only swaps ScrollRaster for BltBitMap: scroll part drops,
  rows part rises the same. No reason to flip the default.
- [ ] FP3.8 R1/W34: UP-Term in ROM: fix `InternalLoadSeg failed (ABI)` (b3e8aea9); boot proof in fs-uae with the
  ROM image; branch `feature/r1-rom` belongs to another session: coordinate first (list under Blocked on me).
- [ ] FP3.9 W10: CC1 blitter overlap and CC2 sprite cursor proof with `tools/rig/cc_rig.py` (exists only on branch `feature/cc1-cc2`, not on main; emulator); branch
  `feature/cc1-cc2` belongs to another session: same coordination.

## Phase 4: unix-tool-ports.md (1 agent, sonnet; per tool: recipe, package, rig test, kit entry)

Per tool the acceptance is the plan's own row plus: package in `../upterm-ports/pkgs/<tool>/`, `make dist` lists
it, `install_rig.py --only <case>` installs and runs it. Batches by size (decision: smallest first):
- [x] FP4.1 (done 2026-10-16, rig 2 060, userland_rig --only, research/2026-10-16_fp41-batch-a.md: bc+dc 6/6, ed 11/11, column 11/11, iconv 9/9, ctags 9/9, jq 19/19 x5; man pages + kit-stage entries present; `make dist` is `make kit-stage` in upterm-ports; fresh checkout needs `make host-<pkg>` first, the rig tool says so) batch A: 4.6 bc, 4.10 ed, 4.9 column/iconv, 4.11 ctags, 4.7 jq.
- [x] FP4.2 (done 2026-10-17, rig 2, research/2026-10-16_fp42-batch-b.md: mg 2/2, joe 2/2 + man fix upterm-ports b1b94a0 merged d4bf289, nc 4/4 (no listen/connect case; no licence dir), wget_rig.py all ok against host http.server, tldr ok against local server only; wget/tldr functions were already in dist/vshrc 79bd9a34) batch B: 4.5 joe, 4.2 mg, 5.1 nc, 5.9 tldr, 5.3 wget as a vshrc function (curl is in `dist/net`).
- [x] FP4.3 (done 2026-10-15, upterm-ports fix/finish-fp4c 2c5b497 make 12/12, e81d105 gawk 21/21, 034aee6 lua 10/10, userland_rig --only on rig 3) batch C: 4.8 gawk, 5.5 make, 4.4 Lua (pi has its own: reuse before building a second one).
- [ ] FP4.4 batch D: 4.1 vim, 5.7 rsync, 5.6 git, 5.8 perl. APPROVED (owner 2026-10-17): one port per agent, vim first on rig 2; next port = new agent. vim DONE 2026-10-17 (upterm-ports 3de93af, merged; notes thoughts/shared/fp44-vim-notes.md there): rig 2 userland_rig.py --only vim 2 of 2 PASS (files and screens byte-equal to Mac build). Gaps: xxd not built (ixemul lacks stpcpy: fix in ixemul-vtcon, open), :help docs pruned, runtime (syntax, defaults.vim) and :!cmd untested on rig, stack 262144 not measured. rsync BLOCKED 2026-10-17 (upterm-ports 7f8eb55, thoughts/shared/fp44-rsync-notes.md there; source read, not built): do_recv (main.c:1032) forks generator/receiver sharing memory on every receive incl. local copy; ixemul vfork only, no threads. Push-only client (Amiga sends over ssh: piped_child via posix_spawn, no do_recv) is untested candidate. 5.7 rsync SKIPPED (owner 2026-10-17): not portable to ixemul. perl DONE 2026-10-17 (upterm-ports cf8f22f, merged; notes fp44-perl-notes.md): 5.42.3 via perl-cross, -Ud_fork, system() via do_spawn patch; rig 2 userland_rig.py --only perl 3 of 3 PASS. m68k_scan switch-table fix, regression test verified failing on revert. Open: no rig case loads a module (use strict, lib/perl5 26 MB untested); ext/re, Time-HiRes, DB_File, NDBM, Compress-Raw-* disabled; times() off (no sys/times.h). Order now perl, then git (git: source read for fork use first). git source read DONE 2026-10-17 (research/2026-10-17_fp44-git-fork.md, git 2.56.0): portable with limits. start_command child is exec-only (run-command.c:787), vfork swap ~15 lines with global/signal save. Blocker = start_async with NO_PTHREADS forks in-process (run-command.c:1209): clone/fetch/pull (fetch-pack.c:985 sideband demux), push (send-pack.c:703), clean/smudge filters. Local commands work. Network needs demux replaced (~100 lines). Owner 2026-10-17: git local first (vfork start_command), then network (demux) as a second agent. Fork design research DONE 2026-10-17 (research/2026-10-17_fork-design.md, Fable): git network = ix_vfork + ix_vfork_resume in start_async (ixemul-vtcon library/vfork.c:604-622, verified; ~40-60 lines, replaces the demux rewrite); rsync = vfork+exec per site plus exec'd receiver helper (effort L), would reopen the skipped 5.7 (owner call). Real fork by copy and MMU fork rejected. Phases FK1-FK6 in the doc. Owner 2026-10-17: git network agent uses ix_vfork_resume in start_async (FK1, fixes every async caller in one place), not the demux rewrite. rsync 5.7 REOPENED after git network passes on the rig: FK3 then FK4.
  - git local (2026-10-17): upterm-ports feature/fp44-git (a7056cd, df86097, 58d4ac5), rig 2 7 of 7, NOT MERGED. Held: pkgs/zlib/recipe.mk drops to -O0 because gcc -O2 miscompiles deflate (stray 0x00 before the adler32 trailer); that is a workaround, and the same -O2 zlib already ships in libarchive, file, mandoc, bzip2. Needs a root-cause hunt (which pass, fix gcc or the source construct) before merge; owner to approve the hunt. Also open: csprng fallback (proper fix getentropy in libixcompat), templates not shipped, `VTC:` clone path read as ssh host.
  - wasabi 0.11b2 (2026-10-17, owner: "update to the wasabi beta"): pin be4c055, efc7f040; rig 3 install_rig --only wasabi 29 of 29 (case now restarts the kit daemon on KIT_WASABI_PORT, the rig daemon holds WASABI_PORT). zlib -O2 root-cause hunt APPROVED (owner 2026-10-17), git merge waits on it.
  - zlib -O2 root cause (2026-10-17): gcc bug, m68k peephole2 "combine clr if possible" merged clr.w x + clr.l x+2 into clr.l x (trees.c bi_windup). Fix gcc aee333a76 on fix/zlib-o2 in cpython-amiga/build/gcc/src; new cc1 installed (old kept as cc1.pre-clrmerge-2026-10-17); test f2dc4dfc 4 [OK]. Open: zlib back to -O2 + rebuild zlib users + merge git (agent running); FULL KIT REBUILD with new cc1 (any binary may carry the bad merge); upstream PR to bebbo/gcc via amiga-gcc/projects/gcc amiga6 (owner approval, outward-facing).
  - git network FK1 (2026-10-17): upterm-ports feature/fp44-git-network (e43cc29, 70d09bd, bd732f6), MERGED to main 5f834d0 and pushed 2026-10-17 (owner approved; branch deleted). start_async via ix_vfork + ix_vfork_resume (patch 0004). rig 2 userland_rig.py --only git 8 of 8: file:// clone and fetch byte-equal to host git, sentinel GIT_ASYNC_SENTINEL proves the ix_vfork_resume child ran. Open: git push file:// HANGS (send-pack + receive-pack both start_async; 3 processes leak, Ctrl-C ignored; push case removed from cases), root-cause hunt next; X:path read as ssh host (cases use file:///VOL/...); git-upload-pack/receive-pack not on PATH (SKIP_DASHED_BUILT_INS); ssh/https FK2/FK5 untested. Notes: upterm-ports thoughts/shared/fp44-git-network-notes.md. Push hang FIXED (bd732f6): the async child read start_async locals after ix_vfork_resume (a5 frame reused by the parent; close(fdin[1]) hit the wrong fd, receive-pack muxer never saw EOF); child now closes pipe ends before the resume and passes only async in a2; in_async() by pid. rig 2 git 9 of 9 incl. push. Patch 0004 reviewed sound; merged. Open: send-pack sideband_demux reads fd[1] through shared memory (theoretical leak), 'file:' read as an ixemul device in push.err (harmless).
  - FULL KIT REBUILD with the fixed cc1: APPROVED (owner 2026-10-17). Step 1 inventory (which kit binaries use the replaced cc1, rebuild order) -> research/2026-10-17_kit-rebuild-inventory.md, agent running. Step 2 rebuild in place in upterm-ports main checkout ONLY after the git push-hang agent finishes (it holds that checkout on feature/fp44-git-network with uncommitted edits); no worktree: disk 3.5 GiB free, upterm-ports/build alone is 2.6 GB. Step 3 make dist + dist-check + install_rig on rig 3. Rebuild agent STARTED 2026-10-17 on upterm-ports main (git network not in it); progress in scratchpad rebuild-progress.md, result research/2026-10-17_kit-rebuild-result.md. Inventory DONE (research/2026-10-17_kit-rebuild-inventory.md): ~168 executables from 7 repos affected (ports ~153, ixkill, ixpipe-handler, screen, tmux, wasabid, bebbossh x7, nvim, python3) + static libs libixcompat, libevent, ports zlib/ncurses/bzip2/xz/libarchive; vbcc builds (vtcon, Claude, amiga-pi) and ixemul (gcc 2.95.3) not affected; prebuilt coreutils 5.2.1, bebboget, installcerts, curl not rebuilt. cpython-amiga uses its own cc1 (build/gcc/bin, 8 Oct, branch amiga-cpython = amiga6 + amiga/gcc 0001-0004) which LACKS the clr-merge and addx fixes; the installed cc1 branch fix/zlib-o2 contains all four amiga/gcc patches plus both fixes (checked by git log --grep on build/gcc/src). Decision: cpython-amiga drops its private cc1 and uses the installed one (its own build-cc1.sh header says the patches belong in the installed toolchain; they are there now). Order: libixcompat, ports libs, ports programs + kit-stage, tmux/screen, neovim, vtcon wasabi + bebbossh, cpython, vtcon make dist.
  - REBUILD DONE 2026-10-17 (result research/2026-10-17_kit-rebuild-result.md, vtcon b9f2ea65): every repo rebuilt with the installed cc1, make dist + dist-check rc 0, sha256 314 changed / 2183 same / 3 new. Unpushed source commits: cpython-amiga db25c41 773dbe9, tmux-amiga a420e3a, neovim-amiga 829f6b0, screen-amiga/src 93af76e (the 2026-10-15 __stack patch, applied then but never committed). Rig 3 install_rig --force 10 of 19 FAIL: rig 3 System: (DH0 106 MB) fills up after base + Python3 (1 block free, "disk is full"); stale expectations (coreutils 5.2.1 vs ports ls 9.12, amiga-pi 1.1.0 vs 1.2.0, TERMINFO leftover /VTC/terminfo); claude-bare reason unread. userland_rig for changed packages NOT run.
  - [ ] Rig 3 capacity: enlarge System: (or clean it), fix stale expectations, install_rig --failed, userland_rig for changed packages.
  - [x] cexp missing in ixemul libm: gcc -O2 turns sin+cos of one argument into cexp; cpython link failed; WORKAROUND in cpython-amiga (cexp/cimag/creal from newlib) must move to ixemul-vtcon libm (root cause), then drop the cpython copy. Also: cpython `make compat dist` does not depend on `m68k` (stale python.exe); run `make m68k` explicitly. DONE 2026-10-17: ixemul-vtcon 64725fc, cpython-amiga 5737e43, test vtcon 0d3f67fc (make test ONLY=ixemul_sincos), all pushed. Rig 1 2026-10-17: rebuilt dist runs, cmath/sin/cos values match host CPython exactly, c3-sentinel 27 OK 0 FAIL. Stale libnewlibwcs.a fixed in cpython-amiga ef36ad1 (test-newlibwcs). Note research/2026-10-17_ixemul-cexp.md.
  - [ ] git kit packaging gap (A1200 run 2, e61cad82): no git-upload-pack/git-receive-pack, no UP-Term:libexec/git-core, no share/git-core/templates; file:// clone fails `bad line length character: vsh:`. Then rerun clone/fetch/push + Amiga `git fsck` (the real zlib -O2 proof).
  - [x] FS-UAE log filled the Mac disk (7.9 GB, truncated 2026-10-17): per-rig logs_dir build/rigN/logs, 50 MB cap at start + ami.req (5aaa877b), logwatch beside each running rig caps logs + serial.log every 30 s (7a90e5a0). Not pushed. All rigs down; restart needs owner OK; first restart: check which line flooded.
  - [x] ami.py reboot for the real Amiga (06b39996, A1200 back after 32 s). Rig 1 restarted 2026-10-17 22:00 with per-rig log (49 KB after boot, not growing); rigs 2/3 left down until needed.
  - A1200 kit install: Rushhours:UP-Term-inst; compression bzip2/xz/gzip-text byte-identical, round trips PASS (e61cad82). LIBS: had been removed by `Assign LIBS:` (no target = remove); restored.
- [ ] FP4.5 5.4 BebboSSH patches: K6 (BebboSSH 1.46.1, VT 4ae821c) is related; decide from the plan row whether 5.4 is
  already covered, tick with that evidence or do the remaining patches.
- [ ] FP4.7 gcc addx fix: cc1 fixed and installed (e5657808 test, gcc d4d9db8f6), PR
  https://codeberg.org/bebbo/gcc/pulls/15. Open: rebuild the 33 upterm-ports userland binaries
  listed in research/2026-10-16_gcc-addx-cc.md, drop upterm-ports tools/m68k-cc workaround,
  re-run cp/du/ls on a rig. upterm-ports is another repo: owner says go.
- [ ] FP4.6 3.7 coreutils 9.12: only after FP0.6 (owner says who commits `recipe.mk`).

## Phase 5: large projects (each starts with its own research + plan, per the workflow; code only after approval)

W39 gcc 16 string-object hang is a root-cause hunt (opus subagent, bounded: stop and report after 10 tool
calls without a foothold, then the owner decides). The rest are products; "finish" is a scope decision, so
each gets Phase 1-2 documents first (`thoughts/shared/research/` and `plans/`), and the owner approves each
plan before any code. This is the one place this plan stops for the owner.
- [ ] FP5.1 W16 MicroPython port. [ ] FP5.2 W19 TUI prefs app (prefs/ has GUI files only). [ ] FP5.3 W38 tiny LLM (marked LAST).
- [ ] FP5.4 W40 CPython asm (marked future). [ ] FP5.5 W55 native multiplexer. [ ] FP5.6 W56 browser thin client (marked LATER).
- [x] FP5.7 (done 2026-10-15: hang already fixed in ixemul-vtcon 45f8bd3 + e8bcc9d, re-tested, note thoughts/shared/research/2026-10-15_w39-gcc16-hang.md; open W39 rest = CPython/neovim rebuild, owner queue) W39 gcc 16 hang hunt (ixstackext 35bee4a2 is the state).
- [ ] FP5.8 (added 2026-10-16, owner request) Real debugger for C:Claude / amiga-pi on the Amiga: registers, memory, breakpoints, step, so the agent can build, run and debug an executable (today it has files + shell only, no register/memory/breakpoint tool). Phase 1 first: research existing Amiga debuggers/monitors and remote-debug stubs we could reuse or wrap (gdb remote stub for AmigaOS, bebbo toolchain gdb, FS-UAE/WinUAE debugger + GDB server, wasabi's own probes, classic monitors) before any design. Also survey the owner's demo system project (/Users/spot/Code/UpRough/Up_Rough_Demo_System, confirmed by owner 2026-10-16; check urds-prof and urds-tmcrash beside it too): it has much debugging tooling built for an emulated environment; owner does not know what or if any of it is reusable on a real Amiga, so the research must sort each piece into reusable / emulator-only / adaptable with evidence; document in thoughts/shared/research/, owner approves the plan before code.

## Order, size, cost checkpoint

Phase 0 and 1: one sonnet agent each, a few hours of wall clock, light (the rig is the load). Phase 2: medium.
Phase 3: nine independent items, about one agent-session each. Phase 4: 18 ports, large (batches A and B are
the cheap ones; vim, git and perl are the big ones). Phase 5: research documents only until approved.
Order: 0, 1, 2, 3, 4, 5. Phases 3 and 4 do not share files: they can run as the two agents at the same time.
Checkpoint (the cost rule): ask before starting Phase 3, Phase 4 and each Phase 5 product; Phases 0 to 2 start
on the owner's word.

## Verification (separate from manual)

Automated: per item the named test; landing gate `make dist` + `make dist-check` when kit content changes;
`make test ONLY=<suite>` for the touched suites. Manual (owner): the visual sign-off list of Phase 1, the
real-key halves (FP1.4), approvals of Phase 5 plans, answers on FP0.6, FP3.8, FP3.9 (other sessions' branches).
