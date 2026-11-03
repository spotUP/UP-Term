# Rig proof: green line, depth/zoom clone, scrollbar knob (rig 33, OS 3.2, main febc293d)

Setup: kit built 11:53 (STALE: `make dist-base` refused, "upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports"; not run, sibling repo). Installed that kit via
`UPTERM_RIG=33 UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0" INSTALLER_KEEP=1 python3 tools/rig/installer_rig.py ONLY fresh-all --purpose ...` ([PASS] fresh-all, all ticked, cold reboot inside).
Installed Amarchy + amarchy-flat were the OLD ones (md5 differed from build/amiga). Overlaid fresh build/amiga/Amarchy and amarchy-flat (built 14:37, HEAD) by wasabi put/rename (md5 of Amarchy matches build), then `wasabi reboot --yes` (cold boot). `rig.py install` itself fails (pi-llmprobe link error _strtod/_MathIeeeDoubBasBase, unrelated).
Shots: ~/Desktop/upterm-shots/vtcon16-rig33-{boot,windows,inactive,small}.png

## Q1 green line: PASS
Grab after cold boot (Welcome to Amarchy open). Screen title bar = rows y0..19; rows 17-19 of the whole 640 width: 1a1b26 (row 17 has 11 text px a9b1d6). Row 19 at x=100,300,500: 1a1b26, 1a1b26, 1a1b26. No pixel with g>0xc0, r,b<0x70 in y0..21 (none). Workbench screen title later "Amiga Workbench Release 3.2" (mode hold ended) still no green.

## Q2 depth/zoom: partly differs from the stated criterion
Windows via `wasabi arexx WORKBENCH 'WINDOW SYS:Prefs OPEN'` (+ Utilities). Prefs at 18,28 599x340, title bar y28-47. Pixels (c=1a1b26 ink, a=7aa2f7 bar, x from 570, every 2nd row):
ACTIVE Prefs:
    32 aaaaaacccccccccccccaaaaaaaaacccccccccccaaaaaaaabbb
    34 aaaaaacccccccaaaaacaaaaaaaaacaaaaaaaaacaaaaaaaabbb
    36 aaaaaacccccccaaaaacaaaaaaaaacaaacccccccccccaaaabbb
    38 aaaaaacccccccaaaaacaaaaaaaaacaaacccccccccccaaaabbb
    40 aaaaaacaaaaaaaaaaacaaaaaaaaacccccccccccccccaaaabbb
    42 aaaaaacaaaaaaaaaaacaaaaaaaaaaaaacccccccccccaaaabbb
    44 aaaaaacccccccccccccaaaaaaaaaaaaacccccccccccaaaabbb
  Depth (x576-588): back box outline, front box in top-left SOLID (rows 32-38 x576-582 filled). Zoom (x598-612): outline box + SOLID filled box x602-612 y36-45; right edge x612 complete y36-45. No full-height line at x598 (left edge rows 32-41 only).
INACTIVE Prefs (Utilities activated; colours inverted, a=ink, c=bar):
    32 ccccccaaaaaaaaaaaaacccccccccaaaaaaaaaaaccccccaabbb
    34 ccccccaacccaacccccacccccccccacccccccccaccccccaabbb
    36 ccccccaacccaacccccacccccccccacccaaaaaaaaaaaccaabbb
    38 ccccccaaaaaaacccccacccccccccacccacccccccccaccaabbb
    40 ccccccacccccccccccacccccccccaaaaacccccccccaccaabbb
    42 ccccccacccccccccccacccccccccccccacccccccccaccaabbb
    44 ccccccaaaaaaaaaaaaacccccccccccccaaaaaaaaaaaccaabbb
  Depth: two outline boxes, front box HOLLOW (interior bar colour). Zoom: two outline boxes, hollow, right edge x612 complete.
Verdict: inactive = PASS (two outline boxes, hollow). Active = front box SOLID, not hollow -> active and inactive are NOT the same shape. FAIL on "active and inactive same shape" (and on "two outline boxes" for active, unless solid fill is intended for active). No full-height stray line in either.

## Q3 scrollbar knob: NOT measured (no scroller reached)
`UP-Term:bin/amarchy-flat STATUS` before (cold boot + windows opened):
  flat=on calls=406 chained=0 / pens text=2 line=3 back=0 outline=3 button=4 ptext=0 / dopus=absent /
  rectfill installed=yes calls=2465 hits=40 shits=97 side=2 looks=1 / text installed=yes calls=1183 hits=0 /
  clone draws=89 fallback=0 / pens-held 16 refs=2 dim=250 knob=251 / frame=hooked calls=0 flat=0
`amarchy-flat DOPUS` printed nothing; STATUS after: calls=460, rectfill calls=2877 hits=48 shits=121, dopus=absent, pens-held dim=243 knob=245.
knob= is a PEN NUMBER (251 -> 245), not visibility. Knob RGB vs track RGB not measured: the Utilities window (186x170) never showed a scrollbar and the size-gadget drag (mouse down at 337,275 -> 240,170) did not resize it. dopus=absent (Directory Opus not in this install) so DOPUS had no effect to observe.
