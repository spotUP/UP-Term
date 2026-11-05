---
date: 2026-11-05
topic: S57-START-POINTER-EDITOR-HANG
status: open (cause not proven on a rig)
---

# Amarchy start blocks on SYS:Prefs/Pointer after a fresh 3.2 install

## Symptom (rig 128, s57-rl15, test register row 106)

- After the Installer reboot, Workbench shows only the RAM Disk icon and a busy pointer.
- `wasabi ps`: process "Background CLI (SYS:Prefs/Pointer)" in wait for minutes.
- Installer step 34 fails: the AMARCHY ARexx port never appears.

## Static reading (worktree da62f6b3)

- amarchy.c opens the AMARCHY port (rexx_host_open, line ~582) only after the
  start tick has run desk_dos_flat_pens_refresh, marker_recover, wb_backdrop and
  desk_dos_start_apply. So any editor that blocks inside the start apply also
  keeps the port from opening.
- desk_dos_start_apply (desk_dos.c ~1456-1592) writes the palette (dk_put_palette).
  It then starts `SYS:Prefs/Pointer FROM ... USE` through dk_put/dk_editor with
  no IPrefs idle wait. dk_wait_reset runs only when desk_pens_reset() says the
  pens changed. desk_editor_waits_iprefs() (desk_core.c:204) names only Workbench
  and WBPattern.
- dk_apply carries an older note: "SYS:Prefs/Pointer raised a Program Error
  requester when started under the running reset". The start apply does not
  follow that rule.
- dk_editor runs SystemTags synchronously with NIL: input and output. A requester
  or an IPrefs wait inside the editor therefore blocks Amarchy with no timeout.

## Candidate violated invariants

1. A prefs editor starts only when IPrefs is idle, after the palette notify has
   finished. The start apply breaks this for Pointer after a palette write.
2. The AMARCHY ARexx port exists before any start work that can block. Without
   it, the Installer check and the BAR STATUS/OFF calls from child processes
   depend on an external editor finishing.

## Fresh-all baseline

- Test register rows 83 and 87: fresh-all PASSED on main 9fd26e8c and 7cf4ac17.
- Between 7cf4ac17 and the rig 128 build, the only amarchy changes are 4ea76673
  (RL20) and the RL15 work (8a704261, 1229912b).
- The hang is therefore either intermittent, or caused by the extra
  dk_windows_close/open that RL15 put around the palette write.

## Rig 130 run (2026-11-05, worktree da62f6b3, --os 32, fresh-all)

- Build: make amiga and make dist (kit-main) both returned rc 0.
- Installer reached "VTC:distkit/Install (100% done)". Log: "[WARN] the
  Workbench stayed PAL (W43)".
- REPRODUCED: `FAIL 34 after the reboot: the AMARCHY ARexx port answers STATUS:
  no port`, then `[ABORT] fresh-all: auth failed: the Amiga took the connection
  but its daemon did not answer for 15 s`. installer_rig.py rc=2.
- Worse than rig 128: after the reboot wasabid itself stops answering. `ps`
  and `windows` time out; one `guru` call got through ("No Guru recorded").
  fs-uae for rig 130 at ~110% CPU (running, not a crashed emulator).
  So the block holds something that wasabid's ps/windows also need (an
  Intuition or Workbench lock, or a Forbid), not only Amarchy's own process.
  This points at a lock taken around the editor call (dk_windows_close /
  desk_wb_close in the main process) rather than at the Pointer editor alone.
  Not proven: no snoop could be taken.

## Open

- Reproduce on a rig:
  - `ps` for "SYS:Prefs/Pointer"
  - `wasabi windows` and `ui list` for the requester text
  - `snoop --task 'Pointer*' --entry` for the blocking call
- Run the Stack 262144 probe on the same run.
- Check whether "amarchy-flat BOOT left nothing resident" (ledger rows 4151 and
  4154) is the same block: the setup reads the resident list while the start
  apply hangs.
- Fix only after the measurement names the blocking call.

## Bisect step 1 (RL15 reverted): answered from the register, no rig run (2026-11-05, vtcon-c1)

- Verdict: RL15 (8a704261, dk_flat_pens window bracket) is NOT the cause.
- `git diff fdb5e99f 69f34ee9` outside thoughts/: the only shipping-code change is 8a704261's 7 lines in
  amarchy/desk_dos.c. The rest is the palette_closes test target, tests/amiga/copcheck.c and rawdrag.c (CONPROBES, not in
  the kit), tests/test_flat_core.c and tools/rig/amarchy_roundaa_rig.py. So "main with RL15 reverted" builds the same kit
  as fdb5e99f.
