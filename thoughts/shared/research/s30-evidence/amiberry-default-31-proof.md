# Amiberry 8.2.0 default, OS 3.1 rig 58 boot proof (2026-11-02, vtcon-05) - FAIL / inconclusive
- Branch s48-amiberry-default merged with origin/main (3 merges, last 58d7641e, clean, no conflicts); tests fixed in 55157152.
- Rig tests (tests/test_rig_*.py): branch 19 failed/242 passed, main 19 failed/233 passed; identical failing set (pre-existing, start() refactor into _start). 4 extra branch-only failures fixed.
- start: build/rig58/emu = amiberry. Boot reached BOOTX shell prompt, boot.log "wasabid started".
- start rc=1: "rig: the boot lacks the assigns ETC". Lead: build/rig/vtc (rig 1 seed) missing in main checkout (rig 1 folder gc'd; only sys.hdf left), so rig 58 was seeded from an empty dir and VTC:etc is absent.
- ami.py ping: "auth failed: the Amiga took the connection but its daemon did not answer for 150 s". Amiberry CPU 75-100%. Port 1291 listened by Amiberry only.
- Not tested: RTG 800x600 switch, Assign check (no wasabid).
- Left on disk: /Users/spot/Code/UP-Term/vtcon/build/rig/{vtc,boot,logs,shots,serial.log,vtcon-rig.fs-uae} created 13:09 by this run (rm denied by classifier).

## Run 2 (2026-11-02, vtcon-05) - PARTIAL: Amiberry boots, wasabid answers; rig.py start rc=1 on missing VTC:etc
- Worktree HEAD 58d7641e (origin/main not ahead, no merge needed). build/rig58/emu = amiberry; start rc=1 "the boot lacks the assigns ETC".
- Cause confirmed: build/rig/vtc (rig 1, 37 MB, 45 entries) has NO etc drawer, so rig 58's seeded VTC: has none; `Assign ETC: EXISTS` rc 5 "ETC: not assigned". Not an Amiberry fault.
- Unlike run 1, wasabid answered: `ami.py ping` 3/3 ("wasabid 0.11b2"), `Assign SYS: EXISTS` rc 0. boot.log reached "wasabid started"; BOOTX shell prompt on serial.
- Amiberry 54-78% CPU, sample main thread = normal m68k_run_jit / do_cycles_normal / draw_line_fast (no hang). Run-1 "never answered" was not reproduced.
- Not tested: RTG 800x600 switch (rig did not finish start), desktop windows by text.
- Fix lead: restore etc into rig 1's VTC: (kit/stage source for etc), then re-run.

## Run 3 (branch s48-amiberry-default merged with origin/main 453ddd0b, worktree HEAD 719e95c6)

- Result: FAIL. `rig.py start` rc=1: "auth failed: the Amiga took the connection but its daemon did not answer for 150 s - frozen, or stuck on something". S:Shell-Startup not read, same cause.
- VTC:etc fix works: build/rig58/vtc/etc/termcap exists (the Run 2 "lacks the assigns ETC" error is gone).
- boot.log last lines: "... XCON mounted / LIBS: assigned before Amarchy / assigns done / wasabid started". Boot script ran to the end; wasabid never answered.
- Emulator Amiberry (/Applications/Amiberry.app), pid 30275, CPU 75-80% for 4+ min. `ami.py ping` also returned no answer (60 s and 80 s timeouts). Assign ETC:/SYS: checks and windows/screens not possible.
- Host: other sessions' rigs and an installer_rig run (vtcon-16) were active on the machine during this run.
