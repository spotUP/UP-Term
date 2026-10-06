---
date: 2026-10-04
topic: Port plan for the Unix command-line tools UP-Term is missing
tags: [plan, userland, ports, ixemul, ncurses, gnulib, vfork, kit, licences]
status: draft
---

# Port plan: a full Unix userland in UP-Term

Input: `thoughts/shared/research/2026-10-04_unix-tool-gaps.md`, which has the inventory, the evidence and the ranked
gaps. This plan closes those gaps in the order a user hits them.

**Done** means that every tool listed in phases 1-3 has been installed by the kit into `SYS:UP-Term/bin`, reached
through vsh's `$PATH` in an UP-Term window, and has passed its rig check. Each tool has a man page, and its
licence obligations are met in the release. Phases 4-5 are separate finish lines with their own criteria.

## Decisions for the owner (blocking only the items named)

**ANSWERED 2026-10-04 by the owner: D1 = a (current upstream), D2 = b (separate UP-Term-src.lha),
D3 = grow vsh, D4 = am-git optional once its licence is confirmed, D5 = optional Installer component.**

- **D1. Port current upstream, or ship the 1998 Geek Gadgets binaries first?**
  - Option a: port current upstream from the start.
  - Option b: ship the GG binaries (grep 2.1/2.5, sed 4.1.4, gawk 3.0.3, findutils 4.1, diffutils 2.7, patch 2.5, tar 1.12, gzip 1.2.4, less 321) now and replace them later.

  **Recommended: a.** Once phase 0 exists, porting a GNU tool follows one recipe, so phase 1 closes the
  first-hour gap in about the time it takes to vet the GG set on the rig. The GG archivers carry published CVEs
  (gzip 1.2.4, tar 1.12). None of the GG tools speaks UTF-8, and grep 2.1 has no `--color`. Option b also ships
  binaries the kit would have to take back out. *Blocks: nothing; phase 1 starts with a.*
- **D2. Where do the GPL sources go?**
  - Option a: in the kit, as coreutils, BebboSSH and curl do today.
  - Option b: a companion `UP-Term-src.lha` published next to every release, with the kit's README naming it.

  **Recommended: b, once the GPL sources exceed ~5 MB.** Phases 1-3 add about 25-30 MB of source tarballs (est.: grep 1.9 MB,
  sed 1.4, gawk 3.5, diffutils 1.9, findutils 2.2, tar 2.3, gzip 0.8, xz 1.6, libarchive 7.5, ncurses 3.7, nano 3.3,
  less 0.6, mandoc 0.5 MB as .tar.xz/.gz, from memory of upstream sizes, unverified). That would triple a 10 MB kit for
  a source copy few users open. GPLv2 §3 and GPLv3 §6(d) both allow the source on an equivalent server,
  provided the directions are clear and the release ships alongside it. *Blocks: the phase 1 kit item.*
- **D3. bash and zsh.** **Recommended: do not port them now. Grow vsh** (item 0.6). Both shells fork without
  exec for subshells, `$(...)`, `( )` and pipelines of builtins. On a vfork-only ixemul, that means
  rewriting their execution layer (L/XL) and carrying the result forever. vsh already runs these constructs without fork.
  Revisit if a concrete program needs bash itself. *Blocks: nothing.*
