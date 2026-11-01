# "Please insert volume UP-Term:" at vsh start (static investigation, 2026-10-25)

No rig run. Method: grep for every Lock/Open/LoadSeg and pr_WindowPtr in shell/, handler/, render/, claude/datadir*.
Prior table of consumers: thoughts/shared/research/2026-10-07_upterm-assign-boot.md.

## Result in one line
No vsh-side site on the vshrc/banner path was found that locks a "UP-Term:" name with requesters on. Every
volume-name lookup vsh does for PATH, test/-f, cd fallback, glyph pages and history IS guarded. The requester
therefore most likely comes from a process vsh starts (ixemul child, or a CLI-path/Type child) or from an
unguarded vsh site reached by an Amiga-form "UP-Term:..." name. Confidence in any single cause: LOW (~30%).

## Guarded (ruled out)
- config/upassign.h:87-116 upassign_ensure, 123-136 upassign_vsh: pr_WindowPtr=-1 around Open/Lock. vsh.c:2128 call is safe.
- shell/vsh.c:795-823 resolve() PATH loop (Lock of each PATH dir): guarded, "skipped". sh_path_next (shell/sh_exec.c:49) turns
  /UP-Term/bin into UP-Term:bin, so the Lock is of "UP-Term:bin" but under -1.
- shell/vsh.c:1608-1632 os_stat (used by [ -f ], [ -r ], hash_search sh_exec.c:4789, command lookup sh_exec.c:6801): guarded,
  including lock_name inside it. So tutorial_hint's tests under $HOME, `[ -f ENV:vsh/banner ]`, `[ -f $UP_HLCAT_FILE ]` are safe.
- shell/vsh.c:1470-1477 lock_name Unix-reading fallback ("/UP-Term/x" -> "UP-Term:x"): guarded; os_is_link 1552-1569 guarded.
- handler/complete.c:630, 966 (command cache, completion), history_load/history_path (complete.c:754-818, run in the -1
  worker; dd_dir/hf_path use sys->quiet_kind, claude/datadir_amiga.c:157-165): guarded.
- handler/vtcon_handler.c:952, 988 config/watch workers: guarded and read ENV:/ENVARC: only.
- render/vtwin.c:360, 396 (UF_DIR "UP-Term:unifont/", CE_DIR "UP-Term:emoji/") via vo_read -> render/outline.c:268 worker sets -1.
  A missing page is "not installed" (no requester). Rules out the banner glyph-page theory.
- dist/vshrc has no UP-Term: literal in code (lines 20,47,103,129 are comments); PATH is only a string at line 111.
- vsh.c:2060 canonical_home Lock($HOME) and 2073 source_if Lock: unguarded, but HOME defaults to SYS: (vsh.c:2173) and the
  files are ENV:/$HOME; only a HOME of the form UP-Term:... would trigger. Not the default.

## Unguarded vsh sites (candidates, ranked)
1. shell/vsh.c:1467 lock_name() first Lock(used) and shell/vsh.c:344-348 os_open() Open(path), reached from
   resolve() line 779 (any command name containing ':' or '/'), os_chdir 1490, os_realpath 1521, list_dir 1679, os_open 329/335.
   They are unguarded for the Amiga-form name. Anything that hands vsh a literal "UP-Term:..." name (a script line, an
   alias/function the user adds, hash result) while the assign is missing raises the requester. Nothing in dist/vshrc
   does this before or during the banner, so this explains the A1200 only if a user rc or typed command does.
2. shell/vsh.c:826-841 resolve() CLI command-dir loop (cli_CommandDir nodes): `CurrentDir(node[1]); Lock(name)` with
   requesters ON, for every external command (Type, hl, grep). If a Path-list entry is a lock whose volume is gone
   or a late-binding assign, the relative Lock asks for its volume. Also line 845 Lock("C:"). Cannot be confirmed
   statically; check the startup Path list (`Path` output) on the rig. This runs for `Type ENV:vsh/banner`.
3. Children vsh starts (CreateNewProc/SystemTags, vsh.c:232, 621) inherit the real pr_WindowPtr, and vshrc exports
   PATH=/UP-Term/bin:... to ixemul programs (grep, hl, ls, less). ixemul's own path search only avoids requesters when
   ix_no_insert_disk_requester is set (see 2026-10-07 doc: library/__plock.c, ix_settings.c:35; a saved ixprefs without
   it, or a program setting its own flags, gets requesters). Fits "hang behind requester" and the A1200 (own ixprefs).
   But banner timing: the only ixemul child before the banner finishes is none (Type is a native C: command), so this
   would hit at tutorial_hint's `grep` (only when $HOME/.tutorial exists) or at the first user command. Unverified.

## Why the observed 1 KB banner then hang is not explained
Type is not ixemul and touches only ENV:. The glyph path is guarded. Next step on the rig (not run): get the requester's
owning process (`Status`/ixemul debug, or snoop of Lock names) to see which task raised it; check `Path` list and
`Assign LIST UP-Term:` (is it a deferred/PATH assign made by upgetty/User-Startup before vsh, so upassign_ensure's
FindDosEntry returns "have" for an assign whose target is missing? upassign.h:97-100 returns 1 on ANY dos-list hit,
including a late-binding assign pointing at a missing volume; that is a real hole: the requester then arises on the first
unguarded Lock through it, e.g. candidate 2 or 3).

