---
date: 2026-10-22
topic: where upkg spends its time on a 68060 Amiga; asm versus higher-level fixes
status: partial (rig 2 numbers are 060 JIT at full speed and noisy; stage split inferred from source plus recorded A1200 figures)
---

# upkg speed profile

## Flow (from source)

upkg is the opkg 0.9.0 C binary (upterm-ports/pkgs/opkg, 24 patches). Only `upkg browse` involves a script (dist/upkg-browse, 289 lines, vsh).
Process starts come from three call sites in the patched source (libopkg/opkg_download_wget.c:119 curl, libopkg/pkg.c:1384 maintainer scripts via `/bin/sh -c`, libopkg/opkg_cmd.c:976 sh -c for hooks). Everything else (Packages.gz gunzip, Packages parsing, status read/write, ar/tar extract via libarchive) is in-process.

| Command | Process starts (besides upkg) | In-process work |
|---|---|---|
| `upkg update` | 1 curl per feed (Packages.gz; no .sig) | gunzip, write list, keep .prev |
| `upkg list` / `list-installed` | 0 | load all feed lists + status, print |
| `upkg list-upgradable` | 0 | same load; since patch 0017 no file-owner index |
| `upkg install <small>` | curl (1) + sh -c postinst (1) + vsh upkg-ask (0-1) + Execute install.dos (1) + every command install.dos runs + rm | sha256 of the ipk, ar/tar/gz unpack to T:upkg-stage, copy |
| `upkg browse` | 1 upkg (loads once, writes .all/.inst/.upg/.new) + 1 vsh script + upkg-ui per screen; `upkg info` per Enter | the vsh row-building loops (below) |

## Measurements (rig 2, 060 at --max, base staged in VTC:UPT, feed served from this Mac)

Wall clock from the Mac around `ir.run_slow`, so each includes about 1.1 to 1.9 s of wasabi round trip and vsh start. `Echo hi` through a vsh script = 1.1 s; raw `Echo` = 1.3-1.9 s. Another rig was busy, so repeat spread is large.

| Command | runs (s) | median (s) | minus 1.1 s baseline |
|---|---|---|---|
| upkg --version | 1.1 1.1 1.1 | 1.1 | 0.0 |
| update (7 pkgs) | 1.6 1.6 1.5 | 1.6 | 0.5 |
| list (7 pkgs) | 1.2 1.2 1.2 | 1.2 | 0.1 |
| list-installed | 1.2 1.2 1.2 | 1.2 | 0.1 |
| list-upgradable | 1.2 1.2 1.3 | 1.2 | 0.1 |
| install up-claude (480 KB) | 3.3 2.8 2.7 | 2.8 | 1.7 |
| curl Packages.gz alone | 1.7 2.3 1.8 | 1.8 | 0.7 |
| curl 480 KB ipk alone | 1.9 1.7 1.5 | 1.7 | 0.6 |
| update, 300-pkg synthetic feed | 2.2 2.5 2.3 | 2.3 | 1.2 (noisy) |
| list, 300 pkgs | 2.0 1.7 2.0 | 2.0 | 0.9 (noisy) |
| list-upgradable, 300 pkgs | 2.3 3.9 5.3 | 3.9 | noisy |
| list-browse, 300 pkgs | 8.0 3.4 4.5 | 4.5 | noisy |
| browse row loop, 300 rows (vsh) | 6.3 5.9 6.0 | 6.0 | about 5; the run then died at the second heredoc ("unable to open redirection file"), so this is the first loop only; indicative |

No Mac-host build of upkg was timed, so no host ratio is given (the host build exists under upterm-ports/build/host/opkg; not run: stage split needs real-speed hardware first).

Limit of this data: a 060 JIT rig at --max is faster than the real chip and the Mac load varied, so the C-side stages (parse, extract) are below the noise floor. They cannot be split from these runs. Recorded real-speed A1200 figures from the opkg patch notes are used instead (recipe.mk): one upkg load = about 3 s from PFS3 (patch 0022); list-upgradable before patch 0017 = 41 s versus 1.1 s for list-installed (file-owner index stat of every path).

