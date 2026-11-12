# Amiga startup order: patcher, Workbench-dependent commodity, indexer (2026-11-08)

Sources: A = /Users/spot/Code/Reference/amigadeveloperdocs/ADCD_2.1 ; N = /Users/spot/Code/Reference/NDK3.2R4/ReleaseNotes ; P = this project.
Method: grep of both trees. Budget-limited, ~15 calls. The local docs are thin on boot ORDER: that is stated as UNVERIFIED where it applies.

## Doc-backed facts

1. Reserved Workbench tool types: TOOLPRI=n (task priority at launch), STARTPRI=n (start order of icons in the WBStartup drawer), DONOTWAIT (Workbench does not wait for the program started from a WBStartup icon to return). Source: A Libraries_Manual_guide/node0244.html (lines 51-56 of the text-stripped page).
2. Default WAIT time for icons in the WBStartup drawer is five seconds (so a WBStartup tool that neither returns nor says DONOTWAIT holds the drawer scan for that time). Source: N wb-RelNotes line 464 (workbench.library 44.629).
3. The WBStartup tool types are shown in the Information window even outside WBStartup (DONOTWAIT/STARTPRI/WAIT; "Do not wait" gadgets exist for WBStartup icons). N wb-RelNotes lines 950-958, 1074-1075, 1089.
4. Skipping the WBStartup scan at boot: hold Shift (LoadWB, N wb-RelNotes 227-229, also `loadwb skip`) or Alt (N wb-RelNotes 940-941). Confirms the drawer is scanned by workbench.library when LoadWB starts Workbench, i.e. WBStartup tools start AFTER LoadWB has been invoked, not before.
5. WBStartup processing: tools that fail to load are not started (N wb-RelNotes 1573-1574); "wait until finished" mis-associated return messages when more than one tool asked to wait, fixed by the 47.x rewrite (N wb-RelNotes 2609-2614). Scan of the drawer is serialised against OpenWorkbenchObject() (N wb-RelNotes 1615-1616).
6. A program started from WBStartup gets a WBStartup message (argc==0, sm_ArgList), as from any icon. Source: A Libraries_Manual_guide/node023E.html, node0248.html.
7. IPrefs is a CLI program that sets up notification on the preferences files and applies them (opens/reopens screen, palette, patterns, pointer). Source: A Libraries_Manual_guide/node0229.html (lines 25-35); N IPrefs-RelNotes lines 114-116 (palette/pointer change closes and reopens the Workbench screen), 231-232 (IPrefs re-opens the screen on mode change), 206-210 (CloseWorkbench / non-WB windows on screen can block it).
8. IPrefs stack: V44.1 "stack size is now inherited from the parent CLI" (N IPrefs-RelNotes line 7); a later version "increased the IPrefs process stack size to 8192 bytes. This fixed a few inexplicable crashes" (line 56); and IPrefs was reworked to "require much less stack" (lines 318-323). So IPrefs is stack-sensitive across versions; its drawing runs on its own process stack. A patched sysiclass called from IPrefs adds frames to THAT stack.
9. IPrefs and Intuition are coupled (N IControl-RelNotes line 79 "combination of iprefs and intuition"; lines 142-143 old IPrefs/Intuition versions mismatch). Relevant to patching Intuition classes before IPrefs runs.
10. IPrefs takes the IntuitionBase master lock paths and can call OpenScreenTagList (N IPrefs-RelNotes 11-16). A patch that opens a screen/lock-heavy call or prints to a window while IPrefs holds state risks deadlock (inference, UNVERIFIED as cause).
11. 3.2 system modules: updating of system modules disabled when booting without startup-sequence (N system-startup-RelNotes 118-126). SetPatch runs inside the startup-sequence (N SetPatch-RelNotes 118, 149).
12. Project fact: console.device / XCON: / PTY: are mounted BEFORE S:User-Startup runs (P dist/install.dos lines 233-234). S:User-Startup content is created/edited by install.dos with ;BEGIN/;END blocks (P dist/install.dos 259-270, 552-568, 587-602). Today all three programs start from S:User-Startup:
   - Amarchy: `Run >NIL: UP-Term:bin/Amarchy` (install.dos 563-568)
   - amarchy-index: block ";BEGIN UP-Term Amarchy index" (install.dos ~575-585, tail seen at line 584)
   - amarchy-flat: `Run >NIL: UP-Term:bin/amarchy-flat BOOT` (install.dos 597-602; also rig FLAT_BOOT at tools/rig/amarchy_gui_rig.py:1307)
   No SYS:WBStartup use for these today (grep of dist and tools/rig finds WBStartup only in lucide-icons mapping and a rig comment about an AppMenuItem). dist/lucide-icons lines 49 and 93 only map icons.

## NOT found in local docs (UNVERIFIED)

