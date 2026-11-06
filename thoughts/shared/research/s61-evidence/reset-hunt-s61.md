---
date: 2026-11-06
topic: rig hard-reset after Run vsh <>XCON: on OS 3.2 (s61)
build: worktree vtcon-s61-reset at 52d2fc3a, rig 161 (os32, 060 --max; second boot --mmu)
status: cause found (strong lead), not fixed, font checks not run
---

# Reset after opening XCON: on a 3.2 rig

## Split (rig 161, 060 --max with JIT, no ENV:up-term config)

- `Run >NIL: vsh RAM:szs plain <>XCON:0/30/640/200/ftplain/CLOSE`: RESET. Same as on rig 96.
- `echo hi >XCON:0/30/640/200/t/CLOSE`: RESET (cpu resets 10 -> 12). vsh, Run and the title are not needed.
- `echo hi >CON:0/30/640/200/t/CLOSE` (stock CON:): ALIVE.
- Under `Stack 262144`: still RESET. Stack is not the cause.
- Not every open resets: some probes at 13:03-13:06 were ALIVE. The failure is intermittent.
- `rig.py wasabi guru`: "No Guru recorded (T:lastguru absent)". The debug stream showed nothing.

## The MMU run (rig 161 restarted with --mmu, which turns the JIT off, plus MuForce)

- 4 x `echo hi >XCON:...` and 1 x the full `Run >NIL: vsh ... <>XCON:...`: all ALIVE, no reset.
- MuForce logged two hits in the handler's command-cache worker:

```
LONG READ from 00000040                        PC: 40AB341C
Addr: 00000040 40B484F8 ...   Data: 00000000 40000010 ...
Name: "UP-Term command cache"
LONG READ from 00000044                        PC: 40AB3428
Name: "UP-Term command cache"
```

## Cause (strong lead)

handler/complete.c, the W22 command-cache warm-up, started by `complete_warm(opener(c))` at the first
open (vtcon_handler.c:8285):

- `warm_worker` checks `task_alive(opener)` under Forbid. `walk_init` then saves
  `w->path = BADDR(cli->cli_CommandDir)`, a pointer into the opener's CLI path list. Then it calls Permit.
- `cc_cache_warm` -> `cw_p_next` walks `node[0]` (next) and `node[1]` (lock) later, with no Forbid.
  The worker runs at priority -1.
- The opener of `echo hi >XCON:` (or the shell that parses Run's `<>XCON:`) exits as soon as the open
  replies. Its CLI path nodes and their locks are freed (UnLock) while the worker still walks them.
- The two hits match this exactly. A freed node held next = 0x10, so BADDR(0x10) = 0x40. Then
  `node[0]` was read at 0x40 and `node[1]` at 0x44 (A0 = 0x40, A1 = 0x44).
- With the JIT and no MMU, a garbage node or lock goes on to DupLock/Examine and takes the machine down.
  This would explain why the reset does not reproduce under the slower --mmu run.

It is not a recent regression: complete.c last changed on 11-01. The 11-06 font commit 0d051940 was read.
Its null-conf paths are safe (upconf_window_profile, upconf_font_fallback and upconf_font_source all check for null).

Unverified: that the JIT reset is this same use-after-free. The MMU run proves the use-after-free, but it did not reset.

## Fix direction (not made: it needs a design call)

The path must be copied while the opener cannot end. Copying the lock BPTRs under Forbid is not enough,
because the locks themselves are UnLocked when the CLI ends. DupLock is a DOS call, so it breaks Forbid.
The handler must not make DOS calls itself (see the comment at vtcon_handler.c:2325).

Options:
- (a) Hold the first ACTION_FINDINPUT/FINDOUTPUT reply until the worker has DupLock'ed the opener's path
  into a private list. The opener is blocked in DoPkt during that time, so the list is stable. The worker
  frees its copies at the end.
- (b) Warm only C: (opener = 0) at open, and walk the path at the first Tab, from a request where the
  opener is waiting.

Regression test: tests/test_complete.c. A fake path whose nodes are freed after walk_init must never be
read; the warm-up walks only its private copy.

## Font checks 1-4

NOT RUN. The budget was spent on the reset hunt.
