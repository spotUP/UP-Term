# FP4.6: coreutils 9.12 port, done check

Recipe: upterm-ports pkgs/coreutils/recipe.mk (origin/main 6eb18ad, in main at cf1e644), patch 0001
(savewd: no fork, ixemul has only vfork). 86 programs in coreutils_BINS; stdbuf, who, pinky, timeout dropped
(recipe comments give the reasons).

Results (2026-10-17):
- `make coreutils JOBS=4` in upterm-ports: rc 0, check_bin OK for every binary (hunk, ixemul, no fork,
  $STACK 262144, 68020 soft-float only). Log: scratchpad cu-build.log.
- Rig 3, `UPTERM_RIG=3 python3 tools/rig/userland_rig.py --only coreutils`:
  `coreutils: PASS, 43 of 43 cases pass` (outputs byte-equal to the Mac build, build/expected/coreutils).
- Rig 3 stopped after a quiet period.

Open:
- Stack 262144 is provisional (grep's value); not measured for sort/du/ls/cp on large inputs.
- Cases cover 43 invocations over ~30 programs; the other ~55 programs only pass check_bin.
- stdbuf, who, pinky, timeout absent (libc gaps in ixemul/libixcompat: inet_pton, inet_ntop, gai_strerror, fork).
- The kit still ships GG coreutils 5.2.1 until the drawer is staged (kit-stage not run here).
