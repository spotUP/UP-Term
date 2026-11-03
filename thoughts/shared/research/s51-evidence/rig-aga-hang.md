---
date: 2026-11-02
topic: rig.py aga froze wasabid (RIG-AGA-WASABID-STACK)
status: fixed in branch s51-aga-hang, commit ee2b2700 (not merged)
---

# rig.py aga: wasabid stops answering after the AGA reboot

## Symptom
On the default master-sys.hdf rig (kit + Amarchy + DOpus), `rig.py aga` failed: its inner `rig.py start`
raised CalledProcessError. serial.log reached the vsh `BOOTX:` prompt (from `C:upgetty LOOP`), fs-uae
listened on 1313, a host TCP connect succeeded at once but recv timed out: wasabid accepted and never answered.
Every earlier AGA success (s28 flat pens, s31 aspect borders) ran on `--os 32` rigs; kitty-te.md had the same
failure on rig 63 with aga256. Reproduced on rig 80 at main 41d6d12c.

## Evidence (temporary BOOTX:hb heartbeat: Date + Status FULL every 5 s)
- Machine alive, heartbeat kept ticking.
- `Process  6: stk  4096, gv 150, pri   1 Loaded as command: BOOTX:wasabid`
  (C:Run hands wasabid the boot shell's 4096-byte stack).
- Chip RAM free 1986912; amisslmaster.library 5.27 present; AmiSSL: and LIBS: assigns fine.
- wasabid startup order (build/wasabi/src/wasabid.c ~7120-7220): listen, open_discovery, guru_arm,
  tls_open (tls.c:96, OpenAmiSSLTags), rexxport_open, WaitSelect loop. It accepted (listen done) but never served.

## Fix
`C:Stack 262144` before `C:Run >NIL: <NIL: BOOTX:wasabid ...` in rig.py GO (WASABID_STACK).
Same AGA boot after the fix: `Process 6: stk 262144 ... BOOTX:wasabid`, start rc 0, `ami.py ping` ->
`wasabid 0.11b2`. `[WARN] the Workbench stayed PAL (W43)` from retry_wb_reset is expected on AGA.

Regression test: tests/test_rig_wasabid_stack.py (FAILED on main rig.py, OK with the fix).

## R1 check: kitty_z_rig on AGA rig 80 (screenmode.name = aga, fixed rig.py)
    shot 640 256
    picture box x 20..83 y 50..65: red 752 glyph 272 glyph colours [((170, 170, 170), 272)]
    first cell x 20..27: red 94 glyph 34
    PASS
Shot: ~/Desktop/upterm-shots/s51-kitty-z-aga-rig80.png

## Open
- Exact overflow site in wasabid (likely AmiSSL/TLS init) and why only AGA triggers it: not proven.
- Upstream wasabid (creep-ltx/wasabi) should carry its own __stack / $STACK cookie.
- `--os` rigs use the same GO text, so they get the stack too.
