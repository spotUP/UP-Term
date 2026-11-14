# DOpus boot hold check (rig 7, --os 32) -- NOT RUN, result: UNKNOWN

Verdict: neither PASS nor FAIL. No boot of the new build/amiga/Patina was observed; no splash/requester/sentinel data exists.

What happened
- `UPTERM_RIG=7 rig.py dopus --os 32` (purpose "up-term-e8 agent: DOpus boot hold check") never got past its own `rig.py start --no-dopus`
  step: no fs-uae process ever appeared. The start sat in repeated `make -q build/amiga/...` freshness checks while the machine was
  saturated (buildq queue of 9+ builds from other sessions, rig 9 start from another agent stuck in the same stage; log only showed
  `rm/mv build/vbcc-aos68k.cfg.tmp: No such file` from concurrent make runs). Waited several wait-cycles, then stopped at the call budget.
- Rig 7 stopped (`rig.py stop`), build/rig7 removed, ps confirms no rig-7 process. Rig 9 / 81 untouched.

Next time
- Start only when `buildq --status` is idle; run `make amiga` through buildq first so the start's `make -q` checks return at once.
- Then: `rig.py dopus --os 32`, `rig.py start --os 32`, confirm SYS:WBStartup/Patina.info exists and VTC Patina is the 19:18 build (md5),
  `Delete ENV:Sys/#?` for a fresh state, reboot, poll wasabi list_windows + screen title every 3 s, read
  PATINA_DESK_START_PLAN / PATINA_DESK_START_WRITES via ENV (sentinel logic: patina/desk_dos.c:1798, 1866; patina_demo_rig.py env()).
- PASS = no "Intuition is attempting to reset" requester; PLAN nonzero and WRITES set only after window `Directory Opus` (task dopus_progressbar) is gone.
