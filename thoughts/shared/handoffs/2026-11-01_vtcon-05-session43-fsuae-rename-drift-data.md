---
date: 2026-11-01
session: vtcon-05 session 43
branch: main
head: 71ee06d9
status: finished, nothing in flight
---

# vtcon-05 session 43: FS-UAE rename crash fixed, A1200 kit refresh, drift checks data

## Done
- MG-CLOSE-WINDOW-STAYS: f598bc05 (handler/closeeof.h, tests/test_closeeof.c). A1200 L:vtcon-handler md5 e24aef8c, backup L:vtcon-handler.s43bak2. mg and nano exit and the window closes.
- A1200 full kit install at vtcon f360ce97 / upterm f8ff312, every option ticked. Notes: thoughts/spot/term-findings/a1200-kit-install-s43.md.
- A1200-DRIFT-DATA: 3bf2481d (merged 44d37e26). a1200_drift checks themes and lucide.dat by md5, bar.conf by existence. A1200: 60 files, 0 differ.
- NVIM-HEAP-CORRUPT / nvim :q emulator death (todos.md s43d, findings thoughts/spot/term-findings/nvim-quit-emulator-death.md "s43d fix"):
  - FS-UAE moved to v3.2.35 + FPU commit (branch spotup/3.2.35).
  - Fix A (rename frees an a_inode other open Keys point at) 4d0a93212, fix B (bsdsocket stopped before expansion memory is freed) bf6a7ce72, branch fix/filesys-rename-open-key in /Users/spot/Code/Emulators/fs-uae.
  - /Users/spot/Code/Emulators/fs-uae/fs-uae = fixed build. fs-uae.pre-s43 = unfixed 3.2.35. The 3.1.66 binary is gone (rebuild from fix/arm64-fpu-inf if needed).
  - Guest test tools/rig/renameopen.c (353765dd), `make build/amiga/renameopen`. Revert check: unfixed binary dies 2 of 2.
  - Upstream: issue FrodeSolheim/fs-uae#392, PR #393 (A+B, fs-uae-3), PR #394 (A, main, not built on main).

## Open
- Wait for upstream review of fs-uae #393/#394.
- creep-ltx/wasabi PRs #2, #3, #4: creep merges from Tuesday 2026-11-03; #2 before #3. Pin stays f59c839 until then.
- Amarchy bar overflow on A1200 (right group runs off the edge, System cut, launch icon covers label): sent to Amarchy session vtcon-32. Ledger sub-bullet under the "2026-10-29 bar-prio" row. Not vtcon-05 work.

## Gotchas
- Rig 8 belongs to the Amarchy s28 session. Never touch rigs 1, 2, 8, 10.
- Other sessions' rigs pick up the new fs-uae binary on their next start.
