---
title: s28 FLATLIVE-PEN0-RESTORE, rig 10 (OS 3.2, aga256, depth 8), origin/main abffffaa
---
Question: saved palette entry 0 = 102030 but pen 0 read 000000 after flat OFF in flatlive-baseline-rig.md. Stock IPrefs or Amarchy?

Palette.prefs = entries 0-7 102030 a0b0c0 405060 d0e0f0 111111 222222 333333 444444 (ENVARC:Sys and ENV:Sys). Driver: amarchy_desk_rig put_flat_prefs + pick(),
pens read with wbpens (T:wbpens). Columns are pens 0-7; pens 252-255 read 111111 222222 333333 444444 in every row.

Run A: file in ENVARC before boot, no Amarchy running, stock IPrefs only: pen 0 = 102030 (1-7 as below). So stock IPrefs DOES apply entry 0.
Run A, then Amarchy started, flat cycles x3 (and a second identical pass):
| state            | 0      | 1      | 2      | 3      | 4      | 5      | 6      | 7      |
| start, flat off  | 102030 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |
| flat ON (1,2,3)  | 000000 | 222222 | bebebe | 8d8d8d | 0000ff | ff00ff | 00ffff | ffffff |
| flat OFF (1,2,3) | 102030 | a0b0c0 | 405060 | d0e0f0 | 0000ff | ff00ff | 00ffff | ffffff |
Run B (the baseline's order): boot with no palette file (pen 0 aa aa aa), THEN put the file in ENVARC and ENV, start Amarchy, cycles x3:
start pen 0 = aa; flat ON (1,2,3) 000000 222222 bebebe 8d8d8d 0000ff ff00ff 00ffff ffffff; flat OFF (1,2,3) 102030 a0b0c0 405060 d0e0f0 0000ff ff00ff 00ffff ffffff.

Verdict: pen 0 is restored to 102030 after every flat OFF in 6 of 6 cycles, in both setups. The FAIL in flatlive-baseline-rig.md does not reproduce on origin/main
abffffaa; no product change, no code defect found. That run's pen 0 was already 000000 before the first flat ON, i.e. its start state came from earlier
theme/backdrop activity on that rig (unrecorded), not from the saved file. Unverified: what exactly left pen 0 black there.

Pens 4-7: after flat ON they stayed stock (0000ff ff00ff 00ffff ffffff) in all 6 cycles here; the "cycle 3 takes theme colours" of the baseline did not appear.
That is the open FLAT-PEN5-MAGENTA problem (dk_live_pens SetRGB32 overwritten after desk_dos_apply), not new; pens 4-7 on flat ON remain unfixed.
