---
title: Port the vtcon test rigs from FS-UAE to Amiberry
date: 2026-11-01
branch: s29-amiberry-rigs
---

# Rig port: FS-UAE to Amiberry (v8.2.0)

Backend: `tools/rig/amiberry.py` (config spelling, launch command, title link). Machine decisions stay in
`tools/rig/rig.py` (`amiberry_machine()` mirrors what `setup()` builds for FS-UAE). Select with
`rig.py start --emu amiberry` or `UPTERM_RIG_EMU=amiberry`; `paths.EMU`. Default is still fs-uae (see the end).

## Map: fs-uae part, Amiberry equivalent, gap

| rig.py / paths.py | fs-uae option | Amiberry | Gap |
|---|---|---|---|
| rig.py:78 CFG | `vtcon-rig.fs-uae` | `vtcon-rig.uae` (WinUAE-style `key=value`, no `[fs-uae]` section), started `Amiberry -G --log -f <cfg>` | none |
| rig.py setup() machine | `amiga_model=A1200\|A500` | `chipset=aga\|ocs`, `chipset_compatible=A1200\|A500` | none |
| cpu_lines() 946 | `cpu=68060-NOMMU\|68060\|68LC060\|68020`, `fpu=...` | `cpu_model=68060\|68020`, `fpu_model=68060\|68882`, `mmu_model=68060` only with `--mmu`; `--nofpu` omits fpu_model | `ShowConfig` reads "CPU 68060/FPU 68882" on the 060 rig (FS-UAE value not compared); LC060 is the 060 without fpu_model, not a distinct model |
| unimplemented_lines() 957 | `uae_cpu_no_unimplemented`, `uae_fpu_no_unimplemented` | `cpu_no_unimplemented`, `fpu_no_unimplemented` (false) | none |
| RAM | `fast_memory` (KB), `chip_memory` (KB), `zorro_iii_memory` (KB) | `fastmem_size` (MB), `chipmem_size` (512 KB units), `z3mem_size` (MB) | A500 slow RAM written as `bogomem_size=2` (unverified on a 500 run) |
| JIT | `jit_compiler` | `cachesize=8192` or 0 (0 for `--exact`, `--mmu`) | JIT accepted on the arm64 build; speed not compared |
| speed / exact | `uae_cpu_speed`, `uae_cpu_cycle_exact`, `uae_cpu_compatible` | `cpu_speed`, `cpu_cycle_exact`, `cpu_compatible` | none |
| RTG (`graphics_card=uaegfx`, hw sprite) | | `gfxcard_type=ZorroIII`, `gfxcard_size=16`, `gfxcard_hardware_sprite=true` | Picasso96 uaegfx screenmode not re-proved visually on Amiberry (rtg gap, see below) |
| network | `bsdsocket_library=1` | `bsdsocket_emu=true` | wasabid answers on rig 20 (ping, exec, get); TCP port per rig unchanged |
| drives 1169-1180 | `hard_drive_N`, `_label`, `_priority`, `_read_only` | `uaehf<N>=dir,rw\|ro,DH<N>:<label>:<path>,<pri>` (host dirs); `uaehf<N>=hdf,...` for sys.hdf | HDF line form written but not run by this port's 3.2 proof; 3.1 run below |
| kick | `kickstart_file` | `kickstart_rom_file` | A500 ROM: use paths.ROM_A500 (Amiberry's own kick40063 stayed black) |
| sound | `uae_sound_output=exact`, `volume` | `sound_output=exact\|none`, `sound_volume` (attenuation) | volume scale mapped 100-x |
| floppy clicks | `floppy_drive_volume(_empty)=0` | `floppy0sound=0` | none |
| clipboard | `clipboard_sharing` | `clipboard_sharing` | works (log shows amiga_clipboard_want_data) |
| joystick | `joystick_port_1=none` | `joyport1=none` | none |
| window size | `window_width/height`, `zoom`, `scale=integer`, `texture_filter` | `gfx_width_windowed`, `gfx_height_windowed` | no integer-scale / nearest-filter key found; zoom crop not mapped |
| title 983-998 / title_line() | `title = vtcon rig N (...) - PURPOSE` | none; Amiberry titles the window "Amiberry - [<config file name>]". Solved: `launch_path()` starts it on a symlink `build/rigN/win/<title>.uae`; `config_description` carries the text too | works: see proof. No hand renaming |
| HOST_KEYS 983 | `modifier_key=rctrl`, Cmd to Right Amiga, Right Option to Left Amiga | only `rctrl_as_ramiga` in amiberry.conf (global, not per rig) | GAP: Cmd/Option remap not done; owner must test keys |
| serial (SERIAL, `serial_port`) | file | Amiberry wants a serial device ("Error finding serial port <file>") | SOLVED in s48: see "## s48 serial" (TCP listener + serialwatch) |
| screenshots_output_dir | per-rig shots dir | not a .uae key; amiberry.conf `screenshot_dir`, overridden per run by `-o screenshot_dir=<rig>/shots` on the command line (amiberry.command) | DONE host-side (s48); the hotkey press on the live rig is unverified |
| logs_dir, FSUAE_LOG_DIR | per-rig fs-uae log | none; stdout (`--log`) appended to `build/rigN/logs/fs-uae.log.txt`, so cap_fsuae_log, fsuae_crash, fsuae_reset_count keep reading it | reset-loop and crash line formats of Amiberry not verified (patterns written for fs-uae) |
| uaem_write_flags | `nhsparwed` | none; not needed | NO GAP (s48): Amiberry writes a .uaem only when needed, no unconditional log |
| `--debugger`, dbg, dbgbreak | console_debugger FIFO | none wired | GAP |
| mine() 1664 | ps line has CFG and "fs-uae" | ps line has `<rig>/win/` and "amiberry" (EMU_KEY) | none |
| _start() 1700-1790 | `open -a` or `nice <bin> CFG` | `nice -n 19 Amiberry -G --log -f <link>`, detached, own session | none |
| stop() 1895-1925 | lock DH0:, 8 s quiet, `pkill -TERM -f CFG`, then -9 | same sequence; pkill pattern is EMU_KEY | none (rig 20 stopped clean) |
| watchdog 1940-2025, status() 2029 | reset-loop and crash from fs-uae.log.txt | same files, Amiberry output | counts not exercised |
| [WARN] drift line | emulator independent | unchanged | none |
| Other files naming fs-uae | muforce.py:118, standalone_rig.py:319-349, ami.py:520, detach_rig.py:51, upkg_wb_rig.py, fpu_inf_rig.py | not changed | those paths still assume fs-uae; they do not select Amiberry |

## Proof

### OS 3.2 (rig 20, `--max --os 32 --emu amiberry --purpose "Amarchy s29: Amiberry port check"`)
- `ami.py ping`: `wasabid 0.11b2`
- `Version FULL`: `Kickstart 47.115, Workbench 47.2 (04/10/2021)` (3.2)
- `SYS:Tools/ShowConfig`: `PROCESSOR: CPU 68060/FPU 68882/MMU n/a`, AGA PAL, 1024 MB Z3 fast + 8 MB + 0.5 MB, 2 MB chip, uaegfx boards present
- md5 `VTC:vsh` over wasabi `get` = `22ba11bf3893aee623d5e367ecb3318d` = `build/amiga/vsh`
- window title (System Events): `Amiberry - [vtcon rig 20 (060 max, OS 32) - Amarchy s29: Amiberry port check]`
- stop: `stopped`, no Amiberry process left
(the 3.2 block above was re-read on the final code: same results, title included)

### OS 3.1 (rig 20, `--max --emu amiberry --purpose "Amarchy s29: Amiberry port check 3.1"`, sys.hdf clone of images/nyhd2.hdf)
- wasabid answers; `Version FULL`: `Kickstart 40.68, Workbench 40.42 (02/18/94)`
- config has `cpu_model=68060`, `fpu_model=68060`; `Version 68060.library`: `47.2` (loads only on a real 060 model)
- `ShowConfig` (3.1 tool) prints `CPU 68040/68040fpu`: that old tool names a 060 as 040; the 3.2 tool reads 68060. Not compared with an FS-UAE 3.1 rig (rigs 12/14 not touched).
- md5 `VTC:vsh` = `22ba11bf3893aee623d5e367ecb3318d` = `build/amiga/vsh`
- window title: `Amiberry - [vtcon rig 20 (060 max) - Amarchy s29: Amiberry port check 3.1]`
- the `uaehf0=hdf` line for sys.hdf booted

## Decision: default stays fs-uae
Both proofs pass, but three open gaps need the owner: (1) Cmd/Option key mapping (Right Amiga on Cmd, Left Amiga on Right Option) has no per-rig key; (2) serial-to-file (kprintf/`--serial`) does not work; (3) `--debugger`/`dbgbreak` not wired. Also unchecked: A500 and `--stock` configs on a real run, RTG screenmode look, integer scaling/nearest filter, reset-loop watchdog on Amiberry log text, .uaem log flood. Host test: `tests/test_rig_amiberry_config.py` (9 tests, config generator + machine from flags).

## s48 serial

Source read: Amiberry `src/osdep/amiberry_serial.cpp` `openser()`: a `serial_port` starting with `TCP:` opens a TCP
listener (`uae_tcp_listen_uri`, default port 1234, `/wait` suffix blocks boot until a client connects); anything else
goes to libserialport `sp_get_port_by_name`, which fails for a file. No file or pty-to-file form exists.

Mechanism: `amiberry.config_lines` writes `serial_port=TCP://127.0.0.1:<12000 + rig>` (`paths.serial_tcp_port`); `rig.py start`
spawns `rig.py serialwatch` (one per rig, lock `<rig>/.serialwatch.lock`), which connects, appends the stream to the same
`SERIAL` file (`<rig>/serial.log`) FS-UAE rigs write, and retries until the emulator is gone. `logwatch`
(`cap_fsuae_log(extra=[SERIAL])`) and every other reader are unchanged. `--serial <path>` (a pty for getty_rig) is not
translated: the Amiberry config leaves serial_port out then.
Host test: `tests/test_rig_amiberry_config.py` `MachineFromFlags.test_serial_is_a_tcp_port_never_a_file`.
Proof, rig 30 (`--max --emu amiberry`): Amiberry log "TCP: Listening on 127.0.0.1 port 12030" then "TCP: Serial connection accepted";
`build/rig30/serial.log` filled with the Amiga's upgetty shell on the serial line, e.g. `New to a Unix shell? Type  tutorial  for a short hands-on course.`
Unverified: a probe string of our own (`Echo >SER:` and `>AUX:` do not exist on this rig; upgetty owns serial.device); kprintf via RawPutChar not exercised.


## s48 screenshots

- Source (Amiberry checkout /Users/spot/Code/Emulators/amiberry, 6e3ca06): `screenshot_dir` is an amiberry.conf path key (src/osdep/amiberry.cpp, table near line 3818), read by get_screenshot_path() for every PNG. The .uae parser has no such entry. `-o <amiberry.conf line>` on the command line goes through the same parser (parse_amiberry_cmd_line), so the setting is per run and the shared amiberry.conf stays untouched.
- Rig: tools/rig/amiberry.py command(cfg, shots) appends `-o screenshot_dir=<rig>/shots`; rig.py amiberry_machine() passes shots_dir. Rig 30 process list showed the argument.
- Amiberry has no default screenshot hotkey (config key `amiberry.screenshot`); config_lines now sets `LCtrl+LShift+F10`.
- Unverified: a synthetic osascript key press (control+shift+F10, Amiberry frontmost) produced no PNG in build/rig30/shots or ~/Documents/Amiberry/Screenshots (both empty). Cause not isolated: key not delivered by System Events, or hotkey path; needs a human press of Ctrl+Shift+F10 in the rig window.

## s48 uaem

Verdict: no gap, no code change. Amiberry has no uaem_write_flags key (grep of src: no hit) and needs none.

- src/osdep/fsdb_host.cpp:415 fsdb_needs_uaem_sidecar: a sidecar is written only for a non-empty comment or a mode the host file cannot represent (default rwed bits write none; line 450 removes a stale one). The set-attrs path (793-870) logs only on error; no unconditional write_log per protect/filenote/date as in FS-UAE's fsdb_host.cpp.
- Measured on rig 30 (Amiberry, 060): Copy SYS:C to VTCX:s48t ALL (~250 files), Protect +s then -s, Filenote comment=hi, Filenote cleared, Delete ALL. During the workload 307 .uaem existed (comment ones only, not one per file), 0 after the delete; total .uaem in rig drawers 7236 before and after (those predate the run). Log grew 2565 lines, all wasabi socket-thread traces; 0 lines mentioning uaem, metadata, writing, fsdb or FILESYS.
- Unverified: the 7236 pre-existing .uaem in build/rig30 were not traced to an emulator.
