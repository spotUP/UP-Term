---
date: 2026-11-08
session: s63 amarchy
topic: where each Amarchy kit part starts at boot (Startup-Sequence / User-Startup / WBStartup)
status: research, no code changed
---

# Amiga startup order for the Amarchy kit

Note: the owner's term "system:WB-Startup" is the drawer SYS:WBStartup (no hyphen).

## 1. Rules with citations

R1. Order in the real 3.2.2 Startup-Sequence (the rig's own copy):
SetPatch (l.18) -> AddDataTypes REFRESH QUIET (l.84) -> IPrefs (l.85) -> `Execute S:User-Startup` (l.96-97) -> LoadWB (l.100) -> EndCLI >NIL: (l.101).
Source: build/rig3/os-322/S/Startup-Sequence.os. So S:User-Startup runs BEFORE Workbench exists; WBStartup is processed only when LoadWB brings Workbench up.

R2. The boot console is forced open by System-Startup (item 16) and the Startup-Sequence runs in it as a shell: NDK3.2R4/Autodocs/system-startup.doc:49-62. Anything a User-Startup line prints goes to that boot console, and a console window needs a screen. Before LoadWB the only screen is the early one; output there is what the owner sees as the requester / "Output window" with Workbench not opening. (Mechanism inferred from R1+R2 plus owner report; not stated in one doc line. Unverified.)

R3. Convention for background programs: `run >nil: prog` in User-Startup (Amiga Mail Vol 2, ADCD_2.1/AmigaMail_Vol2_guide/node0114.html:20-23). Our `Run >NIL:` lines follow it; the `Run` itself prints nothing with >NIL:, but the started program's own stdout is NIL: only if it inherits it (Run >NIL: gives the child NIL: as output).

R4. WBStartup semantics: drawer SYS:WBStartup, icons started by Workbench in STARTPRI order (STARTPRI=n; default priority ordering), DONOTWAIT tells Workbench not to wait for the program to return, TOOLPRI sets the task priority: Libraries_Manual_guide/node0244.html:51-56 (RKM "Tool Types"), also NDK3.2R4/ReleaseNotes/wb-RelNotes:464 ("Default WAIT time for icons in the WBStartup drawer is five seconds") and :956. Holding Shift at boot skips the drawer (wb-RelNotes:227, :940). DONOTWAIT/STARTPRI only make sense in WBStartup (wb-RelNotes:956).

R5. Windows and screens belong after LoadWB: Workbench-RelNotes:203 shows even Workbench blocks AmigaDOS requesters for its own scan; programs that open windows/requesters should run when Workbench is up (WBStartup), not from User-Startup.

R6. Assigns and Path belong in User-Startup (needed by everything later): Amiga Mail node0114.html:23; Picasso96 installer adds only an Assign to S:User-Startup: Picasso96Install/InstallPicasso96:474-482 and :4717-4724 (`Assign <MainPath>: ...`), and prints nothing there.

R7. P96: no start line of its own. Its installer puts the monitor in DEVS:Monitors (loaded by the OS, at library-open time) and settings in ENVARC:/Picasso96Settings; WBStartup is used only for the printer patch (InstallPicasso96:428, :501). The guide says: "Ensure that a monitor file is installed in DEVS:Monitors ... If you use a custom Startup-Sequence, ensure that this file is loaded": documentation/P96_english.guide:1436-1438. Also remove stale driver "Picasso" from Devs:Monitors OR WBStartup (guide:312). The screenmode switch ("SetSwitch() - Picasso96 800x600x32") happens when IPrefs opens the Workbench screen (after R1 l.85), so anything that opens a screen/window before then races it. (Last clause: inference from owner's rig-2 freeze note in ks31-boot-crash.md; unverified.)

R8. AfA OS 4.8 (aminet misc/os/AfAOS4.8, extracted at /Users/spot/Code/Reference/AfAOS/x/AfA_OS). Docs/AfA_OS_Documentation.txt:238-241: `c:morelibspace` "if possible in the first line of your startup-sequence but latest before the setpatch command" (:246-250 `REBOOT` variant, because OS3.1 does not reboot after the first SetPatch). :280-289: `c:AfA_OS_Loader` goes "after (beneath) the AddDatatypes command in your startup-sequence"; "Tools like Visual Prefs and Birdie should be started later than the AfA_OS_Loader or text centering will not work properly." Install_AfA_OS:141 leaves the Startup-Sequence edit to the user. So the loader is in place before IPrefs/User-Startup/LoadWB, and our SetFunction patches (amarchy-flat, Amarchy) start after it, same class as Visual Prefs/Birdie. This replaces the earlier second-hand source thoughts/shared/handoffs/s28-evidence/round-corners-afa.md:34-36, which agrees.

R9. Early-boot output: `Echo` or program banners in User-Startup (before LoadWB) are the requester trigger per owner; the kit's convention for quiet is `>NIL:` on every command (we do it) and `Echo >NIL:` is not needed if nothing prints.

## 2. Our current lines (dist/install.dos)

All are appended to S:User-Startup, i.e. run at R1 l.96, before LoadWB:

| Block | Line(s) | Command | Now | Docs say |
|---|---|---|---|---|
| UP-Term assign | 261-267 | `If EXISTS` + `Assign UP-Term:` | User-Startup | right (R6) |
| UP-Term (GG:) | 848-856 | `Assign GG: UP-Term:` | User-Startup | right (R6) |
| tmp | 892-897 | `Assign TMP: T:` | User-Startup | right (R6) |
| AmiSSL | install-amissl.dos:46-54 | Assign, Path | User-Startup | right (R6) |
| Amarchy | 563-568 | `Run >NIL: UP-Term:bin/Amarchy` | User-Startup | WBStartup (R4,R5): it opens windows/menu |
| Amarchy index | 580-585 | `Run >NIL: UP-Term:bin/amarchy-index` | User-Startup | WBStartup or stay (no UI; only a scan; R3) |
| Amarchy-Flat | 597-602 | `Run >NIL: UP-Term:bin/amarchy-flat BOOT` | User-Startup | AfA OS reskin and P96 screen are set by IPrefs; flat patches SetFunction on intuition/graphics. Must start after AfA loader and (to be safe) after Workbench screen: WBStartup, STARTPRI after AfA (R7,R8) |
| serial / wasabi / SolidWin | 961, 1022, 1049 | `Run >NIL: C:upgetty/wasabid/SolidWin` | User-Startup | daemons, no output: may stay (R3); they open no windows |
| console / device | 1272, 1313 | `C:UPConsole >NIL: CON ON / DEVICE ON` | User-Startup | replaces CON: handler; must be before windows open: stay (R6 spirit; unverified) |
| RemLib (OS 3.0/3.1) | 1146 | in S:Startup-Sequence | before SetPatch | right: ROM-resident lib removal must precede SetPatch |

## 3. Proposed layout

- S:Startup-Sequence: only RemLib (existing, before SetPatch). AfA OS lines are the AfA installer's (morelibspace first; AfA_OS_Loader after AddDataTypes). We do not touch them (R8).
- S:User-Startup (pre-Workbench, silent, no windows): the assigns, Path, console/device handler switch, daemons (upgetty, wasabid, SolidWin). Every line `>NIL:` and guarded (R3, R6).
- SYS:WBStartup (after LoadWB): Amarchy, amarchy-index, amarchy-flat as project/tool icons with `DONOTWAIT`, STARTPRI ordering index < flat < Amarchy (or per owner), and TOOLPRI left default (R4, R5). Uninstall deletes the icons by name; Update replaces the old User-Startup blocks via unstartup.sh.
- Boot.log marker lines (rig) must also move: the "UP-Term shadows done" sequencing in ks31-boot-crash.md row 11 is the check for leg W.

## 4. Open questions only the owner can answer

1. Which hard evidence for "echo before WB gives requester"? Is it from a rig boot.log line, so we can name the exact line that prints?
2. RESOLVED: AfA OS archive fetched to /Users/spot/Code/Reference/AfAOS; R8 now cites its documentation.
3. Should amarchy-flat also run on a Shift-skipped WBStartup boot (then Amarchy would not start; accept)?
4. Should the console/device switch (UPConsole) stay in User-Startup although it runs before IPrefs finishes? (It must precede any window.)
5. Should kits on KS 3.1 (no WBStartup STARTPRI handling by 3.1 workbench.library 40? unverified) keep User-Startup as a fallback?

## Owner decisions (AskUserQuestion, 2026-11-08)

- Layout adopted: silent assigns, AmiSSL, UPConsole and daemons stay in S:User-Startup (all >NIL:); Amarchy, amarchy-index
  and amarchy-flat move to SYS:WBStartup as DONOTWAIT icons ordered by STARTPRI. A User-Startup fallback for KS 3.1 is kept
  only if a rig test shows KS 3.1 does not honour STARTPRI/DONOTWAIT. Accepted cost: Shift at boot skips the Amarchy set.
- Fetch aminet misc/os/AfAOS4.8 into /Users/spot/Code/Reference so the AfA OS start rule rests on its own readme/install script.