- **D4. git: bundle am-git, or port real git (L)?** **Recommended: offer am-git 0.12.1 as an optional
  component** once its licence allows redistribution (unverified: its readme names no licence). A real git port
  (run-command over vfork, plus zlib, a TLS story for ixemul and curl's HTTP transport) is L; it waits until
  phases 1-3 are done. *Blocks: item 5.6.*
- **D5. Neovim in the kit?** It is 33 MB with its runtime, needs 9.7 MB of RAM and takes 22 s to reach the TUI at A1200 speed.
  **Recommended: an optional Installer component (CPU040/060 and RAM shown in the prompt), with vim as the
  default vi.** *Blocks: item 4.3.*

## Shared rules for every port

- **Runtime: ixemul-vtcon for the Unix userland.** It accepts `/SYS/x` and `SYS:x`, reads `$PATH`, handles
  signals, termios and SIGWINCH, and finds `/bin/sh`.
  - A native (libnix) build fits only tools whose arguments are not paths, such as curl, ssh and ping.
  - Existing native tools (UnZip 6.0-31, LhA, AvePING) are *pointed to* in the README, not rebuilt.
- **Toolchain:** bebbo `~/opt/amiga/bin/m68k-amigaos-gcc` 6.5.0b, `-mcrt=ixemul -m68020 -O2`, soft float. This
  matches the kit's coreutils (020 soft-float), and on an A1200 without an FPU the soft-float path is the one that runs. Link with
  `-lixcompat`, using the ixemul-vtcon SDK headers (`ixemul-vtcon/docker/install-sdk-headers.sh`).
  - The toolchain is not on PATH. The ports Makefile sets `CROSS := $(HOME)/opt/amiga/bin/m68k-amigaos-`.
- **One ports repo**, `~/Code/upterm-ports`, with its own git like tmux-amiga.
  - Structure: one directory per package, holding `recipe.mk` (upstream URL, version, sha256, configure flags or a
    hand-written config.h), `patches/` (`git format-patch` style) and `check/` (the rig test, see Verification).
  - It also holds a shared sysroot `build/sysroot` for the libraries (zlib, bzip2, liblzma, ncurses6, libarchive,
    pcre2). The existing tmux-amiga, screen-amiga and neovim-amiga repos stay where they are.
  - Why one repo: about 30 packages, each a recipe plus a few patches. A repo per package would multiply the
    plumbing; a single tree with a single sysroot is how pkgsrc and buildroot solve it.
  - Pin versions at port time. The versions named below are the targets as of this writing; check the current stable release
    and record the one used in `recipe.mk`.
- **Configure:** autoconf cross builds with `--host=m68k-amigaos CC="$(CROSS)gcc -mcrt=ixemul"`, plus a per-package
  `config.site` of cache values for run tests (gnulib's `gl_cv_func_*`). curl-bebboget is the precedent that a
  configure cross build works with this gcc (its readme lists the line). Where configure fights, use a hand-written config.h
  as tmux did (`tmux-amiga/amiga/tmux/config.h`).
- **No fork.** Every `fork()` site becomes one of the three proven forms:
  1. vfork + execve, with the child touching only its own state;
  2. `posix_spawn` (item 0.2);
  3. a re-exec of self with a marker variable.

  A host check (item 0.4) fails the build when a binary imports `_fork`.
- **Every bug found in ixemul gets fixed in ixemul-vtcon, with a rig probe** (the P6/P7 practice), not worked around in
  the port.
- **`$STACK:` cookie** in every binary that recurses (grep, sed, gawk, find, less, nano), measured with the
  existing stackprobe approach.

## Phase 0: prerequisites (shared, before any tool)

| ID | Item | Effort | Detail |
|---|---|---|---|
| 0.1 | **Ports repo + sysroot + recipe driver** | M | `~/Code/upterm-ports`: top Makefile (`make <pkg>`, `make sysroot`, `make kit-stage`), per-package `recipe.mk`, downloads verified by sha256, sources unpacked to `build/src/<pkg>`, patches applied with `git apply`. Resumable and recording, per the global §6a: `build/state/<pkg>.{built,checked}` stamps; `make <pkg>` rebuilds only that package. |
| 0.2 | **libixcompat: posix_spawn / posix_spawnp over vfork** | M | Extract `tmux-amiga/amiga/vspawn.c:18-79` (child: SIG_DFL, chdir, setsid, dup2s, TIOCSCTTY, sigmask, environ swap, exec) and libuv's exec-errno return (`neovim-amiga/vendor/libuv/src/unix/process.c:505-535`) into `ixemul-vtcon/compat/spawn.c` with the POSIX file-actions and attribute API. tmux and screen move onto it later (not required). Host test: compile-only against the SDK; rig probe `tests/amiga/spawnprobe` (spawn, file actions, a failing exec gives the errno, environ is untouched in the parent). |
| 0.3 | **libixcompat: wide characters and the missing POSIX calls** | M | `mbrtowc mbrlen mbsrtowcs wcrtomb wctob btowc mbsinit wcwidth wcswidth iswprint iswalpha... towlower/towupper nl_langinfo(CODESET)`: UTF-8 only, plus "C"/Latin-1 by `LANG`/`LC_ALL`. wcwidth comes from vtcon `engine/vtwidth.h`, the same table tmux uses (`tmux-amiga/amiga/utf8proc.c`), so the terminal and its programs agree on widths. Also `clock_gettime(CLOCK_REALTIME/MONOTONIC)` and `getaddrinfo/freeaddrinfo/gai_strerror` over ixnet's gethostbyname. **Measure first:** list the symbols the phase 1 tools fail to link on (the tmux T2.2 rule: from link errors, not guessed). |
| 0.4 | **Host binary check** `upterm-ports/tools/check_bin.sh` | S | For each binary, it checks: an AmigaOS hunk header; opens `ixemul.library`; **no `_fork` import** (nm of the link map); a `$STACK:` cookie where required; no 68030+ instructions (objdump scan for `move16`, `pflush` etc.); and records the size in `build/state/sizes.tsv`. It runs in `make <pkg>`. |
| 0.5 | **Rig runner** `tools/rig/userland_rig.py` (in vtcon, beside the other rig scripts) | M | For each package `check/` script: copy the binary to VTC:, run its command list through vsh in an XCON window, capture stdout to `VTC:out/<pkg>/<n>.txt`, and diff against `expected/<n>.txt`. The expected files are produced on the host by the *same upstream version*, built for macOS by the same recipe (`make host-<pkg>`). `--only <pkg>` and `--failed` are supported. Verdicts are written per package to `build/rig/userland/<pkg>.verdict`, and passed ones are skipped on rerun (§6a). |
| 0.6 | **vsh: the builtins scripts need** | M | `eval`, `exec`, `trap` (EXIT, INT, TERM: ixemul signals reach native vsh as breaks; measure which), `local`, `umask` (ixemul umask is per process: decide by probe whether vsh passes it), `getopts`, `type` (an alias of `which`), and `which` printing the resolved path (`$PATH` walk, `sh_path_next`). Host-tested in `tests/test_sh_exec.c` like the existing builtins (plans/2026-09-29-vsh.md S2.4). These are vsh's own items; they go into the vsh plan's ledger when started. |
| 0.7 | **ixnet.library in the kit** | S | It is missing today (install.dos/Makefile have no ixnet). The kit copies ixemul-vtcon's `build295/ixnet/68020/amigaos/ixnet.library` with the same `.orig` keep and Uninstall rules as ixemul.library. Check: install_rig gains "ixnet present after Install, restored after Uninstall". Needed before any ixemul network tool (nc, telnet). |
| 0.8 | **ncurses 6 with wide characters (ledger N1)** | M | ncurses 6.5 (or current), `--host=m68k-amigaos --enable-widec --without-cxx --without-ada --disable-shared --with-terminfo-dirs=/ENV/up-term/terminfo --with-default-terminfo-dir=/ENV/up-term/terminfo`, terminfo compiled on the host by the host `tic` (as the kit already does). It needs 0.3 for widec. Its tools (`tput clear reset tset infocmp tic toe`) ship. tmux/screen relinking against it is a separate later choice (they work on 5.5 today). Rig: `tput cols`/`tput lines` match the window; `reset` restores the terminal after `cat /dev/urandom`-style garbage (a recorded stream file); the curses `ncurses` test program draws the ACS box. |

Phase 0 success criteria:
- `make sysroot` builds zlib, ncurses6 and libixcompat from scratch on the Mac.
- spawnprobe passes on the rig.
- vsh's new builtins pass the host tests.
- install_rig is still green with ixnet added.

## Phase 1: the first hour (text tools, pager, editor)

| ID | Tool(s) | Source (target version) | Build route | Effort | Fork sites / special work |
|---|---|---|---|---|---|
| 1.1 | grep, egrep, fgrep | GNU grep 3.11+ | autoconf cross + config.site, gnulib | M (the first gnulib port; sets the recipe pattern) | none; `--color=auto` checks isatty; UTF-8 through 0.3; PCRE (`-P`) off at first |
| 1.2 | sed | GNU sed 4.9 | gnulib recipe from 1.1 | S | `-i` writes via a temp file + rename: check rename over an existing file on FFS (ixemul rename semantics; probe) |
| 1.3 | awk | **onetrue-awk** (BWK, the second edition, 2023+) as `awk` | plain Makefile | S | `system()` and `|` pipes use popen, which is ixemul's (vfork + /bin/sh = vsh). gawk 5.x is item 4.8 for those who need it |
| 1.4 | less, lesskey | less 668+ (current stable) | autoconf cross, ncursesw (0.8) | S | `!cmd` and `|` use system/popen; `v` runs `$VISUAL`/`$EDITOR` through the shell; set `LESSCHARSET=utf-8` when the locale says UTF-8 |
| 1.5 | nano | GNU nano 8.x | autoconf cross, ncursesw | M | SIGWINCH (P6: TIOCSWINSZ -> SIGWINCH); the ESC-O key timing seen in nano 1.2.5 (ledger P2) to be rechecked with ncurses 6; `--enable-utf8`; syntax files shipped |
| 1.6 | find, xargs | GNU findutils 4.10 | gnulib recipe | M | `-exec`, `-execdir`, `-ok` and xargs use fork+exec. Port onto `posix_spawn` (0.2). locate/updatedb are left out (they need cron and a DB) |
| 1.7 | diff, cmp, diff3, sdiff | GNU diffutils 3.10+ | gnulib recipe | S-M | diff3/sdiff spawn diff: posix_spawn. `--color` |
| 1.8 | patch | GNU patch 2.7.6 / 2.8 | gnulib recipe | S | It runs ed/merge only for unusual inputs: use posix_spawn |
| 1.9 | man pages + `man` | **mandoc** 1.14.6 (ISC) | plain configure (`configure.local`) | M | `man` runs the pager via `$MANPAGER`/`$PAGER` (less). The kit installs pages under `SYS:UP-Term/man/man1` with a `MANPATH` in vshrc. Every phase's tools must contribute their upstream pages; UP-Term's own (vsh, UPTerm, upgetty, sz/rz) are written in mdoc |
| 1.10 | kit integration | - | `dist` + install.dos + Uninstall | S | One drawer `Files/userland` staged by `make kit-stage` in the ports repo; Install copies it to `SYS:UP-Term/bin` (the coreutils pattern, install.dos:100-108); Uninstall removes exactly the staged list; licences per D2 |

Phase 1 verification:
- **Host:**
  - `make grep sed awk less nano findutils diffutils patch mandoc` builds.
  - check_bin passes for every binary.
  - `make host-<pkg>` builds the same version for macOS and writes the expected outputs.
- **Rig, automated:** `userland_rig.py --only <pkg>` passes for each package.
  - grep: `-r`, `-i`, `-E`, `-c`, `-n`, `--color=always` SGR bytes, a UTF-8 pattern `café`, and an exit status of 1 on no match.
  - sed: `s///g`, `-E`, `-i` on a file in RAM:, `-n p`.
  - awk: field sums, `printf`, `system("echo x")`.
  - less: `less -R` of a coloured file, captured in the engine with the existing capture tooling (`make capture`): it renders and `q` exits.
  - nano: open, type, save, and compare the bytes.
  - find: `-name`, `-type d`, `-exec echo {} \;`, `-print0 | xargs -0`.
  - diff: `-u`; patch: applies that diff back.
  - man: `man grep` reaches less, and `man -T utf8` renders.
- **Reachability (one test):** install_rig gains one step. After Install, a fresh UP-Term window runs `grep --version`
  through vsh with the kit's `PATH`, and the sentinel is the version string coming from `SYS:UP-Term/bin/grep`.
  The test fails on a kit without the tool.
- **Owner, on the rig or the A1200 (manual):** the first-hour script in an UP-Term window:
  1. `ls --color`, `grep -rn TODO SYS:S`, `man grep`, `nano S:User-Startup` (then quit without saving);
  2. `find SYS:Prefs -name "*.prefs" | xargs ls -l`, `diff -u a b | less -R`.

  PASS = each runs with colour and no requester or Guru. FAIL = whatever they print or do instead.

Phase 1 success criteria:
- Every tool in 1.1-1.9 is in the kit and passes its rig check.
- The reachability test passes.
- The owner's first-hour script passes.
- The research doc's ranks 1-4, 6, 7 and 9 are closed (the editor, rank 3, is closed by nano; vim is phase 4).

## Phase 2: archives

| ID | Tool(s) | Source | Route | Effort | Notes |
|---|---|---|---|---|---|
| 2.1 | zlib (lib), gzip, gunzip, zcat | zlib 1.3.1; GNU gzip 1.13+ | plain / gnulib recipe | S | |
| 2.2 | bzip2, bunzip2, bzcat (+ libbz2) | bzip2 1.0.8 | plain Makefile | S | The native bzip2 often in C: is untouched (the Unix one lives in SYS:UP-Term/bin) |
| 2.3 | xz, unxz, xzcat (+ liblzma) | XZ Utils **5.6.3+ or 5.8.x** (never 5.6.0/5.6.1, the backdoored releases) | autoconf cross, `--disable-threads` | S-M | |
| 2.4 | tar, cpio, unzip (read zip/7z/lha/iso) | **libarchive 3.7+/3.8 `bsdtar`, `bsdcpio`, `bsdunzip`** (BSD licence), linked statically with zlib, bzip2 and liblzma | autoconf/cmake cross | M | Compression is in-process, so tar needs no fork for `-z/-j/-J`. Install `bsdtar` as `tar`. GNU tar is not ported, because it spawns gzip and needs rmt/fork paths. bsdtar reads `.lha` and `.lzh` too. Amiga protection bits: decide by a probe whether `-p` maps `rwed` (via ixemul chmod) sensibly |
| 2.5 | zip | Info-ZIP Zip 3.0 | its unix Makefile, ixemul | S | UnZip stays native: UnZip 6.0-31 is the recommended one, named in the README. bsdunzip covers Unix users |
| 2.6 | lha, lzx | not ported | - | - | Native LhA 2.15 and LZX are what Amiga users have. `bsdtar -xf x.lha` covers reading |

Verification:
- **Host:** round trips. Archives made on the Mac with the same versions are unpacked on the rig, and archives made on the rig are unpacked on the Mac
  (byte-compare trees: `tar cf - | sha256sum` on both sides; `sha256sum` arrives in 3.6 or the Mac does the hashing).
- **Rig:** `userland_rig --only tar`: `tar xzf`, `tar cJf`, `tar tvf`, extracting a GitHub release tarball with long names
  (FFS 30-character name limit: record the behaviour; PFS/SFS allow 107), and a `../evil` path refused.

Success: all of 2.1-2.5 are in the kit and pass. A real source tarball, for example the one `curl` downloads, unpacks on FFS without a requester.

## Phase 3: daily comfort

| ID | Tool(s) | Source | Effort | Notes |
|---|---|---|---|---|
| 3.1 | file | file 5.45+ (BSD) | S-M | The compiled magic is ~8 MB (est.); ship a trimmed magic (text, archives, images, audio, Amiga hunk/IFF/LhA/ADF entries) and keep full magic optional |
| 3.2 | tree | tree 2.x (GPL-2) | S | |
| 3.3 | ps, top | **own code** in vtcon (native vbcc, like UPDemo): `ps` lists CLI processes and ixemul pids (ixemul's process list: probe for an API; else exec task list + CLI numbers), `top` is a terminal program with xterm drawing (alternate screen, SGR) refreshing CPU (via `Forbid` task snapshot deltas, as atop does: method to be measured), memory, tasks | M | htop is not portable (needs /proc). These are UP-Term's own small tools |
| 3.4 | watch | procps-ng `watch` or a small own one | S | Spawn per interval: posix_spawn |
| 3.5 | script | BSD script(1) | S | PTY: from P5/P6; `/dev/ptyXY` via ixemul; vfork child on the slave |
| 3.6 | ncdu | ncdu 1.x (C, ncurses) | S-M | Not 2.x (Zig) |
| 3.7 | coreutils 9.x | GNU coreutils 9.5+ replacing 5.2.1 | M-L | Brings sha256sum/b2sum, timeout, realpath, truncate, df (`statfs` via ixemul: probe), UTF-8 aware cut/wc. Many binaries; build as `--enable-single-binary=symlinks`? AmigaOS has no symlinks for executables in the general case. Measure: either separate binaries (~150 KB each, est.) or one binary + tiny stubs that exec it. **Decide by measured kit size** |
| 3.8 | hl, mdv | own (feature/highlight-markdown) | - | Its own plan (2026-10-04-highlight-markdown.md); listed so the kit order includes it; `hl` doubles as `bat`, `mdv` can show tldr pages (5.9) |
| 3.9 | fzy | fzy 1.0 (MIT) | S | The fuzzy finder (fzf is Go, so it cannot be built); vsh Ctrl-R / file picker integration is a vsh item later |

Success: 3.1-3.6 and 3.9 are in the kit and pass. 3.7 is decided by measurement and done either way.

## Phase 4: editors and languages

| ID | Item | Effort | Notes |
|---|---|---|---|
| 4.1 | vim 9.1, ixemul + ncursesw, `--with-features=normal`, no GUI, `--enable-multibyte` | M-L | `:!cmd` and `system()` via vsh. The native Aminet vim 9.1 stays unused: it has no UTF-8 and does not follow PTY:'s SIGWINCH. Install as `vim` and `vi` |
| 4.2 | mg (OpenBSD, public domain / BSD) | S | A tiny Emacs-like editor; this replaces emacs-nox, which is not ported |
| 4.3 | Neovim 0.12.5 from ~/Code/neovim-amiga as an optional component (D5) | S (packaging) | dist from `make -f Makefile.v012 dist`; Installer option shows the RAM/CPU needs |
| 4.4 | Lua 5.4 ixemul build | S | Unix paths and io.popen over vfork. amiport's native 5.4.7 has no popen and takes Amiga paths only |
| 4.5 | joe 4.x | S | Optional, if asked for |
| 4.6 | bc, dc | GNU bc 1.07.1 | S |
| 4.7 | jq | jq 1.7+/1.8 (MIT), oniguruma off | S-M |
| 4.8 | gawk 5.x | gnulib recipe | M | For users who need GNU awk extensions; `awk` stays onetrue-awk |
| 4.9 | column, iconv | util-linux `column` (or BSD); GNU libiconv 1.17 `iconv` | S each |
| 4.10 | ed | GNU ed 1.20 | S |
| 4.11 | ctags | Exuberant ctags 5.8 (GPL-2) | S | universal-ctags is L |

Success: 4.1, 4.2, 4.4 and 4.6-4.10 are in the kit and pass. Neovim is shipped as the D5 decision says.

## Phase 5: network and development

| ID | Item | Effort | Notes |
|---|---|---|---|
| 5.1 | nc | OpenBSD netcat (BSD), ixemul + ixnet | S | Needs 0.7 |
| 5.2 | telnet CLI | inetutils telnet, or own small client on ixnet | S | vshrc's `telnet()` already expects a `telnet` |
| 5.3 | wget | not ported; a `wget` vshrc function over curl (`curl -fLO`) | S | |
| 5.4 | bebbossh fixes | upstream patches (research 2026-10-03): TERM and LANG from the variables, window size via ACTION_VTCON_GWINSZ and changes | S-M | GPL, author active |
| 5.5 | make | GNU make 4.4.1, posix_spawn | M | |
| 5.6 | git | D4: am-git now; real git 2.x later | S / L | Real git: `run-command.c` via posix_spawn (git already has a spawn-style path for Windows, `compat/mingw.c`, to model on), zlib, HTTP through libcurl requires an ixemul-built libcurl + TLS (AmiSSL from an ixemul program: the socket descriptors differ; **risk, probe first**) |
| 5.7 | rsync | rsync 3.x over bebbossh | L | Two processes over pipes: an ixemul rsync spawning native bebbossh through IXPIPE: (exists). Probe the pipe throughput first |
| 5.8 | perl | perl 5.40 | L | Cross-configuring perl is hard (miniperl must run on the target, or use perl-cross). After everything else; owner call |
| 5.9 | tldr | an own `tldr` script: curl a page from tldr-pages, render with `mdv` | S | Depends on 3.8 |
| 5.10 | python | PY1 (separate plan) | - | Ledger :674 |

## Order of work (and why)

1. Phase 0, in this order:
   - 0.1 ports repo, 0.4 check_bin and 0.5 rig runner (the means to verify);
   - 0.6 vsh builtins (own code, in parallel with the rest of phase 0);
   - 0.3 wchar layer, measured from grep's and less's link errors;
   - 0.2 posix_spawn;
   - 0.8 ncurses 6;
   - 0.7 ixnet.
2. Phase 1, in this order:
   - grep, sed and awk first (1.1 sets the gnulib recipe; 1.2 reuses it);
   - then less (it needs ncurses 6);
   - then nano;
   - then findutils, diffutils and patch (they need posix_spawn);
   - then mandoc and pages, then the kit.
3. Phase 2 (archives): independent of ncurses, so it can run in parallel with the second half of phase 1.
4. Phases 3, 4 and 5 in rank order. Within a phase, order by the research doc's rank.

Rig use: one rig, at most two emulators at once (memory note `max-two-emulators.md`). The rig runner batches
packages per boot and records verdicts. Never run a full sweep after a single-package change.

## How the tools ship

- **Location:** `SYS:UP-Term/bin` (on `$PATH`, the coreutils decision of 2026-10-02: GNU names must not shadow `C:Sort`/`C:Date`), and
  `SYS:UP-Term/man` for pages. Nothing goes in C:, except that `less`/`nano` could get C: aliases only if the owner asks.
- **Installer components:**
  - "Unix tools" (phases 1-3, default on);
  - "vim" (default on);
  - "Neovim" (off, D5);
  - "git (am-git)" (off, D4).
  - The existing SSH, curl and bebboget options are unchanged.
- **Size budget (est., to be replaced by `build/state/sizes.tsv`):**
  - phase 1 binaries ~3-4 MB;
  - phase 2 ~1.5 MB;
  - ncurses tools and terminfo ~0.5 MB;
  - vim ~2 MB;
  - man pages ~1 MB uncompressed.

  The kit .lha should grow by about 4-5 MB packed. RAM: each tool is loaded only while it runs. GNU tools at A1200 speed load in about 1 s
  per 500 KB from FFS (unverified, measure on the rig).
- **Licences:**
  - **GPL-2/GPL-3:** grep, sed, gawk, findutils, diffutils, patch, gzip, nano, coreutils, bc, ed, make, tree, ncdu,
    joe, ctags. The release carries COPYING and the complete corresponding source, including **our patches and build
    recipes** (the ports repo's `patches/` and `recipe.mk` count as "scripts used to control compilation"), per D2.
  - **GPL-3 tools additionally** need the licence text and no added restrictions. Installation Information applies
    only to User Products sold with the software, which is not our case.
  - **ixemul (BSD-ish/GPL mix, already shipped):** unchanged.
  - **BSD/ISC/MIT:** file, onetrue-awk, mandoc, libarchive, xz/liblzma (0BSD/PD), zlib, bzip2, fzy, jq, mg, nc. Ship the licence notice in
    `SYS:UP-Term/doc/licenses/<pkg>`.
  - **am-git:** licence unknown; D4 waits on it.
  - **Mechanism:** `make kit-stage` writes `Files/userland/SOURCES.txt` (package, version, upstream URL, sha256, licence) from the
    recipes. It is the single source of truth for the README's licence section and for the source archive.

## Biggest risks

1. **The wchar layer (0.3) is the hidden dependency of UTF-8 in grep, less, nano, ncursesw and vim.** If its tables or
   `mbrtowc` disagree with the terminal's width table, editors misplace the cursor on every non-ASCII line.
   Mitigation by design: one table (`engine/vtwidth.h`) for terminal and libc.
2. **gnulib vs ixemul 48.2.** gnulib replaces what it finds broken, and configure's run tests cannot run in a cross build,
   so cache values decide. A wrong value gives a binary that builds and misbehaves on the rig. The first port (grep) sets the
   config.site; budget M for it, not S.
3. **posix_spawn on vfork has the shared-memory trap.** A child that writes globals or mallocs corrupts the parent
   (screen's `displays=0`, ledger :927). The implementation must touch only its own stack and kernel state;
   spawnprobe checks the parent's environ, heap and errno after both a failing and a succeeding exec.
4. **Speed and memory on a stock A1200.** tmux started in 9.3 s only after a malloc cache. GNU tools linked with gnulib are
   500 KB-1 MB each. `grep -r` over a hard disk on a 14 MHz 020 may be slow enough that users call it broken.
   Measure each phase 1 tool's start time on the rig and record it. A slow path gets an ixemul fix, as malloc did.
5. **Filesystem semantics:** the FFS 30-character names (tar extraction), no symlinks for binaries, protection bits
   as Unix modes (the "everything green" `ls`), `rename` over an existing file, `/tmp` = `TMP:`. Each one has a probe
   in the package that first hits it.
6. **Rig time is shared and scarce** (one rig, overheating Mac). Mitigation: host-built expected outputs, a
   batched and resumable rig runner, and `--only`.
7. **Licence compliance drift:** a GPL binary in the kit without its exact source and patches is a violation. Mitigation:
   SOURCES.txt is generated from the recipes, and `make dist` fails when a staged binary has no SOURCES.txt row.

## Checklist (IDs, for the ledger)

Phase 0:
- [x] 0.1 ports repo
- [x] 0.2 posix_spawn
- [x] 0.3 wchar + POSIX compat
- [x] 0.4 check_bin
- [ ] 0.5 userland_rig
- [ ] 0.6 vsh builtins
- [x] 0.7 ixnet in the kit
- [x] 0.8 ncurses 6

Phase 1:
- [x] 1.1 grep
- [ ] 1.2 sed
- [ ] 1.3 awk
- [ ] 1.4 less
- [ ] 1.5 nano
- [ ] 1.6 findutils
- [ ] 1.7 diffutils
- [ ] 1.8 patch
- [ ] 1.9 mandoc + pages
- [ ] 1.10 kit

Phase 2:
- [ ] 2.1 gzip
- [ ] 2.2 bzip2
- [ ] 2.3 xz
- [ ] 2.4 bsdtar
- [ ] 2.5 zip

Phase 3:
- [ ] 3.1 file
- [ ] 3.2 tree
- [ ] 3.3 ps/top
- [ ] 3.4 watch
- [ ] 3.5 script
- [ ] 3.6 ncdu
- [ ] 3.7 coreutils 9
- [x] 3.8 hl/mdv in the kit
- [ ] 3.9 fzy

Phase 4:
- [ ] 4.1 vim
- [ ] 4.2 mg
- [x] 4.3 Neovim option
- [ ] 4.4 Lua
- [ ] 4.5 joe
- [ ] 4.6 bc
- [ ] 4.7 jq
- [ ] 4.8 gawk
- [ ] 4.9 column/iconv
- [ ] 4.10 ed
- [ ] 4.11 ctags

Phase 5:
- [ ] 5.1 nc
- [ ] 5.2 telnet
- [ ] 5.3 wget function
- [ ] 5.4 bebbossh patches
- [ ] 5.5 make
- [ ] 5.6 git
- [ ] 5.7 rsync
- [ ] 5.8 perl
- [ ] 5.9 tldr
- [x] 5.10 python (PY1)

Total: 53 items (8 + 10 + 5 + 9 + 11 + 10), 0 done.

Status note 2026-10-06: ticked the rows with evidence (0.1-0.4, 0.7, 0.8, 1.1, 3.8, 4.3, 5.10). Still open and verified: 0.5 userland_rig.py (no file), 0.6 vsh builtins (no eval, exec, trap, local, getopts, umask), 3.7 (coreutils 5.2.1 shipped, not 9). 4.4 Lua unconfirmed; the rest not checked.
