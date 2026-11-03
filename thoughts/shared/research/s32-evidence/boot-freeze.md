---
date: 2026-11-03
topic: UAE rig boot freeze (S32)
status: narrowed, cause not proven
rig: 65 (plain 3.1 tree, KS 3.1, 68060 JIT, cpu_speed max), HEAD 952d2457
---

# Rig boot freeze: evidence

## Repro
- 2 boots of rig 65 from main; boot 2 (00:47) froze. Rate 1 of 2.
- Rig L:vtcon-handler md5 47c684cb04eb77987535418d0de29a90 = build/amiga (no drift).
- Host load at freeze: rig 65 fs-uae 85%, rig 96 fs-uae 83%, Chrome 64%, WindowServer 40%; no vitest.

## Guest state at the freeze
- boot.log complete through "wasabid started"; serial.log ends at `BOOTX: %`.
- lsof: one LISTEN on *:1298 (one wasabid; double-wasabid ruled out), one ESTABLISHED + one CLOSE_WAIT.
- wasabi ps: "the Amiga took the connection but its daemon did not answer for 15 s". Screen frozen (owner).
- No Guru (no T:lastguru).

## Emulator state (sample + lldb on live fs-uae pid 84351)
- All service threads idle in SDL_SemWait (filesys x2, ide, hardfile, bsdsocket x2). Not an emulator deadlock.
- Emulation thread 93% CPU in m68k_run_2 -> do_specialties STOP loop (newcpu.cpp:3851-3910); hsync_handler runs.
- CPU: sr=0x2000, stopped=1, intmask=0, ipl=0, pc=0x10031600, s=1.
- **intena = intena_internal = 0x682C.** (Earlier read as INTEN clear; wrong: 0x682C has bit 14 SET. See "INTENA ring" below.) intreq = 0x0041 (TBE + BLIT pending).
- ExecBase 0x10000810: IDNestCnt=0xFF, TDNestCnt=0xFF (exec thinks interrupts enabled), TaskReady empty, TaskWait non-empty.
- ThisTask 0x102db980 = NT_PROCESS "Background CLI".
- Exec code relocated to fast RAM (0x10031600 starts `46FC 2000` move #$2000,SR; then IDNestCnt handling) - 68060.library / SetPatch relocation.

## Conclusion (narrowed)
SUPERSEDED by "INTENA ring" below: INTEN is set and VERTB is served; the freeze is task level, not an interrupt lockup.
Original (wrong) reading: permanent guest lockup, INTENA master enable cleared while exec's IDNestCnt = -1 and no task ready.
CPU sits in STOP #$2000 forever: no interrupt can arrive, so timer/vblank/bsdsocket never wake any task.
Exec itself never leaves this pair (Switch sets IDNestCnt -1 and writes $C000), so some code wrote
$4000 to $DFF09A without the matching IDNestCnt, or after exec's enable.

## Lead from vtcon-ee (not proven)
ixemul-vtcon/library/trap.S:376-387 resume_sigreturn writes IDNestCnt/TDNestCnt into the task and
SysBase+0x126, then `move.w #$4000,$dff09a` if the restored IDNestCnt >= 0, else $C000.
Saved value from setjmp.c:104 (SysBase 0x126 at save time). vsh uses ixemul signals at the prompt.
Against it: the code writes SysBase 0x126 and INTENA together, so alone it leaves a consistent pair
(0 + disabled), which the next Switch heals. Our state (-1 + disabled) needs a $4000 write after
IDNestCnt was set -1. Possible if sigreturn's store/write pair is split by exec activity, or if a
patched (relocated) Switch/idle path differs from ROM. Not decided.

## Next probe (decisive)
Local fs-uae ring buffer in INTENA() (src/custom.cpp:5058): record (value, m68k PC, ThisTask) for each
write with bit 15 clear and bit 14 set; dump on hang. The last $4000 writer's PC names the culprit
(ixemul trap.S vs relocated exec vs other). Also A/B `rig.py start --no-060lib`.

## Side finding
First rig 65 start failed RIG-KIT-DERIVE (tools/rig/rig.py:760): build/amiga/vtcon-handler was rebuilt
by a concurrent make in the main checkout during the start. Rig start races a concurrent build.

## INTENA ring (vtcon-16 agent, 2026-11-03)
Probe: local fs-uae clone /Users/spot/Code/Emulators/fs-uae-intena, branch probe/intena-ring (40a17e7c2, 9a8cedf1a;
not merged, not the rigs' default binary). Ring of 256 INTENA writes touching bit 14, enabled by UPTERM_INTENA_RING=<file>,
dumped on SIGUSR2 (records v, intena before/after, PC (JIT block start), SR, A7, (A7), ThisTask + name, IDNestCnt/TDNestCnt).
Run 1: rig 66, main, --max, 8 boots; boot 3 froze ("daemon did not answer for 150 s"), the same symptom as rig 65.

**Correction: the premise "intena 0x682C = INTEN clear" is wrong.** 0x682C & 0x4000 = 0x4000: INTEN is SET
(0x682C = INTEN|EXTER|RBF|VERTB|PORTS|SOFT). Every healthy boot also shows intena 682c. Nothing leaves INTEN clear.
Frozen boot 3 ring (frames 19080-19089, dump at 19090 = ~6 min after start, so emulated time runs):
- Exactly 26 bit-14 writes per frame, the same cycle each frame: level-3 interrupt (SR 2319, ROM 00fc123a/00fc1296/
  00fc1584/00fc1602/00fbfcf6, VERTB servers), then task 'Background CLI' (102d7348) runs briefly in user mode
  (SR 0010/0019, exec ROM 00f827ec..00f828dc, user returns 100bed20, 10004c10, 00200104; supervisor code 1009c132/1009c168),
  then exec's relocated dispatcher enables and idles: last write `v c000 pc 10030e92 sr 2000` (enable), then STOP.
- intreq 0x0041 (TBE, BLIT) holds only sources INTENA has off; VERTB fires each frame and is serviced.
Verdict: no INTEN-clear writer exists. The machine is live and idle; the freeze is task level: one process
('Background CLI', a Run'd command) wakes once per vblank (WaitTOF/Delay-style poll) and nobody else runs, while
wasabid never answers. The trap.S resume_sigreturn lead is not involved (no $4000 is left standing).
Side: boots 5-8 of run 1 (after `rig.py stop --keep` between boots) failed "the boot lacks the assigns ETC" - an
artefact of reusing the kept folder, not the freeze.
Run 2 (probe 9a8cedf1a also dumps ThisTask's CLI command name, cli_Module and code bytes at ring PCs): no data.
Every boot failed "the boot lacks the assigns ETC" while main moved under it (84de0ec2 "Rigs start SolidWin at every
boot"), and the loop was SIGTERMed from outside at boot 6. Rig 66 stopped and cleared.
Next probe: one frozen boot with UPTERM_FSUAE=/Users/spot/Code/Emulators/fs-uae-intena/fs-uae UPTERM_INTENA_RING=<file>,
SIGUSR2 the fs-uae on "auth failed": the `thistask ... command '<name>'` line names the polling process; then check
whether it is wasabid (Run'd from BOOTX:go, so named "Background CLI") and what it polls. Raw ring of run 1:
scratchpad ring-run1.txt (lines 515-771 = frozen boot 3).

## Rig 91 (vtcon-ee s53)
4 boots (AGA start, reboot, RTG restart, manual start) -- no freeze. Every start exited 1 "the boot lacks the assigns ETC" with boot.log "early LIBS: assigns done" = RIG-START-ASSIGN-RACE; the rig answered afterwards. Details: thoughts/shared/research/s53-evidence/k36-rig-proof.md
