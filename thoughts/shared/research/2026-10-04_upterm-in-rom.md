---
date: 2026-10-04
topic: UP-Term in a Kickstart ROM (ledger R1, phase 1 -- measured on the host, not booted)
tags: [rom, kickstart, romtool, console.device, con-handler, r1]
status: draft
---

# UP-Term in ROM: phase 1

Everything below was measured on the host from the built binaries and the
owner's ROM files. Nothing was booted. Anything still unproven is marked
**UNVERIFIED**, with the step that will prove it.

## Answer in brief

- **Neither module can run from ROM as it is built.** Both are ordinary vbcc
  builds with BSS (and DATA, for the handler), and they write to their own
  globals. `up-console.device` also cannot *replace* the ROM's console.device.
  It sits on top of it: it forwards CDInputHandler, RawKeyConvert, the private
  vectors -54..-72, ROM units (DD16) and the default keymap to the ROM device,
  and `upc_init` returns 0 if there is no ROM `console.device`. The ROM
  console.device therefore stays in the image.
- **What works: the ROM carries the two hunk files as data.** A small ROM-able
  module (`build/amiga/uprom`, one read-only hunk) loads them into RAM with
  dos.library's own loader (`InternalLoadSeg`, reading from ROM). It then
  switches over exactly as `UPConsole DEVICE ON` + `CON ON` do, and mounts
  `XCON:`. RAM use is the same as a disk install. No code in the device or the
  handler changes.
- **512 KB: does not fit.** The 3.1 (40.63) image has 444 bytes free. Every
  module it could give up adds 112,620 bytes, and the UP-Term module needs
  403,028, so it is 289,964 bytes short. With deflate compression (about 57%)
  the device alone (86 KB) would fit, but the handler (139 KB) would not.
- **1 MB: fits**, with 121,244 bytes to spare in the ext half. The serial set
  (107 KB uncompressed) would just fit as well, but it is not wired yet (see
  Blockers).
- **A test image exists:** `make rom` → `build/rom/upterm-1m.rom`. Its
  checksum is right, and `make test-rom` checks it with 8 checks.

## 1. Module sizes (`make amiga` after merging main at 45e7c26)

| file | bytes | hunks (amitools BinFmt) | ROM-able as is? |
|------|------:|-------------------------|-----------------|
| up-console.device | 152,112 | CODE 208 (RomTag stub) + CODE 146,304 + BSS 36 | no: BSS (SysBase, DOSBase, GfxBase, ... globals); RomTag has rt_Flags 0 (InitResident by UPConsole, never coldstart) |
| vtcon-handler | 248,400 | CODE 238,824 + DATA 40 + BSS 96, libvc | no: DATA/BSS, libvc; no RomTag |
| pty-handler | 11,176 | CODE + BSS 36 | no (BSS) |
| upgetty | 10,632 | CODE + 4 DATA + BSS | no |
| vsh | 67,416 | CODE + 4 DATA + BSS 10,464 | no |
| sz / rz | 9,060 / 9,068 | CODE + DATA + BSS 9,292 | no |
| **uprom** (new) | 403,028 | **one CODE hunk, 17 relocs** | **yes** (mkrom.py refuses anything else) |

All of them are position-dependent: they use absolute RELOC32 (580 to 2,349
per file), which is normal for hunk files. romtool resolves those relocations
for a module placed in ROM. That is not the blocker. The blocker is that the
modules write to themselves.

Deflate -9 sizes, for the compression option: device 86,043, handler 138,606,
pty-handler 6,699, upgetty 6,340, vsh 38,144, sz 5,833, rz 5,828.

## 2. What the ROMs can drop

ROMs on this machine (none are copied into the repo):

- `~/Code/Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200`. This is
  the rig's `KICK`, identical to `~/Documents/Amiberry/ROMs/kick40068.A1200.rom`.
  Its header says **40.63 (A500/A600/A2000)**, whatever the file is called.
  amitools' split database knows it.
- `~/Code/Kickstart v3.1 rev 40.63 ...rom`. Despite its name this is 40.68,
  relocated to $200000 (a map-ROM copy). It is not usable as a source.
