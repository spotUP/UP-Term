---
date: 2026-10-17
topic: kit rebuild after the m68k gcc clr-merge cc1 fix
status: read-only inventory, nothing built
---

# Kit rebuild inventory (gcc cc1 clr-merge fix, 2026-10-17)

## The compiler that changed

- Installed cc1: `/Users/spot/opt/amiga/libexec/gcc/m68k-amigaos/6.5.0b/cc1` (14,514,848 B, 17 Oct 12:49). Old one kept beside it as `cc1.pre-clrmerge-2026-10-17` (16 Oct 15:32).
- Driver: `/Users/spot/opt/amiga/bin/m68k-amigaos-gcc` (= `m68k-amigaos-gcc-6.5.0b`, bebbo gcc 6.5.0b). Every `AGCC`/`CROSS` user in the workspace points at it: vtcon, screen-amiga, tmux-amiga, neovim-amiga, cpython-amiga, upterm-ports, ixemul-vtcon/compat.
- EXCEPTION cpython-amiga: it links its own cc1 through `-B cpython-amiga/build/gcc/bin` (Makefile `GCCB`/`ACFLAGS`). That cc1 is dated 8 Oct, md5 881f93fc..., 26,233,792 B = the `cc1.pre-sret` generation, i.e. OLDER than the clrmerge fix. It is NOT rebuilt by a plain `make` (rule depends only on tools/build-cc1.sh and amiga/gcc/*.patch). Python must get a new cc1 first (see sequence step 7).
- Source of the fix: gcc aee333a76 on fix/zlib-o2 in `cpython-amiga/build/gcc/src`.
- Free disk when written: 3.5 GiB.

## Table (affected = built by the gcc 6.5 driver / its cc1)

| Archive | Binary | Source repo | Compiler: affected? and evidence | Rebuild |
|---|---|---|---|---|
| UP-Term | ixkill | vtcon shell/ixkill.c | YES: `$(AGCC) -mcrt=ixemul` (Makefile:633-635) | `make build/amiga/ixkill` |
| UP-Term | ixpipe-handler | vtcon (src ixemul-vtcon/utils) | YES: `$(AGCC) -mcrt=ixemul -nostdlib` (Makefile:628-630) | `make build/amiga/ixpipe-handler` |
| UP-Term | screen | screen-amiga/src | YES: `AGCC ?= ~/opt/amiga/bin/m68k-amigaos-gcc` (Makefile.amiga:9) | `make -C screen-amiga/src -f Makefile.amiga clean screen` |
| UP-Term | tmux | tmux-amiga (build/tmux-bin; libevent.a too) | YES: AGCC, AAR (Makefile.amiga:7-8) | `make -C tmux-amiga -f Makefile.amiga clean all` (clean target exists at :44 of screen's; verify tmux has one, else `rm -rf tmux-amiga/build`) |
| UP-Term | bebbossh, bebboscp, bebbosshd, bebbosshkeygen, libcryptossh.library 020+060 (7 files) | vtcon dist/net/bebbossh-1.46.1 (tarball + 22 patches) | YES: tools/build_bebbossh.sh with `AGCCBIN`=dir of AGCC (Makefile:314-326) | `make bebbossh-clean bebbossh` |
| UP-Term (+ Claude) | wasabid (daemon; the Claude archive ships it too; the base ships wasabi client + wasabi_mcp.py, host/python) | vtcon build/wasabi (pin tools/nas/wasabi.rev) | YES: `make -C .../wasabi/src wasabid CC=$(AGCC)` (Makefile:305) | `make wasabi-clean wasabi` |
| UP-Term | userland: 153 programs in upterm-ports/build/kit/userland/bin (awk bc bzip2 file find git grep gzip jq joe less lua luac make mandoc mg nano nc ncdu patch perl script sed tar tree vim xz zip, GNU coreutils set, ...), from 30 packages | upterm-ports pkgs/* | YES: `CROSS := $(AMIGA)/bin/m68k-amigaos-` (mk/common.mk:15); no -B/private cc1 in common.mk | see sequence step 3 |
| UP-Term | libixcompat.a (static, linked into ports AND neovim/python builds), ixcompat headers | ixemul-vtcon/compat via upterm-ports pkgs/ixcompat (LOCAL) | YES: `AGCC ?= ~/opt/amiga/bin/m68k-amigaos-gcc` (compat/Makefile:18; header says "Built with bebbo's gcc") | `make -C upterm-ports clean-ixcompat ixcompat` |
| UP-Term, UP-Term-nvim, Python3 | ixemul.library, ixnet.library | ixemul-vtcon (build295) | NO: `docker/build.sh` builds with gcc 2.95.3 in image `ixemul-gcc295` (build.sh:8, :44 `m68k-amigaos-gcc` inside the container; bebbo gcc only supplies NDK headers) | none |
| UP-Term | vtcon-handler, UPConsole, up-console.device, pty-handler, vsh, UPTerm, UPDemo, upgetty, uptelnet, hl, mdv, ps, top, upicon, upupdate, sz, rz, "UP-Term Prefs" (18) | vtcon | NO: `VC := vc +vbcc-aos68k.cfg ...` (Makefile:402); rules use `$(VC)`, link `-L$(VBCC_PREFIX)/.../lib -lvc` (:801,:956,:974). grep of Makefile finds `m68k-amigaos-gcc` only at :548 (AGCC) and :564 (test probe); no gcc in these recipes | none |
| UP-Term | coreutils 5.2.1-9 in Files/coreutils (dist/gg) | vendored | NO: vendored prebuilt Aminet binaries "unmodified" (dist/gg/coreutils-5.2.1/SOURCE.txt) | none |
| UP-Term | bebboget, bebboget00, installcerts | vendored | NO: Aminet binaries "unmodified" (dist/net/bebboget-1.11/SOURCE.txt) | none |
| UP-Term | curl .020/.040/.060 | vendored | NO: vendored Aminet binaries (Makefile:1042 comment) | none |
| UP-Term | fonts, unifont, emoji, themes, terminfo, scripts | data | NO: not compiled | none |
| Claude | Claude, uptelnet, upupdate | vtcon | NO: `$(VC)` (Makefile:473-478) | none |
| Claude | wasabid | see above | YES (counted once above) | |
| amiga-pi | amiga-pi | amiga-pi (via `make -C amiga-pi amiga`) | NO: amiga-pi/Makefile:95 "amiga-pi for AmigaOS (vbcc, 68020)"; compiled in VTCON_DIR with vbcc cfg | none |
| UP-Term-nvim | nvim (bin/nvim, stripped) + uvsmoke, compat_probe (test probes shipped in build/v012/dist) | neovim-amiga (Makefile.v012 + Makefile for libamigacompat) | YES: `AGCC ?= $(AMIGA)/bin/m68k-amigaos-gcc` (Makefile.v012:11), no -B; all libs (libuv, lua, luv, lpeg, unibilium, utf8proc, tree-sitter, parsers, amigacompat, amigawide) are gcc objects | see step 5 |
| Python3 | bin/python3 (python.stripped), python314.zip (bytecode only, host-compiled), c3-sentinel.py | cpython-amiga | YES: AGCC with `-B build/gcc/bin` private cc1 (STALE, see above); libamigacompat/libcpyamiga/libnewlibwcs also gcc | see step 7 |
| UP-Term-src | source archive | all | NO: text | `make dist-src` |

Counts (binaries/files that are compiled code):
- AFFECTED: ixkill, ixpipe-handler, screen, tmux, wasabid, 7 bebbossh files, 153 ports programs, nvim (+2 probes), python3 = 168 shipped executables + libixcompat.a, libevent.a and the ports sysroot libs (zlib, ncurses, bzip2, xz, libarchive, lua) that they link. Distinct "built things" at repo level: 7 repos.
- NOT AFFECTED (built): 18 vtcon vbcc binaries + Claude, uptelnet, upupdate (counted in the 18 for uptelnet/upupdate) + amiga-pi + ixemul.library + ixnet.library = about 21 files.
- NOT AFFECTED (vendored prebuilt): coreutils 5.2.1 (about 100 files in dist/gg), bebboget, bebboget00, installcerts, curl x3.
- Caveat: the 153 ports figure counts names in `upterm-ports/build/kit/userland/bin` as it is today, which includes names that coreutils/others emit; the package list is `ls upterm-ports/pkgs` (30 dirs). Unverified: whether any single ports recipe overrides CC to something else; common.mk and `grep -lE 'LOCAL|vbcc'` showed only ixcompat (LOCAL, gcc) and perl (matched "setlocale"), so none found.

## Ordered rebuild sequence (dependency order)

0. (no rebuild) ixemul-vtcon `docker/build.sh`: gcc 2.95.3, not affected. ixemul SDK headers are needed by step 1 only.
1. ixemul-vtcon/compat -> libixcompat.a: `cd upterm-ports && make clean-ixcompat ixcompat` (owner of the sysroot; another agent is working in upterm-ports, so do this when it is idle).
2. upterm-ports sysroot libs: `make clean-ncurses ncurses` (also installs terminfo); then zlib, bzip2, xz, libarchive (dependency chain from recipes: file needs ixcompat zlib bzip2 xz; libarchive ixcompat zlib bzip2 xz; git/mandoc need zlib; less/joe/ncdu need ncurses): `make clean-zlib zlib clean-bzip2 bzip2 clean-xz xz`.
3. upterm-ports programs: for each other pkg `make clean-<pkg> <pkg>` (PKGS: awk bc coreutils ctags diffutils ed file findutils fzy gawk git grep gzip iconv jq joe less libarchive lua make mandoc mg nano nc ncdu patch perl script sed tree vim zip), then `make kit-stage` (writes build/kit/userland + SOURCES.txt which `make dist` requires). Whole-tree alternative: `make clean` (removes build/{obj,host,state,expected,sysroot,src,kit}, keeps build/dl) then `make sysroot` and each pkg; this also drops the 942 MB host build, so prefer per-package clean. Unverified: that `kit-stage` triggers builds itself; run the per-pkg targets first.
4. tmux-amiga and screen-amiga (independent of each other and of ports; they link ixemul and, for tmux, libixcompat.a from step 1): `make -C tmux-amiga -f Makefile.amiga` after `rm -rf tmux-amiga/build`; `make -C screen-amiga/src -f Makefile.amiga clean screen`.
5. neovim-amiga (needs libixcompat? it uses `-lixcompat` for probes, and its own amiga/compat): `cd neovim-amiga && make clean-libs` (or rm -rf build/m68k build/v012/m68k) then `make` (0.4.4 Makefile: libamigacompat.a), `make -f Makefile.v012 libs`, `make -f Makefile.v012 host-deps` (host, arm64, not affected, may be kept), `make -f Makefile.v012 sysroot && tools/configure-nvim.sh` (RULES.md Commands for the v012 cmake configure; verify it targets v012), then `make -f Makefile.v012 dist` (ninja nvim_bin, strip, build/v012/dist/nvim). Delete `build/v012/m68k/nvim` first or ninja will not recompile objects whose flags did not change.
6. wasabid + bebbossh (vtcon): `cd vtcon && make wasabi-clean wasabi bebbossh-clean bebbossh`.
7. cpython-amiga: first the private cc1. Its own cc1 is the pre-fix generation; rebuild or replace it: `cd cpython-amiga && rm build/gcc/bin/cc1 && make cc1` (tools/build-cc1.sh builds from build/gcc/src; check that the fix commit aee333a76 is what it builds, since build/gcc/obj/gcc/cc1 is also dated 8 Oct and identical md5, so make will rebuild only if sources are newer; run `make -C build/gcc/obj` or the script). Then `make test-gcc-btst`, `make clean-m68k` equivalent (rm -rf build/m68k/build, keep configure outputs only if flags unchanged), `make compat m68k dist`. Output: build/m68k/dist/Python3.
8. vtcon `make amiga` is vbcc and need not rebuild; but the dist depends on step outputs: `cd vtcon && make dist` (dist-base needs ports kit-stage, screen, tmux, wasabid, bebbossh; dist-nvim needs neovim-amiga/build/v012/dist/nvim; dist-python needs cpython-amiga/build/m68k/dist/Python3; dist-pi runs `make -C amiga-pi amiga`, vbcc, already current). Dist version gates: tools/pkgs.py, tests `make dist-check`.

## Build-output disk (existing build dirs, du on 2026-10-17)

| Repo | Existing build size | Notes |
|---|---|---|
| upterm-ports/build | 2.6 GB (host 942 MB, src 911 MB, obj 523 MB, sysroot 96 MB, kit 92 MB, dl 103 MB) | a per-package rebuild rewrites obj+sysroot+kit only: about 0.7 GB peak; never `make clean` (host+src re-extract = another 1.9 GB) |
| cpython-amiga/build | 719 MB (gcc 544 MB, rest ~175 MB) | cc1 rebuild writes obj tree again if sources changed; m68k rebuild about 170 MB |
| neovim-amiga/build + vendor012 | 406 MB + 99 MB (v012 178, host 117, m68k 95, hostdeps 16) | m68k + v012 m68k about 270 MB to redo |
| vtcon/build | 6.8 GB total, but only build/wasabi, build/bebbossh, build/dist/* change | about 100 MB for wasabi+bebbossh; build/dist kit trees are rewritten (rm -rf $(KIT)), net zero if old ones are removed first |
| amiga-pi/build | 48 MB | not rebuilt |
| ixemul-vtcon build295 / build | 13 MB / 2.1 MB; compat 2.2 MB | libixcompat rebuild small |
| screen-amiga/src | 6.7 MB | |
| tmux-amiga/build | 2.7 MB | |

Total new writes if done one repo at a time with the old outputs overwritten: about 1.2 GB peak. Safe with 3.5 GiB free if neither `make clean` nor a dist of all archives at once (the dist copies 6 archives under vtcon/build/dist; check `du -sh vtcon/build/dist` first and remove stale archives).

## Blockers / open points

- cpython-amiga private cc1 is stale (8 Oct, pre-sret generation); must be rebuilt or the Python build repeated through the installed cc1 (remove `-B$(GCCB)/`, but that drops the btst patch 0001 unless the installed cc1 already contains it: unverified).
- upterm-ports is being used by another agent; steps 1-3 must wait for it.
- docker image ixemul-gcc295 not inspected beyond build.sh header; assumed gcc 2.95.3 as stated there.
- tmux Makefile.amiga clean target not confirmed (only the first 10 targets were grepped).
