## ThisTask probe at the flat-on hang (rig 10, 2026-11-01)

Question: at the flat-on hang, which task is ThisTask, and does a 10003D80-style USP sit just below that task's tc_SPLower?

Rig 10, 3.2, `--max --os 32 --debugger`, `rig.py dbg "g"` to boot. `amarchy_gui_rig.py --setup` staged the s28-crash-hunt build; md5 on the rig equals the worktree build: Amarchy 8a350a9ac4766c3e83f8ed735d2a5b7f, amarchy-flat 18d0667922958fbae7265ee377007c20 (VTCX:Apps/bin).

### Stacks before the pick (RAM:taskdump)
| task | tcb | sp | tc_SPLower-tc_SPUpper |
|---|---|---|---|
| input.device | 10009c92 | 1000ac92 | 10009cf0-1000acf0 (4 KB) |
| « IPrefs » | 10089798 | 1008b3f6 | 1008987c-1008b87c (8 KB) |
| Workbench | 100ffe10 | 10101d76 | 100ffef4-10101e34 |
| amarchy-flat | not running (STATUS `flat=absent`) | | |
Lowest stacks in the system: input.device 10009cf0, DH0 10010fd4, scsi.device 1000b138. **No task's stack lies at or near 10002000-10004000.**

### Finding: the rig 8 ThisTask read was right, and it is input.device
rig8-crash.md read the long at ExecBase+0x114 = **10009C92** and dropped it as "odd". It is even (…92), and it is exactly input.device's task address on this boot (ROM tasks are allocated in fixed order at boot; ExecBase 1000089C on both rigs; same-address match, not read at the hang on rig 10). So at the rig 8 hang **ThisTask = input.device** (inferred by the address match).

input.device tc_SPLower = 10009cf0. USP 10003D80 is **0x5F70 = 24432 bytes below** it; ISP 10002318 is lower still. That is not "just below": a 4 KB stack overflowing by about 1 KB (the patch estimate in rig8-crash-analysis.md) ends near 100098xx, not at 10003D80. IPrefs' stack is high (1008987c), so IPrefs as the low-stack owner is ruled out.

### Reproduction on rig 10: no hang (3 attempts)
1. `tools/rig/amarchy_desk_rig.py flat` (main path) with flat prefs off: completes, `[OK] 0 failed`, no hang. amarchy-flat stayed absent.
2. `put_flat_prefs(True)` (Amarchy did not start amarchy-flat within 6 s: `flat=absent`), then the same script: completes, 1 [FAIL] in its own checks, no hang. The script's main path rewrites ENV:Amarchy/prefs to `amarchy-gui-theme = white` / `desktop-theme = on`, which drops the flat keys, so the pick ran with flat off and amarchy-flat absent.
3. Direct `pick()` after `Run >NIL: VTCX:Apps/bin/amarchy-flat BOOT`: STATUS still `flat=absent` (BOOT did not stay resident), and the pick timed out in the rig script ("move in theme list: no stable state within 30s", cursor on the last row) – a script issue, wasabid kept answering.
So the flat-on hang was not reproduced: no ThisTask/USP was read at a hang on rig 10, and no held-amarchy-flat control run was possible. Runs 1-2 are pick runs with amarchy-flat absent that did not hang (consistent with, not proof of, amarchy-flat being required).

### Verdict
- ThisTask at the rig 8 hang: input.device (address match 10009C92).
- Stack-overflow theory as stated (USP just below a small system stack's tc_SPLower): **NO**. USP is 24 KB below input.device's stack and inside no task's stack; that points to SP loaded from a bad value (a corrupted saved frame / link or a wild RTE/RTS path on input.device), not a gradual overflow. The patch code runs on input.device (4 KB) still holds as the context.
- Open: a reproduction with flat on at the pick. It needs ENV:Amarchy/prefs with `desktop-theme = on` and `flat-system-look = on` together and amarchy-flat resident before the pick; the desk script's main path does not keep the flat keys.
