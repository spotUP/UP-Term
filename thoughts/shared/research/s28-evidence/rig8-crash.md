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