- The exact order of IPrefs vs LoadWB inside the stock S:Startup-Sequence (ADCD grep hits for Startup-Sequence are only unrelated code examples: A Includes_and_Autodocs_3._guide/node05C1.html, AmigaMail_Vol2_guide/node017C.html). Commonly known (from memory, not verified here): stock 3.1 Startup-Sequence runs SetPatch, ..., `IPrefs`, ..., `LoadWB`, then `EndCLI`; S:User-Startup is run from Startup-Sequence via `Execute S:User-Startup` (3.1: before IPrefs? placement differs per version: UNVERIFIED).
- Whether the Workbench screen is open when WBStartup tools start (inferred yes: WBStartup is scanned by workbench.library as it initialises, which is after LoadWB; the screen opening is workbench.library startup, but no doc states "screen open before first WBStartup tool"). UNVERIFIED.
- Whether IPrefs has finished before WBStartup tools run (IPrefs is started by Startup-Sequence before LoadWB on stock layouts, but IPrefs re-applies prefs asynchronously via notification; no doc guarantees "done"). UNVERIFIED.
- Picasso96 installer placement, AfA OS / AfA_OS_Loader placement: no local docs (grep for picasso96/AfA found nothing relevant). UNVERIFIED. Memory only: P96 installs `P96:` assigns and a `Picasso96Mode` / `P96Prefs` step in S:User-Startup, and monitors load via DEVS:Monitors and LoadMonDrvs; not verified here.

## Recommended layout (reasoned from the facts above, not from a doc that states it)

(a) amarchy-flat (sysiclass patcher): must be active before Workbench opens windows AND before IPrefs draws, otherwise IPrefs/Workbench draw some imagery unpatched and the first draw through the patch happens on IPrefs's small stack. Place it in S:Startup-Sequence area before IPrefs (or S:User-Startup if the install puts User-Startup before IPrefs: check the target's Startup-Sequence, UNVERIFIED) as a headless Run with no window output (no Echo, no Open of CON:), because console output at that point has no screen. The patch must itself keep its stack usage on the stack of the caller small (drawing through the patched sysiclass runs on whatever stack IPrefs/Workbench has: fact 8). Its init must not need the Workbench screen.
(b) Amarchy commodity: SYS:WBStartup icon with `DONOTWAIT` and a high-numbered `STARTPRI` so it starts after the patcher is in and it does not hold the drawer for 5 seconds (facts 1, 2). WBStartup implies workbench.library is up, which is the screen the commodity needs (inferred). The program still has to check for the screen itself (LockPubScreen("Workbench")) as the guarantee is not documented. Keep the existing S:User-Startup block only as fallback; do not start from both.
(c) amarchy-index: also WBStartup, DONOTWAIT, later STARTPRI than (b), run at low task priority (TOOLPRI=-10 or lower, fact 1) so it does not compete with the boot; no Workbench screen needed, so it could equally stay a User-Startup `Run >NIL:` with `ChangeTaskPri`.
No output at all before Workbench opens (owner hint): consistent with fact 12 (user-startup console exists) but the stack/IPrefs crash is not explained by output alone.

## Open questions

1. Read the actual stock 3.1 and 3.2 S:Startup-Sequence from the rig images (not available in local docs) and record where IPrefs, LoadWB and `Execute S:User-Startup` sit. This decides whether S:User-Startup is before or after IPrefs.
2. KS 3.1 boot crash: capture the stack pointer and stack size of the IPrefs process when it enters the patched sysiclass (fact 8 suggests a CLI-inherited, small stack; ask the rig to log `Stack` for the boot CLI and IPrefs). Check whether the patch runs BEFORE IPrefs at all today (it starts in User-Startup, so IPrefs/LoadWB may already be running when the patch installs, i.e. patching under a live drawer).
3. 3.2 P96 boot freeze: P96 monitor/driver loading (LoadMonDrvs, P96 screenmode changes) triggers IPrefs reopening the Workbench screen (N IPrefs-RelNotes 114-116, 231-232); a patch installed or drawing during that reopen is a suspect. UNVERIFIED; needs a rig trace.
4. How do Picasso96 and AfA_OS_Loader place their patches? Needs their install scripts (not local); fetch from the installed images or Aminet.
5. Does WBStartup STARTPRI order get honoured by 3.1 workbench.library as in 3.2 (docs here are 2.1 era ADCD plus 3.2 NDK notes)? Test on the 3.1 rig.

## Verified by main session 2026-11-08: stock Startup-Sequence order

Read from the clean images (~/Downloads/clean-wb/Clean Workbench Installs/Workbench_3.1.hdf and Workbench_3.2.2.hdf, S/Startup-Sequence via xdftool):

| Step | 3.1 line | 3.2.2 line |
|---|---|---|
| SetPatch | 4 | 18 |
| AddDataTypes | 45 | 84 |
| IPrefs | 46 | 85 |
| ConClip | 47 | 86 |
| Execute S:User-Startup | 51-52 | 96-97 |
| LoadWB | 58 | 100 |
| EndCLI | 59 | 101 |

Consequence: on both versions IPrefs is already running when S:User-Startup starts amarchy-flat, so the patch is
installed under a live IPrefs today. "Before IPrefs" is reachable only by editing S:Startup-Sequence itself (insert
between AddDataTypes and IPrefs). SYS:WBStartup runs after LoadWB, so a patcher there installs under a live
Workbench and must refresh the windows already drawn.

## Owner decisions (s64, 2026-11-08; supersedes the s63 ledger note "KS 3.1 fallback only if test shows need")

1. amarchy-flat starts BEFORE IPrefs. The installer inserts one marked line into S:Startup-Sequence between
   AddDataTypes and IPrefs (no output, `>NIL:`); uninstall removes exactly that marked line. Accepted cost: we edit
   the system's own Startup-Sequence.
2. Amarchy (commodity) and amarchy-index both start from SYS:WBStartup icons with DONOTWAIT. STARTPRI: Amarchy
   before the indexer; the indexer gets a low TOOLPRI. Amarchy keeps waiting for the Workbench screen itself
   (LockPubScreen), because no doc guarantees it is open yet. User-Startup lines for all three go away.
