---
date: 2026-11-03
topic: rig 96 frozen after fresh full install (INSTALLER_KEEP fresh-all)
status: cause NOT proven; frozen boot was destroyed before it could be probed
---

# Rig 96 freeze after fresh-all install

## Timeline (host files)

- 23:56 installer_rig `reboot_amarchy_check` (tools/rig/installer_rig.py:1077) answered the reboot page Later,
  slept 10 s, ran `rig.py reboot` = `stop()` + `start --keep-sys` (tools/rig/rig.py:1821). New fs-uae pid 46817
  started 23:57. installer_rig then wrote no verdict (fresh-all.json is still the 23:30 run).
- 23:57 boot: build/rig96/serial.log ends at the vsh banner + `Empty-12MB-FFS: %` prompt, i.e. the installed
  S:User-Startup reached `Run >NIL: C:upgetty LOOP` (it is after `Execute BOOTX:shadows-up`).
- At ~00:06 build/rig96/boot/ held NO boot.log for that boot, although both BOOTX:go (first `Run` in
  S:User-Startup) and BOOTX:shadows-up start with `Echo >>BOOTX:boot.log ...`. Directory mtime was 23:57
  (an entry created or removed then). Unexplained; no host Python deletes boot.log (grep tools/: only Amiga
  script strings and ami.py:667 reader).
- 00:05 `ami.py ping` (port 1329): TCP connect timeout (not refused: listener present, Amiga not accepting,
  consistent with a hung Amiga side). fs-uae 60-85 % CPU.
- 00:05:46 the coordinator's own shell ran `UPTERM_RIG=96 rig.py stop --keep` + `rig.py start --max --os 32
  --keep-sys --purpose "Amarchy session: rig 96 owner eye-check full install"`. pid 46817 gone, new pid 85957
  at 00:06. This destroyed the frozen state before this hunt could probe it (no ps / guru / muforce of it).

## The 00:06 boot (same reboot procedure) is healthy

- boot/boot.log complete: go started ... early UP-Term shadows done ... assigns done ... wasabid started.
- `wasabi ps` on 1329 answers: 38 tasks, Amarchy, amarchy-gui, amarchy-menu, upgetty, SolidWin, Workbench all
  `wait`; stack headroom fine everywhere (Amarchy 16384/16154 free, menu 262144/261386).
- Screenshot ~/Desktop/upterm-shots/rig96-freeze-0010.png (00:08): Workbench + Amarchy "Welcome to Amarchy"
  window, bar clock "Tue 00:08" = host time 00:08:20, bar CPU 13 %. Not frozen.

So the hang is intermittent: the same stop + start --keep-sys gave one hung and one good boot.

## Side findings (real, not the freeze)

1. Two wasabids in every installed-kit rig boot: BOOTX:wasabid (rig, port 1329) and the kit's
   `;BEGIN UP-Term wasabi` -> `Run >NIL: C:wasabid`. rig.py:1244 says "one daemon, no other agent in the boot".
   Through fs-uae's bsdsocket the kit's daemon listens on HOST port 1234 = rig 1's wasabi port:
   `lsof -iTCP:1234 -sTCP:LISTEN` -> fs-uae 85957 (rig 96). With rig 1 down, anything defaulting to 1234
   (ami.py without UPTERM_RIG, the amiga MCP server) now reaches rig 96's kit wasabid. With rig 1 up, two
   emulators bind one port. Candidate for refuse_port_clash-style handling or stripping/redirecting the kit's
   wasabi block on rigs.
2. No requesters.log newer than this rig's start exists under build/rig*/.

## Not done / next probe

- Reproduce: on the next hang, BEFORE any restart: `wasabi --port 1329 ps` (expect timeout), then
  `nc -vz 127.0.0.1 1329`, host-side screenshot, `ls -la build/rig96/boot/`, `tail serial.log`; if the
  serial getty is up, the Amiga is partly alive. Then `rig.py start --max --mmu` + `muforce on` for a trap.
- Suspect order: (a) the kit C:wasabid racing BOOTX:wasabid / bsdsocket at boot (both start within the same
  second; port 1234 clash when rig 1 holds it), (b) BOOTX: handler hang (no boot.log at all), (c) Amarchy
  start with all feature keys on. Stack ruled out for the live boot only.