- fdb5e99f does not contain 8a704261 (`git merge-base --is-ancestor`: no). Rig 131 ran `installer_rig.py ONLY fresh-all`
  on it (register row 108, rig96-drag-freeze.md "Hunt attempt 3"): checks 1-33 ok, then FAIL 34 "AMARCHY port: no port", then
  [ABORT] wasabid silent for 15 s after the post-install reboot. That is the same failure as rigs 128 and 130.
- Rig 125 (df0c1923, also without RL15, register row 99): the same silent wasabid after the post-install reboot.
- Register rule 9: the same build, config and recipe means no rerun, so rig 132 was not started.
- Between the PASS baseline 7cf4ac17 (rows 83 and 87) and fdb5e99f, the only shipping change is RL20 (4ea76673:
  amarchy/gui.c fill_round/rich_put, richaa_core.c; amarchy-gui links it). RL20 is the remaining code suspect.
- Confound, not ruled out: every failing run was on a loaded host. Rig 125: host about 610% CPU. Rig 131: rigs
  96/127/129 plus a node pre-push hook, each at 85-100%. A vitest at 210-280% was also running at this check. The PASS
  runs' host load was not recorded. A wasabid that stops answering while fs-uae runs at about 110% also fits guest
  starvation (memory "Rig hang? check host load").
- Next (needs a new brief): fresh-all on 7cf4ac17 again on a quiet host, and on fdb5e99f with RL20's gui.c reverted,
  both with host load logged. That separates "RL20" from "host load / intermittent".

## A/B fresh-all (rig 133, 2026-11-05, vtcon-c1)

Recipe both runs: `rig.py start --max --os 32 --idle-minutes 0 --allow-stale-base ...`, then
`INSTALLER_KEEP=1 installer_rig.py ONLY fresh-all`; build = make worktree-ready/wasabi/amiga, `make dist` via kit-main.
Host load logged every 60 s (`ps -Ao pcpu,command | sort -rn | head -5`).

| Build | Verdict | Failing step text | Max non-fs-uae host CPU |
|---|---|---|---|
| A 7cf4ac17 | FAIL | 34 "after the reboot: the AMARCHY ARexx port answers STATUS: no port"; then [ABORT] wasabid silent 15 s | 94% (Defender epsext, clang); rig 96 fs-uae about 110% |
| B fdb5e99f minus RL20 (git revert -n 4ea76673, clean) | FAIL | same text, same abort | 114% (Spotlight mdworker); clang about 96% before start |

- Conclusion: RL20 is NOT the cause. The build recorded as PASS (7cf4ac17, rows 83/87) now fails identically, so the
  failure is not a code change between 7cf4ac17 and fdb5e99f. Remaining suspects: host load/state (the PASS runs were
  on a rig 96 with a different history) or an environment difference (the earlier PASS runs used --keep rigs that were
  not fresh-started the same way). Host load here was moderate (under 120% non-fs-uae), so heavy load alone is not
  needed to fail.
- No ps/windows (wasabid silent) and no screenshot (rig already down after the abort). Not dug further, per brief.

## Capture on rig 134 and root fix (vtcon-ee agent, 2026-11-05)

Reproduction on main (rig 134, 3.2, 060 max, fresh-all, 03:22): FAIL 34 "no port", then [ABORT] auth failed after 15 s.
Host load stayed modest the whole run: fs-uae for rigs 134 and 96 at about 115% each, every other process under 60%
(scratchpad load134.log). CPU starvation is not needed for the failure, so the 15 s wasabid timeout is not the cause.
The fixed `sleep 5` + single STATUS probe in reboot_amarchy_check is also not the cause: the system was frozen, not slow.

Evidence after the failed boot: Env-Archive/Amarchy/desk-original held ENV-Sys-palette.prefs.absent and
ENV-Sys-pointer.prefs (03:22) and nothing later, so Amarchy's start apply ran the Palette and Pointer steps and stopped.
On rig 96 (PASS, 11-04) the same folder also holds the Workbench/font/wbpattern files, so the apply went further there.
ENVARC Pointer.prefs and the desk-original copy are byte-identical between rig 96 and rig 134 (md5 4da47600...), and
S:User-Startup is identical, so the input is the same; the difference is timing.

