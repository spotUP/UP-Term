---
date: 2026-11-08
topic: KS 3.1 + DOpus rig boot crash (reset loop) isolation
status: partial - reproduced and windowed, NOT bisected, PC NOT mapped
head: 4e445b19
---

# KS 3.1 + DOpus rig boot crash

## Hypotheses tried (read this first; update it with every leg)

Crash: KS 3.1 only, at boot, about 2 in 3 boots, right after "UP-Term shadows done" while the Amarchy set starts.
Two failure forms: ROM reset loop (hundreds to thousands of resets) or silent hang. KS 3.2 is not affected.

| # | Hypothesis | Test (leg) | Result | Status |
|---|---|---|---|---|
| 1 | RectFill/Text patch overflows the stack budget | A2: budget fix in (flat 8f326587) | 2 FAIL / 1 OK | ruled out as sole cause |
| 2 | amarchy-index causes it | B: index block stripped | VOID (xdftool strip on kept sys.hdf) | not answered |
| 3 | amarchy-flat is involved at all | C: flat replaced by a 40-byte stub | 5/5 OK | CONFIRMED: flat is involved |
| 4 | Two amarchy-flat instances patch at once (start race) | G1-G6: single-instance guard fix (kept, test flat_single_instance) | 3 OK / 3 FAIL | ruled out |
| 5 | RectFill/Text patch bodies (icl_label, rf_*, pens) | PT: pass-through bodies | 2 FAIL / 2 OK | ruled out |
| 6 | One of the hooks installs | NH: no hook_install | 5/5 OK | CONFIRMED: a hook |
| 7 | Which hook | NV, NVC2, NVC2NC (clone off) | only clone off is 5/5 OK | CONFIRMED: sysiclass (c2) clone path |
| 8 | DOpus DrawBox patch, marker_write, frameiclass, RectFill/Text swaps | part 6 legs | not needed for the crash | ruled out |
| 9 | Clone path runs out of stack on a foreign task | SG: free stack measured at clone_render entry, refuse below 6144 | cmin 1052 bytes on IPrefs; 5/5 OK | likely; 5/5 alone is p about 0.13 |
| 10 | Fix for 9: clone render on a private 16 KB stack (StackSwap) | part 8 (5/5 KS 3.1 boots OK, KS 3.2 OK), part 9 (chw=16384 was a scan bug: KS 3.1 stamps 0xBAD1BAD3 at the stack bottom; real depth 1564-1632 bytes) | 5/5 + 2/2 OK | CONFIRMED: fix, merged s63 |
| 11 | Rig boot order: something in S:User-Startup starts before Workbench opens; output before WB gives a requester and WB never opens; P96 / AfA OS are timing-sensitive (owner, 2026-11-08, via the musicplayer session) | leg W: move the Amarchy set start (Amarchy, amarchy-index, amarchy-flat BOOT) from S:User-Startup to SYS:WB-Startup; check every pre-WB line for output; read P96 install scripts and AfA OS docs for start order | - | queued (10 done) |

Owner hints (2026-11-08, relayed): "something is started before the wb is opened ... it's a rig problem"; "the answer to
that issue is either in the p96 install docs/scripts or in afaos"; "if anything echoes output before the wb is loaded we
get a requester and the wb is not opened"; "stuff that should be opened after the wb has loaded should go into
system:WB-Startup/". Rig 2 (3.2, P96 800x600x32) freezes at "SetSwitch() - Picasso96 800x600x32", last boot.log line
"UP-Term shadows done"; Amarchy block off 4/4 OK (small sample). Notes: /Users/spot/Code/MusicPlayer/thoughts/spot/agent-reports/I1145.md runs 1-9.

Not run: D (pre-05678a64 flat), F (Stack 262144), install+rtg path, PC map of the reset. If 10 fails: next is leg P
(sysiclass objects and bitmap prebuilt in the main task), not a re-run of 1-8.

## Result in one paragraph

HEAD (4e445b19) puts a KS 3.1 40.68 + DOpus 5.82 rig (068060, `--max`) into a ROM
reset loop intermittently at boot. Reproduced 2 times out of 3 boots on rig 97. The
crash window is after `shadows-up` finishes ("UP-Term shadows done" is always the
last boot.log line) and inside `go`'s `C:Wait 15`, i.e. while the S:User-Startup kit
blocks `Run` `UP-Term:bin/Amarchy` and `UP-Term:bin/amarchy-index` (and Amarchy's
start gate starts amarchy-flat, amarchy-gui, amarchy-menu). The crashing binary is
NOT yet identified, and the PC is NOT mapped. Verdict below is a hypothesis.

