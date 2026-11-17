# S84 WB title bar probe (NO VERDICT)
Build: make amiga + patina-menu at main 55224096-dirty (Patina/handler 15:29, patina-menu 15:31). Rig 84, --os32, stopped at end.
Blocked, two setup problems:
1. The rig Workbench stayed PAL / 32-colour (rig.py start and `rig.py rtg` both print "[WARN] the Workbench stayed PAL (W43)"). dripens pal: has 32 entries, pens `0 1 1 2 1 3 1 0 2 1 2 1`, so pen 253 / deep screen is not reproduced.
2. The picker could not run: patina_theme_save_rig one_select -> "open menu: no stable state within 60s", ENV:Patina/prefs theme line empty, PATINA_* sentinels all unset. Whether the Patina commodity was running was not checked.
BEFORE sample (only one): scr 100B5C00 pens `0 1 1 2 1 3 1 0 2 1 2 1` cols AAAAAA 000000 FFFFFF 6688BB; unchanged after the failed pick.
Probe script (scratchpad probe.py, not in repo) wraps patina_theme_save_rig.wait_apply_done to run dripens at apply-return, +2, +5, +10 s; reusable once a deep rig with Patina running exists.

## S84 probe 2 (deep rig 84)
Rig: default machine (no --os32), start --purpose "vtcon S84: WB title bar pens probe"; first dripens showed 32 pal entries, so `rig.py rtg` -> 256 pal entries; then `rig.py dopus` (stop first, installs DOpus 5.104) + start + rtg; dripens re-copied via ami.put to VTCX:Apps/bin/dripens. Final: 256 pal entries, DOpus5:DirectoryOpus + patina-gui + patina-menu running. Probe: `probe.py 1 1` (two selects: vantablack, then white). Raw samples (scr / cols 0-3 per sample; pens line identical in all: 0 1 1 2 1 3 1 0 2 1 2 1):
=== BEFORE t=0.6s
scr: 10030520
cols: 1A1B26 414868 A9B1D6 7AA2F7
PATINA_WBPENS=- PATINA_WBCLOSES=- PATINA_DEFERRED=- PATINA_DESK_APPLY_R=-
###  select 1
=== apply-returned t=14.8s
scr: 10030520
cols: 000000 BEBEBE 7A7A7A 8D8D8D
PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6
=== +2s t=16.8s
scr: 10030520
cols: 000000 BEBEBE 7A7A7A 8D8D8D
PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6
=== +5s t=19.8s
scr: 10030520
cols: 000000 BEBEBE 7A7A7A 8D8D8D
PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6
=== +10s t=26.7s
scr: 10030520
cols: 000000 BEBEBE 7A7A7A 8D8D8D
PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6
### FAIL unexpected: "System Request" (task « IPrefs », screen "Directory Opus  2,053,016 graphics  1,068,224,104 other", buttons Retry|Cancel)
=== END t=43.0s
scr: 10030520
cols: 000000 BEBEBE 7A7A7A 8D8D8D
PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6
### vantablack select 1
keep white: requester seen yes at 1.2 s | Keep returned True at 1.2 s | requester gone yes
=== apply-returned t=76.9s
scr: 10030520
cols: FFFFFF 000000 808080 6E6E6E
PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2 PATINA_DESK_APPLY_R=6
=== +2s t=78.9s
scr: 10030520
cols: FFFFFF 000000 808080 6E6E6E
PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2 PATINA_DESK_APPLY_R=6
=== +5s t=81.8s
scr: 10030520
cols: FFFFFF 000000 808080 6E6E6E
PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2 PATINA_DESK_APPLY_R=6
=== +10s t=88.4s
scr: 10030520
cols: FFFFFF 000000 808080 6E6E6E
PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2 PATINA_DESK_APPLY_R=6
### FAIL unexpected: "System Request" (task « IPrefs », screen "Directory Opus  2,053,016 graphics  1,068,225,264 other", buttons Retry|Cancel)
=== END t=104.0s
scr: 10030520
cols: FFFFFF 000000 808080 6E6E6E
PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2 PATINA_DESK_APPLY_R=6
Note: pal 252-255 not printed by dripens (line cut); pal 0-3 identical to cols 0-3 in every sample. Both selects ended with an IPrefs 'System Request' (Retry|Cancel) FAIL in the harness, after the +10 s samples; the colours did not change across it.
Theme hex: vantablack 000000/ffffff/505050/8d8d8d vs WB cols 000000 BEBEBE 7A7A7A 8D8D8D; white ffffff/000000/c0c0c0/6e6e6e vs WB cols FFFFFF 000000 808080 6E6E6E (fg/dim quantised, bg and accent exact).
VERDICT: STABLE-RIGHT (screen address 10030520 constant before, at return, +2/+5/+10 s and END; cols 0-3 unchanged between apply return and +10 s for both applies; no IPrefs reset overwrote them). Caveat: dim/fg pens differ from the theme hex.

