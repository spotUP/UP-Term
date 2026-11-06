---
date: 2026-11-06
topic: command cache use-after-free fix (s61)
build: worktree vtcon-s61-ccfix, commit 1bd9220b on origin/main 35d58372; rig 162 (os32, 060 --max, then --mmu + MuForce)
status: fixed; rig proofs below
---

# Command cache path copy (s61 ccfix)

## Design
- At a window's first Open the handler holds the reply. The "UP-Term command cache" worker DupLocks the opener's
  path into a private cc_path, stores it in con->path, signals, and the handler replies. Lookups walk only the copy.
- The copy is reference counted (window + warm-up). The last reference UnLocks, from a worker process
  ("UP-Term path free" when the window holds the last one).

## Host test
- `make test ONLY=complete`: [OK] complete: 601 checks, 0 failed.
- Revert proof (cc_path_copy keeps the Shell's lock instead of the DupLock):
  `FAIL tests/test_complete.c:116: lock > 5000`, `[FAIL] complete: 601 checks, 2 failed`. Restored: 0 failed.
- `make amiga`: rc 0.

## Rig 162, JIT (060 --max, os32)
- Rig handler md5 28b6009486f4548874c6d9b2d78a783f == build/amiga/vtcon-handler.
- 20 x `echo hi >XCON:0/30/640/200/t/CLOSE`: every exec `rc 0`, ping rc 0, FS-UAE "CPU reset" count 0 before, 0 after.
- Tab: C:Date copied to SYS:Utilities/TabProbeDate (path dir, not C:). In `NewShell XCON:...` typed `TabProbeD`,
  Tab, ` >RAM:tab.txt`, Return. RAM:tab.txt = "Friday 06-Nov-26 13:46:01": the completed command ran.

## Proof B (rig 162, --os 32 --mmu, MuForce on)

10 x `echo hi >XCON:0/30/640/200/t/CLOSE` via ami.py exec: `10 rc 0`.
hits.log 0 bytes after the run; `rig.py muforce log`: `no hits`. No "UP-Term command cache" hit.
