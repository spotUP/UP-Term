# Launching a Workbench icon by script (A1200 and rigs) -- 2026-10-17

Question: how to "double-click" an icon from a script, exactly as Workbench does (project vs tool, default
tool, stack, tool types such as WINDOW), on the owner's A1200 and on the rigs. Background:
2026-10-17_shellicon-a1200.md, "Not achieved: step 3" (wbrun seemed to open no window, C:WBRun said
"not enough memory", a DOpus double-click opened the Information window).

## Answer

`tests/amiga/wbrun.c` (rewritten) builds the WBStartup itself, as Workbench does. It is the right primitive on
both machines; OpenWorkbenchObjectA is not (below). Regression test: `tools/rig/wbrun_rig.py`, passes on rig 3
(OS 3.1) and on the A1200 (OS 3.2), checked through wasabi's window list, no screenshots.

    wbrun NAME [NAME ...]

- NAME's icon is a project (WBPROJECT): its default tool is loaded from the project's drawer, then the
  shell's path, then C:. Arguments are the tool, then the project. Stack (min 4096) and TOOLPRI from the
  project icon.
- Otherwise NAME is the tool; further NAMEs are the shift-selected icons (the old `wbrun TOOL PROJECT` form,
  still used by tools/rig/install_rig.py, keeps working).
- The process: NP_Input/Output 0, no console task, no window pointer, NP_HomeDir = the tool's drawer, no
  NP_CurrentDir (2026-10-19: dos never unlocks it at exit, the DupLock of UP-Term:bin leaked and Uninstall
  left the drawer; the real icon start leaks nothing), NP_Cli FALSE; sm_ToolWindow from TOOLWINDOW. The program reads its own tool types (WINDOW, FROM)
  from the WBArgs, as under Workbench.
- wbrun waits for the message to come back (it holds the tool's seglist): use `Run >NIL: wbrun ...` for a
  tool that stays open.

## Root causes

1. **C:WBRun "not enough memory"**: WBRun 45.7 calls workbench.library OpenWorkbenchObjectA. On the A1200
   (workbench.library 47.42) a probe calling it for SYS:Utilities/Clock got FALSE with IoErr 103
   (ERROR_NO_FREE_STORE); WBRun prints that IoErr as "not enough memory". Directory Opus 5 replaces Workbench
   there (process 1 "Loaded as command: LoadWB", ARexx port DOPUS.1, no WORKBENCH port): a "Workbench" task
   exists but nothing serves the launch requests. Rig 3 is the same setup on OS 3.1 (workbench.library 40.5, no
   OpenWorkbenchObjectA at all). So the library call is unusable on both; memory was never the problem.
2. **Old wbrun "opened no window"**: it did open one. On the A1200 `wbrun SYS:System/CLI SYS:System/Shell`
   added a third AmigaShell window, 680x270 at 0,0, inactive, listed behind two identical full-screen
   AmigaShell windows (task CON) already there. It was hidden, not missing. The two older windows are most
   likely leftovers of the earlier session's attempts (unverified); they were left in place.
3. **Old wbrun deviations** (fixed): caller had to name the tool (no project resolution), fixed 65536 stack
   instead of the icon's, TOOLPRI and TOOLWINDOW ignored, NP_CurrentDir 0 and no PROGDIR:, console task and
   window pointer inherited from the shell.

## SYS:System/CLI

The Shell icon is a project with default tool SYS:System/CLI. Both machines have the file: cli 38.1 on rig 3
(OS 3.1, 1180 bytes), cli 47.2 on the A1200 (1068 bytes). The 47.2 disassembly: it uses arg[1] when
sm_NumArgs > 1, reads STACK, WINDOW (default `CON:0/50//130/AmigaShell/CLOSE/ICONIFY`) and FROM (default
S:Shell-Startup) from that icon, copies the path of the reply port owner's CLI, CDs to SYS:, then
SystemTagList with SYS_InName=WINDOW, SYS_CmdStream=FROM, SYS_Asynch, SYS_UserShell, NP_Name "Shell Process",
and returns the WB message at once. So "CLI returned its message" right after start is normal.

## Test results (tools/rig/wbrun_rig.py)

- Rig 3: Clock window (task Clock) opens, closes on Project/Quit, wbrun gets its message back. Shell icon:
  new AmigaShell window at 0,50 800x130, task KCON, which is the icon's `WINDOW=KCON:0/50//130/...`: the
  WINDOW tool type is honoured. EndCLI closes it.
- A1200: same passes. The Shell window came up at 0,0 680x540, task CON, not the icon's
  `CON:55/110/680/360`. Every AmigaShell window on the A1200 is full-screen (the two old ones too), so the
  CON: handler there seems to ignore the geometry; not checked further (unverified). Both windows the test
  opened were closed; RAM:wbrun, RAM:wbopen, RAM:iconprobe deleted.

## Not done

- Directory Opus ARexx (port DOPUS.1) was not checked for a launch / double-click command. wbrun does not need
  it.
- An icon not reachable from a drawer lock (only a .info, as SYS:System/Shell) is found through NAME.info.
