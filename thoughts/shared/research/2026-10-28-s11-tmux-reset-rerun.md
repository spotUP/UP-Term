# Amarchy > Apps > Tmux on a fresh derived-kit rig 7 (2026-10-28, s11 sub-agent)

Worktree pg8-text2 (353f4f52). Host busy: 4 fs-uae rigs (2,4,9,10) at 75-95% CPU each.

## Start
rig.py start --max (rig 7): first boot "[ERROR] no wasabid answer; boot.log: go started", auto retry, came up. No md5-drift refusal.
[WARN] 16 file(s) in VTC: differ from the build: Claude, UPConsole, UPTerm, Apps/bin/amarchy-index, Apps/bin/amarchy-menu, Apps/bin/hl, Apps/bin/mdv, pty-handler, Apps/bin/rz, Apps/bin/sz, upgetty, uptelnet, vsh, Apps/bin/vsh, vtcon-handler, gg/bin/sh
(VTC: was stale vs the worktree build; ran `rig.py install` and rebooted, case A repeated with current build.)

## Kit md5 (rig via ami.py get, host md5)
| file | rig | reference | match |
| C:vsh | b28faacf... | build/dist/up-term/Files/vsh b28faacf... | yes |
| VTC:vsh | b28faacf... | same | yes (build/amiga/vsh is 84ad5a54, not the kit file) |
| LIBS:ixemul.library | 605b42c6... | paths.IXEMUL_LIB 605b42c6... | yes |
| DEVS:up-console.device | 0d42bb95... | build/amiga/up-console.device 0d42bb95... | yes |
Assign ENV: EXISTS rc 0. `Which vsh` = System:C/vsh (identical bytes to VTC:vsh). Tmux's process was "VTC:vsh".
So the old-kit C:vsh/ixemul/up-console hypothesis is ruled out on a derived-kit clone.

## Case A (Amarchy started by boot) - run twice (stale VTC: and after install+reboot)
Menu opened (Left Amiga+Alt+Space over menuhost window), Return on Apps, Down, Return on "Tmux" (rows: UP-Term, Tmux, Editor, Claude, Apps>, Coding>...).
After ~20 s: machine NOT reset. wasabid answers. Requester on Workbench Screen:
  "Software Failure" (task Rushhours:Tools/UP-Term/bin/amarchy-menu, screen "Workbench Screen", buttons Suspend|Reboot)
Window list: W0 "Software Failure" 286x101 active, W1 "UP-Term" 640x300, W3 "menuhost". Process list: amarchy-menu waiting, Shell Process (VTC:vsh) waiting, stack headroom fine.
`rig.py wasabi guru`: "no Guru recorded (T:lastguru absent)" - the requester is a live Software Failure (task held), guru code/PC NOT captured (wasabi debug not started before the click; no Stack rerun done, budget).
Verdict A: Guru in amarchy-menu (the task named is amarchy-menu, not vsh/tmux), machine survives until Reboot is pressed; same with current build.

## Case B (hand-started) - INCONCLUSIVE
Reboot, arexx AMARCHY QUIT, `Run >NIL: UP-Term:bin/Amarchy`. Afterwards the Amarchy 800x600 screen was gone: Workbench Screen 640x256 depth 2, windows "UP-Term", "menuhost", a "System Request" (task IPrefs, Retry|Cancel). Menu click then did not reach an Amarchy menu. Not a valid tmux test; needs a clean redo (check that QUIT actually ended boot Amarchy and the hand start opened its screen before driving the menu).

## Open
- Capture guru code+PC: `rig.py wasabi debug start` before clicking Tmux, then Stack 262144 rerun.
- Case B redo.
- Helper note: amarchy_menu_rig.x9_level/rows_now returned no rows here (ENV:AMARCHY_ROWS not published), I drove Return/Down by keys.

## Crash capture (rig 7, 2026-10-28 01:0x, sub-agent 2; worktree pg8-text2, rig.py install then start --max, no VTC: drift WARN)
Reproduced twice (once plain 060 rig, once --max --mmu with `rig.py muforce on`). Menu opened with Left Amiga+Space (key RAW space, qual 0x40 only; with +Alt it did not open) over the "menuhost" UP-Term window; keys Return (Apps), Down, Return (Tmux).
Requester (ami.py requesters): "Software Failure" (task Rushhours:Tools/UP-Term/bin/amarchy-menu, Workbench Screen, Suspend|Reboot).
Text (tesseract on an upscaled crop of the requester, screenshot ~/Desktop/upterm-shots/s11-guru-rig7.png):
  "Software Failure" / "Rushhours:Tools/UP-Term/bin/amarchy-menu" / "Program failed (error #87000004)." / "Wait for disk activity to finish." / Suspend Reboot
Guru code: error #87000004 (OCR; low word 4 = 68k vector 4 illegal instruction by reading; the 0x87 high byte is unverified). `rig.py wasabi guru`: "no Guru recorded (T:lastguru absent)" (task held, Alert not routed there).
PC: NOT captured. The requester shows no PC; wasabi has no register/memory read; MuForce (MMU rig) logged "no hits" (illegal instruction is not a memory hit). Open: need a PC source, e.g. FS-UAE debugger/trace on the held task, or a SetFunction/exception handler in amarchy-menu, or bisect by KPrintF around the Tmux launch. objdump/addr2line exist at ~/opt/amiga/bin/m68k-amigaos-{objdump,addr2line} (not on PATH); they map the PC once it is known, build/amiga/amarchy-menu (479692 B, cookie).
Stack verdict: NOT a stack overflow. Source amarchy/menu.c:51 MENU_STACK 262144, `long __stack`, binary strings contain "$STACK: 262144"; `wasabi ps 'amarchy*'` on the held task: STACK 262144, FREE 258202. So step 5 (rerun under Stack 262144) is already the normal condition; a hand start adds nothing. Debug stream (debug.log) had no KPrintF output at all.
Commands: UPTERM_RIG=7 rig.py wasabi debug start; python3 -I <script using amarchy_menu_rig.open_terminal/ami.key(RAW['space'],LAMIGA)/RAW return,down,return>; ami.py requesters; wasabi --port 1240 --key rigwasabi ps 'amarchy*'; ami.py shot out.png 0 0 290 105.