- `~/Desktop/KICK_323.rom` (3.2.3, 47.x). UPConsole refuses DEVICE ON on 47,
  and so does the ROM module.

40.63's modules, as split and rebuilt by romtool. The rebuild is byte-identical
to the source ROM, which `make test-rom` checks:

```
104864 intuition  102540 graphics  70664 workbench.library  39948 dos
 24480 filesystem  23444 gadtools  17632 shell  15492 console.device
 14264 exec  12724 layers  10496 scsi.device  10180 con-handler  9336 ram-handler
  9264 icon.library  7432 trackdisk  5684 input  5648 bootmenu  4312 mathieeesingbas
  4256 audio  3864 romboot  ... (build/rom/report.txt has the full list)
```

Droppable on a disk-booting machine (workbench.library, wbtask, icon.library,
audio, mathffp, mathieeesingbas, ramdrive, carddisk, card.resource, bootmenu,
and con-handler once CON: is ours): **112,620 bytes**. workbench.library and
icon.library would then have to come from a 3.1.4+/3.5/3.9 LIBS:. A plain 3.1
install does not ship them. Everything else boots the machine or is used by
the module.

**1 MB image.** On A1200/A4000 a 1 MB ROM (two 27C800) puts the second half
at $E00000. **Exec 40.10 never scans $E00000.** Its RomTag scan table is at
ROM offset $36E: `{$F80000,$1000000},{$F00000,$F80000},-1`. So `mkrom.py`
puts the module in the ext half and adds one 28-byte RomTag to the kick half,
which fits in the 444 free bytes. Its rt_Init, name and id point into the ext
half. Map-ROM accelerators that can map 1 MB work the same way; 512 KB-only
maprom does not.

## 3. How the handler is there before the Shell

