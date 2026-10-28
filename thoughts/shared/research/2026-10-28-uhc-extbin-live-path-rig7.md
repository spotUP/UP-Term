# UHC:ExtBin live path, rig 7 (P8) - 2026-10-28

Rig 7 (purpose "Amarchy s20: UHC-EXTBIN-LIVE-PATH check"). Fresh boot after `rig.py install`; up-uhc installed via install.dos, no reboot after.

Build notes: `make dist-base` fails (needs upterm-ports `make kit-stage`: no build/kit/userland/SOURCES.txt). The stale up-uhc kit (no WBPath) was rebuilt with `make dist-uhc` alone.

## Extra check RIG-NO-THEMES
- PASS themes: 23 .theme files in UP-Term:Amarchy/Themes = 23 in dist/amarchy-themes (`ethereal.theme 593 ... Today 23:18:58`)
- PASS font: `JetBrainsMono.ttf 273900 ----rwed Today 23:18:58` in FONTS:
- PASS C:WBPath: `WBPath 2836 ----rwed Today 23:17:53`

## Install
`[OK] WBPath: UHC:ExtBin added to the Workbench path` / `[INFO] UHC:ExtBin is on the Workbench path: new UP-Term windows and Amarchy launches find its programs...` rc 0. UHC:ExtBin was empty after install; Pipe package installed via `Execute UHC:Packages/pipe` -> `UHC:ExtBin/Pipe 2240`.

## 5a PASS
`taskpath Workbench` ends `System:AmiSSL`, `System:UHC/ExtBin`. New UP-Term window `Path` lists `System:UHC/ExtBin`; typing `Pipe` printed `Pipe: Invalid pipeline` / `***Broken pipe` (Pipe's own usage error: found and run by name).

## 5b PASS
Temp bind (ENVARC:up-term/keymap.user, `super+alt+f8 = run:Execute RAM:b5.script`), Amarchy ARexx `RELOAD` (rows 46 -> 47), `RUN 46`/`RUN 47`. RAM:b5.out contained `Pipe: Invalid pipeline` / `***Broken pipe`: Pipe ran by name from an Amarchy act_run launch. Temp keymap.user and RAM files deleted, RELOAD back to rows=46. (Script stopped after Pipe's rc 20, so Path/Echo lines did not run.)

## 5c PASS (with notes)
Uninstall output: `[OK] WBPath: 0 node(s) for  removed from the Workbench path` then `up-uhc removed.` rc 0. `taskpath Workbench` afterwards ends `System:Opus5/C`, `System:AmiSSL`: no UHC/ExtBin. up-uhc Uninstall by design leaves SYS:UHC and the UHC: assign (Uninstall header comment).
Delete of SYS:UHC: first tries gave `SYS:UHC/ExtBin Not Deleted: object is in use`, `SYS:UHC/C`, `SYS:UHC/S` in use. Holders were shells opened BEFORE the uninstall that copied the path: the 5a UP-Term window (ExtBin) and my own CON window that ran `Path UHC:C UHC:S ADD` (C, S). After closing both, `Delete SYS:UHC ALL FORCE` deleted everything; the directory itself stays "in use" only by the UHC: assign lock; `Assign UHC: REMOVE` then `SYS:UHC  Deleted`. So no lock from the Workbench path remains.

## Found
- Bug: install/wbpath.c line ~102 prints `%d` with an int: AmigaDOS Printf `%d` is 16-bit, so the count shows 0 and `%s` prints empty. Use `%ld` with a long cast. (The REMOVE did work.)
- Self-inflicted: I ran `Assign UP-Term:` and `Assign UHC:` with no target (removes the assign); restored `Assign UP-Term: "Rushhours:Tools/UP-Term"` and `Assign UHC: SYS:UHC` before 5a-5c. Workbench path node was added before that.
- Plan expected ~18 themes; dist has 23.
