---
date: 2026-11-06
topic: Amarchy start freeze on a 3.2 rig, hunt 2
build: te9-1 eec3bc56 (worktree vtcon-te10-8)
rig: 171, AmigaOS 3.2, 060 --max, --allow-stale-base
status: UNPROVEN (exact call); needed step and failure class MEASURED
---

# Amarchy start freeze, hunt 2

## Verdict

UNPROVEN for the exact function. Measured:

- The "freeze" is not a lock deadlock. It is a CPU address error (68k Exception 3) followed by a CPU reset. The guest reboots, so wasabi stops answering.
- Both faulting PCs are inside the hunk 0 range of the Amarchy commodity binary.
- The needed earlier step is a Break of a running Amarchy instance in the same boot, followed by a new `Run Amarchy`. A first start on a fresh boot never crashed (0 of 3). A Break + restart crashed 2 of 4 times.

## Probes

### Boot 1 (P1, capture)
- Status before setup: only wasabid, Status, SolidWin, Workbench. No Amarchy process. So on a fresh boot the setup step "Break the earlier amarchy-flat resident" breaks nothing.
- `amarchy_gui_rig.py --setup --no-build` with `wasabi snoop --entry --task '#?amarchy#?' --ignore-wasabi --for 200` running in the background.
- Result: Amarchy started and its port answered. One 15 s "daemon did not answer" at 16:41 while snoop streamed. ps answered again at 16:42:50. This was snoop load (Amarchy reloads ENV:Amarchy/prefs and the manifest at about 1 Hz; bar idle calls FindTask often). No exception in fs-uae.log.
- ps after setup (stack FREE): amarchy-gui 261614/262144, amarchy-menu 261386/262144, Amarchy 16154/16384, Background amarchy-gui 64594/65536, amarchy-flat 11486/16384, Amarchy bar idle 3998/4096.

### Boot 2 (Break + restart loop)
- Fresh setup, then 90 s of pings: alive.
- Loop iteration 1: Break of Amarchy and all other amarchy processes (setup printed "ok setup: the earlier amarchy-flat resident unloaded on Break"), then `Run >NIL: VTCX:Apps/bin/Amarchy` (amarchy_gui_rig.py start_amarchy, line 1307).
- Connection lost at 16:49:09 at the Run call.
- fs-uae.log line 1245: `Exception 3 (1025da36) at 1025da36 -> f80bc2!`, then `CPU reset PC=f80edc (Kickstart ROM)`, then a reset at f800d0.

### Boot 3 (after the crash reboot, Break + restart loop with seglists)
- First setup, no Break: alive.
- Seglist of the Amarchy commodity ("Background CLI (VTCX:Apps/bin/Amarchy)") read via pr_CLI -> cli_Module: hunk 0 = 0x10235920-0x1028B0A8 (size 0x55788, same as build/amiga/Amarchy hunk 0). Same address on every iteration.
- Other hunk 0 starts: amarchy-flat 0x10856918, amarchy-menu 0x1032BEE0, amarchy-gui 0x103B7C08 or 0x103AA320, Background amarchy-gui 0x10706C68.
- Iterations 1 and 2: alive. Iteration 3 crashed.
- fs-uae.log line 1669: `Exception 3 (1028924e) at 1028924e -> f80bc2!`; line 1670: `CPU reset PC=f80edc (Kickstart ROM)..`.

### Not run
- P2 (start without the Break-unload): covered in practice. On a fresh boot the unload step breaks nothing, and the first start never crashed.
- P3, P4, P5: budget spent on boots 2-3 once the crash reproduced. P4 is answered by the stack sample (see H2). P5 (origin/main binaries) is not done.

## Blocking task and call

- Call where the host saw it: `Run >NIL: VTCX:Apps/bin/Amarchy` after a Break of the earlier Amarchy.
- Faulting code: Amarchy hunk 0, offsets 0x28116 (boot 2) and 0x5392E (boot 3). Address error = odd-address word/long access or jump to an odd address.
- Task: UNPROVEN. Because the old and new Amarchy load at the same address, the PC cannot tell whether (a) a leftover from the Broken instance (patch gate, hook, task, interrupt) jumps into the freed old code that is now partly overwritten by the new load, or (b) the new Amarchy crashes on a path that runs only when leftovers of the old instance exist. The bytes at 0x28116 look like data, not code, which favours (a): a stale pointer into code memory that the new load or another allocation has overwritten.
- No symbol hunk in the binary, so the offsets cannot be mapped to a function.

## Needed earlier step

A running Amarchy instance is Broken and a new Amarchy is started in the same boot. Not a Break of amarchy-flat alone (that was not tested separately).

## Hypotheses

- H1 (needs the Break-unload of amarchy-flat): PARTLY. The needed step is the Break of a running Amarchy (the loop breaks Amarchy and amarchy-flat together), then a restart. On a fresh boot the unload step does nothing, and those starts never crashed.
- H2 (small stack): NOT SUPPORTED. All sampled stacks have large headroom; smallest is bar idle 3998 of 4096 free, amarchy-flat 11486 of 16384. An address error at a code address is not a stack overflow pattern.
- H3 (patch chain restore): PLAUSIBLE, UNPROVEN. Candidate in Amarchy itself: amarchy/wbreset_patch.c patches intuition CloseWorkBench (LVO -78) and OpenWorkBench (LVO -210) through ipatch gates whose hooks (close_hook, open_hook) are in Amarchy's segment. wbr_patch_remove sets pass-through and unarms, but when busy it returns without freeing; any path that leaves a gate pointing at Amarchy code after exit gives exactly this crash on the next WB call. The "Amarchy bar idle" task (amarchy/bar.c:72-100, entry in Amarchy's segment) is the other candidate if idle_stop does not wait for it to exit; in the one sample after a Break only one bar idle task existed.
- C2 (palette write): NOT INVOLVED. No palette write in this recipe.
- C3 (amarchy-flat unload / patch removal on Break): PLAUSIBLE, UNPROVEN. Same class as H3. The faulting PCs are in Amarchy, not amarchy-flat, so the stale pointer most likely points into Amarchy's own freed segment.

## Fix shape (to confirm before coding)

- Level: Amarchy exit path (shutdown order), not the rig script.
- Files: amarchy/wbreset_patch.c (wbr_patch_remove: never leave an armed or callable gate that points into the segment that is about to be unloaded), amarchy/bar.c (idle_stop must wait until the idle task has really left Amarchy's code). Also any other hook Amarchy registers with amarchy-flat, Intuition or commodities.
- Host test that would fail before the fix: drive Amarchy's shutdown in a host build with a fake ipatch layer and a fake task layer; after shutdown, assert no gate, hook or task entry still points into the Amarchy code range (the busy case of wbr_patch_remove included).

## Next single probe

Map the two offsets to functions: link Amarchy with a map (vlink -M) or with symbols, same build, then read 0x28116 and 0x5392E. Alternative: `rig.py start --max --mmu --os 32` with MuForce on, repeat the Break + restart loop, and read the faulting task name and stack from `rig.py muforce log`.

## UNVERIFIED

- Which task executed the faulting instruction.
- The function at offsets 0x28116 and 0x5392E.
- Whether origin/main 319fd963 shows the same crash (data point from rig 172 is unverified; P5 not run).
- Whether a Break of amarchy-flat alone, or of Amarchy alone, is enough.
- Whether wbr_patch_remove took its busy path in these runs.
