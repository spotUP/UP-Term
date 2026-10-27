---
date: 2026-10-27
author: vtcon-05 session 11
status: draft
ledger: PORTS-FPU-VARIANTS (thoughts/spot/todos.md)
---

# Plan: FPU and no-FPU builds of every UP-Term port

## Goal

Every package in upterm-ports (51 packages) and neovim-amiga ships in two variants: a 68020 soft-float
build that runs on every supported Amiga, and a 68060/68881 hard-float build for machines with an FPU.
`upkg install <pkg>` picks the right variant by itself from the CPU and FPU it finds.

Why: NVIM-PERF step 1 (thoughts/spot/term-findings/nvim-perf.md) shows soft-float as the biggest cost in
neovim. Every Lua number operation calls a libgcc stub such as `___muldf3`.

## Owner decisions (2026-10-27, not to be re-opened)

- One ipk per variant, tagged by architecture. upkg chooses the variant from the CPU and FPU it detects.
- FPU flags: `-m68060 -m68881` (plus `-D__NO_MATH_INLINES`, see below).
- Rollout: neovim first, to prove the toolchain, the sysroot libraries and the upkg selection. Then all
  upterm-ports packages in one sweep.

## Evidence warning (2026-10-27)

The 020 vs 060-FPU bench on rig 9 (nvim-perf.md, "## 020 vs 060 build") shows the FPU build only
clearly faster in pure Lua number loops (2.1x empty loop, 1.3x table/string loop). Startup, redraw and
per-command cost are 0.87x-1.35x, inside the +-30% host noise. Rig 9 is an emulator at host speed, so
soft-float stubs cost far less there than on a real 68060. Before the 51-package sweep (step 6), the
gain must be measured on real hardware or with a cycle-exact CPU setting; otherwise the sweep doubles
build and feed size for an unproven gain.

## Design

### Architecture names

opkg already supports this. A conf `arch <name> <prio>` line or `--add-arch <name>:<prio>` registers an
architecture. When two packages with the same name are available, opkg installs the one whose
architecture has the higher priority.

- The no-FPU variant keeps the architecture `m68k-amigaos` (priority 10). Every installed upkg.conf
  already carries this line, so existing installs and the current feed keep working unchanged.
- The FPU variant uses the new architecture `m68k-amigaos-fpu` (priority 20).
- The owner's working names were "m68k-020 / m68k-fpu". Keeping `m68k-amigaos` for the 020 build avoids
  a flag day for every installed upkg.conf. Owner confirmed 2026-10-27: keep `m68k-amigaos` + add
  `m68k-amigaos-fpu`.

### upkg CPU and FPU probe (new opkg patch 0028)

At startup, upkg reads `SysBase->AttnFlags`. If `AFF_68881`, `AFF_68882` or `AFF_FPU40` is set and the
CPU is a 68040 or 68060, it registers `m68k-amigaos-fpu` at priority 20, in the same way as
`--add-arch`. The conf file stays static: a machine whose accelerator card changes gets the right
variant on its next upgrade.

- The FPU builds use `-m68060`, so require a 68040 or 68060 as well as an FPU. A 68030 with a 68882 gets
  the 020 build. Owner decided 2026-10-27: two variants only, no `-m68020 -m68881` third variant.
- Switching variant on upgrade: opkg sees the same package name and version under another architecture.
  Check that `upkg upgrade` replaces the 020 build with the FPU build. If it does not, add a rule in the
  same patch.
- Test: a host unit test of the probe function with a faked AttnFlags value, plus a rig check that rig
  8 (060 with FPU) installs the `-fpu` ipk.

### The feed (vtcon/tools/pkgs.py)

- `ARCH` becomes one value per variant. `ipk` writes `<archive>_<ver>_m68k-amigaos.ipk` and
  `<archive>_<ver>_m68k-amigaos-fpu.ipk`. The `Packages` index lists both.
- The `index` (upkg index 1) stays as it is, for the 020 build only, until no old client reads it.
- The build output directory gains a variant level, for example `build/dist/<variant>/<archive>`.

### Toolchain and sysroot

