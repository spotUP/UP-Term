# PFS3 LoadSeg stall on rig 2
- Rig 2 had no PFS3 (sys.hdf = RDB with FFS "fs 40.1"). Built a 64 MB RDB hdf with rdbtool (scratchpad/pfs/pfs.hdf), handler = pfs3aio 19.2 PFS3AIO-VERSION (2.10.2018) from amitools test/progs/pfs3aio (59120 bytes), added as hard_drive_3 to build/rig2/vtcon-rig.fs-uae (hand-appended; start rewrites it), formatted PT: with SYS:System/Format. Version PFS0: -> pfs3aio 19.2.
- Machine: 060 JIT, --max. Timing = host wall time of `ami.py exec "<path> --version"` (baseline `Version` = 0.13 s), 3 runs each.
| file | PT: (PFS3) | RAM: |
| vsh 237648 B | 0.14 0.15 0.21 | 0.17 0.14 0.15 |
| Claude 743520 B | 0.17 0.21 0.17 | 0.15 0.14 0.15 |
- Result: NOT reproduced; PFS3 vs RAM differ by <= 0.06 s (no 1.5 s stall).
- Newer pfs3aio: tonioni/pfs3aio has no GitHub releases (API list empty), tags v3.0, v3.1 only; HEAD 2026-08-07 "Persist released reserved blocks". Not built/installed (needs source build; nothing to compare as no repro).
- Caveat: emulated disk is a host-cached file, no seek/DMA/transfer latency; 060 JIT. Replay's stall is likely tied to real storage path (SD/FPGA IDE, max-transfer, buffers) which this rig cannot model.

## Replay measurements (main session, 2026-10-21, Mac-side wall time incl. ~0.15 s exec overhead)

- DH0: (System, 276 MB) and DH1: (Rushhours, 3096 MB, holds UP-Term:) are both pfs3aio 19.2, 80 buffers each.
- upkg-ui (297116 B) --help: RAM: 0.12 s (earlier), SYS:T/uu (DH0) 0.53/0.57/0.52 s, UP-Term:bin (DH1) 1.78/1.78/1.87 s.
- Copy UP-Term:bin/upkg-ui TO NIL: 0.19 s, also with BUF=1. Plain reads from DH1 are fast.
- List UP-Term:bin/upkg-ui 0.21 s vs List SYS:C/Copy 0.20 s: lookup in the 184-entry bin drawer is not slow.
- Type is CPU-bound (Type RAM:uu 1.71 s), useless as a probe.
So: reading the file is fast, running it is slow, and slower on DH1 than DH0. Not yet known: whether this is
LoadSeg itself (many small seeks per hunk) or the program's startup (ixemul/libnix opening files on the volume
it was started from).

## Probe 2 (LoadSeg vs ixemul startup, 2026-10-21)
- upkg-ui is ixemul-style (links libixcompat.a + ncursesw, m68k-cc wrapper; no libnix/clib2 in recipe). 3 hunks, no chip flags.
- Startup is NOT the stall: RAM:uux (copy of upkg-ui) --help 0.28-0.32 s whether cwd is RAM: or UP-Term:bin (DH1); `Version` baseline 0.14-0.20.
- Same binary, run from DH1: 1.79 s (x3, identical, so not cache related). From RAM: 0.29. => ~1.5 s is LoadSeg of the file from DH1.
- Other ixemul binaries from UP-Term:bin (DH1) vs RAM: copy: sed 194K 0.37 vs 0.28 (+0.1); upkg 590K 3.36 vs 0.31 (+3.05);
  perl 4.27M 3.44 vs 0.75 (+2.7); upkg-ui 297K 1.79 vs 0.29 (+1.5). Not proportional to size (sed, perl are cheap per byte).
- Non-ixemul(unverified) SYS:C/vgm2wav 409K on DH0: 0.46 vs 0.22 from RAM (+0.24) -> LoadSeg from pfs3 is slower than RAM for any binary; DH1 much worse.
- Copy to NIL (large sequential Read, also BUF=1) is fast, so Read as such is fine; LoadSeg access pattern (hunk-wise Reads, reloc tables) is what pfs3aio on these volumes handles badly.
- Not done: packet-level trace (SnoopDos exists at System:C/SnoopDos but is a GUI, not started); reloc-count correlation (host parse of sed/upkg-ui/upkg: 2477/3989/7099 reloc entries, +0.1/+1.5/+3.05 s; perl 95759 entries +2.7 s breaks it, so no clean model).

