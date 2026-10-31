# FLAT-BUTTON-FILL rig proof (rig 8, commit f14268d2 worktree)

Binary: build/amiga/amarchy-flat md5 950b1aa58221ecc8b0b9357de5eaecc1, equal on rig (both OS).
Setup: palette tests/data/desk/palette-flat-pen8.prefs applied first (stock palette gives stock bevel, no flat decision), flat-system-look=on, flat-window-gadgets=on, probe gwin (GadTools: 2 buttons, string, cycle, listview).
Pens: text=2 line=3 back=0 outline=3 button=4 ptext=0.
Screenshots: ~/Desktop/upterm-shots/s28-buttonfill-{32,31}-{open,pressed,released}.png

| Check | 3.2 | 3.1 |
|---|---|---|
| 1 button/cycle solid pen_button, no outline; string/listview unfilled | PASS (pen4 0,0,255; edge 232/232 fill) | PASS (pen4 0,255,159; edge 232/232 fill) |
| 2 pressed fill = pen_line (102,136,187) | PASS 1507 px | PASS 1541 px |
| 2 pressed label = pen_ptext | FAIL: label stays (255,255,255) | FAIL: same |
| 3 text hits rise during press | FAIL: text hits=0 before/during/after | FAIL: hits=0 (calls 17->18->19) |
| 4 ring/rectfill, no Guru/hang | PASS rectfill hits=12, requesters [], rig answers | PASS hits=8, requesters [], rig answers |
| 5 test_amarchy_stack.py | not run (no amarchy-menu in worktree build) | not run |

Likely cause (code read, unverified by edit): flat.c:259 flat_tx_c requires `r->st.pen_ptext` truthy; this palette has ptext=0, so the label patch is skipped. Retest with a palette where pen_ptext != 0 to separate this from task/position mismatch in flat_tx_text.

## 3.2 STATUS
open:    rectfill calls=130 hits=12 | text installed=no | frame calls=37 flat=9
pressed: rectfill calls=179 hits=12 | text installed=yes calls=4 hits=0 | frame calls=39 flat=10
released: text calls=5 hits=0 | frame calls=41 flat=11
## 3.1 STATUS
open:    flat calls=10 | rectfill calls=68 hits=8 | text installed=yes calls=17 hits=0 | frame calls=35 flat=8
pressed: rectfill calls=69 | text calls=18 hits=0 | frame calls=37 flat=9
released: text calls=19 hits=0 | frame calls=39 flat=10

## Raw logs
### 3.2
VERSION Kickstart 47.115, Workbench 47.2 (04/10/2021)
MD5 rig 950b1aa58221ecc8b0b9357de5eaecc1 build 950b1aa58221ecc8b0b9357de5eaecc1
REQ after palette []
STATUS-list Process  1: Loaded as command: BOOTX:wasabid | Process  2: Loaded as command: BOOTX:wasabid | Process  3: Loaded as command: Workbench | Process  4: Loaded as command: Status | 
flat process listed: True
STATUS0
flat=on calls=0 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=16 hits=0
text installed=no calls=0 hits=0
frame=hooked calls=0 flat=0
POS WIN 20 20 400 190 bt=11 bl=4 V47
GAD BUTTON1 box=10,20,100,16
GAD BUTTON2 box=10,44,100,16
GAD STRING box=136,23,108,10
GAD CYCLE box=260,20,100,16
GAD LISTVIEW box=10,80,140,44

