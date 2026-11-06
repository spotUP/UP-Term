# rig 96 eye-check on 3.2 (vtcon-ee s54, main ecba5144, kit from fb6aa600, no code diff since)
- fresh-all: [PASS] with UPTERM_RIG_FLAGS="--os 32 --max" INSTALLER_KEEP=1; Version = Kickstart 47.115, Workbench 47.2, exec.library 47.13
- amarchy STATUS after reboot: rows=71 enabled=1 bar=1 ticks=121 (ok 34/35 passed)
- Screenshot 1 (fresh boot, window f1host active): backdrop 0E0E14 at 3 points PASS; no EE9900/6688BB/000000/FFFFFF pixels PASS; frame/title 7AA2F7 active, flat 1A1B26 interior
- Screenshot 2 (two CON: shells): ShellB active frame+title 7AA2F7, ShellA inactive frame+title 5A5E75 PASS
- Knob: UNRESOLVED. Prop gadget 23312 is all 1A1B26 in active and inactive windows (no 7AA2F7/5A5E75 knob pixels); CON: windows had no scrollback (typed keys did not reach shell). Needs a window with overflowing content.
- Not done: arrow click ring check, UPDemo (tool-call budget exceeded). amarchy-flat STATUS: Unknown command via exec (binary is build/dist/up-term/Files/amarchy-flat, not on path); ARexx AMARCHY STATUS used instead.
- Warnings: updemo assets (earth.anim, kanim-stream.bin, photo.png) missing in UP-Term: tree; Directory Opus not installed on rig; Workbench stayed PAL (W43).

## Second pass (2026-11-03, agent: knob / arrow ring / UPDemo)
- Windows: XCON:20/30/400/120/FillA/CLOSE and XCON:200/60/400/120/FillB/CLOSE via NewShell FROM RAM:fill (List SYS:C, SYS:Libs x). Content overflowed (scrolled with up arrow).
- Knob: FAIL. FillB prop (586,72 10x75) 750 px all 1A1B26 while active; FillA prop (406,42 10x75) 750 px all 1A1B26 (active, FillB sent behind) and inactive. Expected 7AA2F7 / 5A5E75: no knob drawn. amarchy-flat running, binary size equals build/dist (125816). Arrow glyphs draw in A9B1D6, no box outline. Shots: rig96-eyecheck-32-1-knob-none.png
- Arrow ring: PASS. 6 clicks on FillA up/down arrows (411,123 / 411,134); right edge x418-419 y41-149, bottom y149 x20-419, left x20-21: 0 non-7AA2F7 pixels. Shot: rig96-eyecheck-32-2-arrow-ring.png
- UPDemo: NOT VERIFIED. `UPDemo SCENE 15` -> "wrong number of arguments" (template SCENE/N; `UPDemo 15` parses). UPDemo is C:UPDemo. Via exec it exits at once (no tty); ami.py type/key text never reached the XCON window (Echo to RAM: file not created), so it could not be started inside an UP-Term window.
- Closed FillA/FillB; deleted RAM:fill etc.

## Third pass (2026-11-03, agent: typing cause / UPDemo 15)
- Typing cause: not an ami.py bug. Keys go to the ACTIVE window; FillB and ShellA sit behind f1host (600x200 at 0,20), so every click on FillB's area lands on f1host (becomes active). Typed text then went to f1host (vsh, Unix syntax: `Echo x TO file` is not a redirect). Recipe: open a fresh window from the active shell (`NewShell XCON:x/y/w/h/Title/CLOSE`), it comes up in front and active (uitree flag), then `ami type` + `ami key 44 0` (Return) works. Verified: file written, read back by exec.
- UPDemo 15 in a 72x22 XCON window: "UPDemo: the window is 72x22, it needs 76x20" (needs >=76 columns). In XCON:0/12/640/230/UPD2/CLOSE: scene 15 draws. PASS: window interior went from 3 colours (130582 px background 1A1B26) to 89 colours; 28284 px differ from the pre-run shot (E5E5E5 4011, FFFFFF 3678 px).
- q: process gone from Status, prompt back. PASS: post-quit shot has 3 colours (1A1B26, A9B1D6, 7AA2F7); 397 px differ from pre-run, all within bbox (0,0,156,9) of the window (the new prompt lines); no demo colours left.
- Shots: ~/Desktop/upterm-shots/rig96-updemo-32-{0..6}.png (4 pre, 5 run, 6 post for UPD2).
- Closed UPD, UPD2 (EndCLI). FillB left: covered by f1host, no close/depth gadget in uitree, `ami ui click` needs a gadget; its CLI is process 17.
