# s78 launch fix on the LIVE v1.1 release, rig 9 (3.1 default rig, 060 --max, purpose "s78 vtcon session: v1.1 feed upgrade check")

## Feed path
The base is NOT an upkg package (dist/upkg.conf: "The base is not a package"; upkg-index.txt row `base up-term 1.1 ... 34230787`).
So the base came from the live release asset: `gh release download v1.1 -R spotUP/vtcon -p up-term.lha` (34230787 bytes, matches
the upkg-index size), unpacked with lha, installed on rig 9 by its Files/install.dos (`UPTERM_KIT=<unpacked> upkg_rig.ensure_base`,
DEST Rushhours:Tools/UP-Term). The image's older base had been removed first (Patina QUIT via ARexx; Break did not end
patina-gui/patina-menu; the empty DEST drawer stayed "in use" so ensure_base's delete was tolerated when the drawer listed empty).
No rig.py install, no build/amiga binary used. An add-on-style `upkg -f` pass against https was not run (no base package exists in the feed).

## md5 (rig UP-Term:bin vs Files/ inside the live up-term.lha)
| file | rig | live up-term.lha |
|---|---|---|
| Patina | 216ce26a6e6a5c00915143cef65c18d0 | 216ce26a6e6a5c00915143cef65c18d0 |
| patina-gui | ce8426f4ec921fabab7bfe8bc43fe19e | ce8426f4ec921fabab7bfe8bc43fe19e |
| patina-menu | d2f9c992ac4bc8610bd653becf9305d9 | d2f9c992ac4bc8610bd653becf9305d9 |
These equal the s77 local build md5s (patina-gui ce8426f4, Patina 216ce26a, patina-menu d2f9c992): v1.1 ships the fixed binaries.

## Check 1 Apps > UP-Term: PASS
Menu > Apps > UP-Term (open_apps_row). T:Patina-uterm.00 = `FailAt 2147483647 / UP-Term:bin/vsh / EndCLI >NIL:`. Windows after: one titled
`UP-Term` (plus Patina bar). Screen text: vsh banner art, "R O Y A L   K I N G S   O F   T H E   A M I G A   S C E N E",
tutorial hint, prompt `BOOTX: %` (not `1.SYS:>`). Prompt dir is BOOTX:, not `... bin %` as in s77: Patina here was started by
`Run UP-Term:bin/Patina` from the wasabi cwd, not by the --setup path; the shell is vsh either way.

## Check 2 Agents > Launch the agent, all terminals closed: PASS
UP-Term window closed first (windows: Patina bar only). Menu > Agents > "Launch the agent". Windows after: one titled `Claude` (plus Patina bar),
no vsh window; Status: only `C:Claude` (Process 11), no vsh process. Screen text: "C:Claude for the Amiga ... Workspace trust".

Not checked: pixels (text only, no screenshots taken). Rig 9 was stopped afterwards; its state is discarded by the stop.