## Per-stage split (best available)

| Stage | share | Evidence |
|---|---|---|
| Process starts + shell glue in install | about 60-70% of install (2.8 s total, 1.7 over baseline; curl alone 0.6, so about 1.1 s is unpack + postinst chain) | curl 480 KB alone 0.6 s; install.dos spawns many AmigaDOS commands |
| Network fetch | 0.6-0.7 s per file on loopback (curl start + TLS-free HTTP); dominated by exec/vfork of curl, not bytes | curl alone vs baseline |
| Load of feed lists + status (in-process) | about 3 s per load on A1200, near 0 on JIT 060 | patch 0022 note |
| browse row loops in vsh | about 5 s of the 6 s for 300 rows on the JIT 060; grows with rows x list size | bl.sh run |
| gunzip, tar/ar extract, sha256 | below noise on 060 JIT; libarchive/zlib C | not separable |

## Hot spots above 10 percent and fixes

1. `dist/upkg-browse` row-building loops, lines 187-229 (first pass: `case $new in *"$NL$name - "*`, `${inst#*"$NL$name - "}`, `${upg#*...}` substring scans over whole strings for every row, `rows="$rows..."` append, then `set -- "$@" ...` grows). O(rows x list bytes) in a shell that is already slow per builtin. About 5 s for 300 rows on the JIT 060, more on real hardware. Right fix: do the merge in C inside `upkg list-browse` (it already loads all four lists in one process and knows installed/upgrade/new per package), emit the finished `name<TAB>item<TAB>desc` rows, so the script only pads and passes them on. Estimated gain: 5 s to under 0.3 s for 300 rows. Not asm: a higher-level change (move to the process that already holds the data).
2. install post chain: `postinst` -> `sh -c` -> vsh upkg-ask -> Execute install.dos -> N child commands (libopkg/pkg.c:1384, ipk written by tools/pkgs.py ipk). Each exec is a process start on a 68k (about 0.1 to 0.5 s each; measured baseline 1.1 s for one vsh start through wasabi, much of it round trip). Fix: fewer process starts: skip the upkg-ask start for packages with no questions (the postinst already tests the file; make that `[ -f ...questions ]` first), and let install.dos use builtins/one Copy ALL instead of per-file commands. Estimated gain: 20-30% of install.
3. Feed load (3 s per `upkg` run on the A1200, opkg parse of Packages and status; 41 s before 0017 for list-upgradable): already fixed once by dropping the owner index. Remaining: parse cost scales with feed size. Fix: caching, such as an index of the parsed feed written at `update` time, or lazy parse that skips Description lines. Asm is not justified: libopkg parsing is byte scanning in C compiled by gcc for 68k; an asm strchr/line scanner would give maybe 10-20% of the load, while avoiding the load (one load per `browse`, already done by patch 0022) gives 75%.
4. curl start per fetch (update, install): fewer process starts is not possible for a single feed; the 0.6 s is process start. Keep.

## Verdict on asm

Nothing measured justifies hand-written 68k asm. The three costs are (a) shell loops over whole-string substring scans, (b) process starts, (c) feed load repeated per upkg run. All three are removed by moving work into the process that already has the data or by starting fewer processes. Candidate for later, only if a real-speed A1200/060 profile shows it: the md5/sha256 and the gzip inflate (zlib has m68k asm variants; libarchive is C), on large ipks (nvim 8.5 MB). Not measurable on the JIT rig.

## Open items (not done)

- Real-speed numbers: re-run the harness with `rig.py start --a1200` (68020 real speed) or on the real A1200 via `ami.py long`; the harness is in the session scratchpad (h.py, h3.py) and relies on the staging in build/rig2/vtc/UPT (git-ignored).
- The browse loop run died at the second heredoc (redirection file); cause not determined (T:/TMP: setup for vsh heredocs on this staging). Re-measure both loops separately.
- Per-stage timing inside upkg (parse vs status vs extract) needs timestamps in the C code or a profiler; no code changes were allowed in this pass.