STATUS_open
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=130 hits=12
text installed=no calls=0 hits=0
frame=hooked calls=37 flat=9
GRAB 640 256
/Users/spot/Desktop/upterm-shots/s28-buttonfill-32-open.png
scale 1.0 2.0
BOX BUTTON1 (10, 20, 100, 16) full [((0, 0, 255), 1471), ((255, 255, 255), 129)] interior(2px in) [((0, 0, 255), 1023), ((255, 255, 255), 129)]
BOX BUTTON2 (10, 44, 100, 16) full [((0, 0, 255), 1507), ((255, 255, 255), 93)] interior(2px in) [((0, 0, 255), 1059), ((255, 255, 255), 93)]
BOX STRING (136, 23, 108, 10) full [((19, 169, 7), 1005), ((255, 255, 255), 75)] interior(2px in) [((19, 169, 7), 554), ((255, 255, 255), 70)]
BOX CYCLE (260, 20, 100, 16) full [((0, 0, 255), 1457), ((255, 255, 255), 119), ((19, 169, 7), 24)] interior(2px in) [((0, 0, 255), 1009), ((255, 255, 255), 119), ((19, 169, 7), 24)]
BOX LISTVIEW (10, 80, 140, 44) full [((19, 169, 7), 4832), ((255, 255, 255), 492), ((0, 0, 255), 420), ((102, 136, 187), 416)] interior(2px in) [((19, 169, 7), 4509), ((255, 255, 255), 492), ((0, 0, 255), 360), ((102, 136, 187), 79)]
BUTTON1 edge [((0, 0, 255), 232)]
press at 80 72
/Users/spot/Desktop/upterm-shots/s28-buttonfill-32-pressed.png
STATUS_pressed
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=179 hits=12
text installed=yes calls=4 hits=0
frame=hooked calls=39 flat=10
PRESSED BUTTON2 full [((102, 136, 187), 1507), ((255, 255, 255), 93)] interior [((102, 136, 187), 1059), ((255, 255, 255), 93)]
PRESSED BUTTON1 (not pressed) interior [((0, 0, 255), 1023), ((255, 255, 255), 129)]
/Users/spot/Desktop/upterm-shots/s28-buttonfill-32-released.png
RELEASED BUTTON2 interior [((0, 0, 255), 1059), ((255, 255, 255), 93)]
STATUS_released
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=180 hits=12
text installed=yes calls=5 hits=0
frame=hooked calls=41 flat=11
REQUESTERS []
RIG answers 0
### 3.1
VERSION Kickstart 40.68, Workbench 40.42 (02/18/94)
MD5 rig 950b1aa58221ecc8b0b9357de5eaecc1 build 950b1aa58221ecc8b0b9357de5eaecc1
REQ after palette []
STATUS-list Process  1: Loaded as command: BOOTX:wasabid | Process  2: Loaded as command: BOOTX:wasabid | Process  3: Loaded as command: C:upgetty | Process  4: Loaded as command: C:upgetty | Process  5: Loaded as command: UP-Term:bin/Amarchy | Process  6: Loaded as command: Status | Process  7: Loa
flat process listed: True
STATUS0
flat=on calls=0 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=68.16 inert=unknown sums=05a329c5,aa2c535e
rectfill installed=yes calls=4 hits=0
text installed=yes calls=8 hits=0
frame=hooked calls=0 flat=0
POS WIN 20 20 400 190 bt=14 bl=4 V40
GAD BUTTON1 box=10,20,100,16
GAD BUTTON2 box=10,44,100,16
GAD STRING box=136,23,108,11
GAD CYCLE box=260,20,100,16
GAD LISTVIEW box=10,80,140,48