- Compile with `-m68060 -m68881 -D__NO_MATH_INLINES`. The define is needed because the compat math.h
  clashes with the GCC inline `atan2` (found while building the NVIM-PERF FPU binary).
- Sysroot libraries need FPU builds: libixcompat, zlib, ncurses and every library that a port links.
  The sysroot gains a second tree, `$(SYSROOT)-fpu`, so the two variants never mix objects.
- libm: the ixemul SDK has no FPU multilib (the neovim-amiga Makefile notes there is no libm020 either).
  Research step R1: can GCC link the FPU build against the existing libm, and do `sin` and `pow` stay
  soft-float there? If they do, build a 68881 libm from ixemul-vtcon's libsrc. Never edit
  ixemul-vtcon in place; work on a branch or in a copy owned by this plan.
- ixemul.library itself is not part of this plan. A library variant for the 060 is a separate question.

### Build matrix

- upterm-ports `mk/common.mk`: `CPU ?= -m68020` (line 33) becomes a `VARIANT` knob with the values `020`
  and `fpu`. Each value sets `CPU`, the output directory and `SYSROOT`. Line 59 passes the same value
  to `mk/amiga-ixemul.cmake` as `UPTERM_CPU`.
- `make all VARIANT=fpu` builds the FPU tree. `make dist` builds both.
- neovim-amiga Makefile: `ACFLAGS` gets the same knob, and `B = build/m68k` becomes per variant.
  `build/v012/m68k060` already exists from NVIM-PERF and is the starting point.

### Toolchain wrapper: `-m68881` reaches the link (found 2026-10-27)

`upterm-ports/tools/m68k-cc` line 51 keeps only `-m680[0-6]0`, `-m68020-60`, `-m68020-40` and `-mcpu=*` off the link line; every other `-*` flag (line 52) goes to both compile and link. So `-m68881` reaches the link call, where it can select a multilib or crt0 the same way `-m68020` once did (comment at lines 4-8, test `tools/test_m68k_cc.sh`). Before step 4: decide whether `-m68881` joins the compile-only list (link stays on the libm the FPU sysroot provides, see R1) or is wanted at link time to pick an FPU libm multilib, then extend `test_m68k_cc.sh` for that choice.

## Steps and finish line

1. R1: libm and sysroot research. Answer: which libraries need FPU builds, and does libm come with them.
2. upkg patch 0028 (probe), with a host unit test.
3. pkgs.py: two architectures and two ipks, with tests.
4. neovim: FPU build through the variant knob, an FPU sysroot, both ipks in a local feed.
5. Reachability proof for neovim, on rig 8 (060 with FPU) and on a 68020 rig with no FPU. On each rig:
   `UP-Term:bin/upkg install nvim` installs the matching ipk (read from the status file), nvim starts,
   and the NVIM-PERF startup bench shows the FPU build faster than the 2.3 s default on rig 8.
6. upterm-ports sweep: `VARIANT=fpu` for all 51 packages. Record each result in a resumable list.
7. Release: the feed carries both variants. A 68020 rig upgrade does not change variant, and a 060 rig
   upgrade switches to the FPU builds.

Done = step 7 verified on both rigs, with an answer to the step 2 question about upgrades.

## R1 result and owner decision (2026-10-27)
R1 findings: thoughts/shared/research/2026-10-27-ports-fpu-r1-libm.md. `-m68060 -m68881` selects the "." multilib (68000 soft-float); tools/m68k-cc then links soft libc/libm/libgcc with no error, while -m68881 callers read float results from fp0 (soft libm returns in d0/d1). The only FPU multilib is libm020/libm881 (libm, libc, libgcc, libixcompat).
Owner decision D-R1: pin the existing libm020/libm881 set with an explicit -L in the FPU sysroot path. Compile flags stay -m68060 -m68881 -D__NO_MATH_INLINES. libm transcendental instructions trap to 68060.library emulation on the 060; build a 68060.68881 ixemul set later only if the nvim bench shows the need.
Next: m68k-cc fix: with -m68881, link against the libm881 set (and the FPU libixcompat), host test in tools/test_m68k_cc.sh checks the map file names only libm881 libs, fails before the fix.
