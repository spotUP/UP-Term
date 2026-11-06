# Amarchy restart: stack low-water measure (rig 177)

Recipe: rig.py start --max --mmu --os 32 (rig 177), amarchy_gui_rig.py --setup --no-build (FAIL line 'flat prefs off' as known), muforce on; VTCX:Apps/bin/Amarchy = 369616 bytes (build/amiga/Amarchy measuring build, stack 131072). Script scratchpad/loopstk.py (loopm.py + GetEnv reads), 6 instances: cycle 0 = first start after Breaking the setup instance, cycles 1-5 = Break/Run restarts. Read at +20 s (cycle 0) / +15 s (others) after port up, and after Break.

| inst | STK_SIZE | STK_LO dec (hex) | STK_HI dec (hex) | BSS_LO dec (hex) | BSS_HI dec (hex) | STK_LO-BSS_HI |
|---|---|---|---|---|---|---|
| 0 | 131072 (0x20000) | 1085548984 (0x40B429B8) | 1085680056 (0x40B629B8) | 1084889972 (0x40AA1B74) | 1085548972 (0x40B429AC) | 12 |
| 1 | 131072 (0x20000) | 1085548984 (0x40B429B8) | 1085680056 (0x40B629B8) | 1084889972 (0x40AA1B74) | 1085548972 (0x40B429AC) | 12 |
| 2 | 131072 (0x20000) | 1085551872 (0x40B43500) | 1085682944 (0x40B63500) | 1084892860 (0x40AA26BC) | 1085551860 (0x40B434F4) | 12 |
| 3 | 131072 (0x20000) | 1085554760 (0x40B44048) | 1085685832 (0x40B64048) | 1084895748 (0x40AA3204) | 1085554748 (0x40B4403C) | 12 |
| 4 | 131072 (0x20000) | 1085558792 (0x40B45008) | 1085689864 (0x40B65008) | 1084899780 (0x40AA41C4) | 1085558780 (0x40B44FFC) | 12 |
| 5 | 131072 (0x20000) | 1085562824 (0x40B45FC8) | 1085693896 (0x40B65FC8) | 1084903812 (0x40AA5184) | 1085562812 (0x40B45FBC) | 12 |

STK_USED: 16780 bytes (0x418C) at +15/20 s up and at exit, for all 6 instances (MAIN=2 while up, 4 at exit). Maximum 16780 (> 16384 stack of the production build by 396 bytes). Before cycle 0 the setup instance had left 12124.

Crashes: none. Fs-uae log Exception lines: none; MuForce hits.log: 0 bytes new in every cycle.

Not measured: restarts beyond 5; production 16384-byte stack run; STK_USED between wakes (only updated on main-loop wake/exit); the setup-run first start (replaced by cycle 0).