Sources: the 40.63 RomTags read from the image (flags column: `tools/mkrom.py`
→ report, and the scan in this doc's session); NDK 3.2 `exec/resident.h`
(`RTF_AFTERDOS (1<<2)`); AROS `rom/dos/internalloadseg.c` (the AmigaOS
InternalLoadSeg ABI, kept for binary compatibility).

- In 40.63 `dos.library` has rt_Flags 0 (not COLDSTART). `con-handler`
  (-121), `shell` (-122) and `ram-handler` (-123) are NT_UNKNOWN with flags 0:
  dos finds them by name. **ramlib (flags 0x04, pri -100) is the only
  RTF_AFTERDOS module**: dos calls `InitCode(RTF_AFTERDOS)` once DOS is up,
  which is how ramlib exists before anything is loaded from LIBS:.
- The UP-Term module is RTF_AFTERDOS, pri -101 (after ramlib). Its init
  (`device/uprom.c`) does the following:
  1. `InternalLoadSeg` of the device's hunk file from ROM. The read, alloc and
     free functions are in `device/uprom_tag.s`; alloc and free are exec's
     AllocMem/FreeMem, so UnLoadSeg and RemDevice free the result as they free
     LoadSeg's. Then `upc_device_start`: InitResident, and our node takes the
     name console.device. This is skipped on dos 47.
  2. `InternalLoadSeg` of the handler, then `upc_con_switch`: the CON and RAW
     DosList entries run it (DD19/DP3: dn_SegList, stack 16000, GlobVec -1).
  3. `XCON:` through MakeDosEntry/AddDosEntry, with the kit's `dist/XCON`
     values (Priority 5, StackSize 16000, GlobVec -1, from the seglist).
  4. The state goes into the "UP-Term console" semaphore, so
     `UPConsole STATUS / CON OFF / DEVICE OFF` work on a ROM boot. Each step
     prints `UP-Term ROM: ...` to the serial port.
- Why not the alternatives:
  - An expansion boot node (`AddBootNode` before DOS) would mount a device
    before DOS, but it cannot load a hunk file without dos.library, and CON:
    is DOS's own entry anyway.
  - Renaming our handler "con-handler" in ROM would need it to run from ROM,
    which it cannot (section 1).
- The switch code now lives in `device/upc_switch.c`. It has no C library and
  no writable statics, and both UPConsole and the ROM module use it, so there
  is one implementation.

**UNVERIFIED (the rig boot proves them):**

1. dos runs the RTF_AFTERDOS modules before the boot Shell opens its window.
   If not, the first window is the ROM's and later ones are UP-Term's.
2. The init runs in a Process. InternalLoadSeg calls SetIoErr on failure.
3. The InternalLoadSeg FuncTable ABI on 40.63 is the one AROS documents: read
   is d1 handle / a0 buffer / d0 length.
4. FS-UAE maps a 1 MB kickstart file with its first half at $E00000.
5. On a real A1200 the lower half of a 1 MB ROM appears at $E00000.

## 4. The image

```
make amiga                    # timeout 900; VTCON_NDK=... in a worktree
make rom                      # KICK=<3.1 ROM>, default the rig's 40.63
make test-rom                 # 8 checks; mutations prove they bite
```

`tools/mkrom.py` (amitools in `build/romvenv`, created by the make target):

1. Splits KICK with romtool's database.
2. Rebuilds it unchanged and refuses to continue unless the result is
   byte-identical. romtool 0.8 writes `b"\x0ff"` (two bytes) per kickety-split
   filler byte; mkrom.py replaces that function.
3. Builds the ext half: `uprom` relocated to $E00010.
4. Builds the kick half: the original modules plus the stub RomTag, with a
   correct checksum.
5. Checks the result: kick checksum, exactly one RomTag pointing into the ext
   half (AFTERDOS, -101), the ext module equal to the relocated `uprom`, and
   both hunk files present byte for byte.

Output in `build/rom/` (gitignored):

- `upterm-1m.rom`: ext then kick, romtool combine's order.
- `upterm-kick.rom` and `upterm-ext.rom`: the two halves separately.
- `report.txt`

## Blockers / open

- **512 KB:** impossible uncompressed. A device-only 512 KB image would need
  an inflater in the read function plus dropping WB and icon. That is
  possible, but it is not the target.
- **Serial set** (upgetty, vsh, sz, rz as resident commands; pty-handler as
  `PTY:`): the commands are not pure (DATA/BSS, libvc startup), so
  `AddSegment` resident use would share their data between runs. pty-handler
  could go the XCON way (DosList entry from a ROM-loaded seglist). Space in
  the 1 MB ext half: 121 KB free, 107 KB needed.
- **3.2 (47):** DEVICE ON is skipped as UPConsole does. A 3.2 image would also
  need a split database entry for 47.x, and romtool has none for KICK_323.
- The rig's boot script still runs `Mount XCON: FROM BOOTX:Mountlist`. On a
  ROM boot that fails with "already mounted", which is harmless under
  `FailAt 21`.

## Owner / main-session steps to boot it

1. Rig (FS-UAE A1200, 3.1 disk):
   `python3 tools/rig/rig.py start --kick build/rom/upterm-1m.rom`
2. `build/rig/serial.log` should show, in order:
   - `UP-Term ROM: start`
   - `console.device is UP-Term`
   - `CON: and RAW: are UP-Term`
   - `XCON: mounted`
3. In the boot Shell: `UPConsole STATUS` should print `CON: UP-Term`,
   `RAW: UP-Term`, `console.device: UP-Term`. `NewShell XCON:` should open
   UP-Term. A PASS also means the first boot window is UP-Term's (UNVERIFIED
   item 1).
4. Failure signs:
   - Nothing in the serial log: exec did not find the stub; check the E0
     mapping (UNVERIFIED items 4 and 5).
   - `InternalLoadSeg failed`: the ABI (UNVERIFIED item 3).
   - `not the ROM's entries`: the CON: entries are not there yet at AFTERDOS
     time.
5. Real A1200: burn `upterm-1m.rom` into a 27C800 pair. Byte-swap and
   interleave it with the burner's A1200 split tool. The file is plain
   big-endian ext+kick. Untested here, and real hardware is a separate row.
   To undo, go back to the 3.1 chips.
