# SolidWin title-bar drag on OS 3.2 (rig 79, FS-UAE, --max --os 32)

Verdict: PASS. With C:SolidWin 1.12 running, a title-bar drag with real-mouse events moves the window solidly.

Setup: fresh rig start does NOT install SolidWin (no User-Startup block); copied dist/solidwin/SolidWin to C: and
ran it (Run >NIL: C:SolidWin), as install.dos does with the SOLIDWIN option. Window: WB ARexx
WINDOW 'SYS:C' OPEN; CHANGEWINDOW 'SYS:C' 50 50 400 100. Geometry read from `ami.py uitree` (text).

Drag with IECLASS_RAWMOUSE (tests/amiga/rawdrag.c: press, relative moves with the left-button qualifier, release):
- before 50,50 400x100; mid-drag (button held) uitree 230,100 (window list already moved = opaque, not an outline
  that moves only on release); after 170,94 (dx +120 exact; dy follows pointer acceleration on the 256-row screen,
  rawmouse dy of 80 gave +44 rows).
- Mid-drag screenshot s31-rl2-solid-mid.png measured by PIL: new top edge row 100 is 400/400 non-background, new title
  row 387/390, new left edge 99/99; old bounds (x=170 col, y=90 row) 0 non-background pixels: window drawn at
  the intermediate position, no frame left at the old one.
- Control without SolidWin (wasabi absolute drag): uitree during drag stays 50,50, release jumps to 170,90 (outline drag).

Finding (tooling, not SolidWin): wasabi `mouse move/down/up` sends IECLASS_POINTERPOS (absolute). With SolidWin
running such a drag moves nothing (170,90 stays 170,90 through press, 12 moves, release), without SolidWin it works. So
the ledger row's "held press + mouse steps never moved the window" was SolidWin ignoring absolute pointer events, not
a pointer-mapping problem (mouse y = screen row x2 on 640x256 was right: pointer read back 55 for y 110). Any rig
check of drags while SolidWin runs needs relative RAWMOUSE events (rawdrag), or stop SolidWin first. wasabi defect
to report upstream: no relative-move drag command. Not patched.

Also seen: a "System Request" window (210x58) appeared once right after `Break 5 C` of the SolidWin process; it was gone
before it could be read (requesters list empty afterwards), cause unverified.