STATUS_open
flat=on calls=10 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=68.16 inert=unknown sums=05a329c5,aa2c535e
rectfill installed=yes calls=68 hits=8
text installed=yes calls=17 hits=0
frame=hooked calls=35 flat=8
GRAB 800 600
/Users/spot/Desktop/upterm-shots/s28-buttonfill-31-open.png
scale 1.0 1.0
BOX BUTTON1 (10, 20, 100, 16) full [((0, 255, 159), 1515), ((255, 255, 255), 85)] interior(2px in) [((0, 255, 159), 1067), ((255, 255, 255), 85)]
BOX BUTTON2 (10, 44, 100, 16) full [((0, 255, 159), 1541), ((255, 255, 255), 59)] interior(2px in) [((0, 255, 159), 1093), ((255, 255, 255), 59)]
BOX STRING (136, 23, 108, 11) full [((19, 169, 7), 1142), ((255, 255, 255), 46)] interior(2px in) [((19, 169, 7), 692), ((255, 255, 255), 36)]
BOX CYCLE (260, 20, 100, 16) full [((0, 255, 159), 1479), ((255, 255, 255), 97), ((19, 169, 7), 24)] interior(2px in) [((0, 255, 159), 1031), ((255, 255, 255), 97), ((19, 169, 7), 24)]
BOX LISTVIEW (10, 80, 140, 48) full [((19, 169, 7), 5473), ((102, 136, 187), 420), ((255, 255, 255), 349), ((0, 255, 159), 302), ((0, 0, 0), 176)] interior(2px in) [((19, 169, 7), 5148), ((255, 255, 255), 349), ((0, 255, 159), 230), ((0, 0, 0), 176), ((102, 136, 187), 81)]
BUTTON1 edge [((0, 255, 159), 232)]
press at 80 72
/Users/spot/Desktop/upterm-shots/s28-buttonfill-31-pressed.png
STATUS_pressed
flat=on calls=10 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=68.16 inert=unknown sums=05a329c5,aa2c535e
rectfill installed=yes calls=69 hits=8
text installed=yes calls=18 hits=0
frame=hooked calls=37 flat=9
PRESSED BUTTON2 full [((102, 136, 187), 1541), ((255, 255, 255), 59)] interior [((102, 136, 187), 1093), ((255, 255, 255), 59)]
PRESSED BUTTON1 (not pressed) interior [((0, 255, 159), 1067), ((255, 255, 255), 85)]
/Users/spot/Desktop/upterm-shots/s28-buttonfill-31-released.png
RELEASED BUTTON2 interior [((0, 255, 159), 1093), ((255, 255, 255), 59)]
STATUS_released
flat=on calls=10 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=68.16 inert=unknown sums=05a329c5,aa2c535e
rectfill installed=yes calls=70 hits=8
text installed=yes calls=19 hits=0
frame=hooked calls=39 flat=10
REQUESTERS []
RIG answers 0

## Recheck f3c8d497
Binary build/amiga/amarchy-flat md5 08e014dc7bea4bd8b1ca491dd0558b75, equal on rig (3.2 and 3.1). Same palette/prefs/gwin method as above. Screenshots: ~/Desktop/upterm-shots/s28-buttonfill-v2-32-{open,pressed,released}.png (3.1: none, see below).
Note: a stale ENVARC:Amarchy/flat-starting crash marker from the previous run makes the first start report flat=held why=marker; delete it before each run.

### 3.2: label FAIL, fills PASS
- pens text=2 line=3 back=0 outline=3 button=4 ptext=0 (new binary running: flat=on).
- Button fill (0,0,255): BUTTON1 edge 232/232 fill, interior 1023 px; string/listview unfilled (not re-counted beyond the open histogram, unchanged vs f14268d2).
- Pressed BUTTON2 accent fill (102,136,187): 1507 px full / 1059 interior (same as before).
- Pressed label: still (255,255,255) 93 px, no dark pixels. FAIL.
- STATUS before press: text installed=no calls=0 hits=0. During press: text installed=yes calls=4 hits=0. After release: calls=5 hits=0. Text hits did not rise. FAIL.
- rectfill hits=12, requesters [], rig answers.

### 3.1: NOT MEASURED
Rig 8 on 3.1 froze (wasabid auth failed, 30 s no answer) right after the Palette USE step in two consecutive starts (including a fresh restart); the earlier f14268d2 run on the same rig passed that step. Binary md5 matched on rig before the freeze. No 3.1 numbers for f3c8d497.

## Diag run (25049c30)
Binary build/amiga/amarchy-flat md5 25049c30ef3867e9a5c322dc168617b5 (rig OS 3.2, Workbench 47.2, rig 8). Same method: stale flat-starting deleted, palette-flat-pen8 applied, flat prefs on, gwin probe, BUTTON1 held at 80 72. Script: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/ed52d4b2-2bac-4dde-9f6b-173f9639f5f8/scratchpad/diag8.py

STATUS_open
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=126 hits=12
text installed=no calls=0 hits=0
diag arms=0 rplay=0 rpother=0 task=0 box=0 bx0=0 by0=0 bx1=0 by1=0 cx=0 cy=0
frame=hooked calls=37 flat=9

STATUS_pressed
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=179 hits=12
text installed=yes calls=4 hits=0
diag arms=1 rplay=1 rpother=0 task=0 box=0 bx0=10 by0=44 bx1=109 by1=59 cx=0 cy=0
frame=hooked calls=39 flat=10

