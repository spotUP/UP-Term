---
date: 2026-11-06
topic: S57-PENS47-UNOWNED - theme picks in sequence on a fresh full install, no prefs writes, rig 147, OS 3.2
verdict: MISMATCH on every pick (pens 0-3 stale at tokyo-night, pens 4-7 at graphics defaults); the earlier "ristretto matched" did not repeat
---

# Pens 0-7 after a sequence of theme picks, rig 147, OS 3.2

Build: worktree vtcon-s58-integ, branch s58-integ, code HEAD 25167fd2 (72a18ad3 on top is an evidence commit). Rig 147 `--max --os 32 --idle-minutes 0 --allow-stale-base`, purpose "vtcon-c1: pens47 pick sequence". No prefs writes, no other action on the rig.

## md5 (installed UP-Term:bin/<x> = build/amiga/<x>)
Amarchy e70d5d25..., amarchy-flat a8315ed5..., amarchy-icons 9f699412..., amarchy-gui 39f5f69b...: all four equal.

## Deviation from the brief
Picks are relative (`--selects N-N` = N Down or Up from the current row, direction chosen by the tool). The first pick (3) landed on rose-pine, not ristretto; Up 2 to ristretto is not expressible. Actual order: A tokyo-night (install), rose-pine, solitude, miasma, ristretto, everforest. Ristretto is therefore the fifth state, miasma before it. All picks confirmed by the prefs theme line.

## States (pens 0-3 = LPEN 0-3, pens 4-7 = LPEN 4-7)
| State | prefs amarchy-gui-theme (ENV and ENVARC) | pens 0-3 measured | pens 4-7 measured | pens 4-7 expected | verdict 0-3 / 4-7 |
|---|---|---|---|---|---|
| A install | no theme line (comment header only) | 1a1b26 414868 a9b1d6 7aa2f7 | 3e4254 0e0e14 9ece6a 0e0e14 | tokyo-night same | MATCH / MATCH |
| rose-pine | rose-pine | same as A | 0000ff ff00ff 00ffff ffffff | not computed | MISMATCH vs theme (0-3 unchanged) / MISMATCH |
| solitude | solitude | same as A | 0000ff ff00ff 00ffff ffffff | not computed | same |
| miasma (C) | miasma | same as A | 0000ff ff00ff 00ffff ffffff | 4b4b4b 121212 5f875f 121212 | MISMATCH / MISMATCH |
| ristretto (B) | ristretto | same as A | 0000ff ff00ff 00ffff ffffff | 535154 181414 adda78 181414 | MISMATCH / MISMATCH |
| everforest (D) | everforest | same as A | 0000ff ff00ff 00ffff ffffff | not computed | MISMATCH / MISMATCH |

Expected pens 0-3 for non-tokyo themes were not computed (no expected file for them); they cannot equal tokyo-night values, so 0-3 count as stale. Holder in every state: `owners=7 held=16 refs=2`, ROLES rcount=4, all four role pens have=1, holder `want` for 4-7 = 0000ff ff00ff 00ffff ffffff after the first pick (in A: the theme colours). In the everforest state the screen pointer printed 10058c58, others 10058c60.
Requesters (`ami.py requesters`): empty in all states.

Why this differs from rig147 earlier run (not hunted): that run recorded ristretto as matching; here the first pick already left the stock colours. Both runs started from a fresh-all install; the earlier run also wrote prefs by hand, this one did not, so the hand write is not the cause of C/D.

## Commands
- Install: `UPTERM_RIG=147 UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0 --allow-stale-base '...'" INSTALLER_KEEP=1 python3 tools/rig/installer_rig.py ONLY fresh-all --purpose "vtcon-c1: pens47 pick sequence"` -> `[PASS] fresh-all`.
- Per state: `ami.py put rolepens RAM:rolepens; ami.py exec RAM:rolepens; ami.py exec "Type ENV:Amarchy/prefs"` (and ENVARC:); `ami.py requesters`; `ami.py shot`.
- Picks: `amarchy_theme_save_rig.py --selects N-N --no-build` with N = 3, 1, 5, 3, 13 (FAIL "menu closed" ignored).
- Stop: `UPTERM_RIG=147 python3 tools/rig/rig.py stop`; build/rig147 removed.
- Screenshots: ~/Desktop/upterm-shots/rig147-seq-{solitude,miasma,ristretto,everforest}.png (not looked at). Raw reads: scratchpad pens47-seq/*.txt.
