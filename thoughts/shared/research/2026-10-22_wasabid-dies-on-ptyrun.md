---
date: 2026-10-22
topic: wasabid "dies" at the first tty case of userland_rig.py --only upkg-ui (rig 4)
status: partial -- ptyrun/PATH suspect refuted; real mechanism found (Guru + reset loop); crashing task not yet identified
---

# wasabid "dies" at the first upkg-ui tty case

## Answer in short

wasabid did not die and was not starved. The whole emulated Amiga crashed
(illegal instruction, Kickstart Alert path), and after the reset it fell into a
**reset loop that never boots** (79 more reset cycles, no disk, no bsdsocket).
That is exactly "connection closed by the Amiga", then ConnectionRefused, `rig.py
status` down, fs-uae alive, no crash text in serial.log.

The 695288d5 PATH line (sh really running under ptyrun) is **refuted as a
deterministic cause**: 6 full upkg-ui runs (6 x 10 tty cases through ptyrun + sh)
plus one single-case run on a fresh rig 4 all passed and wasabid kept answering.

## Measurements (rig 4, `rig.py start --max`, wasabi be4c0559)

| # | What | Result |
|---|------|--------|
| M0 | `userland_rig.py --only upkg-ui --case tty:input` on a fresh rig | PASS in 3 s, wasabid up |
| M1 | `userland_rig.py --only upkg-ui --force` (7 plain + 10 tty) | 17/17 PASS, wasabid up |
| M2 | M1 four more times back to back | 4 x 17/17 PASS, wasabid up (about 50 ptyrun+sh runs in total) |
| M3 | fs-uae log of the instance q1 ran against (`build/rig4/logs/fs-uae.log.txt.tail`, copy kept at scratchpad `q1-instance.fs-uae.log`) | line 1209: `Illegal instruction: 0fff at 000108D2 -> 00F80AD2`; line 1210: `Exception 3 (10997fea) at 10997fea -> f80b0e!`; line 1211: `SERIAL: ... setbaud 9600 PC=f8361c` (Kickstart Alert code); line 1214 onward: 160 `CPU reset` lines (80 pairs f80dc8/f800d0), last bsdsocket thread at line 1208, none after; ends with `amiga_quit` (the later stop) |
| M4 | Same count on the healthy instance after M0-M2 | 0 `CPU reset`, 0 exceptions |
| M5 | Soft reboot probe (`ami.reboot`) to test "any reset on this config loops" | inconclusive: **C:Reboot does not exist on rig 4** (`List C:Reboot` rc 20), so `ami.reboot` never reboots a rig; marker stayed in RAM: |

Not done (budget): (a) PATH-line-removed scratch script and (b) `Stack 262144`
probe -- moot while the failure does not reproduce; (c) trivial `ptyrun sh -c
true` is covered many times over by M1/M2.

## What the evidence says

1. **Crash, not a wasabid exit.** A task jumped into chip RAM (PC 0x108D2,
   opcode 0x0FFF), the exception frame push then took an address error at an odd
   address 0x10997FEA (fast RAM, typical of a corrupted/overflowed stack pointer),
   and Kickstart went to its Alert/ColdReboot path. Which task: unknown. The run
   was inside `VTC:vsh VTC:userland/upkg-ui/tty-input.sh` (userland_rig.py:266),
   so the candidates are vsh, ptyrun's vfork child (ixemul vfork shares the
   parent's stack until exec: ptyrun.c:153-174 does setsid/open/ioctl/dup2 on it),
   sh, upkg-ui, or wasabid itself. The odd SP points at stack trouble -- matches
   the "stack first" rule, which could not be applied because it did not repeat.
2. **Why it looks like "wasabid dies" and leaves no trace.** After the first
   ColdReboot, the 68060-NOMMU + KS 3.1 rev 40.63 + MAPROM (`cs_ksmirror_e0`)
   config resets again at f800d0 / f80dc8 forever without reaching the boot
   (M3: 79 cycles, no hdf reads, no bsdsocket). A fresh fs-uae start boots fine
   (M4). So any Guru on a `--max` rig is permanent until fs-uae is restarted,
   and the Alert text goes out at 9600 baud before serial capture sees a boot.
   This also fits rig 2's "wasabid stops answering ~1 min after start" and the
   earlier unexplained rig 4 Q1 pass (an intermittent crash, not a deterministic one).
