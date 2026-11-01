# s28 rig 8 (AmigaOS 3.2): flat-on under Stack 262144

Question: does turning flat on crash the 3.2 rig when Amarchy runs under `Stack 262144`?

Answer: YES, the rig stops answering. Not a Guru; no 68k exception recorded.

Run (2026-11-01 ~02:04 CET, rig 8, `rig.py start --max --os 32`):
- Deployed build/amiga Amarchy, amarchy-menu, amarchy-gui, amarchy-flat to RAM:; md5 of each equals the worktree build (Amarchy 8a350a9a..., amarchy-flat 18d06679...).
- amarchy-flat started with both flat prefs off; `Execute RAM:go` = `Stack 262144`, `Stack` (printed "Current stack size is 262144 bytes"), `Run >NIL: RAM:Amarchy`. Amarchy running: True.
- `put_flat_prefs(True)` (Delete ENV:/ENVARC: prefs then write flat on): the Delete never returned.
  Rig error verbatim: `ERR: the Amiga stopped answering while Delete was running (nothing for 25 s, and a ping got no reply)`
- `rig.py status` line verbatim: `down`
- `rig.py wasabi guru`: `no Guru recorded (T:lastguru absent)`. `wasabi ps`: `cannot reach 127.0.0.1:1241 - [Errno 60] Operation timed out`. `wasabi debug` unreachable (wasabid dead with the machine), so no task/segment map for 1000DDBC.
- FS-UAE log (build/rig8/logs/fs-uae.log.txt): no "Illegal instruction" / "Exception" line; only the boot-time `FPU EXCEPTION 1 ... PC=00F80D6C` (line 1026, pre-run, normal 3.2 boot).
- Host CPU at hang: rig 8 fs-uae 96-100% (spinning guest); rig 4 ~1%; rigs 2/3 had been 44-46% minutes earlier and were not running at the hang. Not host starvation.

Interpretation: prefs write fires Amarchy's live prefs reload (flat on); the guest then wedges (CPU-bound, wasabid dead, no Guru alert recorded). Stack 262144 does NOT avoid it, so the stack-first hypothesis is ruled out for this hang. Unverified: whether it is a guest busy loop or a silent exception loop; needs a debugger rig (`rig.py start --debugger`) to read PC.

## PC read (debugger rig, 2026-11-01)

Rig 8, 3.2, `--max --debugger` (fs-uae-debug), Amarchy/menu/gui/flat md5 equal to the worktree build, `Stack 262144` then `Run >NIL: RAM:Amarchy`. Segment list of the Amarchy processes was not read (wasabi has no seglist command; ps shows tasks only).

Reproduction: `put_flat_prefs(True)` alone, and three off/on rounds, did NOT hang. The hang came when `tools/rig/amarchy_desk_rig.py flat` (the theme-pick path, not `--flat`) ran against the same rig: wasabid went silent, rig status `down`, no Guru.

Debugger (user mode, S=0, 060 with MMU tables, IMASK=0): the CPU sits in a tight loop in HEAP DATA, not in any program or ROM:
- PC alternates 1000DEB4 / 1000DEB8 / 1000DEBA. Disassembly of the bytes there: `1000DEB4 ORI.B #$01,D0` / `1000DEB8 MOVE.B D1,D0` / `1000DEBA BNE.B $1000DEB4` (D0=1000DE03, D1=3, so BNE never falls out). These are node data words (`... 0001 1001 66F8 ...`), i.e. executing data.
- The surrounding memory (1000DDB0..1000DF00) is a DOS list / boot node region: "DOS", "uae.device", "DH3", "DH2" strings and pointers. This is the same area as the earlier `Illegal instruction 7fff at 1000DDBC` (bytes `7FFF FFFE` at 1000DDB8). So both runs: a jump/return landed in this DOS-list data block.
- A7=10003D80 (USP) points into a libc-style errno string table ("Invalid argument", "Unknown error code"), so the stack pointer is clobbered too; stack top holds text, no return address chain. ISP=10002318. A5=1000089A, A6=1000DF50, A3=D7=1048FB58, A4=1000CB54.
- ExecBase=1000089C. ThisTask not identified (word at ExecBase+0x114 = 10009C92 is odd, so the offset read was wrong).

Verdict: not in Amarchy code and not in ROM. Wild PC into heap data with a trashed A7: a corrupted return/jump target, with the stack pointer already clobbered. Open: which task owns it (needs the right ThisTask read) and how A7 got into the errno table (stack of a task whose stack memory was freed/reused?).
