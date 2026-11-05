# Gate run 1: amarchy-flat lead (vtcon-c1, 2026-11-05)

Source dump: rig141-stuck-taskdump.txt (agent commit 6430b154, branch worktree-agent-ae15d4b04d2a14737). The Workbench readiness gate (230fa2bd) did not fix the hang; SYS:Prefs/Font hung this time.

- IPrefs waits on the WB screen LayerInfo 10104150, owned by Workbench (nest 1).
- Workbench also owns its window layer 101048f0 (nest 2) and waits SIGF_SINGLE (wait 00000010), so it sleeps in a semaphore obtain. Its semwait column (0008804c, owner 0, nest 0, queue 0) is not a resolved semaphore.
- The Workbench stack holds the word 1041d52a. That address is inside amarchy-flat hunk 0 (1041a2cc-1043d350), at offset 0x325E. So the Workbench blocks while it runs inside an amarchy-flat patch.
- Hypothesis (unproven): lock-order inversion. A flat patch on the Workbench task holds the screen LayerInfo and window layer, then obtains a semaphore. That semaphore is held by a task that waits for the LayerInfo, for example IPrefs during a palette or font reset. This fits: it passes at 800x600x24 (no palette reset), and a start by hand a minute later works.
- Next: map hunk 0 + 0x325E to the amarchy-flat function (the worktree build/amiga/amarchy-flat md5 is a6923d11; check that the rig binary matches). Name the semaphore it obtains, find its owner. A/B check: the same boot with amarchy-flat not started.