## Most likely
Candidate 3/2 jointly with the upassign_ensure hole at config/upassign.h:97-100: an existing but dangling assign
(deferred or pointing at an absent volume) is accepted as "have", and then unguarded lookups by children or by
resolve()'s CLI-path loop (vsh.c:826-841) trigger the requester. Confidence ~30%; vsh.c:826 and upassign.h:97 are the
first two places to instrument.

## Rig repro (rig 4)
Rig 4 (stale rig.lock, pid 70044 dead), fresh clone. Boot state: `Assign EXISTS UP-Term:` rc 5 (not assigned); `Path` = Current_directory, C: only
(so vsh.c:826-841 cli_CommandDir loop has no volume-bound node on this boot). Via ami.py exec (wasabid process: requesters are suppressed there,
`List UP-Term:` just fails "volume not mounted"); requester cases need a process with a window, so scripts ran in `NewShell XCON:` windows.
Detection: `ami.py requesters` / uitree ("System Request", task name, Retry|Cancel). Cancel clicked via ami.click_px.
- `Assign UP-Term: DEFER Missing:Nowhere` is accepted (LIST shows `UP-Term <Missing:Nowhere>`). A non-deferred `Assign UP-Term: Nowhere:Dir` is
  refused by AmigaDOS (rc 20 "Can't find"), so only late/deferred dangling assigns are possible.
- Plain AmigaDOS, no vsh: `List UP-Term:` in a window -> REQUESTER "System Request" (task List, Retry|Cancel). REPRODUCED. Any unguarded Lock
  of UP-Term: raises it; the owner is the task that locks.
- `VTC:vsh -c "echo hi"` and `VTC:vsh -c "Type ENV:vsh/banner"` (external command, goes through resolve()) in a window with the dangling assign:
  NO requester, both finished, output files written.
- Interactive `VTC:vsh` in a window (process stayed up, typed `Type ENV:vsh/banner` and `grep foo ...`, 15 s): NO requester. (Typed input
  not confirmed visually, only that no requester appeared and vsh stayed alive.)
Conclusion: vsh itself, with a dangling DEFER UP-Term: and the boot Path list, does NOT raise the requester; the upassign_ensure hole
(upassign.h:97-100) is real but needs a consumer that locks UP-Term:... unguarded. Candidate vsh.c:826-845 not supported (no volume-bound Path node
here). Not tested: a Path entry inside UP-Term:, an ixemul child (hl/ls via PATH /UP-Term/bin), the A1200's own ixprefs.

## Rig repro 2 (rig 4)
Same setup (dangling `Assign UP-Term: DEFER Missing:Nowhere`, scripts run in `NewShell XCON:` windows, `ami.py requesters` polled). Budget ended before a clean verdict; evidence:
- Caller survey (grep): upassign_ensure is called at shell/vsh.c:2128, render/outline.c:324, device/upconsole.c:568. It never locks when the assign
  entry exists (config/upassign.h:93-98 returns 1 first), so a dangling DEFER assign passes straight through it: the ensure is not the raiser.
- Guarded: outline.c worker sets pr_WindowPtr=-1 at :268 (covers VO_READ at :324); upconsole.c:567-571 guards its Open. view/vw_plat_amiga.c:143-169
  (hl runs `SystemTags("UP-Term:bin/cat ...")`) has NO pr_WindowPtr guard in the caller (grep: no pr_WindowPtr in view/); the new process takes
  the caller's window pointer, so the DOS lock of UP-Term:bin/cat is a requester site when the caller has a window. UNTESTED (see below).
- Case 1 (`VTC:vsh -c "hl <file>"` / `echo foo | hl`, window): no requester appeared, the shell hung at that step for 40+ s (shell process
  "wait", no separate hl process in `rig.py wasabi ps`); probably hl/vsh waiting on terminal reply in a plain XCON window, not a requester.
- Case 2 plain AmigaDOS unknown command `Bogusname1` with the dangling assign and the boot Path (no UP-Term: entry): no requester, rc 20 as usual.
  The following `VTC:vsh -c "Bogusname2 >NIL:"` also hung with no requester. `Path UP-Term:C ADD` + unknown command: NOT reached (script blocked).
- Case 3 (UP-Term terminal app / handler window with the dangling assign): not run.
- Requester log build/rig4/requesters.log has only 2026-10-19 entries (a "startup message" Abort-only requester, owner tasks ls and cat, from
  `vsh -c "cat System:.t1"` / `which cat grep rm ls` runs): an ixemul child program (ls, cat) from the UP-Term bin dir raised a requester on an earlier
  rig run; that is the closest real evidence for case 1 (ixemul programs, owner = the child task, not vsh).
Open: rerun on a fresh boot with cases split into separate windows, `ls`/`cat`/`grep` as the ixemul child instead of hl, then `Path UP-Term:C ADD`.
