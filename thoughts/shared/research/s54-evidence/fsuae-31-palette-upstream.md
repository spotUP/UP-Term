# FS-UAE upstream report draft: 3.1 palette.prefs save crashes guest (RTG switch)

Status: DRAFT, not filed. Filing is outward-facing; owner approves first.

## Summary
On FS-UAE with AmigaOS 3.1, a 68060 CPU and graphics_card = uaegfx (Picasso96), copying a palette.prefs
file to ENV:Sys/palette.prefs makes IPrefs reset the Workbench. The display switches RTG -> native -> Picasso96.
After that switch the CPU often runs from address $A6 / $7A and the guest dies.

## Steps
1. Boot AmigaOS 3.1 with uaegfx RTG Workbench, 68060.library.
2. `Copy RAM:palette.prefs ENV:Sys/palette.prefs`.
3. Watch the log during the IPrefs Workbench reset.

## Observed (log lines)
- `BEAMCON0 0000 -> 0020 PC=10031f1e` (68060.library Dispatch idle STOP), `SetSwitch() - Picasso96`
- `Illegal instruction: 0b7a at 000000A6 -> 00000000`
- then thousands of `filesys_lput/wput called PC=0000007a`, or `IDE1 DATA write without request!? 00 PC=0000007A`,
  or `CPU reset PC=f80dc8` followed by a reset loop. FS-UAE exits.

## Frequency
Bare OS, no Intuition patchers: 2 of 6 boots crash. With patchers loaded the odds rise to about 3 of 4.
The same save through the Workbench Palette prefs program on 3.2 applies cleanly.

## Evidence
thoughts/shared/research/s54-evidence/31-copy-crash-bisect.md (rounds 1-3, commits 117c7add, 766108d7).
FS-UAE version: 3.2.35, built from source at v3.2.35-3-gbf6a7ce72 (github.com/FrodeSolheim/fs-uae). The 3 local commits on top touch FPU Inf handling on native host-double builds, filesys open handles following a rename, and bsdsocket reset on exit; none touches the CPU core, chipset or RTG switching, but the report should say so and offer a retest on plain v3.2.35.

Config (tools/rig/rig.py, `--max` 060 rig, stock 3.1 ROM):
    amiga_model = A1200
    cpu = 68060-NOMMU, fpu = 68060, uae_cpu_speed = max, uae_cpu_cycle_exact = false, uae_cpu_compatible = false
    jit_compiler = 1
    fast_memory = 8192, zorro_iii_memory = 1048576
    graphics_card = uaegfx, uae_gfxcard_hardware_sprite = true
    bsdsocket_library = 1, hard_drive_0..2 = host directories, serial_port = file
Host: macOS 15 (Darwin 24.6.0), Apple Silicon.

## Stock v3.2.35 retest (s56)

Binary: plain tag v3.2.35 (`git describe` = v3.2.35, commit 4ae7ddaec, no local commits), built in a separate worktree
with `./bootstrap && ./configure && make -j2` (the main checkout's config.status has no configure arguments either).
Rig 68, `rig.py start --max --os 31` (bare stock 3.1 Workbench, 060 JIT + uaegfx), `UPTERM_FSUAE` = that binary,
confirmed in `ps`. SolidWin (started by the rig's boot script) was switched off by a `; C:SolidWin` line in S:User-Startup;
`ps` on the guest at Copy time listed only the stock Workbench process. Recipe: fresh boot (first run after start,
five after `rig.py reboot`), 20 s settle, `Copy RAM:palette.prefs ENV:Sys/palette.prefs` with the owner's GUI-save bytes, 30 s wait.

Result: 6 of 6 boots crash on plain v3.2.35 (patched build: 2 of 6). A control boot with no Copy stayed alive 100 s.
The failure differs in form from the patched-build report: the fs-uae process disappears right after the RTG -> native switch,
before any `SetSwitch() - Picasso96`, with no `Illegal instruction at 000000A6`, no `CPU reset` line, no IDE line and no
macOS crash report. The log is buffered, so its last lines are what had been flushed:
- R1 to R6: wasabi `cannot reach 127.0.0.1:1301 - Connection refused` after the Copy; `cpu_resets 0->0`, `ide 0->0`, no reset-loop file.
- Last guest-related log lines per run: `SetSwitch() - amiga` x1 to x3 (R1 3, R2 2, R3 1, R4 2, R5 3, R6 2), then nothing.
- One extra boot with SolidWin running (not counted) died the same way (`SetSwitch() - amiga` x2).
Reading: the Copy-triggered Workbench reset kills plain v3.2.35 every time, so the bug is not in the three local commits.
It is unverified whether the 6/6 host-process death and the patched build's 2/6 guest crash are the same fault or two
(the stock process exits without the guest-side jump to $A6); a debugger run (lldb on the stock binary) would tell.