Capture (Amarchy autostart held in the rig's host S:User-Startup, then `Run UP-Term:bin/Amarchy` by hand, snoop + debug on):
- Snoop order: Palette editor runs and exits; `SYS:Prefs/Pointer FROM T:amarchy-desk/pointer.prefs USE` loads, opens the
  work file, Open("ENV:Sys/Pointer.prefs", new) ok, SetProtection, CurrentDir, then no further DOS call.
- ps for 80+ s: "Background CLI (SYS:Prefs/Pointer)" in wait, Amarchy in wait inside SystemTags, IPrefs in wait. No windows.
  wasabid stayed responsive (by hand, outside the boot); at boot the same block froze the system and silenced wasabid.
- Control: on a fresh reboot the same Pointer USE run alone (`Run SYS:Prefs/Pointer FROM ENVARC:Sys/Pointer.prefs USE`)
  exits within 8 s. The editor hangs only when it starts while IPrefs still works on the Palette write.

Cause: desk_dos_start_apply ran the Pointer USE straight after the Palette USE with no IPrefs idle wait. dk_wait_reset
(only when desk_pens_reset) returns when the new screen is up, not when IPrefs is done. dk_apply already notes the same
hazard ("SYS:Prefs/Pointer raised a Program Error requester when started under the running reset"); the revert path waits
for IPrefs after Pointer (DESK-PTR-REVERT). The start path had neither wait.

Invariant: no stock editor USE starts while IPrefs still handles an earlier prefs write.

Fix: desk_editor_waits_iprefs (amarchy/desk_core.c) now also returns 1 for Palette and Pointer, so dk_editor (the only
editor-run path) takes the IPrefs idle mask before those runs and waits for IPrefs idle after them. This covers the start
apply, dk_apply and the revert. Regression test: tests/test_desk.c test_desk_editor_waits_iprefs expects Palette and
Pointer = 1 (fails with the old function).

Proof: fresh-all on rig 134 (stopped + cleared first, fresh start, 3.2, 060 max, INSTALLER_KEEP=1), build = this branch
(make amiga + make dist via kit-main): [PASS] fresh-all. "ok 35 after the reboot: the AMARCHY ARexx port answers STATUS"
(STATUS bar=1 ticks=32), "ok 36 Left Amiga + Space opens the Amarchy menu window". Max non-fs-uae host CPU 93% (XProtect, clang).

Open: why 7cf4ac17 passed on rig 96 on 11-04 is timing only (same input files); the race window depends on how long IPrefs
takes on the palette notification. reboot_amarchy_check still probes STATUS once after a fixed 5 s; not changed here.

## Residual hang (rig 136)

2026-11-05, vtcon-c1 agent, rig 136 (3.2, 060 max, idle watchdog off), build from a83eceb7 (fix 9f9ea2dd in).
Status: cause narrowed, NOT fixed. Agent budget ran out before a fix; the next brief starts from here.

Baseline: fresh-all with INSTALLER_KEEP=1 on rig 136 reproduces rig 135: "FAIL 34 after the reboot: the AMARCHY
ARexx port answers STATUS: no port", then "[ABORT] daemon silent 15 s".

Config difference the 9f9ea2dd proof missed: the rig 134 PASS ran with an 800x600x24 Workbench (first `S` line of
build/rig-reports/rig134/installer/fresh-all.pages.txt). Rigs 133, 135 and 136 run PAL 640x256x8. On the 8-bit
screen every Palette USE is a Workbench reset (desk_pens_reset), so the fix was never proven in the failing config.

Step numbers 34 vs 35: no recipe or code difference (installer_rig.py and amarchy/ are identical apart from the fix).
The numbering moves with conditional checks at run time: the room-page block (3 checks) and
check_other_disk_leaves_sys_upterm (1 check, only when the drawer is not on the system volume).

Capture A, autostart held (S:User-Startup line `Run >NIL: UP-Term:bin/Amarchy` commented out), rig rebooted, then
`ami.py exec "Run >NIL: UP-Term:bin/Amarchy"` about 1 minute after boot. Snoop shows every editor finishes:
  05:05:55.992 LoadSeg SYS:Prefs/Palette   05:06:02.752 Pointer   05:06:03.908 Font
  05:06:05.028 Palette (second)            05:06:11.483 WBPattern 05:06:12.102 Workbench
AREXX AMARCHY STATUS then answers: hits=0 rows=71 enabled=1 active=0 wins=0 ... bar=1 ticks=38.
Note: each 8-bit Palette USE takes 6 to 7 s and the start apply runs two of them.

Capture B, autostart restored, plain `rig.py reboot`, `wasabi ps` every 3 s from 05:10:07 for 60 s:
  0x10a4d848 proc 0 wait 32768 31666 1  Background CLI (SYS:Prefs/Pointer)     (never leaves wait)
  0x10100118 proc 0 wait 262144 256346 4 Background CLI (UP-Term:bin/Amarchy)   (blocked in dk_editor)
  0x10586300 proc 1 wait 8000 6658 12   Workbench                              (LoadWB already started)
  0x10089798 proc 0 wait 8192 6350 -    IPrefs
No requester open (`ami.py requesters` empty). The Pointer editor shows no snoop call in 60 s.

Hypotheses:
- (c) fix not in the binary: ruled out. Same binary, same disk completes all editors when started by hand.
- (b) a later editor hangs: ruled out for this failure. It is still the Pointer editor; Font, WBPattern and
  Workbench are never reached.
- (a) the idle check reads idle too early: not proven or ruled out. The hang needs the boot-time start.
  S:User-Startup runs (Startup-Sequence line 87) before LoadWB (line 90). Amarchy's start gate runs the
  start apply on the first tick that sees the Workbench screen, which is before or while LoadWB starts the Workbench
  process. The Pointer USE then waits on something the starting Workbench or IPrefs holds.

Next step (not done): read the Pointer editor's tc_SigWait and the Workbench process state while it hangs
(wasabi ps has no signal column, so add a probe), and test gating desk_dos_start_apply on the Workbench being
fully up (the "WORKBENCH" port present and the Workbench process idle) instead of on the screen only.
reboot_amarchy_check still probes STATUS once after a fixed 5 s; with two 8-bit Palette resets the start apply
takes about 20 s even when nothing hangs.

## Pointer wait probe (rig 141)

vtcon-c1 opus agent, 2026-11-05. Rig 141 (060, OS 3.2, PAL 640x256x8), fresh-all install, then a reboot. The hang
was live during the probe: the busy pointer never clears (the owner watched it). That makes this a deadlock, not a
slow start. Tool: `tests/amiga/taskdump.c`, extended with a semaphore-wait walk, `SEM <hex>` and `LAYERS`. All
probes were read-only.

### Step 1: who waits on what, during the live hang

| Task | Waits | On | Held by |
|---|---|---|---|
| Workbench 105be990 | SIGF_SINGLE (00000010), in a graphics.library frame (sp+100) | not found yet (see below) | - |
| IPrefs 10089798 | SIGF_SINGLE | WB screen LayerInfo.Lock 10054cb8 | Workbench (nest 1, queue 1 = one waiter, IPrefs) |
| SYS:Prefs/Pointer 10899bc0 | SIGF_SINGLE | icon.library base+6c (10058d70) | shared (owner 0, nest 1, queue 2 = 1 holder + 2 waiters) |
| amarchy-index 10108b20 | SIGF_SINGLE | the same icon.library lock 10058d70 | shared |
| Amarchy main 10103128 | SIGF_DOS (00000100) | the synchronous SystemTags child (dk_editor running Pointer) | - |

- The icon.library base 10058d04 is on Workbench's stack 4 times, so Workbench is the likely shared holder of the
  icon lock (unverified: a shared holder is not recorded).
- Workbench holds the root-window layer lock 10101510 of layer 101014c8 (window 10102aa4) with nest 2 and queue 1.
  Exec raises QueueCount on every obtain, so nest 2 and queue 1 mean no waiter on that layer.
- The SemaphoreRequest on Workbench's stack is at sp+2e. Its pred is 10101568, a heap address 0xa0 into that
  layer. The address 16 bytes before it is not a valid semaphore. Workbench's wait target is still open. The step 2
  snoop follows.
- The blitter is free: GfxBase BlitOwner 0, BlitLock -1. SIGB_BLIT is bit 4, the same bit as SIGF_SINGLE, so this
  had to be checked.
- Busy pointer: amarchy has no WA_BusyPointer, SetWindowPointer, SetPointer or WA_Pointer (grep, 0 hits). The
  window Pointer field on the WB layers is 0. The busy pointer is Workbench's own wait pointer during LoadWB.
- Amarchy starts from User-Startup, before LoadWB. Its start gate sends `MENU INVOKE WORKBENCH.BACKDROP` and then
  runs the prefs editors while LoadWB is still opening its windows.

### Disk-state check (the owner's lead: "maybe it tries to write")

- `Info` at the hang shows every volume Read/Write and 0 errors: DH0 Empty-12MB-FFS, DH1 VTCX, DH2 BOOTX, DH3 Work
  and RAM. No volume is Validating or Write Protected. DF0 has no disk.
- No stuck task waits on a DOS packet. Workbench, IPrefs, Pointer and amarchy-index wait on SIGF_SINGLE (a
  semaphore), not on SIGF_DOS. Only Amarchy main waits on SIGF_DOS, and that wait is for its SystemTags child.
- The filesystem tasks are idle. DH0 waits 00002100 with 0 messages on its port, RAM waits c0000100 with 0
  messages, and DF0 waits 40000100. No packet is queued, so no write is blocked in a handler.
- Result: this is not a blocked write. It is a semaphore cycle between Workbench, IPrefs and icon.library users.

### Step 2: full-boot snoop (rig 141, 2026-11-05 08:29:58 on)

wasabi snoop of DOS and icon.library calls for all tasks, from S:User-Startup until the hang
(682 events lost at 08:30:59; the tail of each task is intact):

| Task | Last call | Returns |
|---|---|---|
| Workbench | 08:30:47.985 FindTask(NULL) | ok, then silent (SIGF_SINGLE wait in a graphics frame) |
| IPrefs | 08:30:47.986 FindTask(NULL) | ok, then silent (waits on WB screen LayerInfo, held by Workbench) |
| amarchy-index | 08:30:47.994 GetDiskObject("Empty-12MB-FFS:WBStartup") | never |
| Amarchy main | 08:30:59.488 SetVar("Result2") | ok, then SystemTags(Pointer) |
| SYS:Prefs/Pointer | 08:30:59.520 GetDiskObject("SYS:Prefs/Pointer") | never |

Workbench, IPrefs and the icon.library lock all freeze within 10 ms at 08:30:47.98, 11 s before
Pointer starts. Pointer blocks in its first GetDiskObject, before it opens any prefs file. It is a
victim of a hang that starts in the Workbench icon pass, not its cause.

The snoop has a gap from 08:30:17 to 08:30:47.160, so the last calls before
47.160 are not covered. The sequence it does cover:

- 47.18: Amarchy runs SYS:Prefs/Palette USE. Palette writes
  ENV:Sys/Palette.prefs at 47.211 and exits.
- 47.507: IPrefs reads the new Palette.prefs and starts to apply it. At 47.508
  it opens workbench.library.
- 47.626: Workbench opens locale.library. Workbench is still in its LoadWB
  startup pass.
- 47.98: every task stops making calls. In the step 1 dump of the hang,
  IPrefs waits on the WB screen LayerInfo, owned by Workbench (nest 1, queue 1;
  the WB window layer has nest 2). Workbench waits with SIGF_SINGLE
  (ObtainSemaphore) in a graphics frame. Workbench also holds the icon.library
  lock shared.
- Every later GetDiskObject blocks on the icon lock. That includes
  amarchy-index and Pointer, which Amarchy runs next. Pointer only shows the
  hang; it does not cause it.

### Cause

Violated invariant: Amarchy must not run the stock prefs editors until
Workbench has finished its LoadWB startup pass. Amarchy starts from
S:User-Startup, before LoadWB. desk_start_gate ran desk_dos_start_apply as
soon as the WB screen existed. At that point the WORKBENCH ARexx port was not
up, so desk_dos_wb_backdrop skipped its wait. Palette USE then made IPrefs
apply the palette while Workbench held its screen layer lock. The IPrefs and
Workbench lock waits close a cycle. The run by hand works because Workbench
is idle by then.

Not confirmed: the exact semaphore that Workbench waits on (PalExtra or IBase).
Rig 141 went down before the read-only check could run.

### XCON: double mount (owner lead)

This is not in the chain, as far as the snoop shows. vtcon-8c found that the
rig harness ran a second `Mount XCON:`, which failed with "already mounted".
The handler has no ACTIVATE. No vtcon-handler task is among the tasks blocked
at 47.98. The fix is on main as 0598658c. The live DEVS dump could not run,
because rig 141 was down.

### Fix

desk_start_due() takes a new `wb_ready` argument. desk_start_gate checks
readiness on each 1 s tick, once the WB screen is up and the tick works.
desk_dos_wb_ready(now) returns a verdict from desk_wb_ready_state(). The fix
uses no timing sleep.

- workbench.library 45+ (OS 3.1.4, 3.2): readiness means the WORKBENCH ARexx
  port answered `GETATTR application.version`. Workbench serves that port
  from its main loop, so an answer means the LoadWB pass is over. That is an
  OS guarantee.
- workbench.library below 45 (OS 3.1, no ARexx port): readiness means the
  Workbench root window is open and the Workbench task sat in Wait outside a
  semaphore wait (not SIGF_SINGLE) on 2 ticks in a row.
- Both paths have the same bound: 30 ticks of 1 s, so 30 s, with a log line
  on timeout. The slowest measured pass at 8-bit lasts seconds.
- With no Workbench task at all, the gate opens at once. With no Workbench
  tick, the existing one-shot fallback still runs.

Regression test: test_desk_start_waits_for_workbench. The gate holds while
the WB screen is up but Workbench is not ready. The verdict cases cover a port
that answers at once, a port that appears late, a port that never appears
(timeout at 30 s), and the OS 3.1 idle path, including its timeout.

### Readiness signal: options weighed (coordinator review)

- (a) WBStartup ordering. Workbench runs SYS:WBStartup items after its own
  first pass. This would be one signal for every OS version. But the step 2
  snoop froze at 47.98, before any WBStartup item ran, so these timestamps
  cannot prove the order. Using it also means moving the apply out of
  User-Startup into a new WBStartup entry, which changes the install. Not
  chosen; it stays the candidate to unify both paths once a rig snoop shows
  that WBStartup items start after the icon pass on 3.1 and 3.2.
- (b) The WORKBENCH ARexx port answers. This is an OS guarantee (the port is
  served from the main loop), but only on 3.1.4 and later. Chosen for v45+.
- (c) Root window open and the Workbench task idle. This is a proxy: one
  sample could catch a DOS packet wait in the middle of the pass, so the fix
  takes 2 ticks in a row. It is used only where (b) does not exist (OS 3.1).
  The fix does not prove it on a 3.1 rig. Proof on --os 32 covers path (b) only.

## Proof run on rig 141 (2026-11-05): the gate does NOT fix the hang

Build: worktree-agent-ae15d4b04d2a14737 at 00b5121e (230fa2bd plus the origin/main merge with 0598658c), amarchy md5 f2d9cce2389102ba4913ac6ab5258c2d. `make test ONLY=desk`: [OK] desk: 2298 checks, 0 failed.

Run 1 (rig 141, 3.2, 060 max, --idle-minutes 0, fresh-all) failed at step 34: "FAIL 34 after the reboot: the AMARCHY ARexx port answers STATUS: no port", then "[ABORT] fresh-all: auth failed ... daemon did not answer for 15 s". The owner saw the rig stuck at the Workbench with the busy pointer still up. `wasabi guru`: no Guru recorded.

Read-only dump of the stuck rig (taken before anything else ran):

- `wasabi ps`: Background CLI (SYS:Prefs/Font) in wait, Background CLI (UP-Term:bin/Amarchy) in wait, Workbench, IPrefs. SYS:Prefs/Pointer is not in the list, so Palette and Pointer had finished. Font is the editor that hangs this time.
- taskdump: IPrefs is in semwait on 10104150, the Workbench screen LayerInfo, owner "Workbench" nest 1 queue 1.
- taskdump: Workbench waits 00000010 (SIGF_SINGLE). It holds the LayerInfo, plus layer 101048f0 at nest 2.
- taskdump: SYS:Prefs/Font is in semwait on 10058d70, owner 0, nest 1 queue 1. The lock is held shared, as in the first snoop.
- taskdump: Amarchy waits 00000100 for the editor to finish.

Verdict: the ARexx readiness gate passed, because Workbench served GETATTR, and the same IPrefs/Workbench layer deadlock still happened. The cause is wider than "the apply runs before LoadWB has finished". IPrefs applying a prefs change while Workbench is mid-redraw deadlocks later too: a Palette change makes Workbench redraw, and that redraw runs into IPrefs reopening the screen for the next prefs change. The violated invariant is better stated as "no prefs editor USE may start while IPrefs or Workbench is still processing the previous prefs change". "Workbench finished LoadWB" is not enough. Next hunt: what Workbench waits on under SIGF_SINGLE while it holds the LayerInfo. A signal semaphore? Its sem 0008804c shows owner 0, queue 0, which looks like a missed wakeup. Answer that before the next fix.

Harness fault: after run 1 aborted, the proof script stopped and cleared rig 141 for run 2 against the coordinator's hold order, so the stuck state was lost after the dump. Run 2 never started: rig.py refused with RIG-STALE-BASE (origin/main 816e8a2f not merged).
