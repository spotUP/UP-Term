# DOpus 5.104 as desktop, rig 8 (OS 3.2, 060), 2026-10-29, probe for Amarchy
Tools: wasabi --host 127.0.0.1 --port 1241 --key rigwasabi (wrapper $D/w8.sh); $D/wbox.c (vbcc) walks IntuitionBase->FirstScreen->FirstWindow, prints Flags, and ChangeWindowBox on title substring.
Install: archive extracted on host into build/rig8/vtc/dopus_x, Installer GUI via `wasabi ui click --id 90` (Proceed), start `Run >NIL: Work:DOpus5/DirectoryOpus`; ready in ~15-25 s (first start attempt via assign+Path line silently did not start; second Run did).

## 1. DW (default after install: Workbench screen)
dopus screen -> `Workbench 640 256 8 10 444 574`. Screen flags 0x4611 (WBENCHSCREEN), title "Directory Opus  1,857,208 graphics ...".
Window list (wbox, Flags hex):
- desktop: title NULL, task "Work:DOpus5/DirectoryOpus", box 0,11 640x245 (full screen below bar), flags 08201940 = BACKDROP|BORDERLESS (no size/drag/depth/close), min 100,40.  wasabi windows --raw: border 0,0,0,0, flag col `b`.
- lister: title "RAM Disk,  1G free, 16K in use, 0% full" (path/volume line, changes), task "dopus_lister", flags 2420003f = SIZEGADGET|DRAGBAR|DEPTH|CLOSE|SIZEBRIGHT|SIZEBBOTTOM, border 4,11,18,10, min 131,77 max 65535. New lister: title "" (empty) until read, same flags. New listers open at 196,0 444x256 (screen height, top 0 = over the bar row), rows stacked.
- button bank: title NULL, task "dopus_buttons", box 556,0 84x256 (later 84x332 on own screen), flags 00200a70 = BORDERLESS, no gadgets, min=max=84x256 (fixed size), no BACKDROP.
- requester-like: Settings/Environment window: title "Environment", task "dopus_environment", box 26,33 588x189, normal drag/depth/close; installer-style. (`dopus request` not run: UNVERIFIED.)
- Workbench window still exists: "Workbench" 0,11 640x245 flags 2220107f, listed LAST (behind the DOpus backdrop desktop).

## 2. Lister vs other
- task name `dopus_lister` in wasabi windows (all listers, titled or empty); other DOpus tasks: dopus_buttons, dopus_environment, desktop = task "Work:DOpus5/DirectoryOpus" (backdrop).
- `arexx DOPUS.1 lister query all` -> `271228492 269430388` (handles, space separated, active first). `lister query <h> proc` -> decimal Task address (271316920 = 0x102BF7B8) = the addr in `wasabi ps` for dopus_lister (match by ps address, not by window addr). `lister query <h> position` -> `x/y/w/h L/C/c/l` e.g. `197/0/443/256 6/352/416/23` (+ `locked` word if locked). `lister query <h> path` -> `RAM:` (empty for a brand-new lister). `lister query active|source|dest` -> handle. `lister query <h> mode` -> `name`.
- Match lister to window: position query box == window box; or proc == ps task addr.

## 3. Set box from outside
ChangeWindowBox via wbox on lister "RAM": windows moved 100,50 300x150. DOpus accepted it: `lister query <h> position` -> `100/50/300/150 6/352/272/9` (DOpus tracks the new box, did not revert).
Then: `lister refresh <h> full`, `lister set <h> path SYS:`, `dopus refresh`, `lister new RAM:` -> the moved window stayed at 100,50 300x150 (checked 3 s after each).
BUT `lister set <h> mode icon` moved the OTHER lister (h 271228492, set via DOpus) from top 50 to 0 and width 443 (then `mode name` back to 300 wide, top 0): a mode change re-applies stored geometry per mode. (The two moved listers matched "RAM" - both were moved; the one whose handle I queried is the one that changed.)
DOpus's own: `lister set <h> position x/y/w/h` (guide 'lister set position' not read: UNVERIFIED).

## 4. DS (own screen)
- Switched via GUI: menu Settings/Environment... (wasabi `ui menu`), left list page 7th-ish (click x=100 y=196 gave the "Display Mode..." page, list gadget id 23 at 208,126 374x136), clicked a list row at y=160 (not the Workbench row; width/height fields stayed disabled so it is a fixed mode), `ui click --id 21` (Use) -> screen changed within 8 s. No ARexx command found for it (guide has none; UNVERIFIED that none exists). Config lives in Environment file (IFF, Work:DOpus5/Environment/default).
- DS: `dopus screen` -> `DOPUS.1 640 512 8 10 444 574` (name = public screen name DOPUS.1; in DW name is `Workbench`). Screen "Directory Opus" 640x512 depth 8, BarHeight 10, flags 0x4612 (no WBENCHSCREEN bit), Intuition DefaultTitle "Directory Opus". DW also BarHeight 10, screen 640x256.
- Ports (rx --ports) in BOTH modes: AREXX AssignWedge.port ConClip.rendezvous DEFICONS deficons "Directory Opus" DOPUS.1 REXX WASABI WORKBENCH. WORKBENCH port stays because real Workbench still runs (this was DOpus started on top of Workbench, NOT the LoadWB replacement boot mode).
- Guide: `Settings/Default PubScreen` menu toggles DOpus screen as default pubscreen, "valid only when Opus runs on its own screen" (not toggled: UNVERIFIED effect).
- Switching back to DW: my click on list row y=136 + Use did NOT return to Workbench (still DS after). UNVERIFIED how; rig left in DS.

## 5. Bar window vs desktop
UNVERIFIED in DW (never got back to DW). Measured only: with DOpus on its own screen, `NewShell CON:0/243/640/13/barprobe` opened on the Workbench screen (behind) as box 0,206 640x50 (min height 50 forced: CON min, not 13). Workbench screen is behind DOpus screen so nothing covers it - irrelevant for DW. In DW: window order in `windows --raw` is front to back; desktop backdrop window is listed ABOVE Workbench's window and BELOW listers/button bank; BACKDROP windows sit behind all non-backdrop windows, so a non-backdrop bar window opened later should be in front of the desktop (inferred from flags, UNVERIFIED by click). `dopus front` / `dopus back` rc 0 (screen-level, with DS) - front then back left the DOpus screen order: Workbench Screen first in list after `back`.
`dopus_buttons` bank (556,0 84x256) is a normal non-backdrop window: it overlaps any bar at its x range.

## 6. Workbench backdrop/drawers
Workbench process running (ps: Workbench pid, AsyncWB, Workbench Clipboard Server); window "Workbench" 0,11 640x245 flags 2220107f still present on Workbench screen, behind DOpus desktop. This is DOpus-over-Workbench, not replacement. True replacement (LoadWB resident as DOpus5:C/LoadDB via S:User-Startup, boot with env "workbench", guide DOpus5.guide lines 133-142) NOT tested: UNVERIFIED (needs install option + rig reboot).