## S84 probe 3 (System Request and Retry)

Rig 84, DOpus 5 installed first, start --purpose, then rtg (800x600 depth 24, 256 pal entries). Probe scripts: scratchpad probe3.py (apply hook, samples every 3 s to 60 s), probe3c.py (sample, click Retry, sample +1/+3/+6/+10/+15 s). Raw sample lines below (timestamp = seconds since probe start; pal 0-3 = cols 0-3; pal 252-255 shown).

Setup finding: after `rig.py rtg` with DOpus a System Request (Retry|Cancel, window 279x101 at 0,0, task IPrefs) was ALREADY up before any theme apply (run A, BEFORE sample reqUp=y). Retry clicks (ami.script move/button) did not clear it; Cancel did. So the first run's requester is the boot-time one, not one caused by the apply.

Run A (requester up from boot, theme select 1 -> vantablack, apply returned t=15.9 s; Retry clicked at t~32 s):
```
=== BEFORE t=0.5s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 1A1B26 414868 A9B1D6 7AA2F7
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_WBREFUSED=-  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
###  select 1
=== apply-returned t=15.9s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== +2s t=18.4s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== +5s t=21.0s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== +10s t=25.8s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== +16s t=31.6s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
### System Request seen; clicking Retry
=== retry+1s t=35.5s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== retry+3s t=38.1s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== retry+6s t=40.7s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
=== retry+10s t=44.4s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
### requester seen=True
### FAIL the menu closed on the pick (it must stay open and recolour)
=== END t=49.2s reqUp=y
  scr: 10032D70
  pens: 0 1 1 2 1 3 1 0 2 1 2 1
  cols: 000000 BEBEBE 7A7A7A 8D8D8D
  pal252-255: 3E4254 0E0E14 9ECE6A 0E0E14
  PATINA_DEFERRED=1 PATINA_DESK_APPLY_R=6 PATINA_WBCLOSES=1 PATINA_WBPENS=1 PATINA_WBREFUSED=0  (all PATINA_* vars listed in p3.log; only these four are sentinels of interest)
```
Run B (Cancel first, then select 1 again): harness failed before the hook (no menu window after Keep); the requester came up again by END t=59 s (reqUp n -> y), scr unchanged 10032D70, cols FFFFFF 000000 808080 6E6E6E, pens 0 1 1 2 1 3 1 0 2 1 2 1. Run C (probe3c, requester up, Retry clicked):
```
now t=1 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
retry+1 t=2 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
retry+3 t=5 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
retry+6 t=8 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
retry+10 t=11 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
retry+15 t=17 reqUp=y scr=10032D70 pens=0 1 1 2 1 3 1 0 2 1 2 1 cols=FFFFFF 000000 808080 6E6E6E pal252-255=3E4254 0E0E14 9ECE6A 0E0E14 PATINA_WBREFUSED=0 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_DEFERRED=2
```
VERDICT: NO-RESET. In all samples (before apply, apply+0/2/5/10/16 s, Retry+1..15 s) the screen address stayed 10032D70, pens stayed `0 1 1 2 1 3 1 0 2 1 2 1`, cols 0-3 stayed at the applied theme, pal 252-255 constant (3E4254 0E0E14 9ECE6A 0E0E14), PATINA_WBREFUSED=0. Retry never succeeded while DOpus windows were open: the requester stayed up (or returned at once) after every Retry and no Workbench reset happened. Caveat: the timing 'appears ~10+ s after apply' was not reproduced cleanly (run A: already up from rtg boot; run C: up at end of run B); second run selects 2 2 not done (budget).

## S84 probe 4 (title bar pixels)

Rig 84 (dopus, start --purpose "vtcon S84: WB title bar pixel probe", rtg 800x600, 256 pal; no System Request was up). Probe: scratchpad probe4.py (dripens + PATINA_* + `ami.grab()` front-screen pixels in memory, no PNG on disk). Picks via t.one_select: `1` = vantablack (from the tokyo-night start), then `2` = starship (from vantablack); the theme line named the picked theme each time. Keep: vantablack requester was not seen (Keep click found no window), starship requester seen and Keep clicked. Harness verdict FAIL on both only because fg/dim colours are quantised (known, probe 2).