## Re-copy test (2026-10-21, owner OK, Replay DH1)
Copy CLONE of upkg-ui and upkg to fresh files in UP-Term:bin, `X --help >NIL:` wall time, alternating runs:
upkg-ui 1.87/1.87 s, fresh copy 0.85/0.83 s; upkg 4.08/3.39 s, fresh copy 1.34/1.40 s. RAM: ~0.3 s.
A fresh copy of the same bytes loads 2-3x faster: on-disk layout (fragmentation of files written by the
kit install) is a large part. The fresh copy is still ~0.5-1 s slower than RAM:, so a second part remains.
sed was written the same minute and is cheap, so the install did not fragment every file. Copies deleted.

## Alignment probe (2026-10-21, Replay DH1, lsprobe4: Read 1024, Read BIG, then 1024-byte Reads to the end of UP-Term:bin/upkg)
Source read of pfs3aio (scratchpad pfs3aio-source.md): a Read that leaves the file position off a 512-byte block
boundary sends later small Reads down the indirect path (disk.c:652, buffer + 3-sector read); no 256 KB threshold exists.
Measured, each case twice, identical:
- BIG 517600 (position after it 518624, unaligned): stalls of 1373 ms at 544224 and 576992; total 3.14 s.
- BIG 517120 (position 518144, aligned): no stall; total 0.39 s.
- BIG 262240 (unaligned): ten stalls of 1373 ms, one every 32768 bytes from 281696; total 14.4 s.
- BIG 262144 (aligned): no stall; total 0.56 s.
So the trigger is the unaligned file position, not the size of the large Read. Every stall is exactly 1373 ms and they
recur every 32768 bytes (64 blocks): this looks like a timeout or retry, not slow media. LoadSeg's Reads follow hunk
sizes, so it lands unaligned for most large programs. Which side owns the 1373 ms (pfs3aio's indirect read or the
Replay's scsi.device) is not yet separated.

## Emulated pfs3aio probe
Rig 2 (FS-UAE 68060, pfs3aio 19.2 PT: as PFS0:, file PT:upkg 590604 bytes, probe in RAM:; the rig was already up from the earlier agent, DH0 read-only so the FFS control reads DH0:C/amiga-pi 2086312 bytes). Verdict: no stall on any case, PFS3 or FFS.
== PFS3 BIG 517600 run 1
rc 0
 big read 517600 at 1024 into 1065a5f0: 3 ms
PT:upkg: 6 ms
== PFS3 BIG 517600 run 2
rc 0
 big read 517600 at 1024 into 1065a5f0: 0 ms
PT:upkg: 3 ms
== PFS3 BIG 517120 run 1
rc 0
 big read 517120 at 1024 into 1065a5f0: 0 ms
PT:upkg: 3 ms
== PFS3 BIG 517120 run 2
rc 0
 big read 517120 at 1024 into 1065a5f0: 0 ms
PT:upkg: 3 ms
== PFS3 BIG 262240 run 1
rc 0
 big read 262240 at 1024 into 1065a5f0: 0 ms
PT:upkg: 15 ms
== PFS3 BIG 262240 run 2
rc 0
 big read 262240 at 1024 into 1065a5f0: 0 ms
PT:upkg: 17 ms
== PFS3 BIG 262144 run 1
rc 0
 big read 262144 at 1024 into 1065a5f0: 0 ms
PT:upkg: 12 ms
== PFS3 BIG 262144 run 2
rc 0
 big read 262144 at 1024 into 1065a5f0: 0 ms
PT:upkg: 11 ms
== FFS DH0:C/amiga-pi BIG 262240 run 1
rc 0
 big read 262240 at 1024 into 1065a5f0: 2 ms
DH0:C/amiga-pi: 73 ms
== FFS DH0:C/amiga-pi BIG 262240 run 2
rc 0
 big read 262240 at 1024 into 1065a5f0: 0 ms
DH0:C/amiga-pi: 116 ms
## pfs3aio HEAD on rig 2
Rig 2 --max (060). Copy S/pfs/pfs-head.hdf = pfs.hdf with RDB filesystem replaced by S/pfs3aio/pfs3aio (66816 bytes, HEAD 211f7f0), added as hard_drive_3 by hand (start rewrites the config; fs-uae launched directly after rig.py start was killed).
Source md5 27d6f6b09904b05bfc9d344f62c3f1f0 (VTC:upkg 590604); old PT:upkg read back through HEAD md5 27d6f6b09904b05bfc9d344f62c3f1f0
$ Version PFS0:
rc 0
pfs3aio 20.0
$ Info (PFS0)
PFS0:      58M    3072  116338   3%   0  Read/Write PT
$ Delete PT:upkg; Info
rc 0
PT:upkg  Deleted
PFS0:      58M    1918  117492   2%   0  Read/Write PT
$ Copy VTC:upkg PT:upkg
rc 0
PFS0:      58M    3072  116338   3%   0  Read/Write PT
readback md5 27d6f6b09904b05bfc9d344f62c3f1f0 size 590604
$ Delete again, Copy again
rc 0
PT:upkg  Deleted
PFS0:      58M    1918  117492   2%   0  Read/Write PT
rc 0
PFS0:      58M    3072  116338   3%   0  Read/Write PT
readback2 md5 27d6f6b09904b05bfc9d344f62c3f1f0
rc 0
Directory "PT:" on Wednesday 21-Oct-26
vsh                       237648 ----rwed Today     19:02:27
Claude                    743520 ----rwed Today     19:02:27
upkg                      590604 ----rwed Today     19:56:15
3 files - 3075 blocks used
## probe lsprobe4 PT:upkg (flags 0)
== HEAD BIG 517600 run 1
rc 0
 big read 517600 at 1024 into 10565168: 0 ms
PT:upkg: 3 ms
stalls>=5ms(second pass) 0
== HEAD BIG 517600 run 2
rc 0
 big read 517600 at 1024 into 10565168: 0 ms
PT:upkg: 12 ms
stalls>=5ms(second pass) 0
== HEAD BIG 517120 run 1
rc 0
 big read 517120 at 1024 into 10565168: 0 ms
PT:upkg: 3 ms
stalls>=5ms(second pass) 0
== HEAD BIG 517120 run 2
rc 0
 big read 517120 at 1024 into 10565168: 0 ms
PT:upkg: 2 ms
stalls>=5ms(second pass) 0
== HEAD BIG 262240 run 1
rc 0
 big read 262240 at 1024 into 10565168: 0 ms
PT:upkg: 15 ms
stalls>=5ms(second pass) 0
== HEAD BIG 262240 run 2
rc 0
 big read 262240 at 1024 into 10565168: 0 ms
PT:upkg: 14 ms
stalls>=5ms(second pass) 0
== HEAD BIG 262144 run 1
rc 0
 big read 262144 at 1024 into 10565168: 0 ms
PT:upkg: 11 ms
stalls>=5ms(second pass) 0
== HEAD BIG 262144 run 2
rc 0
 big read 262144 at 1024 into 10565168: 0 ms
PT:upkg: 11 ms
stalls>=5ms(second pass) 0
Totals: no stall >= 5 ms in any case; totals 517600 3/12 ms, 517120 3/2 ms, 262240 15/14 ms, 262144 11/11 ms. Rig 2 stopped by rig.py stop (no fs-uae left). Hardfile copy pfs-head.hdf gained writes (upkg copied/deleted); rig 2 config now has the hand-appended hard_drive_3 line until the next start.

## Replay's own pfs3aio on rig 2
Question: is the pfs3aio the Replay runs for DH1: the same binary as the one rig 2 ran? Answer: yes, byte-identical. No rig run needed.
- Replay `Version DH0: FULL` and `Version DH1: FULL`: "pfs3aio 19.2 (10/02/2018)"; `Version L:pfs3aio FULL`: "Professional-File-System-III 19.2 PFS3AIO-VERSION" (59120 bytes, 28-Aug-21). Different strings, so the file was not assumed to be the running copy.
- DH0 and DH1 are both on scsi.device unit 0, dostype 0x50445303 (PDS3), DosEnvec buffers 80, maxtransfer 0x1fe00, both nodes share one handler seglist (2003c17): one handler code for both.
- Read the Replay's first 600 blocks with CMD_READ through a tiny program (scratchpad rdbdump.c; RAM: output, deleted afterwards; no write to any disk). rdbtool info on that image: FileSystem #0 PDS3/0x50445303 version=19.2 size=59120; FileSystem #1 DOS3 45.13.
- md5 of that RDB filesystem (rdbtool fsget 0): 2912d39e917903ca5d3cc9e9bdb6fb33
- md5 of Replay L:pfs3aio (ami.py get): 2912d39e917903ca5d3cc9e9bdb6fb33
- md5 of rig 2's S/pfs/pfs.hdf RDB filesystem (fsget 0): 2912d39e917903ca5d3cc9e9bdb6fb33 (amitools test/progs/pfs3aio, same md5)
So the earlier rig 2 run with pfs3aio 19.2 already used the Replay's exact binary: no stall (all four lsprobe4 cases) with it on rig 2, 1373 ms stalls on the Replay. The binary is not the difference; the Replay's scsi.device / controller / medium (or the rig's emulated hardfile path) is. Differences left: dostype PDS3 vs PFS3 (same code), mask/maxtransfer/buffers of the mount (Replay 0x7ffffffe / 0x1fe00 / 80; rig hdf 0x7ffffffe / 0xffffff / 30), and the device under it.
Rig 2 was already down; nothing started, nothing to stop.
