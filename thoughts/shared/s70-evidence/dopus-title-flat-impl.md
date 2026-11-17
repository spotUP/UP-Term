# dopus-title-flat implementation evidence (worktree vtcon-dopus-title-flat, branch dopus-title-flat)

Commits: 49d0373c (S3), 1ec72382 (S1), 1dee38a8 (S2a), 6e587158 (lock glyph), 2fa4972c (dopusiclass hook).
Host: make test ONLY=flat_core 4161 checks 0 failed. Amiga: make amarchy-flat OK. Budget: _c2_disp_reg headroom 64, _d5_disp_reg 68.

Done (V = verified by host test/build): S3.1 S3.2 S3.3 S1.1 S1.2 S1.3 S2a.1-3 S2b.1 S2b.2 S2b.3 S2b.4 S2b.5 S2b.6 S2b.7 S2b.8(host) = 18 V.
S3.5: grep of Program/*.c DrawBox/Do3DBox users: lister_idcmp, lister_dual, lister_title, lister_toolbar, start_menu, toolbar, display, graphics, backdrop_render -> all Program-drawn raised boxes (no ring wanted, O6); Environment buttons are class-drawn (bevel body), keep ring. = 19.
Open (U, need rig 68): S1.0 (zoom GACT_TOPBORDER), S3.4 (status row/toolbar no ring, Environment ring kept), R.1, R.2. = 3 open. Total 19 of 22 by this count.
Rig: pid 16435 fs-uae rig68 hung at >99% CPU; `rig.py reboot` attempt 1 started, did not finish within 120s (background).
Found: wh uninitialised in c2_which_31 (S2a, vbcc warning 171) fixed; flat_glyph_draw frame growth fixed by moving lock remap out.

## Rig checks (rig 68, 2026-11-10, worktree build of dopus-title-flat, purpose "s70 session: DOpus button frames and title gadgets")
Rig: reboot succeeded first try (no "(go not started)" hang). `rig.py dopus` already installed (stamp 0800b7b3). flat-starting deleted, build/amiga/amarchy-flat pushed to RAM:, started with flat prefs on. Screen "Directory Opus" 800x600 (RTG square pixel). STATUS: `dopus=73.8 patched=7`, after fresh lister `dopus bevel calls=0 face=7 d5=6`, after refresh face=15 d5=6.
- S1.0 MEASURED (RAM:gdump, a one-off C dump of window FirstGadget chain, Device List window; scratch, not committed):
  id=11 (x=-94) act=0x8141 (TOPBORDER set), id=10 (x=-70) act=0x8041 (TOPBORDER set), id=13 (x=-45, zoom position) act=0x8001 (TOPBORDER NOT set), depth id=0 x=-22 act=0x8001. Flags 0x8014 (RELRIGHT), GadgetType 0x0005 (custom). => the zoom-position gadget has no border flag: geometry branch is LIVE for it; lock/iconify carry TOPBORDER.
- S3.4 FAIL (partial): sentinel d5=6 > 0 and face=7..15 > 0 (PASS), but title strip pixels (grab x340..450, rows 2..12) show glyphs only for the two right-hand gadgets (x~403-415 and ~425-435); lock (x349-372) and iconify (x373-397) areas are solid band colour 7aa2f7: BLANK, in both Device List and Ram Disk windows, also after `dopus refresh lister`. No dividers. Status row / scroll-arrow buttons (x380..445, y538..573): arrows drawn, no ring around the arrow glyphs; horizontal line-pen rows at y549/563 remain (frame lines of the areas, not per-button rings; unverified). Toolbar and Environment-button rings NOT checked (no budget).
- R.1: not passed (depends on S3.4 lock/iconify blank). R.2 (hires side 2): NOT RUN (budget).
- Suspicion (unverified): d5 counts draws but lock/iconify glyph pixels never reach the screen or are overpainted afterwards; needs a root-cause look at d5_draw / which rastport and clip it draws into.

## Root cause + fix (s70 agent)

- Violated invariant: the title accent band (tx_title_draw) must end before every title-bar gadget. Level: gadget classifier in flat_core (data/constant level), not the d5 draw path.
- Cause: `flat_title_gadget` (amarchy/flat_core.c:891) tested Activation 0x10 as GACT_TOPBORDER and excluded 0xE0 as LEFT|RIGHT|BOTTOMBORDER. intuition.h: RIGHTBORDER 0x10, LEFTBORDER 0x20, TOPBORDER 0x40, BOTTOMBORDER 0x80. DOpus lock (0x8141) and iconify (0x8041) failed both branches, tx_span (flat.c:1272) left them out, band ran to x 397 and tx_title_draw (flat.c:1290) overpainted the glyphs the d5 hook had drawn (d5 counted, pixels lost). Zoom passed only via the geometry branch. Proof before fix: clicking the Ram Disk lock (GM_RENDER only, no frame refresh) showed the glyph; any frame refresh erased it.
- Fix: commit 0abc0179 on dopus-title-flat (TOPBORDER 0x40, exclude mask 0xB0, header comment fixed).
- Test: tests/test_flat_core.c test_title_gadget_filter now uses rig Activation words (zoom 0x0001, iconify 0x8041, lock 0x8141) and checks each border bit. Fix reverted: 5 FAIL (ng 4 want 6, x1 397 want 348). Fix in: `[OK] flat_core: 4164 checks, 0 failed`. amarchy-flat build OK.

Rig 68 (binary on rig cmp-identical to worktree build; STATUS d5=12 face=52):
- S3.4 lock+iconify (Ram Disk lister, screen x549..651 y14..27): PASS. Open padlock at x567..571 rows 17..24, iconify bar x584..601 rows 23..24, both a9b1d6 on 7aa2f7 band; zoom/depth glyphs 1a1b26.
- Open: lock/iconify glyph pen is pen_text (a9b1d6), zoom/depth use ptext (1a1b26). c2_glyph (flat.c:581) does not pass p->text, unlike the clone path. Consistency fix not made.
- R.1: PASS for title strip pixels + d5 sentinel; Device List lister was not open after reboot, only Ram Disk measured.
- R.2 (hires side 2): NOT RUN.
- Toolbar rings: FAIL (unverified intent). Ram Disk button bank rows 28..40: full-width 7aa2f7 lines at rows 28 and 40, 2 px 7aa2f7 vertical separators between buttons.
- Environment window rings: NOT RUN.

## Final checks (s70 agent 2)
- Pen fix: commit 6033146d "flat: draw DOpus lock and iconify in the title text pen like zoom and depth". Root cause: c2_get_pens sets p->text = pen_ptext on the title band; the clone path honoured it, flat_glyph_draw had no text_pen, so the d5 lock/iconify glyphs drew in pen_text (a9b1d6) while zoom/depth drew in ptext (1a1b26). Shared rule flat_glyph_fg_pen() now used by both paths. Host test test_band_glyphs_take_text_pen: 4185 checks 0 failed with fix; 12 FAIL with text_pen ignored. flat-budget passes (c2_disp_reg 68, c2_disp_stk 76, flat_rf_c 64).
- Toolbar lines: source draw order is RenderImage then do3dbox (Program/lister_toolbar.c:198-199, 375-379, 470-472), so the S3 face read ReadPixel(x0+side, y0+1) samples the brush's own top-left pixel, not a stale pixel: hypothesis (b) ruled out by source. Whether the 7aa2f7 rows 28/40 are brush edge pixels or the face-pen ring is NOT measured (rig froze, below). No toolbar commit.
- Rig 68: reboot with purpose "s70 session: DOpus title gadgets and button frames final checks" OK, flat-starting deleted, build pushed to RAM:, second Run started it (STATUS flat=on calls=7, d5=0 face=0 before redraw). Then `dopus refresh all` + DeviceList NEW + `lister new RAM:` -> wasabid stopped answering (auth timeout 150 s). Cause unknown: possible crash in the new build during the d5 redraw, or a DOpus/requester hang. UNVERIFIED; first job for the next session. Rig left up and held, not stopped.
- Measured before the freeze (flat absent at that moment, so stock values): both listers title-right strip 1a1b26/a9b1d6 only.
- S3.4 Device List / Ram Disk: NOT RUN (freeze). R.1: NOT RUN. R.2: NOT RUN. Toolbar rings: NOT RUN. Environment rings: NOT RUN.
- Checklist: 19 of 22 verified; open S3.4, R.1, R.2 (S1.0 fixed by 0abc0179, rig PASS on Ram Disk in the previous run).

## Freeze check (s70 agent 3)
- Found on arrival: rig 68 fs-uae alive (108% CPU, booted 08:32:48) but wasabid answered no auth (frozen ~08:34, last-command 08:39). No T:lastguru. serial.log ends at the boot prompt, no Guru text. retry.log's "System Request Retry/Cancel" lines are the IPrefs screen-reset request at rig start (rig.py WB_RESET_RETRIES), not the freeze. A wasabi `debug` client from another session (pid 11467, started 2026-11-09 23:05 from the main checkout, log build/rig68/wasabi/debug.log) is still connected to port 1301; it heard nothing at the freeze. Left alone (not ours).
- Sequence per run: rig.py reboot --purpose "s70 session: DOpus freeze check"; Delete ENVARC:/ENV:Amarchy/flat-starting; put binary to RAM:amarchy-flat; Break the boot's UP-Term:bin/amarchy-flat and wait for STATUS flat=absent; Run RAM:amarchy-flat; rx `dopus refresh all`; `command DeviceList NEW`; `lister new 'RAM:'`; Echo pings between steps and 30 s after.
- 6033146d (binary cmp-identical to worktree build): NO freeze. First Run gave flat=on calls=7; after the sequence flat=on calls=290, dopus calls=64 flat=64, face=49 d5=18; pings alive throughout and 30 s later.
- 0abc0179 (sources checked out from 0abc0179 for the three flat files, built, sources restored to HEAD): NO freeze. Same counters (calls=290, face=49 d5=18), alive 30 s later.
- Control (accidental, 6033 not pushed, no flat at all): same DOpus sequence, no freeze.
- Verdict: the build did not cause the freeze; it was transient. Unverified guess: the earlier run's first Run came right after a Break and STATUS said flat=absent (old instance still unloading / two instances); this run waited for flat=absent before Run and the first Run always took.
- Diff review 6033146d: p->text initialised -1 in c2_get_pens, plan_roles resolves fg/bg pens before any put, no new stack use in put (plan grows 4 bytes on the private-stack-free path; flat-budget c2_disp_reg 68). Nothing that can fault.
- S3.4 Ram Disk (6033146d, 800x600 square, screen x330..443 y0..13): PASS. Open padlock x359-363 rows 3-10, iconify bar x377-394 rows 9-10, zoom x403-415, depth x425-439, all 1a1b26 on 7aa2f7 band.
- S3.4 Device List (raised; title text ends x78, so it was the front window): PASS, same pixels as Ram Disk.
- R.1: PASS (pixels above + STATUS d5=18 face=49 on the square DOpus screen).
- R.2 (hires side 2): NOT RUN (needs rig.py aga + DOpus on a hires screen; budget).
- Toolbar rows 28/40 and separators: NOT RUN for Ram Disk. Device List at x0..443 rows 26..42 has no full-width 7aa2f7 line at 28 or 40 (row 28: 1a1b26 1-3, a9b1d6 4-419). Source verdict (ring vs brush edge): NOT RUN.
- Environment window rings: NOT RUN.
- Rig 68 left up, 6033146d in RAM:amarchy-flat running, idle watchdog 15 min.

## Last checks (s70 agent 4)
- Rig 68, build 6033146d (RAM:amarchy-flat cmp-identical to worktree build), purpose "s70 session: DOpus R.2 hires, toolbar and Environment checks". Procedure followed: reboot, delete flat-starting, put binary, Break boot flat (Process 13), STATUS flat=absent, Run, STATUS flat=on calls=7.
- FREEZE REPRODUCED TWICE with flat on. Run 1: Break + Run, then `rx "ADDRESS DOPUS.1; lister new 'RAM:'"` as the first DOpus command: wasabid dead (ami.py connect timeout) for 5+ min, fs-uae 100% CPU, serial.log only the boot prompt. Run 2 (after reboot): refresh all (ok, ping), `command DeviceList NEW` (ok, ping, flat=on calls=58 face=16 d5=2), `lister new RAM:` (rx rc 10, rig alive), then `lister new 'RAM:'` -> dead again. So with 6033146d + flat on, `lister new 'RAM:'` freezes in 2 of 2 runs here, while agent 3 saw 0 of 2 (same commit) and a no-flat control did not freeze. Not yet isolated: no flat-off control of this exact command was run in this agent (a reboot + control was denied by the permission layer, budget). The earlier "transient" verdict is therefore doubtful; first job next: reboot, flat-off, `lister new 'RAM:'`; then flat-on with `wasabi guru`/`snoop` armed.
- R.2 (hires side 2, AGA): NOT RUN (rig frozen before `rig.py aga`).
- Toolbar rows 28/40 + 2 px separators on Ram Disk, flat on vs off, source verdict: NOT RUN (lister never opened; the Ram Disk lister is the command that freezes).
- Environment rings (O7): NOT RUN.
- Rig 68 state: fs-uae alive but wasabid unresponsive, `rig.py hold --hours 2` active, not stopped.

## Freeze root cause (s70 agent 6)
- Verdict: flat NOT shown to be the cause; the freeze did NOT reproduce. 0 freezes in 8 runs (1 flat off, 7 flat on) with RAM:amarchy-flat = worktree build 6033146d (222152 bytes, 08:41), rig 68 purpose "s70 session: DOpus lister freeze root cause (flat on vs off)", flat-starting deleted each boot, boot flat (Process 13) Broken, STATUS flat=absent before every Run.
- Flat OFF control (boot 1): `rx "ADDRESS DOPUS.1; lister new 'RAM:'"` rc 0, Ram Disk lister opened, Echo alive at +5 s and +30 s.
- Flat ON (boot 1, flat=on calls=7 before): the same command 5x in a row, each rc 0 and alive; then agent 4 run 2's exact sequence (refresh all, command DeviceList NEW, `lister new RAM:`, `lister new 'RAM:'`) all alive. STATUS after: flat=on calls=104, dopus calls=16 flat=16, face=11 d5=6 (d5 path ran).
- Flat ON cold (boot 2, agent 4 run 1 exactly: reboot, Break, Run, `lister new 'RAM:'` as first DOpus command within ~2 s): rc 0, alive at +3 s and +20 s; STATUS flat=on calls=113, d5=4.
- Note: the unquoted `lister new RAM:` rc 10 is an ARexx syntax error (RAM: parsed as a label) and never reaches DOpus; the quoted form sends "LISTER NEW RAM:". So the unquoted run is not a DOpus control.
- No hang PC: nothing hung, so no trap/map was possible. Debugger route if it recurs: `rig.py start --debugger` (+ fs-uae debug build) and `rig.py dbgbreak` for the PC.
- Source review of the branch diff (merge-base 6d3df261): every new gadget-chain walk in flat.c is bounded (n < 64) except hprop_rows/scroller_pen/sring_note (`for (g = w->FirstGadget; g; ...)`, flat.c:1446, 1488, 1735), which hang only on a cyclic chain; img_note_new tag walk bounded by guard 64; Forbid sections are short table ops. No branch code path found that can spin.
- Also: "fs-uae 100% CPU" is the normal idle reading on this rig (90-112% measured while healthy), not evidence of a hang.
- Proposed: no flat fix. Re-check agent 4's conditions (host load, foreign clients on port 1301, other sessions' commands on rig 68 at 09:05-09:33) if it recurs; hardening only: bound the three unbounded gadget walks to 64 like the rest (consistency, not a proven fix).
- Rig 68 left up, alive, worktree flat running, `rig.py hold --hours 2`.

## R.2, toolbar, Environment (s70 agent 7)
- Rig 68, RAM:amarchy-flat = worktree build 6033146d (222152 bytes), purpose "s70 session: DOpus R.2 hires, toolbar and Environment checks". Start state: flat=on calls=149, d5=4, RTG 800x600 square.
- Toolbar (check 2): MEASURED, verdict BRUSH, not flat's ring. Second lister (Ram Disk, window x209 y14), toolbar buttons screen rows 28..40, x453..651. Flat on: row 28 a9b1d6 (button top), row 40 414868 (button bottom), 2 px separators 414868+a9b1d6 at x553/554, 573/574, 593/594, 633/634. No 7aa2f7 full-width line exists in this build (the 7aa2f7 lines the caller quoted were not present). Flat off (Break process 9, STATUS flat=absent, dopus refresh all): rows 28 and 40 and all separators identical pixel for pixel; only the right margin x635..650 changed (1a1b26 flat on -> 7aa2f7 off) and rows 26/41. => lines are brush pixels. The front lister (Ram Disk, x0 y0) showed no toolbar at all.
- Flat re-run after the off measurement: FREEZE. Last commands: `Delete ENVARC:Amarchy/flat-starting ENV:Amarchy/flat-starting QUIET` then `Run >NIL: RAM:amarchy-flat` via the EXEC request, about 09:44 (wasabid dead at 09:47, requester-watch read_ui timed out). Note this is a Run right after a Break + refresh all, the same pattern as agent 2's freeze.
- Reboot once (09:50, `rig.py reboot --purpose ...`): rig came back, ping OK. Then `rig.py aga --purpose ...` (09:55): rig rebooted, boot.log ends "wasabid started", but wasabid never answered ping through 10:00 (5+ min), fs-uae alive 105% CPU, no CPU-reset lines, no serial text. Cause unknown (AGA boot of wasabid, or the same freeze). Second reboot not attempted (hard rule: once).
- R.2 (hires AGA, side 2): NOT MEASURED. Environment rings (O7): NOT MEASURED. Rig 68 left in AGA mode (ScreenMode prefs = AGA), wasabid unresponsive, `rig.py hold --hours 2` set, never stopped. Next session: reboot, check wasabid answers, then flat run from RAM: (copy worktree build; reboot copies main's), `rig.py rtg` if AGA does not come up.

## Freeze root cause (s71 agent 1)
- Verdict: NOT FOUND on the rig (0 repro attempts: rig 68 did not answer after the one allowed reboot). Source: the brief's leading hypothesis (second copy starting while the first unloads) is DISPROVED as a double-patch/deadlock; one real lifetime defect remains that fits the "Run soon after Break" pattern: LIKELY, unverified.
- Hypothesis as stated, disproved by source (worktree flat.c): main() takes the instance guard under Forbid (flat.c:2664-2671): record "Amarchy.Flat" present and task_alive -> "already running", rc 5, nothing patched. The exiting copy drops the record (drop_record, flat.c:2796, 2368-2381) only AFTER dopus_unpatch, tx_remove, rf_remove, hook_remove all returned REMOVED (flat.c:2760-2775). So a Run during the unload either exits rc 5 or starts after every slot is restored; no double patch, no Forbid/Wait cycle between the two copies. Also: agent 7's freeze Run came minutes after STATUS flat=absent (measurements in between), not during an unload.
- Violated invariant (candidate): code and data reachable from a patched entry must outlive the last caller inside it. The unload uses a fixed sleep instead: tx_remove Delay(5) (flat.c:1387), rf_remove Delay(5) (flat.c:1914), hook_remove Delay(5) (flat.c:2236), dopus_unpatch Delay(2) (flat.c:2261), then FreeVec(r) (flat.c:2380) and the CLI UnLoadSegs the program. But no entry tail-jumps: flat_tx.s / flat_rf.s `jsr _flat_tx_c/_flat_rf_c` and the C bodies call the stock routine and return through our code (flat_tx_c flat.c:1340-1355 does SetAPen/SetBPen after the stock Text); c2_disp_*/c8/d5_disp_reg (flat.c:670-686) call the saved dispatcher and return through ours; the DOpus body patches run the copied prologue from r->dtramp[] INSIDE the freed record (flat_body_install, dopus_scan flat.c:2310-2320). A caller blocked in stock Text/RectFill/DrawBox (layer lock, blitter, starved by priority) longer than 40-100 ms returns into a freed segment / runs a freed trampoline. Benign while the freed memory is untouched (why it "works" most runs); a Run of a same-size binary right after reuses the first-fit block (LoadSeg reads unrelocated hunks, then relocates; data/BSS re-initialised: g_r = 0) -> wild jump or NULL g_r -> freeze. UNVERIFIED: no straggler was observed; why a caller would be parked minutes (agent 7) is not explained.
- Branch vs main: the Delay-based unload and the jsr stubs are on main bba0f55a identically (main flat.c Delay(5) at 1350, 1877, 2179, Delay(2) at 2202; flat_tx.s/flat_rf.s jsr; dtramp in r at 2260). The branch only adds one more such entry: the dopusiclass h_Entry hook d5_disp_reg (flat.c:670-674, installed dopus_scan ~2336-2345, removed in dopus_unpatch under the same Delay(2)). Not branch-specific.
- Repro: NOT RUN. Rig 68 `rig.py reboot` 10:20: fs-uae up (pid 95864), serial reached the BOOTX prompt, ScreenMode prefs native AGA (DisplayID 0x00029000, retry.log), but wasabid never answered (auth timeout 150 s, then connect timeouts). Same state agent 7 saw after `rig.py aga`: wasabid silent on the AGA boot is reproducible WITHOUT any flat restart, so the AGA-boot wasabid silence is a separate fault (unverified whether it is flat BOOT on AGA or wasabid). `rig.py rtg` needs wasabid (ami.req Copy) plus a stop/start, so it was not possible under the one-reboot rule. Freeze count: worktree 0 of 0, main 0 of 0.
- Proposed fix (not done): replace the sleeps with a real quiescence count. Every entry (flat_tx.s, flat_rf.s, c2/c8/d5 dispatchers, d4 replacements) increments a counter in a block that is NEVER freed (one small AllocMem'd public "Amarchy.Flat.stubs" block holding the counter and the tiny entry stubs + DOpus trampolines, reused by the next start), decrements after the stock call returns. Unload: restore slots under Forbid, then wait until the counter is 0 (Delay(1) loop, bounded with "busy, not unloading" like the still_chained paths), then free r. The stubs' last instructions (subq; rts) live in the permanent block, so no window remains. Root, not workaround: it enforces the lifetime invariant instead of guessing a caller's duration.
- Regression test (proposed): tests/test_flat_core.c, pure model in flat_core.c: flat_quiesce_enter/leave/ready on a counter; assert (a) after flat_swap_remove with one caller entered, flat_unload_ready() is 0 and stays 0 until flat_quiesce_leave; (b) ready is 1 with no callers; (c) the main-loop decision returns BUSY (not unload) while in flight. Fails before: flat_core has no in-flight state, the unload decision today is unconditional after REMOVED.
- First job next (needs a working rig): RTG boot (`rig.py rtg` on a rig whose wasabid answers), then decisive check: Break with a DOpus lister mid-redraw (loop `dopus refresh all` in a background ARexx) + immediate Run of the SAME binary, vs the same with a 2 s Delay between Break and Run of a DIFFERENT-size binary (predicted: no freeze, freed block not reused at the same place). If the count says straggler: `rig.py dbgbreak` on the frozen rig for the PC (outside flat's new seglist = dangling).
- Rig 68 end state: up, wasabid NOT answering, `rig.py hold --hours 2` set 10:27, never stopped. Scratch: reboot1.log (<10 KB) left in the session scratchpad.

## Gadget walk caps (s71 agent 4)

- amarchy/flat.c: hprop_rows, scroller_pen, sring_note walks now `for (g = w->FirstGadget, n = 0; g && n < 64; g = g->NextGadget, n++)` (sring_note uses n2); matching the existing pattern (literal 64, counter n/i, no shared constant).
- Walks over NextGadget in amarchy/*.c: 11 total, all in flat.c (145, 605, 702, 729, 745, 787, 804, 1280, 1446, 1488, 1735). 11 bounded, 0 unbounded after the change (3 were unbounded before).
- Regression test: none. The three functions are static in flat.c (m68k-only, uses struct Window/Gadget and Intuition), not in flat_core.c. A host test would need flat.c's gadget walks extracted into flat_core with a host-visible gadget struct; not invented here.
- Build: make amarchy-flat OK, build/amiga/amarchy-flat 222192 bytes. Host: make test ONLY=flat_core -> 4185 checks, 0 failed.
- Commit: 87eb263b on dopus-title-flat (not pushed).

## Rig 68 recovery (s71 agent 3)

- `rig.py aga|aga256|rtg` = `screenmode()` (tools/rig/rig.py:2128-2145): asks wasabid to `Copy VTC:screenmode.<mode>.prefs ENVARC:Sys/screenmode.prefs`, writes the choice into build/rig68/screenmode.name, then stop + `start` (a fresh clone, which re-applies the mode via `sys_hdf_screenmode`, :1276, called only in the fresh-clone branch at :2391).
- Host files: build/rig68/screenmode.name (was `aga`), build/rig68/vtc/screenmode.{aga16,aga256,rtg,rtg360}.prefs (DisplayID 0x00029000 vs 0x50041303, all present), and ENVARC:Sys/screenmode.prefs inside build/rig68/sys.hdf (Prefs/Env-Archive/Sys/screenmode.prefs, dated 09:52 = the `aga` run).
- `rig.py reboot` uses `start --keep-sys` (:2160, :2381-2388): sys.hdf is kept as left, `sys_hdf_screenmode` is NOT called, so the AGA prefs stay inside the image. A plain reboot could never restore RTG; screenmode.name alone does nothing on a keep-sys boot.
- Why wasabid is silent in AGA: NOT FOUND. boot.log of the AGA boot reached "wasabid started" (boot/boot.log); the emulator ran, the agent never answered. Not investigated further (RTG boot answers).
- Host change: screenmode.name `aga` -> `rtg` (backup in scratchpad screenmode.name.bak); one reboot driven by a scratchpad wrapper (no tracked file edited) that called rig's own reboot() with stop() followed by `sys_hdf_screenmode(sys.hdf)` (rewrites ENVARC:Sys/screenmode.prefs from vtc/screenmode.rtg.prefs, while the emulator was stopped).
- Result: wasabid 0.11b2 answers ping; new fs-uae pid 31818; `amarchy-flat STATUS` flat=on calls=202; `hold --hours 2` set. Screen mode saved in prefs: RTG (not re-read from the screen).
- rig.py defects: (1) `reboot`/`start --keep-sys` after `aga`/`rtg` never rewrites the mode, and `screenmode()` copies the prefs only live; a screenmode.name change plus reboot is silently ignored (fix: call sys_hdf_screenmode in the keep-sys branch at :2381). (2) `screenmode()` hangs/dies when wasabid is down, so aga has no escape route; a host-side `rig.py rtg` that writes sys.hdf while stopped would be one.

## Environment rings and toolbar margin (s71 agent 5)

- Build: worktree vtcon-dopus-title-flat HEAD 87eb263b, build/amiga/amarchy-flat 222192 bytes (10:32, same size as HEAD build), copied to RAM:amarchy-flat. RTG 800x600 square, rig 68, purpose "s71 session: DOpus Environment rings and toolbar margin check".
- Flat restarts: (1) 10:36:56 Break of boot flat (UP-Term:bin/amarchy-flat, proc 13), absent at 10:37:00, +6 s wait, delete flat-starting, Run at 10:37:11, on at 10:37:19. (2) Off-measurement: Break 10:44:12, absent, measured 10:44:26, Run 10:44:36, flat=on calls=7. No freeze; wasabid answered throughout; lister new RAM: and ARexx fine 8 s after Run.
- Environment window opened with `ami.py ui menushort "Ram Disk" 4` (right-Amiga 4; ARexx `command Environment` does NOT open it, it raised an error requester that was clicked away). Window "Environment" at x174 y183 452x234, flat on.
- Q1 (plan O7 = Environment buttons and fields keep their rings, rig 63 decision; plan states no colour number): MEASURED ring present on every class-drawn button. Button 21 (x352..451, y396..412): x352 and x451 at y404 = 7aa2f7; y393/y396/y412 at x400 = 7aa2f7 (396 and 412 are the button top and bottom); interior 1a1b26; label pixel a9b1d6 (y408). Button 214 (x180..279): x180 and x279 at y404 = 7aa2f7, y396/y412 = 7aa2f7. Cycle/check gadget 218 (x372, y204 top) 7aa2f7. Ring colour 7aa2f7 = pen_line, the same colour as the toolbar separators/lines. Verdict: PASS for "ring stays" (1 px ring, pen_line); no stated colour expectation, so the colour is reported only.
- Q2 (lister Ram Disk window x209 y14, toolbar rows 28..40): flat on, rows 28/34/40, x634..650 = 1a1b26 on all three rows and col x640 rows 28..30 = 1a1b26; row 28 and 40 are 7aa2f7 only up to x633, x633 at y34 = 7aa2f7 (last separator). Flat absent (Break, 10:44:26): identical values, 1a1b26. The s71-agent-7 reading "7aa2f7 with flat off" did NOT reproduce (toolbar bank on this boot differs: row 28 now 7aa2f7, not a9b1d6).
- Path: no flat code paints this margin. It is not a DrawBox rectangle (no ring at the margin edges in either state) and the only flat fills are c2_fill (amarchy/flat.c:94-98) for DrawBox/FieldBox/Bevel rings (d4_flat_pens, flat.c:920-936, flat_dopus_box_draw flat_glyph.c:529-531) and the window outer-edge RectFill patch (flat.c:1394+); neither reaches x635..650 inside the lister. The fill is DOpus's own toolbar clear (no DOpus source in the repo, so its line is NOT MEASURED).
- Verdict Q2: CORRECT (bar/window background 1a1b26 equals the button interiors; unchanged by flat; the plan (dopus-title-gadgets-flat.md) states no expectation for it, only ring/face rules at its lines 276-315, 340-353).
- Rig end state: up, RTG, flat from RAM: (worktree 87eb263b) flat=on, Environment window closed, 2 listers open (Ram Disk x0 y0 and x209 y14), hold --hours 2 set at 10:46. No screenshot files taken (pixels read in memory via ami.grab), none to delete; scratch left: px.py, o7.txt, q2.txt, q2off.txt (<10 KB) in the session scratchpad.

## Flat restart stress test (s71 agent 6)
- Result: INCOMPLETE. Worktree build 1 freeze in 1 cycle (probe, not a valid cycle count); main build 0 of 0. The 30-cycle run did not restart anything.
- Scripts (scratchpad 816a805a.../scratchpad): traffic.rexx (loop: `lister new 'RAM:'`, 2x `dopus refresh all`, `lister close`, stop flag RAM:T/stop; ~hundreds of iterations per 20 s, ran fine) and cycle.rexx (Status FULL -> Break N C -> Delete flat-starting -> `Run >NIL:` + STATUS poll, retry count logged to RAM:T/"cyc- wt.log").
- Freeze 1: ~10:52, the probe `ami.py exec "rx RAM:T/cycle.rexx 1 RAM:amarchy-flat t0"` (foreground, 1 cycle, traffic running, the 222192-byte build from RAM: running at that time) -> wasabid silent >60 s. One `rig.py reboot` (10:53:46, up 10:56:00). Not attributable with certainty: a foreground rx under wasabid exec also hung the exec channel.
- After reboot the rig started UP-Term:bin/amarchy-flat (220432 bytes = main build) as process 13 (stk 32768, pri 0). All 30 "cycles" in log reported runs=1 ok=1 with calls rising: Break 13 C had NO effect (process stayed, tried 4 times incl. traffic stopped, 15 s wait). Flat refuses to unload (flat.c:2761-2785 prints "still chained" to NIL, not visible). So the restart could not be exercised; retries min/max 1/1 are meaningless. The worktree binary in RAM: was never the running copy.
- Rig 68 end state: up, main-build flat=on (not worktree), traffic stopped and files deleted, one lister opened, hold 2 h. Skipped the main-build 15 cycles.

## Flat Break refusal and stress test (s71 agent 7b)
- Question 1: NOT FOUND (refusal text never captured). Question 2: not run (flat does not unload reliably; single reboot used).
- Rig 68 was silent (wasabid down) at start; the one `rig.py reboot` (11:12) recovered it (RTG, wasabid 0.11b2). The reboot also showed the earlier freeze was not a flat stress freeze: the prior agent left it up, it went silent on its own.
- Status list on the fresh boot: process 13 = UP-Term:bin/amarchy-flat (220432 bytes, main build), so the Break target number was right.
- Boot copy, young, no DOpus traffic: `Break 13 C` unloaded it at once (STATUS flat=absent). So the refusal is not boot-specific; it needs prior state.
- Worktree copy (222192 bytes, RAM:, `Run >RAM:flat.log`, process 12) + detached rx traffic (lister new/close + 2x dopus refresh all, ~560 iterations/40 s): `Break 12 C` (also after traffic stopped, and 8 more times) -> process stayed, flat=on. RAM:flat.log stayed empty even after `Break 12 D` (which must print STATUS), so the log is not a usable channel: PutStr output of a running copy stays buffered (a second copy's "already running" line and STATUS do reach a file because that process exits). Refusal line therefore unobserved.
- STATUS after traffic (worktree): `flat=on calls=8242 chained=0`; `dopus=73.8 patched=7 calls=37 flat=0 sums=780fd46b,ba2cf172,bdaa116a`; `dopus bevel calls=0 face=0 d5=24`; `rectfill installed=yes calls=19732 hits=108 ...`; `text installed=yes calls=256 ... ctask=dopus_clock`; `frame=hooked calls=3`. Fields (flat.c:1957-2060): d5 = r->d5draws, draws of the dopusiclass h_Entry hook (flat.c:666); patched = installed DOpus swaps; chained= is the foreign-chain counter. patched stays 7 after Break, text/rectfill still installed -> consistent with the first refusal gate (d4_installed true, flat.c:2757-2760: "a patch is still chained on a DOpus entry, not unloading") but also with Break not being handled; UNVERIFIED which.
- Pattern: Break worked before DOpus traffic (boot), failed after lister new/close + refresh traffic. Leading suspect: a foreign patch/hook landed on a DOpus entry or dopusiclass during lister creation (flat_swap_remove returns STILL_CHAINED when the slot is no longer ours). Next step: make refusal observable (Flush after PutStr in the Break paths, or write the refusal into STATUS), then Break.
- Rig end state: after the last Break series wasabid went silent again (11:19, ping timeout); no second reboot allowed, stopped. Worktree flat (222192) was still running as process 12 when last seen; RAM:T/traffic.rexx, traffic.cnt, stop flag, `RAM:T/cyc- wt.log`, RAM:flat.log, RAM:amarchy-flat could NOT be deleted (rig silent). Hold set from host (see report).

## Flat unload after lister traffic: root cause (s71 agent 8)

**Case A** (main loop stuck; Break never reached). Rig 68, RAM:amarchy-flat built from 417c8970, DOpus 73.8.
- Before traffic: `loop beat=16 stage=wait refused=none refusals=0`, 3 s later `loop beat=21 stage=wait ...`; `dopus=73.8 patched=7 calls=0 flat=0 sums=780fd46b,ba2cf172,bdaa116a`.
- Traffic (detached rx loop: lister new RAM:, 2x refresh all, lister close), polls at counts 200/460/680/940: every poll `loop beat=33 stage=pens refused=none refusals=0`; dopus flat calls kept rising 2286 -> 4349 -> 6079 -> 8174. Beat froze within ~12 s of traffic start.

**Cause: NOT FOUND (narrowed).** The main task stops inside pens_run (amarchy/flat.c, stage PENS: after ipd_lock_wb_screen succeeded, before draw_sync). Calls in that window: ipd_epoch (Forbid), aspect_update (flat.c ~1557: LockIBase(0) + GetVPModeID), flat_pens_verify -> ipd_pen_is_free (iconpen_dos.c:60, ObtainPen/ReleasePen = WB ColorMap PalExtra semaphore), flat_pens_follow/refresh (ObtainBestPen/ReleasePen/GetRGB32), UnlockPubScreen. Ruled out: a Ctrl-C refusal path (refused=none), a stuck Wait (stage is not wait). Intuition and the DOpus patches keep running, so the wait is on a lock some other task holds long-term (PalExtra semaphore or IBase most likely), or a spin in flat_pens_*. Lock-order hazard noted: c7_screen_which (flat.c ~141) takes LockIBase inside a draw hook while the drawer holds layer locks; pens_run takes IBase (aspect_update) while holding a pub-screen lock.

**Freeze mechanism: likely, not proven.** New fact: the rig froze (wasabid silent, rig.py status `down`) with traffic running and NO Break sent after traffic. So the freeze is not caused by Breaks; it follows the stuck main task (holding the WB pub-screen visitor lock and whatever lock it took) plus continued lister traffic.

**Commits** (branch dopus-title-flat):
- 417c8970 flat: keep a main-loop heartbeat, stage and Ctrl-C refusal in the shared record and print them in STATUS (FLAT-LOOP-BEAT).
- a571cb2f flat: STATUS from a second copy names the semaphore the resident task waits on (FLAT-STUCK-DIAG: tc_State, tc_SigWait, waited semaphore from SemaphoreList / PalExtra / LayerInfo / layer locks, owner, nest).

**Test added:** tests/test_flat_core.c test_status checks flat_loop_line (`[OK] flat_core: 4191 checks, 0 failed`). FLAT-STUCK-DIAG is Amiga-only, no host test; it ran on the rig against a non-stuck task and printed `stuck state=4 sigwait=C0003000 sem=00000000` (no semaphore wait, as expected for a task in Wait).

**Rig proof:** Break C after traffic: NOT TESTED (rig froze before the first Break). 2 of 2 reboots used.

**Rig end state:** rig 68 up after reboot, boot copy UP-Term:bin/amarchy-flat (process 13, pre-417c8970 build: its record lacks the beat fields, STATUS shows `stage=bad`), DOpus running, RAM:flatdiag = a571cb2f build. Traffic files gone with the reboot. `rig.py hold --hours 2` set. Lister count not checked.

**Open items:**
1. Reproduce with the a571cb2f build resident (Break 13 C, Run RAM:flatdiag), start traffic, and run a second copy's STATUS as soon as `stage=pens` holds (before the rig freezes, ~1-2 min) to read `stuck ... sem=... owner=...`. If sem=0 and state=4 with sigwait bit 4 (SIGF_SEMAPHORE) set, the lock is private: IBase (aspect_update) is the next suspect.
2. Add finer sub-stages inside pens_run (aspect, verify, follow, refresh) so the line pins the call without the semaphore scan.
3. Install the new build into UP-Term:bin so the boot copy carries the beat fields.

## Stuck lock capture (s71 agent 9)

Rig 68, RAM:amarchy-flat = a571cb2f binary (224240 bytes, same as host build/amiga/amarchy-flat), DOpus 73.8. Boot copy broken first, flat started from RAM, beat moved (3, 5; stage=wait). Sampler (Execute script, every 2 s, >> to UP-Term:s71-stuck.log) then traffic (lister new RAM:, 2x refresh all, close; 1660 cycles). No Break sent. Log copy: /private/tmp/claude-501/-Users-spot-Code-UP-Term/816a805a-c913-43d4-8f63-a8cac4e09207/scratchpad/s9final.log.

Last moving sample (beat 15, stage=wait) and first stopped samples:
```
loop beat=15 stage=wait refused=none refusals=0
stuck state=4 sigwait=C0003000 sem=00000000  owner=00000000  nest=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=73.8 patched=7 calls=0 flat=0 sums=780fd46b,ba2cf172,bdaa116a
loop beat=17 stage=pens refused=none refusals=0
stuck state=4 sigwait=00000010 sem=00000000  owner=00000000  nest=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=73.8 patched=7 calls=8 flat=8 sums=780fd46b,ba2cf172,bdaa116a
```

Direct STATUS after traffic stop (stuck for good, beat still 17, flat calls 13457):
```
flat=on calls=13457 chained=0
loop beat=17 stage=pens refused=none refusals=0
stuck state=4 sigwait=00000010 sem=00000000  owner=00000000  nest=0
```

Process list at the first stopped sample: 1 DO_LAUNCHER, 2/4 wasabid, 3 Status, 5 SolidWin, 6/7 upgetty, 8 Amarchy, 9 RAM:amarchy-flat, 10 amarchy-gui, 11 DirectoryOpus, 12 amarchy-menu, 13 Status. (Same 12 processes after traffic stop.)

**Result:** stuck line on every stopped sample: state=4 (TS_WAIT) sigwait=00000010 (SIGF_SEMAPHORE only) sem=00000000 owner=00000000. The task waits on a semaphore that is NOT in the public SemaphoreList, any screen's PalExtra, LayerInfo or layer locks: a private semaphore (not IBase-in-list candidates scanned). Owner not identifiable with this tool; no further digging per brief. Rig did not freeze during the ~3 min observed (sampler still wrote; traffic stopped by flag after ~2.5 min). Reboots used: 0 of 1.

End state: sampler and traffic stopped, RAM:T files and UP-Term:s71-stuck.log deleted, RAM:amarchy-flat (a571cb2f) still resident but STUCK (beat 17, stage=pens), boot copy UP-Term:bin/amarchy-flat is gone as process (broken, old build), rig.py hold --hours 2 set.

## Waiter chain (s71 agent 10)

Commit 72aa92f9 on dopus-title-flat (amarchy/flat.c: stuck_sem stack scan for SemaphoreRequest + chain print in print_stuck; `make test ONLY=flat_core` 4191 checks 0 failed; the task brief said SIGF_SEMAPHORE, the NDK header only has SIGF_SINGLE = 0x10, used that).
Rig 68, 1 reboot (rig.py reboot, purpose "s71 session: flat waiter chain"). Boot copy (process 13) broken, STATUS flat=absent, RAM:amarchy-flat (72aa92f9 build, 225364 bytes) started detached, beat moved (4, 7, stage=wait). Detached Execute loop: rx lister new 'RAM:', 2x dopus refresh all, lister close. Beat froze at 31 stage=pens within ~2 s of the first sample; 30 samples at 2 s all beat=31.

Chain, STATUS from a second copy (last capture, calls=8136):
```
loop beat=31 stage=pens refused=none refusals=0
chain 1 task=10472CC8 Background CLI state=4 sigwait=00000010 sem=1000B86E private owner=10E025B8 dopus_lister nest=1 queue=973
stuck state=4 sigwait=00000010 sem=1000B86E private owner=10E025B8 dopus_lister nest=1
chain 2 task=10E025B8 dopus_lister state=4 sigwait=00000010 sem=1000B8CA private owner=10E22708 dopus_clock nest=1 queue=2
chain 3 task=10E22708 dopus_clock state=4 sigwait=00000010 sem=1000B86E private owner=10E025B8 dopus_lister nest=1 queue=973
cycle
```
Earlier captures (samples 28, 29, 30) show the identical chain; only queue on 1000B86E grew (821, 842, 861, 973) while Execute traffic kept adding waiters.
Task list (Status FULL, same moment): 1 DO_LAUNCHER, 2/4 wasabid, 3 Status, 5 SolidWin, 6/7 upgetty, 8 Amarchy, 9 RAM:amarchy-flat (stk 16384, pri 1), 10 amarchy-gui, 11 amarchy-menu, 12 DOpus5:DirectoryOpus, 13 rx. Task 10472CC8 "Background CLI" is the flat main task (process 9; its tc_Node name is the CLI default). dopus_lister and dopus_clock are DOpus-internal tasks (not CLI processes).

Reading: the flat main task waits on private semaphore 1000B86E, owned by dopus_lister, which itself waits on private semaphore 1000B8CA, owned by dopus_clock, which waits on 1000B86E: a two-task deadlock cycle inside Directory Opus (lister <-> clock). Flat is only a victim queued behind it (it is not in the cycle). 1000B86E is not inside the IntuitionBase or GfxBase library area (printed "private", so NOT the IBase lock; the stack-scan classification checked ibase+/gfx+ ranges).
Measured: the chain, cycle, task list, beat freeze. NOT measured: what DOpus code takes each semaphore (identity of 1000B86E / 1000B8CA by name), whether the cycle forms without flat's DOpus patches active (flat=0 dopus calls=58 only), what flat's pens_run call hit 1000B86E (stage=pens; the main task is queued on it from inside pens_run or its callees, call not pinned).
End state: rig 68 up, stuck, hold 2 h. RAM:amarchy-flat still resident (needed to keep the state); RAM:s10stop flag left so the traffic loop exits when its rx returns; RAM:s10traffic deleted. Reboots used: 1 of 2. Scratch: $S/traffic.script, ax.sh, st*.txt, cap3.txt, tasks.txt (all small).

## Freeze fix proof (s71 agent 11)

Result: FAIL, different failure. The machine crashes into a CPU reset loop within ~3 s of lister traffic start, twice in a row (reproducible). Not the old freeze (no stuck/chain/cycle line captured), but the proof did not complete.
- Build: worktree HEAD d83e72ea, build/amiga/amarchy-flat 225504 bytes, md5 0c4ff16c151fbef2388517ab452d1bcc, pushed to RAM:amarchy-flat (List shows 225504). STATUS prints no version/hash line.
- Run 1 (after rig.py reboot): boot copy broken (Break 13 C), flat=absent, new build Run, STATUS `flat=on calls=7 / loop beat=3 stage=wait`, then `calls=13 / beat=7 stage=wait`. Detached `Run >NIL: Execute RAM:a11traffic` (rx lister new RAM:, 2x dopus refresh all, lister close, loop). First poll at 12:27:12 already ConnectionRefused; watchdog.log 12:27:23: reset loop (10+ CPU resets in 60 s), first fault: no CPU exception logged (ROM reset).
- Run 2 (rig restarted by the watchdog; same procedure): traffic start 12:28:42; STATUS at 12:28:43 `flat=on calls=261 chained=0 / loop beat=9 stage=wait refused=none refusals=0 / stuck state=4 sigwait=C0003000 sem=00000000` (a wait for signals, no semaphore). Next poll 12:28:45 and every poll to 12:29:10: ConnectionRefused. watchdog.log 12:29:09: reset loop, first fault `Illegal instruction: 01ff at 000127CC -> 00F80AD2`. Logs: /Users/spot/Code/UP-Term/vtcon/build/rig68/reset-loop-20261110-122723.fs-uae.log and reset-loop-20261110-122909.fs-uae.log.
- Beats: first 3, middle 7/9, last 9; cycles reached: about 0 to a few (traffic ran under 3 s, count file not reached). No beat freeze: the guest died instead. Not measured: which code jumped to 0x127CC (not dug into per brief).
- Break C: not tested (rig reset before it). Glyph (depth gadget flat style): not checked, no screenshot taken (rig was gone before the shot).
- Rig 68 end state: up after the watchdog restart, boot copy UP-Term:bin/amarchy-flat (old build) running as process 13, RAM: wiped, no traffic running. Reboots by me: 1 (rig.py reboot); second restart was the watchdog's.
- Scratch left (small): scratchpad a11*.sh, a11traffic.script, a11poll.log, a11-st*.txt.

## Crash bisect (s71 agent 12)

Question: is the reset-loop crash (a) the lister traffic alone, (b) any amarchy-flat build, or (c) only the newest commits?

Traffic: agent 11's RAM:a11traffic (Lab loop; rx "ADDRESS DOPUS.1; lister new 'RAM:'"; dopus refresh all x2; lister close; echo to RAM:a11cnt; Skip back), started with `Run >NIL: Execute RAM:a11traffic`. It does NOT differ from agents 9/10 in the rx commands (same four rx calls per cycle); only the stop-flag name and the counter line differ, and 9/10 ran it differently wrapped (detached Execute loop, same body).
Procedure each run: `rig.py reboot --purpose "s71 session: crash bisect"`, Break boot copy (process 13) where stated, traffic, poll every ~1.5 s for 40 s (scratch: scratchpad/bis-run.sh, bis-R*.log).

| Run | Build / flat state | Verdict | Key line | Time |
|---|---|---|---|---|
| R1 | no flat resident (boot copy Broken, STATUS flat=absent) | CRASHED at ~8 s | watchdog 12:33:05: reset loop, "no CPU exception logged (ROM reset)"; wasabid refused from t=9 | 12:32:45 start |
| R1b | same, repeat | CRASHED at ~1 s | watchdog 12:37:11: `Illegal instruction: 01ff at 000111BC -> 00F80AD2`; refused from t=1 | 12:36:34 start |
| R2 | boot copy of amarchy-flat as booted | CRASHED at ~2 s | last STATUS flat=on calls=953 beat=35 stage=wait; watchdog 12:35:30 reset loop (ROM reset) | 12:34:51 start |
| R3-R5 | not run: R1 did not survive (instructions) | - | - | - |

Answer: (a). The crash occurs with no amarchy-flat resident at all, so it is the lister traffic (DOpus rx lister new/refresh/close loop) on this rig, not flat and not the newest commits. Same illegal-instruction signature class (01ff, jump to garbage, ROM reset) as agent 11's 000127CC. Not measured: whether older rig images or other DOpus versions survive; why agents 9/10 saw hangs and not crashes (their rig state, not the build, may differ). No first-crashing commit: flat is not involved.

## Controlled freeze proof (s71 agent 13)

Result: PASS. Rig 68, same slowed traffic in all three runs, rig.py reboot before each run, boot copy (process 13) Broken first. Traffic RAM:ctraffic (Wait 1 between calls; started with Run >NIL: Execute RAM:ctraffic; counter RAM:c_cnt, 2 bytes per cycle):
```
Lab loop
IF EXISTS RAM:c_stop
  Skip end
EndIF
rx "ADDRESS DOPUS.1; lister new 'RAM:'"
Wait 1
rx "ADDRESS DOPUS.1; dopus refresh all"
Wait 1
rx "ADDRESS DOPUS.1; dopus refresh all"
Wait 1
rx "ADDRESS DOPUS.1; lister close"
Wait 1
echo "x" >>RAM:c_cnt
Skip back loop
Lab end
```
Variants tried: 1 of 3 (Wait 1 variant survived the control; c2 not needed). Old build = 72aa92f9 built in scratch worktree (225364 bytes, removed). Fixed = d83e72ea build/amiga/amarchy-flat md5 0c4ff16c151fbef2388517ab452d1bcc (not rebuilt).

| Run | Build | Verdict | First / last beat | Key lines |
|---|---|---|---|---|
| control | no flat (flat=absent) | PASS: rig answered 90 s (Version every 2 s), 21 cycles | - | c_cnt 42 bytes at 12:42:10; started ~12:40:45 |
| old | 72aa92f9 | HANG | beat 3 (12:44:30) / 22 (frozen from ~12:44:51 to 12:45:59, 90 s) | stage=pens, `chain 1 ... sem=1000B86E private owner=... dopus_lister`, queue grew 13 to 27, calls stuck at 2390; counter 44 bytes (22 cycles) |
| fixed | d83e72ea | PASS | beat 7 (12:47:53) / 93 (12:49:22); 115 at stop | stage=wait whole run, sigwait=C0003000 (signal wait, sem=0), no semaphore wait, no cycle line, calls 113 to 25386; 21 cycles in 90 s |

Break C on the fixed build after stopping traffic (STATUS before: flat=on calls=33786 beat=115 stage=wait): Break accepted, `RAM:amarchy-flat STATUS` prints flat=absent, no amarchy-flat process left. PASS.
Glyph (depth gadget flat): UNCLEAR. A screenshot was taken (800x600) but images may not be read in the main session and no subagent was allowed; no PIL on the host for a pixel check. Screenshot deleted. Needs an eye check.
Odd: the old build beat advanced 3 to 22 before freezing (~20 s), and it hung under the slowed traffic that the control survives, so the hang is flat-specific; the semaphore owner was dopus_lister in both old runs. Rig 68 left up, no flat resident, hold untouched.

## Glyph and Break check (s71 agent 15)

Rig 68, d83e72ea build (md5 0c4ff16c), rebooted first, boot copy Broken (process 13, gone), flat started from RAM: (process 12). STATUS after start: `flat=on calls=7 chained=0`. One lister opened with `lister new 'RAM:'`, 3 s wait, screenshot S1.
- A(a) screen title-bar depth gadget (top right): flat YES. Two-overlapping-windows glyph, no bevel. In S0 (no flat) and S2 (after unload) nothing is drawn there at all (blank title bar end), so the stock state shows no glyph; flat draws one.
- A(b) lister title-bar gadgets: front (new) lister: lock, depth/zoom glyphs flat YES (no bevel; no close gadget visible, only a small square at far left). Rear (older, inactive) lister: zoom gadget area is a blank dark block, depth glyph flat; identical in S0/S2 stock, so not changed by flat on that window (UNCLEAR whether that is wrong or stock behaviour).
- A(c) drawn wrong: the blank dark zoom block in the inactive rear lister title bar (about x=425-510 in a 4x zoom of x500-800, i.e. screen x~606-627, y~14). Otherwise no garbage.
- B: lister close via rx returned rc 10 (message "Returned 10"; lister state unchecked), Break C sent to process 12; first STATUS read: `flat=absent`; 0 amarchy-flat processes in Status. S2: screen depth glyph gone and gadgets as stock; rig answered. RAM:amarchy-flat deleted.
- Kept: S1 screenshot, because of A(c).

## Eye-check setup d588f8e9 (s71 agent 18)

Rig 68 rebooted (purpose "s71 session: owner eye check, flat DOpus title gadgets d588f8e9"). Binary: worktree vtcon-dopus-title-flat build/amiga/amarchy-flat, 225504 bytes, md5 0c4ff16c151fbef2388517ab452d1bcc (make -n: up to date for HEAD d588f8e9; the merge changed no flat input). Put to RAM:amarchy-flat, boot copy (process 13) Broken, new copy Run as process 12. STATUS: `flat=on calls=7 chained=0` / `loop beat=4 stage=wait`.
Two listers opened (RAM:, SYS:), front one not covering the rear fully. Traffic RAM:ctraffic (agent 13 cycle, Wait 1 between calls, lister new SYS:), 60 s plus stop via RAM:c_stop: 16 cycles (c_cnt 32 bytes); beat 17 before traffic, 101 then 106 after stop (stage=wait, calls 189 -> 21508). Both listers still open, traffic files deleted.
Screenshot (one, deleted): screen title-bar depth gadget flat YES (glyph at top right, no bevel); front lister title-bar gadgets (lock, minus, zoom, depth) flat YES.
