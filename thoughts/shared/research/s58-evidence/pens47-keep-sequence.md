---
date: 2026-11-06
topic: THEME-SAVE-RIG-SELECTS-FAIL - harness fix and one pick sequence with Keep answered, rig 148, OS 3.2
verdict: harness fixed (Keep answered, picks kept, no revert); product: pens 0-3 AND 4-7 wrong after miasma and ristretto picks, correct after tokyo-night, rose-pine, solitude, everforest
---

# Pick sequence with Keep, rig 148

Build: s58-integ merged with origin/main, code commit = harness fix. Rig 148 `--max --os 32 --idle-minutes 0`, purpose "vtcon-c1: pens 4-7 pick sequence with Keep", fresh-all [PASS].
md5 installed UP-Term:bin/x = build/amiga/x: Amarchy 2bfd910d..., amarchy-flat cc7086b7..., amarchy-icons f35bb619..., amarchy-gui 1bae23e9... (all equal).

## Cause of the timeout
`one_select` waited (wait_stable, 30 s, differ_from the open menu) for the menu to recolour BEFORE keep_requester. Observed on the rig (uitree watch, 25 s into the run): the pick opens a window "Amarchy desktop theme" (the FP-1 requester, a window, `ami.py requesters` stays empty) and the menu window disappears. The menu does not come back after Keep (watched 150 s, no "Amarchy" window; amarchy-gui and amarchy-menu processes still run). So the wait timed out with "last None", and the branch returned before Keep: the apply reverted after 15 s.
Fix: keep_requester(12) directly after the Return, before the wait; every failure path after the pick calls keep_requester(2) and states "(Keep answered)" or "(FP-1 requester not answered: the apply was left to revert)".

## Proof
Every pick still prints [FAIL] "no menu window after the pick (Keep answered)": the menu no longer stays open after a pick (product behaviour changed vs. what one_select expects: stays open and recolours). Not fixed here. But the pick is KEPT: ENV and ENVARC theme line hold the picked name 22 s after (and starship held after the diagnosis runs). No [OK] line is possible until one_select's expectation of an open menu is changed or the product reopens the menu.

## Sequence (tokyo-night first as a step to reach rose-pine with the tool's relative picks)
| Theme | prefs ENV=ENVARC | pens 0-3 read | pens 4-7 read | pens 4-7 expected (flat=1) | 0-3 | 4-7 |
|---|---|---|---|---|---|---|
| tokyo-night | no theme line (default) | 1a1b26 414868 a9b1d6 7aa2f7 | 3e4254 0e0e14 9ece6a 0e0e14 | same | yes | yes |
| rose-pine | rose-pine | faf4ed cecacd 575279 56949f | 9d9998 e1dbd5 286983 e1dbd5 | same | yes | yes |
| solitude | solitude | 101315 4b4e55 cacccc 798186 | 343d41 080a0b 9fa5a9 080a0b | same | yes | yes |
| miasma | miasma | 101315 4b4e55 cacccc 798186 (solitude's) | bb7744 b36d43 685742 8a8a7e | 4b4b4b 121212 5f875f 121212 | NO | NO |
| ristretto | ristretto | 101315 4b4e55 cacccc 798186 (solitude's) | 0000ff ff00ff 00ffff ffffff | 535154 181414 adda78 181414 | NO | NO |
| everforest | everforest | 2d353b 475258 d3c6aa 7fbbb3 | 505a5e 181d20 a7c080 181d20 | same | yes | yes |
Pens 0-3 expected = the theme's own colours (tokyo/rose/solitude/everforest reads equal the shipped theme as shown by the screenshot dominant colours); for miasma and ristretto 0-3 equal solitude's, so stale. The earlier rig 147 MISMATCH was the revert; this one is real and appears from the miasma pick on.

## Pixel samples (python PIL on rig screenshot 640x256)
(320,230) = the strip drawn in the pen 5/7 role colour: tokyo 0e0e14 (exp 0e0e14), rose-pine e1dbd5 (e1dbd5), solitude 080a0b (080a0b), miasma 080a0b (exp 121212, NO: solitude's), ristretto 080a0b (exp 181414, NO), everforest 181d20 (181d20).
(320,128) = pen 0 background: tokyo 1a1b26, rose faf4ed, solitude 101315, miasma 101315 (stale), ristretto 101315 (stale), everforest 2d353b: matches pens 0 read.
Pen 4 and 6 have no sampled element (no drawn element identified with them; top colours of the miasma shot bb7744, 685742 are pens 4 and 6 read values, 1282 and 349 px, i.e. something draws them, but expected colours differ).
Screenshots: ~/Desktop/upterm-shots/rig148-keepseq-<theme>.png. Raw: scratchpad keepseq/probe-*.txt, shot-*.png.
Not hunted: why miasma/ristretto (themes after solitude with a `desktop`-part change?) leave 0-3 stale and 4-7 wrong.