STATUS_released
flat=on calls=28 chained=0
pens text=2 line=3 back=0 outline=3 button=4 ptext=0
dopus=absent
rectfill installed=yes calls=180 hits=12
text installed=yes calls=5 hits=0
diag arms=1 rplay=1 rpother=1 task=0 box=0 bx0=10 by0=44 bx1=109 by1=59 cx=0 cy=0
frame=hooked calls=41 flat=11

REQ []

## Recheck 4ceaaf64
Binary build/amiga/amarchy-flat md5 9aebbeaa427e9b841ad5a99f153541d2, equal on rig (3.2; 3.1 md5 also matched before the failure). Method as above: stale flat-starting deleted, palette-flat-pen8 USE, flat prefs on, gwin probe, BUTTON1 (box 10,20,100,16, press at centre 80,48 in window coords) held. Script: scratchpad v3.py (derived from drv.py). Screenshots: ~/Desktop/upterm-shots/s28-buttonfill-v3-32-{open,pressed,released}.png (3.1: none).
Pens text=2 line=3 back=0 outline=3 button=4 ptext=0. Pen 0 on screen = (19,169,7) (window background pixel).

| Check | 3.2 | 3.1 |
|---|---|---|
| pressed label pen 0 (19,169,7), no (255,255,255) | PASS: 129 px pen0, 0 white | NOT MEASURED |
| pressed fill accent (102,136,187) | PASS: 1023 px interior | NOT MEASURED |
| released label normal, fill pen_button | PASS: 129 white, 0 pen0, fill (0,0,255) 1023 px | NOT MEASURED |
| text hits before/during/after | 0 (not installed) / 1 / 1 (calls 0/4/5): rises | NOT MEASURED |
| requesters, rig answers | [] / yes | - |

### 3.2 STATUS
open:     rectfill calls=130 hits=12 | text installed=no calls=0 hits=0 | frame calls=37 flat=9
pressed:  rectfill calls=179 hits=12 | text installed=yes calls=4 hits=1 | frame calls=39 flat=10
released: rectfill calls=180 hits=12 | text installed=yes calls=5 hits=1 | frame calls=41 flat=11

### 3.1: NOT MEASURED (rig fault, third time)
Two attempts (second after a 60 s wait). Both: Version FULL (Kickstart 40.68, Workbench 40.42) and md5 ok, `SYS:Prefs/Palette FROM T:flatpal.prefs USE` returned, requesters [] , then the next wasabid call (`Status`) gave "connection closed by the Amiga" (Connection reset by peer) followed by Connection refused. Between the attempts a plain Version call answered, so wasabid restarts on its own; each time it dies right after Palette USE. Last command output before the failure: `REQ after palette []`. No 3.1 numbers for 4ceaaf64.

## Recheck 4ceaaf64 on 3.1
Rig 8, Kickstart 40.68 / Workbench 40.42, no --os flag. VTC md5 on rig = 9aebbeaa427e9b841ad5a99f153541d2 (equal to the worktree build). Script: scratchpad v31b.py (v3.py without the palette step; palette applied beforehand by hand). Pens text=2 line=3 back=0 outline=3 button=4 ptext=0.

Palette workaround: Amarchy had to be stopped with `Break <n> ALL` for amarchy-gui (7), Amarchy (5), amarchy-menu (1); plain `Break <n> C` does not stop them on a fresh boot (Status still lists them). With all three gone from Status, `Palette FROM T:flatpal.prefs USE` returned 0, no requester, no reboot. A first attempt with only `Break C` left the daemon running and Palette USE raised an IPrefs System Request (Retry/Cancel); a run in which the script itself chained Break and Palette also lost wasabid once (reboot); not hunted.

| Check | 3.1 |
|---|---|
| pressed label pen 0 (19,169,7), no white | PASS: 85 px pen0, 0 white |
| pressed fill accent (102,136,187) | PASS: 1067 px interior |
| released label white | PASS: 85 white, 0 pen0 |
| released fill pen_button | PASS: (0,255,159) 1067 px (palette pen 4 on 3.1 run) |
| open (before press) | label 85 white, fill (0,255,159) 1067 px |
| STATUS text hits open/pressed/released | calls 17/18/19, hits 0/1/1 (rises on press) |
| requesters | [] ; rig answers 0 |

Screenshots: ~/Desktop/upterm-shots/s28-buttonfill-v3-31-{open,pressed,released}.png
Result: PASS on 3.1.