## Reproduction

1. `UPTERM_RIG=97 python3 tools/rig/rig.py start --max --purpose "amarchy session: KS 3.1 boot crash isolation"`
   (default KS 3.1 ROM, system DOpus, fresh sys.hdf clone).
2. Watch `build/rig97/boot/boot.log` and `build/rig97/logs/fs-uae.log.txt`.
3. FAIL signature: boot.log stops at "UP-Term shadows done"; fs-uae log fills with
   `CPU reset PC=f80dc8` / `PC=f800d0` lines (546 to 1450 per run); the last non-reset
   line before the first reset is a serial `setbaud 9600` at PC f8361c (ROM).
   No "CPU halted" line in these runs (the earlier run in
   installer-secondary-pen-31.md showed `CPU halted: reason = 3 PC=10b7fd7e`).
4. `rig.py start`'s own retry (fresh re-clone) booted clean on run 1; `rig.py reboot`
   (KEEP_SYS) crashed again in the same window.

| Run | Path | Result |
|---|---|---|
| 1 | start, fresh clone | reset loop, 1450 resets, stop at "UP-Term shadows done" |
| 1 retry | start retry, fresh clone | boot OK: Amarchy, amarchy-flat (stack 32768), amarchy-gui, amarchy-menu, "Amarchy bar idle" (4096 stack, 3998 free), dopus all alive |
| 2 | reboot (KEEP_SYS) | reset loop, 546 resets, same window |
| 3 | reboot (KEEP_SYS) | reset loop, 1056 resets, same window (start's retry also ran, then rig stopped by me) |

Intermittent, not deterministic. Both a start race and "state written by the
previous good boot" fit 3 data points; the KEEP_SYS reboot crashing twice leans
toward state on DH0 OR timing, not decided.

## Bisect table

NOT done (budget). Note for whoever does it: "stock kit" on a rig is NOT an older
build. `sys_hdf_kit_upterm` (tools/rig/rig.py:922) writes the current build's
UP-Term: files into the clone and `refresh_kit_shadows` (rig.py:1974) copies the
current build into build/rigN/vtc on every start/reboot. So every binary on the rig
is HEAD. To bisect, shadow ONE older binary at a time (older amarchy-flat etc. exist
in ../vtcon-te6c-3, ../vtcon-te10-8, build/dist/up-term/Files) or disable one kit
block in S:User-Startup per boot:

| Leg | Change | Boots | Result |
|---|---|---|---|
| A | none (4e445b19 era build/amiga, flat e0f38ace) | 3 | 2 FAIL / 1 OK |
| A2 (s63) | c4eb3756, all five Amarchy-set binaries rebuilt at HEAD (flat 8f326587, budget fix in) | 3 (1 start, 2 reboot) | 2 FAIL / 1 OK: start = reset loop (28+ resets, 198 s); reboot 1 OK (Amarchy, flat, gui, menu up); reboot 2 = hang at "UP-Term shadows done", 0 resets, start's retry also stuck |
| B (s63 part 2) | no amarchy-index block (S:User-Startup block stripped with strip_user_startup_blocks on the kept sys.hdf, real flat 8f326587 back) | 1 attempted | VOID: start --keep-sys never reached BOOTX:go ("go not started" for 180 s, no boot.log); caused by the edited image or the KEEP_SYS path, not comparable to baseline; no verdict |
| C (s63 part 2) | amarchy-flat replaced by a no-op hunk exe (moveq #0,d0; rts, 40 bytes, md5 1284677f37a8f999edfb0350d0016858; VTC: and UP-Term:bin both 40 bytes on the rig) | 5 (1 start, 4 stop --keep + fresh start) | 5 OK / 0 FAIL: boot.log reached "wasabid started", 0 CPU resets, Amarchy, amarchy-gui, amarchy-menu, DO_LAUNCHER (DOpus) alive each time |
| D | amarchy-flat from pre-05678a64 | - | not run |
| E (s63 part 3) | amarchy-flat shadowed by the 40-byte stub for the boot (md5 1284677f); real flat (8f326587) put on the rig as VTC:amarchy-flat-real and started by hand 20 s after "wasabid started" with Amarchy, amarchy-gui, amarchy-menu alive: `Run >NIL: VTC:amarchy-flat-real` | 5 (1 start, 4 stop --keep + start) | 5 OK / 0 FAIL / 0 VOID: every boot reached "wasabid started", 0 CPU resets at boot, at +30 s and after opening a UP-Term window plus a DOpus lister try; ping answered; flat process alive; `STATUS` frame=hooked calls=0 flat=0 each time |
| F (s63 part 3) | as E with `Stack 262144` first | 0 | not run (tool-call budget used) |
| G1-G6 (s63 part 4) | FIXED amarchy-flat (md5 1bc4c04194e2be69682418cc701058db, main checkout, flat.c prefs read before Forbid guard, uncommitted), real boot via User-Startup + start gate, no stub, no shadow; rig 97 `start --max --idle-minutes 0`, then `stop --keep` + 6 s + fresh start per boot | 6 | 3 OK / 3 FAIL: G1 OK, G2 FAIL-hang (0 resets, boot.log ends "UP-Term shadows done", 200 s), G3 FAIL-reset (1524 resets), G4 OK, G5 FAIL-reset (1606 resets), G6 OK. On the 3 OK boots (+30 s after "wasabid started"): exactly 1 amarchy-flat process each (Status), `amarchy-flat STATUS` flat=on calls=35 chained=0, rectfill installed=yes calls=453/516 hits=20, dopus=68.16 patched=3 |
| E | install+rtg path | - | not run |

Each leg needs >= 4 boots because HEAD fails ~2 in 3.

## Crash address

Not mapped. Prior PC 10b7fd7e (installer-secondary-pen-31.md; its log is deleted)
lies in fast RAM, so it is inside a LoadSeg'd hunk, not ROM; "reason 3" is an
address-error/odd fetch class halt (unverified meaning). Mapping needs:
1. Load addresses on a crashing boot: wasabid starts before the kit blocks, so run
   `rig.py wasabi snoop start` with `--task 'amarchy*'` and LoadSeg seglists, or a
   debug KPrintF of each seglist start from the binaries (RULES: no shims in the fix).
   A Guru reset zeroes RAM on this rig (test-register row 125), so post-mortem reads fail.
2. Symbols: HEAD binaries are stripped vbcc hunk files (amarchy-flat code hunk
   0x31a38, Amarchy 0x5812c, amarchy-index 0x22d4c, amarchy-gui 0x7ef64). Relink with a
   vlink map (`-M`) from the Makefile rule (amarchy-flat: Makefile:1182-1187), then
   apply the s28 method (thoughts/shared/research/s28-evidence/rig8-crash-analysis.md).
   Scratch hunk parser with HUNK_DREL32 support was used and discarded.

## Verdict

Boot-time race / start-order fault in the Amarchy set start on KS 3.1, most likely
introduced with 317234ea (amarchy-flat started from the Amarchy start gate when a
flat switch is on), which makes amarchy-flat install its RectFill/Text/frame patches
while DOpus and amarchy-index start in parallel. Confidence: LOW-MEDIUM. Not proven:
no bisect leg, no mapped PC, no stack evidence. Stack overflow (RectFill/Text patch
budget class) is NOT ruled out; the cited flat-rf-tx-budget.md does not exist in this
checkout, so it could not be compared. A plain KS 3.1 API difference is less likely,
because the same binaries boot fine 1 in 3 times on the same ROM.

Other candidates in amarchy/ since 2026-11-06: 05678a64, c4844c4e, b7377f75,
1a0465c8 (DOpus DrawBox flatness by pen), 6850254c (stack probe knob), 2988e11c
(intuition patch gate), cae3a7c2 (restart-path stack).

## Proposed fix level

Unknown until the binary is named. If leg C confirms 317234ea: fix in Amarchy's start
gate (order/handshake so amarchy-flat installs its patches only after the set is up),
not in the patches. If the PC maps into flat_rf.s/flat_tx.s: patch-side stack budget
in amarchy-flat (owned by the other agent's worktree; do not edit here).

## Rig state

Rig 97 stopped, build/rig97 removed. My screenshot boot-freeze-155302.png deleted.
build/rig-reports/rig97/shots/boot-freeze-152540.png and -154624.png belong to the
earlier agent and were left.

## s63 update (2026-11-08)

- Budget fix c4eb3756 does NOT cure it: 2 of 3 boots FAIL with every Amarchy-set binary rebuilt at HEAD
  (VTC:amarchy-flat md5 on the rig = 8f326587, the HEAD build). Fail modes: the known ROM reset loop, and a silent
  hang at the same boot.log line with 0 resets.
- Trap: main's build/amiga held 13:52 (05678a64/4e445b19-era) Amarchy, amarchy-flat, amarchy-gui, amarchy-menu until
  another session rebuilt them at 16:47; the s62 legs ran those builds.
- Binary NOT named, PC NOT mapped. Legs B, C, D and the Stack 262144 leg are still open. Old flats for leg D:
  4e445b19 build md5 e0f38ace (212284 bytes), c4844c4e build md5 0dbb62b9 (212296 bytes); rebuild them, they were discarded.
- Method: run rig.py from a scratch worktree whose build/ symlinks every main build/ entry except amiga/, a copy of
  main's with the one test binary swapped in (start does not call merge_build; kit_entries needs build/dist). A
  `rig.py reboot` that hits the hang retries with a second start: kill the reboot and its `start --keep-sys` child
  before the next boot, or two starts race on one rig (happened in A2-3/A2-4; A2-4 discarded).
- Verdict: not the RectFill/Text stack budget alone (fixed binary still fails). Candidates unchanged: amarchy-flat
  start from the gate (317234ea) racing DOpus/amarchy-index, or another set member. Crash window = Amarchy set start:
  HIGH confidence (5 of 5 FAILs end at "UP-Term shadows done"). Binary: UNKNOWN.
- Fix level: undecided. Leg C (no-op flat) is the next decisive leg; if it gives >= 4 OK, look at
  amarchy/desk_dos.c:559 (dk_flat_start, from desk_dos_flat_ensure at desk_dos.c:628-643 in Amarchy's start gate).

## s63 part 2 (2026-11-08)

Binaries built in scratch worktree at c4eb3756 (flat 8f326587 = same as main's). Leg C: with amarchy-flat replaced by a
no-op exe, 5 of 5 boots OK (each: `stop --keep`, 6 s, fresh `start --max`; no overlapping rig.py start for rig 97; 0 CPU
resets; boot.log passes "UP-Term shadows done" to "wasabid started"). Baseline failed 4 of 6 boots at this build family
(A 2/3, A2 2/3); if the true fail rate is 2 in 3, five straight OKs has probability about 0.13, so this is suggestive, not
proof. Reading: the crash needs amarchy-flat running; Amarchy, amarchy-gui, amarchy-menu, amarchy-index and DOpus alone boot
clean. Next: 5 more leg C boots to reach p < 0.01, then desk_dos.c:559 dk_flat_start (start from the gate, 317234ea) and
amarchy-flat BOOT block of S:User-Startup. Leg B not done: editing the kept sys.hdf (xdftool) then `start --keep-sys` hung
before go (VOID); redo it with a new rig flag that strips the block at clone time (like --no-amarchy-block), not by hand.
Note: "OK" in `Status` lists no process named dopus; DO_LAUNCHER was taken as DOpus.

## s63 root-cause candidate (code read, rig proof pending)

Two boot starters launch amarchy-flat in the same second:

- `dist/install.dos:600`: the User-Startup line `Run >NIL: UP-Term:bin/amarchy-flat BOOT`.
- Amarchy's start gate: `desk_dos_flat_ensure` calls `dk_flat_start` when `FindSemaphore(FLAT_SEM_NAME)` finds no record.

The single-instance guard in `amarchy/flat.c` `main()` was not atomic. It ran `Forbid(); FindSemaphore; AllocVec; amconf_borrow() (DOS file read); desk_flat_state; AddSemaphore; Permit()`. The DOS read waits, and the wait breaks Forbid. A second starter could then run its own FindSemaphore before the first one had added its record. Both pass the guard, and both install patches. This fits leg C: with a stub flat, 5 of 5 boots were OK.

Fix: the prefs read now runs before the guard's Forbid, into a local `flat_state`. The record copies that state inside the Forbid section. Regression test: `make test ONLY=flat_single_instance` (`tests/test_flat_single_instance.py`). It fails on the old main() and passes on the new one.

Open item: the dead-owner branch still does Permit, then drop_record, then Forbid, and does not run FindSemaphore again. A second starter can slip in only when a stale record exists, which is not the boot case.

Proof still needed: 5 or more KS 3.1 boots with the fixed binary, all OK. Leg E (late single start) must not crash. If it does crash, this race is not the whole cause.

## s63 part 3 (2026-11-08)

Leg E (scratch worktree vtcon-s63-lateflat at c4eb3756, rig 97 only): the real amarchy-flat (md5 8f326587), started by hand
20 s after "wasabid started", did NOT crash KS 3.1 in 5 of 5 boots (0 CPU resets, rig answering ping, flat alive, no
failure at start, at the open window, or at idle). Reading: amarchy-flat does not break KS 3.1 by merely running late;
the baseline crash needs the flat to start DURING boot (Amarchy's start gate, desk_dos_flat_ensure / dk_flat_start,
or its overlap with the boot-time shadow pass / DOpus start) = start-order race, not a plain flat fault. Caveat: STATUS showed
calls=0 flat=0, so no drawing call ever reached the hook (the Shell, UP-Term and lister windows did not trigger it; the
hook was installed: frame=hooked). Draw-path faults on KS 3.1 are therefore NOT excluded by E. Harness note: a loop that
reads boot.log must compare its mtime to the start (a stale "wasabid started" from the previous boot gave two false
positives, discarded). Leg F (Stack 262144) not run. Next: start the real flat from the gate during boot with a delay
(Wait before the gate's dk_flat_start) to locate the window, or force flat drawing (Amarchy theme apply) in late start.

## s63 part 4 (2026-11-08)

Leg G: the fixed amarchy-flat (flat.c main() reads prefs before the Forbid guard; md5 1bc4c04194e2be69682418cc701058db) booted
for real on KS 3.1 + DOpus, rig 97, 6 boots (stop --keep, 6 s, fresh start; boot.log and fs-uae log deleted before each start so a
stale "wasabid started" cannot count). Result 3 OK / 3 FAIL (1 silent hang, 2 reset loops with 1524 and 1606 resets), all FAILs
at "UP-Term shadows done". The crash is NOT cured by the single-instance fix: the baseline rate (about 2 in 3) is unchanged.
The race fix did hold where observable: on all 3 OK boots exactly one amarchy-flat process existed at +30 s. (G4's first count of 2
was a `[REFRESH]` log line matching grep, not a second process.) So the double-start race is closed or was never the crash;
leg C (stub flat, 5 of 5 OK) still says the crash needs the real flat running during boot, but leg E says a late start is safe.
CAVEAT: another session edited amarchy/flat.c at 17:54:58 and rebuilt build/amiga/amarchy-flat (md5 b21ae428, 213780 bytes) while
the run was going; G4's start logged `[REFRESH] rig 97: amarchy-flat`, so G1-G3 ran md5 1bc4c041 (1 OK, 2 FAIL) and G4-G6 most
likely ran b21ae428 (2 OK, 1 FAIL; rig copy md5 not captured). Only G1-G3 are clean evidence for the asked-for binary, and they
already fail 2 of 3. Remaining candidates: the flat's boot-time patching (SetFunction on RectFill/Text, DOpus patch, calls=35 before first use) running
concurrently with the boot-time shadow pass or DOpus start, not the double instance. Next: delay the gate's flat start vs the
User-Startup BOOT start (one at a time), and the Stack 262144 leg. Rig 97 stopped, build/rig97 cleared.

## s63 part 5 (2026-11-08)

Leg PT (pass-through): the real amarchy-flat built from c4eb3756 in worktree vtcon-s63-ks31hunt, with one experiment change (`-DFLAT_PASSTHRU`, not committed). flat_rf_c and flat_tx_c still install and still count calls, but they then call the saved original RectFill/Text vector at once and return. No icl_label, bb_claim, rf_* or pen work runs inside RectFill/Text. Everything else is unchanged: install_all order, marker_write, the sysiclass/frameiclass h_SubEntry swap (c2/c8 with the clone path), the DOpus DrawBox/FieldBox patch, and the BOOT retry.

Rig 93, KS 3.1 40.068 (kick40068-a1200.rom) + DOpus, 060 --max --idle-minutes 0. Harness: stop --keep plus fresh start for each boot. Verdict rules: "wasabid started" in boot.log = OK, more than 20 "CPU reset" lines in fs-uae.log = FAIL-reset, 260 s with no wasabid = FAIL-hang. Each md5 was read from the rig copy build/rig93/vtc/amarchy-flat. Base build md5 8f32658775bd4bd5e939cc8d03dc8826; pass-through md5 88b785e0409d2f11271e3b454bf76dcf.

| boot | leg | rig amarchy-flat md5 | verdict | flat processes |
|---|---|---|---|---|
| PT-a | PT | 88b785e0 | OK (hand check: boot.log reached wasabid started) | not taken |
| PT0 | PT | 88b785e0 | FAIL-reset (32 resets by t=33 s) | - |
| PT1 | PT | 88b785e0 | OK | 1 |
| PT2 | PT | 88b785e0 | FAIL-hang, last line "UP-Term shadows done" | - |
| PT3 | PT | 88b785e0 | OK | 1 |

Result: 3 OK / 2 FAIL of 5. The two failure modes match main and leg G (a reset loop and a hang at "UP-Term shadows done").

Conclusion: the crash is NOT in the bodies of the RectFill/Text hooks (flat_rf_c and flat_tx_c draw code, icl_label, ObtainBestPen and ReadPixelArray8 inside Text). The stub flat boots 5/5 OK (leg C), so the cause is in what the real flat does at boot apart from those bodies. Remaining candidates, most likely first:
1. The sysiclass/frameiclass dispatcher swap (hook_install, c2_disp_reg/c2_disp_stk, c8): the clone path under cl_ss runs AllocBitMap on the caller's stack in every task that draws a gadget during boot.
2. The jump-table write mechanics (flat_swap_install plus libpatch_done) racing with other tasks that are inside RectFill/Text during startup. The PT leg still swaps the vectors.
3. The DOpus DrawBox/FieldBox patch.
4. marker_write to ENVARC: (a disk write before the swap at boot).
Next leg to split these: build with hook_install disabled (rf/tx at pass-through or real). If it boots 5/5, the dispatcher swap is the cause.
Experiment patch: scratchpad flat-passthru.patch (worktree removed).

## s63 part 6 (2026-11-08)

Bisect of the real amarchy-flat boot path (install_all / main) on KS 3.1, two rigs side by side. All builds from c4eb3756 in worktrees
vtcon-s63-ks31bisect (rig 93) and vtcon-s63-ks31bisect-nv (rig 95), each with experiment `#define`s in amarchy/flat.c (not committed;
patches in the session scratchpad: flat-bisect-nh.patch, flat-bisect-nvc2-noclone.patch). Config as part 5: KS 3.1 40.068
(kick40068-a1200.rom, read from each fs-uae log), DOpus 5.82-era system install, dopus5.library 68.16 (Version on every OK boot),
060 `--max --idle-minutes 0`, harness stop --keep + fresh start, boot.log and fs-uae log deleted before each start. Verdicts as part 5
(wasabid started = OK, >20 CPU resets = FAIL-reset, 260 s = FAIL-hang). md5 = build/rigN/vtc/amarchy-flat after each boot (the rig's VTC:).

Legs:
- NH: everything as main, hook_install skipped (install_all sets rc = FLAT_OK): RectFill/Text swaps, real hook bodies, DOpus patch, marker_write all on.
- NV: hook_install on (sysiclass c2 + frameiclass c8); no marker_write, no rf_install/tx_install, dopus_scan returns at once.
- NVC2: NV and no frameiclass swap (hook_install returns after the sysiclass swap).
- NVC2NC: NVC2 and the clone route off in c2_draw (no c2_clone, so no clone_render / NewObjectA / AllocBitMap / nested dispatcher call; the block glyph c2_glyph draws instead).

| boot | leg | rig amarchy-flat md5 | verdict |
|---|---|---|---|
| NH-0..4 | NH | 15addd447c07aabe51405b9dc3087462 | 5 OK (flat procs 1 each) |
| NV-0 | NV | 813c217233903f4a1695abb23caa85ee | OK |
| NV-1 | NV | 813c217233903f4a1695abb23caa85ee | FAIL-reset (58 resets by t=28 s; first reset `CPU reset PC=f80dc8 (Kickstart ROM)`, no alert/guru line in fs-uae log) |
| NV-2 | NV | 813c2172 | VOID (stopped mid-boot: NV-1 already proved sufficiency) |
| NVC2-0, 1 | NVC2 | e7e3d5e37dab55f3e9df7a8563864fdf | OK, OK |
| NVC2-2 | NVC2 | e7e3d5e37dab55f3e9df7a8563864fdf | FAIL-reset (36 resets by t=28 s) |
| NVC2-3 | NVC2 | e7e3d5e3 | VOID (stopped mid-boot after the FAIL) |
| NVC2NC-0..4 | NVC2NC | 28760b2fc50f758cabe350c5da28bb09 | 5 OK (flat procs 1 each) |

Reading: with the sysiclass hook off and everything else on, 5/5 OK (baseline about 1 OK in 3; 5 OK by chance about 0.4%). The sysiclass
hook alone (no RectFill/Text/DOpus swaps, no marker, no frameiclass) still reset-loops. The same hook with the clone route off boots
5/5. So the crash is in the c2 clone path: c2_clone -> flat_clone_draw_in -> clone_render (AllocBitMap in clone_ensure, NewObjectA
"sysiclass" in clone_object, the nested call of the saved dispatcher on cl_rp, ReadPixel), run inside the sysiclass dispatcher in
whatever task draws a gadget at boot. Candidates 2 (swap mechanics), 3 (DOpus patch), 4 (marker_write) and frameiclass are cleared.
Not split further: which call inside clone_render (NewObjectA under Intuition's locks, AllocBitMap/ReadPixel on the drawing task's
stack, or the nested dispatcher re-entry) faults. No alert or PC beyond the ROM reset vector in the fs-uae log. Rigs 93 and 95 stopped and cleared.

## s63 part 7 (leg SG): stack guard on the clone path

Build: amarchy-flat md5 cd54291ee0be8d2755063a032c1d2089 (214308 bytes). clone_render measures free = SP - tc_SPLower at entry; refuses (CLONE_WHY_BUSY) if free < 6144; publishes cmin/ctask/ccalls/crefused in the 'amarchy-flat STATUS' line. Patch: flat.c + flat_shared.h (saved in the session scratchpad as flat-leg-sg.patch). Rig 97, KS 3.1 default kit rig (system DOpus + kit), not --os 31 clean (that rig has no kit). STATUS here is the 'amarchy-flat STATUS' line, not the Amarchy ARexx reply.

| boot | verdict | md5 read from rig disk |
|---|---|---|
| 1 | OK | cd54291ee0be8d2755063a032c1d2089 |
| 2 | OK | cd54291ee0be8d2755063a032c1d2089 |
| 3 | OK | cd54291ee0be8d2755063a032c1d2089 |
| 4 | OK | cd54291ee0be8d2755063a032c1d2089 |
| 5 | OK | cd54291ee0be8d2755063a032c1d2089 |

STATUS per OK boot (fields cmin/ctask/ccalls/crefused), S1 after boot, S2 after opening/closing windows:
- boot 1 S1: cmin=1052 ctask=« IPrefs » ccalls=5 crefused=2
- boot 1 S2: cmin=1052 ctask=« IPrefs » ccalls=26 crefused=2
- boot 2 S1: cmin=1052 ctask=« IPrefs » ccalls=11 crefused=5
- boot 2 S2: cmin=1052 ctask=« IPrefs » ccalls=32 crefused=5
- boot 3 S1: cmin=1052 ctask=« IPrefs » ccalls=11 crefused=5
- boot 3 S2: cmin=1052 ctask=« IPrefs » ccalls=32 crefused=5
- boot 4 S1: cmin=1052 ctask=« IPrefs » ccalls=11 crefused=5
- boot 4 S2: cmin=1052 ctask=« IPrefs » ccalls=32 crefused=5
- boot 5 S1: cmin=1052 ctask=« IPrefs » ccalls=11 crefused=5
- boot 5 S2: cmin=1052 ctask=« IPrefs » ccalls=32 crefused=5

Result: 5/5 OK with the guard on. Refusals on every boot (crefused 2 to 5, all fallback busy). Lowest free stack at clone_render entry over all calls: 1052 bytes on task IPrefs. Part 6 without the guard: about 1 in 3 boots reset or hang with the clone route on. Verdict YES: the clone path runs on a nearly full task stack (IPrefs, 1 KB free before the ROM dispatcher, AllocBitMap and ReadPixel nest on top) and refusing below 6 KB removes the crash. Not run: threshold-0 measure-only boots (cmin 1052 already includes refused calls). Caveat: 5/5 at a 1-in-3 base rate is p about 0.13 by itself; the cmin number is the stronger evidence.

## s63 part 8 (private stack fix)

Fix: the c2 clone path (c2_clone -> flat_clone_draw_in) now runs on a private 16384-byte
MEMF_PUBLIC stack through the asm helper flat_on_stack (amarchy/flat_stk.s, exec StackSwap),
while cl_ss is held. The stack is allocated in main before the hooks install and freed after
hook/rf/tx removal under cl_ss. If allocation failed, the clone falls back to the block glyph
(cnostk counter); the clone never runs on the caller's stack. The refuse guard is removed.

Binary: build/amiga/amarchy-flat from branch s63-ks31-stack, md5 6097ecbd8cacd282308edfab5fa210e0
(checked on the rig disk each boot). Rig 97: KS 3.1, 060 --max, DOpus installed.

| boot | result | md5 on rig | clone draws (S2) | fallback | cnostk | cmin (IPrefs) |
|------|--------|------------|------------------|----------|--------|---------------|
| 1 | Workbench + Amarchy | 6097ecbd | 63 | 0 | 0 | 1180 |
| 2 | Workbench + Amarchy | 6097ecbd | 57 | 0 | 0 | 1180 |
| 3 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |
| 4 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |
| 5 | Workbench + Amarchy | 6097ecbd | see sp-a log | 0 | 0 | 1180 |

Boot 1 S2 tail: `lskip=2 cmin=1180 ctask=<IPrefs> chw=16384 cstk=16384 cnostk=0 clone draws=63 fallback=0`
Boot 2 S2 tail: `lskip=2 cmin=1180 ctask=<IPrefs> chw=16384 cstk=16384 cnostk=0 clone draws=57 fallback=0`
(boots 3-5 STATUS lines show the same cmin/chw/cstk/cnostk with draws 53/57/67 across S1/S2 pairs.)
cmin=1180 on IPrefs confirms the cause: the caller stack had about 1.2 KB free, now unused by the clone.

OPEN: chw=16384 equals the whole stack on every boot. The high-water scan walks up from the
stack bottom until the first byte that is not 0xA5, so chw=16384 means byte 0 was already
changed. Either the clone really used the whole 16 KB (no crash seen, but unproven), or the
scan or fill is wrong. Not resolved here (boot-fail rule: no more patches). Next step: dump
the stack bytes, or fill with a second pattern and scan from the top.

KS 3.2 (rig 98, --os 32, same pinned binary started by hand): flat=on, `clone draws=14448`
after opening Palette and Font prefs, so amarchy-flat still draws clones on 3.2. cmin/chw not captured.

Tests: tests/test_flat_clone_stack.py (make test ONLY=flat_clone_stack): fails 3 of 4 on main's
flat.c, passes 4 of 4 on this branch. flat_core, flat_role_core, flat_single_instance pass.
make flat-budget PASS.

## s63 part 9 (chw=16384 explained: measurement bug, not overflow)

Verdict: the private stack is NOT overflowing. KS 3.1 writes the longword 0xBAD1BAD3 into the first 4 bytes of the
swapped-in stack (probe: b0=0xBAD1BAD3, b4=0xA5A5A5A5, fill256=252 of 256, AllocVec size longword hdr=16404 intact,
16 canary bytes below the block all intact). The scan started at byte 0, saw a non-fill byte, and read "full".
A second defect hid behind it: the scan resumed at its last stop, so a later, deeper draw was never seen.

Fix (commit on s63-ks31-stack): CLONE_STACK_SKIP=8 (scan starts past the marker), rescan from the bottom on every draw,
STATUS gains `cprobe fill256= deep= guard= hdr= b0= b4=` (deep = high water ignoring the bottom 256 bytes; b0/b4 decimal).
Test: tests/test_flat_clone_stack.py (2 new tests, both fail on a1a9ea16).

Measured on rig 97, KS 3.1, 060, binary md5 1a4b8257..., after Workbench + Palette + Font prefs:
boot 1: chw=1632 deep=1632 cmin=1176 clone draws=14 fallback=0
boot 2: chw=1564 deep=1564 cmin=1176 clone draws=14 fallback=0
Real depth about 1.6 KB of 16 KB (10%): no overflow, headroom 14.7 KB. cmin on IPrefs is still ~1.2 KB, now unused by the clone.
Not done: the deep frame was not located (not needed at 1.6 KB). Hypotheses row 10 text for main's table:
"10 | private 16 KB stack | part 8-9 | chw=16384 was a scan bug (KS 3.1 stamps 0xBAD1BAD3 at the stack bottom); real depth 1564-1632 bytes | CONFIRMED safe".