3. **Side findings that make this worse:**
   - `userland_rig.py` never takes `paths.rig_lock()` (only browse/upkg/install/
     assign/muforce/wasabi.py do; rig.lock kept browse's 00:27:20 stamp through
     all my runs). It can run concurrently with another rig script on the same rig.
   - `ami.reboot()` relies on `C:Reboot`, which the rig image lacks: every reboot
     through it silently fails and reports "did not come back".

## Root cause status and proposed fixes

Root cause of the *symptom* (proved, M3/M4): a Guru on the rig turns into an
endless reset loop, so a single crash reads as "wasabid died, no crash text".
Root cause of the *crash* itself: **not identified** -- not reproducible in 6
full runs; the PC/SP values point at a stack overflow or a bad jump in one of
vsh / ptyrun vfork child / sh / upkg-ui.

Fixes at the right level:

1. **vtcon `tools/rig/rig.py` (rig infra):** detect the reset loop -- `status()`
   (or a watcher) counts `CPU reset` lines in the fs-uae log after boot and
   reports `crashed: Guru at PC=..., reset loop` instead of plain `down`, keeping
   the `Illegal instruction`/`Exception` lines. Then investigate the loop itself
   in the rig's fs-uae config (`build/rig4/vtcon-rig.fs-uae`: cpu 68060-NOMMU with
   KS 3.1; check whether the soft reset needs a 68060 ROM / disable MAPROM so a
   ColdReboot boots again). Proper fix is in the rig config generator in rig.py,
   not in the test scripts.
2. **vtcon `tools/rig/userland_rig.py`:** wrap `main()` in `paths.rig_lock()` like
   upkg_browse_rig.py:135.
3. **vtcon `tools/rig/ami.py:reboot` / rig image:** ship `C:Reboot` on the rig
   (or use an exec ColdReboot path wasabid already has) so reboot works.
4. **Finding the crashing task:** run the upkg-ui package in a loop on rig 4
   (resumable, stop at the first crash) with each tty script prefixed by
   `Stack 262144` in a scratch copy vs. not; on a hit, read wasabid's debug
   stream (patches.c debug ring) / the fs-uae log exception lines and map PC
   0x108D2 / the faulting task.

## Regression tests

- rig.py: a host test feeding a captured fs-uae log with `Illegal instruction`
  followed by repeated `CPU reset` into the status/watch parser must report
  "crashed / reset loop" (fails today: reports "down"). Fixture: the preserved
  `q1-instance.fs-uae.log` lines 1200-1320.
- Rig: after the config fix, a reboot through a deliberate ColdReboot must bring
  wasabid back (ami.reboot passes) -- fails today on both counts (no C:Reboot; loop).
- userland_rig.py: starting it while another script holds rig.lock must exit with
  the "in use by pid" message (fails today: it runs).

## Fixes 2026-10-22

- Commit ec303ef8: `rig.py status` prints `crashed: <exception line>, N CPU reset(s) since` when fs-uae is alive,
  wasabid is silent and the fs-uae log (or its .tail) shows an exception followed by CPU resets
  (`paths.fsuae_crash`, next to `cap_fsuae_log`); otherwise `down`. `userland_rig.py` main now runs under
  `paths.rig_lock()`.
- Test: `make test ONLY=rig_crashed` (tools/test_rig_crashed.py, fixture tools/fixtures/fsuae-guru-reset-loop.log =
  the relevant lines of the preserved log). Proved by reverting rig.py and userland_rig.py: 2 of 5 checks fail
  ("down" instead of crashed; userland_rig runs while the lock is held); restored, all pass.
- Skipped: the fs-uae config change for a clean reboot of a crashed 68060 rig. The doc only says "check whether
  the soft reset needs a 68060 ROM / disable MAPROM"; no verified change exists, so nothing applied, rig 4 not used.
- Skipped (not asked): C:Reboot on the rig image (fix 3 above), the crash-hunting loop (fix 4).
- Seen, not mine: `make test ONLY=rig_fsuae_log` fails on a clean tree because rig.py start now refuses a rig
  dir without sys.hdf (its test uses a temp dir).