DrawInfo pens (12): DETAIL0 BLOCK1 TEXT2 SHINE3 SHADOW4 FILL5 FILLTEXT6 BG7 HITEXT8 BARDETAIL9 BARBLOCK10 BARTRIM11 = `0 1 2 0 0 3 0 0 2 2 0 2` in both samples. So bar fill = BARBLOCK pen 10 = 0, bar text = BARDETAIL pen 9 = 2, trim = BARTRIM pen 11 = 2.

### pick 1: vantablack
```
scr: 10030520
pens: 0 1 2 0 0 3 0 0 2 2 0 2
rgb: 000000 BEBEBE 7A7A7A 000000 000000 8D8D8D 000000 000000 7A7A7A 7A7A7A 000000 7A7A7A
cols: 000000 BEBEBE 7A7A7A 8D8D8D
pal0-7: 000000 BEBEBE 7A7A7A 8D8D8D 1A1A1A 222222 B6B6B6 222222
pal248-255: CF2F00 FFFF00 7F007F EFAF4F 3E4254 0E0E14 9ECE6A 0E0E14
env: PATINA_DEFERRED=1 PATINA_WBPENS=1 PATINA_WBCLOSES=1 PATINA_WBREFUSED=0 PATINA_DESK_APPLY_R=6
```
| item | pixels | RGB | pal | pen |
|---|---|---|---|---|
| bar fill | x=400/497/500, rows 0-12 | 000000 | 0 | 10 (and 0) |
| title text | x0-259,y0-15, 825 px | 7A7A7A | 2 | 9 |
| trim line | row 13, x=400/497/500 | 7A7A7A | 2 | 11 |
| below trim | rows 14-26 | 8D8D8D | 3 | 5 (FILL; a window title, not the screen bar) |
| | row 27 000000 pal0; 28 1A1B26 (no pal); 29-39 000000; 40-41 1A1B26; 42-44 B3B3B3 pal243 |

### pick 2: starship
```
scr: 10030520
pens: 0 1 2 0 0 3 0 0 2 2 0 2
rgb: 1C1C1E F5F5F7 8E8E93 1C1C1E 1C1C1E FF9F0A 1C1C1E 1C1C1E 8E8E93 8E8E93 1C1C1E 8E8E93
cols: 1C1C1E F5F5F7 8E8E93 FF9F0A
pal0-7: 1C1C1E F5F5F7 8E8E93 FF9F0A 3A3A3C 0E0E10 FF9F0A 0E0E10
pal248-255: CF2F00 FFFF00 7F007F EFAF4F 3E4254 0E0E14 9ECE6A 0E0E14
env: PATINA_DEFERRED=2 PATINA_WBPENS=2 PATINA_WBCLOSES=2 PATINA_WBREFUSED=0 PATINA_DESK_APPLY_R=6
```
| item | pixels | RGB | pal | pen |
|---|---|---|---|---|
| bar fill | rows 0-12 | 1C1C1E | 0 | 10 |
| title text | 821 px | 8E8E93 | 2 | 9 |
| trim line | row 13 | 8E8E93 | 2 | 11 |
| below trim | rows 14-26 | FF9F0A | 3 | 5 (FILL) |
| | row 27 1C1C1E pal0; 28 000000 pal16; 29-39 1C1C1E; 40 000000 pal16; 41 1A1B26 (old Tokyo, no pal); 42-44 B3B3B3 |

Sample before any pick (tokyo-night): fill 1A1B26 pal0, trim row 13 A9B1D6 pal2, rows 14-27 7AA2F7 pal3.

Reading: the screen title bar after each pick is pen0 dark fill, pen2 grey text, 1-row pen2 grey trim: dark bar with a lighter underline reproduces on rig 84 for both picks, and it follows the palette exactly (no stale colours). The owner's "light underline" is the pen 11 trim row (7A7A7A / 8E8E93 on a 000000 / 1C1C1E bar), the text is the same grey. The 418 px of 0E0E14 (pal5/253) in x0-259,y0-15 are identical before/after and not themed (not a DrawInfo pen of the bar; unidentified). Rows 41/1A1B26 and 28/40 are non-palette/other-window pixels below the bar. Not verified: the rows 14-27 band (accent colour) belongs to a window title, not the screen bar.
