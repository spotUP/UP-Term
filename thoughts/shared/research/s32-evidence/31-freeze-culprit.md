# S32-31-FREEZE-CULPRIT: which of ours makes a 3.1 palette.prefs write freeze the guest

Rig 48, AmigaOS 3.1 (no --os flag), 2026-11-03. Version: `Kickstart 40.68, Workbench 40.42`.

## Boot state (Run A part 1: what the image starts)

`wasabi ps` right after boot (rig 48, before any palette write):

- ours / non-stock: `BOOTX:wasabid`, `C:SolidWin`, `C:upgetty` (+ upgetty shell), `UP-Term:bin/Amarchy`,
  `amarchy-gui` (2 procs), `amarchy-menu`, `Amarchy bar idle` (pri -128, ready), `dopus (DOpus5:DirectoryOpus)`
  + dopus_clock/rexx/start_menu, `PTY`, `ixemul task switcher`.
- stock: IPrefs, ConClip, RexxMaster, ramlib, Picasso96, console/input/trackdisk.
- Not running: amarchy-icons, amarchy-flat.
- BOOTX:go (rig harness) starts RexxMast, wasabid; the Amarchy programs come from the installed
  DH0:S startup on sys.hdf.

## Owner save (3.1 Palette prefs by hand, snooped)

Capture: `wasabi debug --with-snoop --entry --ignore-wasabi --output full`, all tasks, log
`<scratchpad>/owner-save.log`. Note: while this stream runs, wasabid does not answer `run`
("daemon did not answer for 15 s") although the stream itself keeps flowing; the guest is not frozen.

### Result (log: scratchpad owner-save.log, 67k lines, deleted after this note)

- Owner opened Palette through DOpus at 21:47:55 (`DO_LAUNCHER LoadSeg("Palette") = ok`) and saved at 21:48:11:
  ```
  21:48:11.354 Palette   Open("ENV:Sys/palette.prefs", create) = ok
  21:48:11.359 IPrefs    Open("ENV:Sys/palette.prefs", read) ...
  21:48:11.363 IPrefs    Open("ENV:Sys/palette.prefs", read) = ok
  21:48:11.363 IPrefs    OpenLibrary("workbench.library", v39) = ok
  21:48:11.397 dopus_closewb  FindTask(...)       <- DOpus reacts to IPrefs' WB close/reopen
  ```
  The stream kept flowing after the save (last line 21:49:49), and after the stream was stopped
  `wasabi run "Version"` -> `Kickstart 40.68, Workbench 40.42` and `ps` listed wasabid, Amarchy,
  amarchy-gui, amarchy-menu, SolidWin, DOpus, IPrefs all alive. **Guest and wasabid both survived the 3.1 Save.**
  Owner confirms: colours applied, DOpus redrawn, pointer and windows alive.
- The silence the owner saw was at 21:46:06-21:47:38, i.e. TWO MINUTES BEFORE the Palette editor
  was even launched. The client logged `nothing heard for 20 s` then `the stream is back`, and
  wasabid then reported `[wasabi: 13000 snoop event(s) lost]` (21:47:46). Earlier, `wasabi run`
  failed with "daemon did not answer for 15 s" at 21:38 while the guest streamed fine.
- Event mix in the log: `Amarchy bar idle` 48540 lines (FindTask ~100/s at pri -128),
  `UP-Term:bin/Amarchy` 11154 (opens ENV:Amarchy/prefs + Lock manifest + FindSemaphore ~1/s), dopus_clock 1029.

### Why wasabid "hangs" (source: build/wasabi/src/wasabid.c, rev 959c33bd)

- `pump_snoop_stream()` wasabid.c:1233-1250: `while ((n = snoop_next_line(...)) > 0) send_log(...)`
  drains until the ring is empty, each send blocking in `send_all`/`io_wait` (wasabid.c:552-630).
  With `--entry` and a producer that never stops (Amarchy bar idle's FindTask loop), the ring
  refills as fast as it drains, so the main loop (wasabid.c:7236-7270, accept at 7358) is not
  reached for long stretches: new `run`/`ps` connections get "daemon did not answer for 15 s".
  The file comment at wasabid.c:701 says it: "one client is served at a time".
- So "wasabid hang" = stream starvation by a busy snoop, not a palette effect. The old
  FLAT-31-PALETTE-HANG verdict ("ANY write of ENV:Sys/palette.prefs freezes the guest") is
  suspect for the same reason; verified below with a trigger run without any stream.

## Trigger run without any stream (owner watching, 2026-11-03)

- Rig 48 restarted (same purpose). State before trigger: Amarchy, amarchy-gui, amarchy-menu,
  Amarchy bar idle, DOpus5, SolidWin, IPrefs, wasabid running. No snoop or debug stream.
- ENVARC:Sys/palette.prefs did not exist, so the trigger file was
  `tests/data/desk/palette-gui-save.prefs` (434 bytes, FORM PREF, PRHD + PALT size 0x190 = 400,
  which is the 3.1 PalettePrefs size), uploaded to RAM:palette.prefs.
- Trigger at **21:55:32**: `Copy RAM:palette.prefs ENV:Sys/palette.prefs`.
  - T+5 s: `Connection reset by peer` (wasabid gone).
  - T+30 s: `Connection refused`.
- build/rig48/logs/fs-uae.log.txt (ends 21:55:42, 25944 lines): from line 25909, 31 lines of
  `IDE1 DATA write without request!? 00 PC=0000007A`, i.e. the 68k CPU runs wild at PC $7A
  (exception vector area) and pokes the IDE port. The FS-UAE process for rig 48 then exited.
  No watchdog.log exists in build/rig48; who ended FS-UAE (crash or watchdog) is not proven.
  Risk: the wild IDE writes may have damaged build/rig48 sys.hdf.

## Verdict so far

- Owner's 3.1 Palette GUI Save (21:48:11): guest and wasabid survive. The "freeze" then was
  snoop-stream starvation (see above), not the save.
- Bare Copy of a palette.prefs file into ENV:Sys with our stack running: **real guest crash**
  (CPU at PC $7A, ~10 s after the write). So FLAT-31-PALETTE-HANG is real for this path, and its
  cause is not proven yet.
- Difference between the two paths still open: file content (our test golden vs an editor-written
  file), Copy vs Palette's own Open/Write, or IPrefs' WB close/reopen racing one of ours
  (Amarchy polling ENV:, amarchy-gui, amarchy-menu, DOpus dopus_closewb, SolidWin).

## Next step

On a fresh 3.1 rig (clean sys.hdf), bisect: before each Copy, `Break` one program
(amarchy-menu, then amarchy-gui, then Amarchy + bar, then SolidWin, then DOpus5). Repeat with the
file the owner's Palette Save wrote (copy ENV:Sys/palette.prefs to RAM: right after a GUI save
first). The first program whose absence stops the crash is the culprit; then run that one under
`rig.py start --max --mmu` + MuForce to get the faulting PC in its code.
