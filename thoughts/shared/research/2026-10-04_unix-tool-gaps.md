---
date: 2026-10-04
topic: The command-line tools a daily Unix user expects in UP-Term, what exists for AmigaOS 3.x, and the gaps
tags: [research, userland, geek-gadgets, ixemul, coreutils, ports, gaps, aminet]
status: draft
---

# Unix tools in UP-Term: inventory and gaps

Owner 2026-10-04: "we researched geek gadgets for their ported bins, what commandline bins/tools do we
need for a 'full fledged' unix term experience? find the gaps for everyday use and write a plan on how
to port them". The plan is `thoughts/shared/plans/2026-10-04-unix-tool-ports.md`.

This research is read-only. No emulator was started. The rig disk was listed but not booted. Aminet facts come
from the live `dev/gg` index (all 411 rows, parsed from aminet.net/dev/gg pages 1-10), Aminet searches and
package readmes fetched on 2026-10-04, and the codewiz Geek Gadgets mirror's top-level listing.
**(unverified)** means I could not confirm it, and I say where I looked.

## Bottom line

- **The most basic set is in place:** colour `ls`, coreutils 5.2.1 (87 tools: cp, mv, rm, cat, head,
  tail, wc, sort, uniq, cut, tr, tee, env, date, du, touch, chmod, ln, mkdir, seq, yes, sleep, printf, test,
  stty, fmt, md5sum ...) on `$PATH` in `SYS:UP-Term/bin`, plus ssh/scp (BebboSSH 1.45), curl 8.22.0, tmux 3.6a and
  GNU screen 4.9.1. A Unix user can already move files around and log in to other machines.
- **The first hour still fails on four things:**
  - text search and editing on the command line: grep, sed and awk are all missing from the kit;
  - a pager: the kit has no less. The rig has GG less 321 from 1998, but the kit does not ship it;
  - an editor: the kit has none. The rig has GG nano 1.2.5, and Neovim 0.12.5 is ported in a sibling repo, but neither is in the kit;
  - archives: tar, gzip, bzip2, xz and zip are missing. Native LhA is usually present.
  - After those come find/xargs, diff/patch, man, file and a `which` that prints a path.
- **Geek Gadgets binaries run on UP-Term's ixemul.** UP-Term's patched ixemul 48.2 is the GG ABI. Measured on
  the rig: less 321, tcsh 6.12, pdksh 4.9, nano 1.2.5, fileutils 3.16 and coreutils 5.2.1 (ledger
  `plans/2026-09-28-vtcon.md:845`, `plans/2026-10-02_unix-userland.md:141`). But most GG text tools date
  from 1998. They predate `--color` and UTF-8, and the archivers among them carry published CVEs. Use them as a
  fallback or a reference, not as the target (see the plan's decision D1).
- **Since September 2026, Aminet has a wave of new native (libnix, no ixemul) ports:**
  - BusyBox 1.37.0 (69 applets);
  - OpenBSD grep 1.68 and Lua 5.4.7 (both from amiport);
  - curl 8.22.0, UnZip 6.0-31, atop and am-git 0.12.1.

  All of them take **AmigaOS paths only**. vsh maps a leading `/` for command names, `cd` and redirections
  (ledger V2), but not for arguments. So `grep x /SYS/S/Startup-Sequence` works with an ixemul grep
  and fails with a libnix grep. **The Unix userland therefore has to be ixemul builds.** Native builds fit
  only for tools whose arguments are mostly host names and URLs (curl, ssh, ping).
- **There is no fork().** ixemul-vtcon has vfork only (`library/vfork.c`). Three ports have already solved
  this with one pattern: tmux `amiga/vspawn.c:18-79`, screen `src/screen.c:364-400` and libuv
  `process.c:505-535`. The parent prepares everything, the child changes only its own state and then calls execve, and a daemon
  re-execs itself with a marker variable. This makes bash and zsh **L/XL ports with doubtful payoff**: subshells and
  `$(...)` fork without exec. vsh, which needs no fork, is the shell to grow instead.

## 1. What UP-Term ships or has ported today

### 1.1 The kit (`make dist` -> build/UP-Term.lha, 10,024,257 bytes, built 2026-10-03 23:19)

Evidence: `Makefile:509-545`, `dist/install.dos`, `dist/Install.installer:50,103`.

| What | Where installed | Runtime | Evidence |
|---|---|---|---|
| vsh (POSIX-style shell, 24 builtin functions) | C:vsh, SYS:UP-Term/bin/sh, GG:bin/sh | native | install.dos:86,184 |
| coreutils 5.2.1-9 (GG, 87 binaries incl. ls, dircolors, stty, kill, fmt, md5sum) | SYS:UP-Term/bin | ixemul 48.2 | install.dos:100-108, dist/gg/coreutils-5.2.1 |
| screen 4.9.1 (497,144 B), tmux 3.6a (1,141,292 B) | C: | ixemul | install.dos:92,96 |
| ixkill (signals for ixemul processes) | C: | ixemul | install.dos:87 |
| sz / rz (own ZMODEM) | SYS:UP-Term/bin | native | install.dos:111 |
| upgetty, UPTerm, UPConsole, UPDemo, UP-Term Prefs | C: | native | install.dos:112-116,343 |
| BebboSSH 1.45: bebbossh, bebboscp, bebbosshkeygen, bebbosshd (+ libcryptossh 020/060) | SYS:UP-Term/bin, optional | native, bsdsocket | install.dos:117-133 |
| bebboget 1.11 + installcerts | SYS:UP-Term/bin, optional | native | install.dos:134-136 |
| curl 8.22.0 (020/040/060 builds, ~1.22 MB each) | SYS:UP-Term/bin/curl, optional | native, AmiSSL 5 | install.dos:137-147 |
| wasabid (optional) | C: | native | install.dos:275 |
| ixemul.library 48.2 [UP-Term] (the old one is kept as .orig) | LIBS: | - | install.dos:316 |
| terminfo vtcon, screen, screen-256color; termcap.vtcon | ENVARC:up-term | - | Makefile:99-110, install.dos:61,84 |

Not in the kit:
- **ixnet.library**: there is no `ixnet` anywhere in install.dos or the Makefile. An ixemul program that
  uses sockets needs it, so this matters for any network tool built against ixemul.
- **a `telnet` client**: vshrc's `telnet()` runs `command telnet` (dist/vshrc), and the kit ships no telnet.
  The user's own telnet is used if one exists (unverified which one; I looked at install.dos and dist/).

`vshrc` sets `PATH=/SYS/UP-Term/bin:/gg/bin:/c` and aliases `ls='ls --color=auto'`, `ll='ls -l'`
(dist/vshrc:51-60), and defines `clear()`, `ssh()`, `scp()`, `telnet()`, `rlogin()` and `remote_term()`.

### 1.2 vsh builtins (counted from the dispatch table `builtins[]`, shell/sh_exec.c:1213-1225)

There are 28 names over 24 functions:
- `: true false echo printf cd pwd export unset set shift exit return break continue read alias unalias`
- `test [ jobs wait fg bg stack source . which`
- `command` is handled inline (sh_exec.c:1465).

**Missing builtins that scripts and users reach for:** `exec`, `eval`, `trap`, `kill` (coreutils `kill`
exists as a binary), `umask`, `local`, `type`, `history` (line history lives in the console handler), `getopts`.
`which` says only "is a command" and does not print the path (ledger follow-up, unix-userland.md).

### 1.3 On the rig only (build/rig/vtc, not in the kit)

| Binary | Version | Runtime | Path |
|---|---|---|---|
| less, lesskey | 321 (GG, 1998) | ixemul | vtc/pkgs/bin |
| pdksh (ksh, sh) | 4.9 | ixemul | vtc/pkgs/shells/bin |
| tcsh | 6.12.00 | ixemul | vtc/pkgs/shells |
| nano | 1.2.5 (2006) | ixemul + ncurses 5.5 | vtc/pkgs/nano-1.2.5-bin-m68k/bin |
| ncurses 5.5-1 tools: clear, tput, tset, tic, infocmp, toe, tack, capconvert | 5.5 | ixemul | vtc/pkgs/ncurses-5.5-1-p-bin-m68k/usr/local/bin |
| BitchX | 1.1 | ixemul + ixnet | vtc/pkgs/BitchX |
| fileutils ls/dircolors (old kit) | 3.16 | ixemul | vtc/gg/bin |

The rig's sys.hdf DH0:C (the owner's 3.1 system, listed with xdftool) has native `lha`, `lzx`, `UnZip`/`Zip`,
`bzip2`, `unrar` and the xad/xfd tools, among about 190 commands. A typical user's machine has native LhA; the other tools vary.

### 1.4 Sibling ports (all bebbo gcc 6.5.0b `-mcrt=ixemul`, ixemul-vtcon, `libixcompat.a`)

| Repo | Upstream | State | Fork workaround |
|---|---|---|---|
| ~/Code/tmux-amiga | tmux 3.6a + libevent 2.1.12 + GG ncurses 5.5 | in the kit; tmux_rig 7/7 | `proc.c:364-412` re-exec self with `TMUX_AMIGA_SERVER=<fd>`; panes/jobs via `amiga/vspawn.c:18-79` |
| ~/Code/screen-amiga/src | screen 4.9.1 + GG ncurses 5.5 | in the kit; screen_rig green | `screen.c:364-400` re-exec with `SCREEN_AMIGA_MASTER=1`; vfork at window.c:1252 |
| ~/Code/neovim-amiga | Neovim 0.12.5 (and 0.4.4); libuv 1.52.1 with an AmigaOS backend, PUC Lua 5.1, unibilium | rig 9/9 (handoff 2026-10-03); **not in the kit**; binary 7.2 MB, dist with runtime 33 MB, 9.7 MB RAM, TUI ready in 22 s | libuv `process.c:505-535` vfork spawn, the child's exec errno returned through a shared variable |
| ~/Code/ixemul-vtcon | ixemul 48.2 + 27 commits (termios, PTY:, job control, SCM_RIGHTS, socketpair, FIONREAD, malloc cache, pipe fstat) | ships in the kit | vfork only; no fork() |
| ~/Code/cpython-amiga | CPython 3.14 (PY1) | in progress, another agent (ledger:674) | - |
| feature/highlight-markdown (worktree `.claude/worktrees/agent-aa881edd74b6d33e4`, uncommitted) | own code: `hl` (bat-like, ~20 languages) and `mdv` (glow-like Markdown) | being built; vbcc native; not in the kit (I1 open) | - |

Toolchains:
- **bebbo m68k-amigaos-gcc 6.5.0b**, 20260819, at `~/opt/amiga/bin`. It is **not on PATH**; the makefiles use the full path.
  - Libraries present: libnix and the ixemul SDK.
  - Libraries absent: **zlib, ncurses, OpenSSL and clib2** (no `libz*`/`libncurses*` under ~/opt/amiga; `-mcrt=clib2` has no ncrt0.o, per research/2026-10-04_python-on-amigaos.md:47).
- **vbcc 0.9hp3** (Homebrew, `vc`).
- **ixemul itself**: gcc 2.95.3 in a Docker image (`ixemul-gcc295`).

## 2. Status words

| Word | Meaning |
|---|---|
| **KIT** | Shipped by UP-Term today |
| **PORTED** | Built in this project or a sibling and run on the rig, but not in the kit |
| **GG** | A Geek Gadgets / ADE ixemul binary exists. Package name, version and year are given. "ABI ok" means it runs on ixemul-vtcon, which is true of every GG binary measured so far. A tool that was never run is marked so |
| **NATIVE** | A current native AmigaOS build exists (libnix/own runtime). It takes AmigaOS paths only |
| **AMIGA-EQ** | An AmigaOS command does the job well enough for daily use |
| **MISSING** | Nothing usable on 68k AmigaOS 3.x |

Effort in the plan: S = under a session, M = 1-3 sessions, L = more.

## 3. Inventory

### 3.1 Shells

| Tool | Status | Evidence | Note |
|---|---|---|---|
| sh (POSIX) | **KIT** (vsh) | shell/sh_exec.c:1213 | Missing exec/eval/trap/umask/local/getopts/type (1.2) |
| bash | MISSING | dev/gg/bash-src.lha 2.01, source only (1998); no 68k binary in dev/gg, the GG 980523 mirror or Aminet search | Subshells, `$(...)` and pipelines of builtins fork without exec. A vfork-only port means rewriting bash's job layer: L/XL |
| zsh | MISSING | Aminet "zsh" finds only ZShell (an AmigaDOS shell, 1996) | Same fork problem as bash |
| ksh | GG, rig | dev/gg/pdksh-bin.lha 4.9 (1998); runs on the rig | Old; no Unix user asks for it |
| tcsh | GG, rig | dev/gg/tcsh-6.12.00-b.lha (2004); job control on the rig (ledger P6) | `&` fails ("No more processes": needs fork) |

### 3.2 coreutils (the list given)

ls with colours, cp, mv, rm, cat, head, tail, wc, sort, uniq, cut, tr, tee, env, date, du, touch, chmod, ln, mkdir,
basename, dirname, seq, yes, sleep, printf, test: all **KIT** (coreutils 5.2.1-9, GG/Aminet
`dev/gg/coreutils-bin-src.lha`, 2010). Rig: all 86 binaries answer `--version`; `wc -c` on a pipe was fixed in ixemul
(unix-userland.md:141-160).

| Gap inside coreutils | Status | Note |
|---|---|---|
| df | MISSING from 5.2.1's build | Not in dist/gg/coreutils-5.2.1/bin. AMIGA-EQ: `Info` |
| xargs | not coreutils, see findutils | |
| UTF-8 awareness (`wc -m`, `cut -c`, `sort` collation) | partial | 5.2.1 predates good multibyte support, and ixemul 48.x libc has no `mbrtowc` (python research:51) |
| timeout, realpath, truncate, numfmt, b2sum, sha256sum, `ls --group-directories-first` | MISSING | Added after 5.2.1 (coreutils 6.x-9.x); `sha256sum` matters for checking downloads |

### 3.3 findutils, search, text

| Tool | Status | Best existing build (Aminet path, version, year, runtime) | Note |
|---|---|---|---|
| find, xargs | GG | dev/gg/findutils-bin.lha 4.1 (1998), ixemul; not run on the rig | 4.1 lacks `-iname`/`-print0`/`-delete` (unverified for each); xargs needs vfork+exec. AMIGA-EQ for find: none good (`List ALL PAT`) |
| grep | GG / NATIVE | GG mirror grep-2.1-bin.tgz (1998); text/misc/grep-2.5-bin-m68k.lha GNU grep 2.5 (2006, gcc 3.4, runtime unverified); util/cli/grep-1.68.lha OpenBSD grep (2026-03-22, amiport, libnix, Amiga paths, no `--color`) | **Not in dev/gg**. AMIGA-EQ: `Search` (no regex) |
| ripgrep | MISSING | No Rust target for m68k-amigaos | ugrep (C++11) is the realistic "fast grep"; no Amiga build found |
| sed | GG | dev/gg/sed-4.1.4-bin-m68k.lha (2006, 68020+); sed-bin 2.05 (1998); BusyBox sed (native) | |
| awk / gawk | GG | dev/gg/gawk-bin.lha 3.0.2 (1998), GG mirror gawk-3.0.3; dev/gg/mawk-1.3.3-src-bin-m68k.lha (2006); amiport awk 2024.12.25 (built and tested, not released) | |
| diff, cmp, diff3, sdiff | GG | dev/gg/diffutils-bin.lha 2.7 (1998) | No `--color`, no unified-diff defaults of later versions |
| patch | GG | dev/gg/patch-bin.lha 2.4 (1998), GG mirror patch-2.5 | |
| less | GG, rig | dev/gg/less-bin.lha 321 (1998) runs on the rig with TERM=vtcon (ledger V2, :1151) | **Not in the kit.** Old: no UTF-8, no `-R` colour fidelity of later versions (unverified), known CVEs in old less (unverified per version). amiport less 692 is console-shim (not ncurses), unreleased |
| more | GG | via less; AMIGA-EQ `More` (SYS:Utilities) | |
| file | GG | dev/gg/file-bin.lha 3.20 (1998) | magic db from 1998 |
| which | **KIT**, partial | vsh builtin `which` only says "is a command" | Must print the `$PATH` hit |

### 3.4 Archivers

| Tool | Status | Best existing build | Note |
|---|---|---|---|
| tar | GG | dev/gg/tar-bin.lha 1.12 (1998) | tar 1.12 has path-traversal bugs fixed later (unverified which CVEs apply). `-z` runs gzip via fork+exec |
| gzip | GG | dev/gg/gzip-bin.lha 1.2.4 (1998) | 1.2.4 has published CVEs (CVE-2005-1228/0988, CVE-2006-4334..4338; versions per NVD, not rechecked today) |
| bzip2 | NATIVE | util/arc/bzip2_68k.lha 1.0.6 (2012) | Often in C: already (the rig's has one) |
| xz | NATIVE (old) | util/arc/xz-utils.lha 5.0.3 (2011), runtime unverified | |
| zip / unzip | NATIVE | util/arc/UnZip-6.0.lha 6.0-31 (2026-09-06, libnix, all CVE patches, ZIP64, bzip2); dev/gg/zip-bin.lha 2.0.1 (1998) | UnZip is good. Zip is old |
| lha | AMIGA-EQ / NATIVE | util/arc/lha_68k.lha LhA 2.15 (shareware, usually installed); util/arc/olha-0.4.4-amiga.run (2026, GPL-2+, read/write); lhasa 0.3.1 (ISC, read only) | |
| lzx | AMIGA-EQ | LZX 1.21r, UnLZX2 (2000) | Amiga format; native tools are right |
| 7z, zstd | MISSING | none found | Lower priority |

### 3.5 Editors

| Tool | Status | Best existing build | Note |
|---|---|---|---|
| vim | NATIVE | text/edit/vim-9.1.lha (2026-03-27, 68020+, `-noixemul`, 2.2 MB binary). It has a builtin xterm termcap, but no UTF-8 and no `:terminal` (research 2026-09-29:57) | No ixemul/ncurses vim exists. Native vim does not use termios or SIGWINCH from PTY: (unverified inside tmux) |
| nvim | **PORTED** | ~/Code/neovim-amiga 0.12.5 (rig 9/9) | Not in the kit; 33 MB with runtime; needs a fast machine |
| nano | GG, rig | dev/gg/nano-1.2.5-bin-m68k.lha (2006, ncurses 5.3, TERM=amiga-f per readme; edits correctly on ixemul-vtcon, ledger :852) | Not in the kit. nano_234_68k (2014) is SDL/pdcurses, not a terminal program |
| micro | MISSING | Go; no m68k-amigaos Go target | |
| emacs-nox | GG | dev/gg/emacs-bin.lha 18.59 (1998) | Emacs 18. A modern emacs is impractical on 68k RAM |
| joe | GG | dev/gg/joe-3.1-bin.lha (2004, ixemul 47+) | Not run on the rig |
| mg | NATIVE (unreleased) | amiport mg 3.7, console-shim, "built and tested" (README; not on Aminet) | |
| jove / atto | GG | jove 4.16 (1998), dev/gg/atto.lha (2022) | |

### 3.6 Network

| Tool | Status | Best existing build | Note |
|---|---|---|---|
| ssh, scp | **KIT** | BebboSSH 1.45 (2026-04-14) | TERM and LANG need fixes (research 2026-10-03_ssh-for-up-term.md); a window resize reaching the remote is unverified |
| sftp (client) | unverified | bebbossh readme says "sftp"; bebbosshd has the subsystem | I did not find a client binary named sftp in dist/net/bebbossh-1.45 |
| curl | **KIT** | curl 8.22.0 (native, AmiSSL 5) | |
| wget | GG / NATIVE (old) | dev/gg/wget-1.11.4-bin.lha (2009); comm/www/wget-1.11.4-amigaos-ssl.lha needs ixemul 61.1 + OpenSSL 0.9.7e | curl covers it; a `wget` alias over curl is enough |
| rsync | MISSING | no Aminet hit | Needs a subprocess (ssh) with pipes |
| telnet | partial | vshrc wraps `command telnet`; no telnet in the kit; DCTelnet (GUI) on Aminet | A small CLI client is easy (own code or inetutils) |
| nc | MISSING | none on Aminet | Small (OpenBSD netcat) |
| ping | NATIVE | comm/tcp/AvePING.lha 1.1 (2026-03-30, 68000, bsdsocket) | |
| ftp | GG / KIT via curl | dev/gg/ncftp-bin.lha 2.4.2 (1998); curl does ftp/ftps | |

### 3.7 Development

| Tool | Status | Best existing build | Note |
|---|---|---|---|
| git | NATIVE | dev/misc/am-git.lha 0.12.1 (2026-08-03, AmLang, https via AmiSSL 4+, ssh via bebbossh) | Not the real git: its own implementation, "validated byte-for-byte against reference git output" per its readme. Licence unverified (readme does not state it) |
| make | GG | dev/gg/make-382-bin-m68k.lha GNU make 3.82 (2011); make-3.81-bin-m68k (2006) | |
| gcc / vbcc | NATIVE | vbcc is native on the Amiga (not checked on Aminet today); GG gcc 2.7.2.1 (1998) | Wrappers (`cc`, `c99`) are small; on-Amiga compiling is a niche on 68020 |
| perl | GG | dev/gg/perl-5005.lha 5.005_03 (2003); perl571.lha 5.7.1 (2002) | Old. A modern perl is L |
| python | in progress | PY1 CPython 3.14 (cpython-amiga); MicroPython 1.29 native; dev/gg/python2.4 alpha | research 2026-10-04_python-on-amigaos.md |
| lua | NATIVE | dev/lang/lua-5.4.7.lha (2026-03, amiport, no io.popen); amiworp-lua550-os3 5.5.0 (2026-08) | Amiga paths |
| ed | GG | dev/gg/ed-bin.lha 0.2 (1998) | |
| ctags | MISSING | none found | Exuberant ctags 5.8 is small C |

### 3.8 Monitors

| Tool | Status | Note |
|---|---|---|
| top / htop | MISSING as terminal programs | util/moni/atop.lha 0.9 (2026-09-18) opens **its own Intuition window**, not a terminal program; util/moni/top.lha (1996). htop needs /proc or a platform backend: none for AmigaOS |
| ps | MISSING | BusyBox omits it ("no fork, so no ps"). AMIGA-EQ: `Status` (CLI processes only) |
| kill | **KIT** (coreutils kill, ixkill) | AMIGA-EQ: `Break` |
| watch | MISSING | Needs a vfork+exec loop |

### 3.9 Multiplexers

tmux 3.6a and screen 4.9.1: **KIT** (ledger P7). Open item: tmux-amiga must be rebuilt for the Unicode 16 width table (ledger U1).

### 3.10 Text tools

| Tool | Status | Note |
|---|---|---|
| man | GG | dev/gg/manutils-bin.lha 2.1 + groff-bin 1.10 (1998). Ships **no pages** for the tools UP-Term carries |
| fzf | MISSING | Go. fzy (C, MIT) is the realistic fuzzy finder |
| jq | MISSING | C; no Amiga build found |
| bc / dc | GG | dev/gg/bc.lha GNU bc+dc (2015, "not SDK"; runtime unverified); bc-1.6 (2002) |
| column | MISSING | util-linux / BSD |
| fmt | **KIT** (coreutils) | |
| iconv | GG library only | dev/gg/libiconv-1.12-m68k-aos.lha (2009); CLI presence unverified |

### 3.11 Terminal utilities and misc

| Tool | Status | Note |
|---|---|---|
| clear | **KIT** (vshrc function) | GG ncurses `clear` on the rig |
| tput, reset, tset | rig (GG ncurses 5.5) | Not in the kit |
| stty | **KIT** (coreutils 5.2.1) | Whether it drives the patched termios fully is **unverified** (P6 tested tcgetattr/tcsetattr through probes, not stty) |
| script | MISSING | Needs a PTY + vfork. Both exist (P5/P6) |
| tree | MISSING for 3.x | util/dir/tree-mos.lha is MorphOS; DirTree-cmd (2021, native) |
| ncdu | MISSING | C + ncurses |
| bat-like | in progress | `hl` on feature/highlight-markdown |
| Markdown viewer | in progress | `mdv`, same branch |
| tldr | MISSING | Needs network + pages; could ride on `mdv` + curl |
| BusyBox (69 applets) | NATIVE | util/cli/busybox.lha 1.37.0 (2026-09-16), libnix, Amiga paths, no sh/ps/top, xargs cannot exec. A useful reference for AmigaOS glue, but not the userland base (paths, flags) |

## 4. Gaps ranked by when a user hits them

Rank = how soon a Unix user hits the gap in a first session × how often they hit it afterwards.

| Rank | Gap | Why it hurts | Best route (plan has details) |
|---|---|---|---|
| 1 | **grep** (with `--color`, `-r`, `-E`) | The first pipe anyone types | GNU grep 3.11, ixemul |
| 2 | **less** (UTF-8, `-R`) | `man`, `git log`, `--help | less`; tmux copy aside, nothing pages | less 668, ncurses |
| 3 | **an editor** (nano now, vim next) | Editing any config file | nano 8.x on ncurses 6; vim 9.1 ixemul; nvim as an option |
| 4 | **sed, awk** | Every script and one-liner | GNU sed 4.9, onetrue-awk or gawk |
| 5 | **tar + gzip/bzip2/xz** | Every source download | libarchive bsdtar + zlib/bzip2/xz; gzip, bzip2, xz CLIs |
| 6 | **find + xargs** | Finding files | findutils 4.10 + posix_spawn over vfork |
| 7 | **man** + pages | "how do I ..." | mandoc + pages for every shipped tool |
| 8 | **vsh builtins**: eval, exec, trap, local, umask, getopts, type; `which` printing the path | Scripts from the Internet break on them | vsh (own code) |
| 9 | **diff / patch** | Changing configs, applying fixes | diffutils 3.10, patch 2.7.6 |
| 10 | **git** | Cloning anything | am-git now (licence check); real git later (L) |
| 11 | **file**, **tree**, **ps/top** | Orientation | file 5.x (trimmed magic), tree 2.x, own ps/top |
| 12 | **zip / unzip** | Downloads from Windows/macOS | Info-ZIP UnZip 6.0-31 native exists; bsdtar reads zip |
| 13 | **tput / reset / stty proof / clear** | A broken terminal after a crash | ncurses 6 progs |
| 14 | **ixnet.library in the kit** | Without it, no ixemul network tool runs | ship it (prerequisite) |
| 15 | **sha256sum, timeout, realpath** (newer coreutils) | Verifying downloads | coreutils 9.x |
| later | make, jq, bc, column, iconv, fzy, ncdu, watch, script, nc, telnet CLI, ctags, ed, tldr | Daily for some users, not in the first hour | plan phase 4-5 |
| defer | bash, zsh, emacs, perl, rsync, real git, ripgrep/fzf/micro (Go/Rust) | L/XL, or no compiler target | owner decisions D3-D5 in the plan |

## 5. Facts the plan depends on

1. **ixemul has vfork, not fork.** The proven idiom (tmux `amiga/vspawn.c`) exists as copy-pasted code in each port.
   GNU tools reach for `fork` (findutils, diffutils' diff3/sdiff, tar `-z`, make), and many gnulib-era tools prefer
   `posix_spawn` when the libc has it. **ixemul 48.2 has no posix_spawn** (not in the SDK headers; I checked no symbol list
   for it explicitly, so this is unverified).
2. **No wide-character layer.** The SDK libc.a has no `mbrtowc`, `nl_langinfo`, `getaddrinfo`, `clock_gettime` or `poll`
   (python research:51; libixcompat has poll, realpath, langinfo, time_r, if_nametoindex, libgen and selfpath:
   `ixemul-vtcon/compat/`). UTF-8 in grep/less/nano/ncursesw needs mbrtowc/wcwidth. tmux already takes widths from
   `engine/vtwidth.h` (`tmux-amiga/amiga/utf8proc.c`).
3. **ncurses 6 is not built.** Every curses port links GG ncurses 5.5's `libncurses.a`, taken from a rig drawer
   (`build/rig/vtc/pkgs/ncurses-5.5-1-p-bin-m68k`). Ledger N1 asks for 6.x with widec.
4. **Paths**: ixemul accepts `/SYS/x` and `SYS:x` (since the `cd /` fix, unix-userland.md). libnix tools accept only `SYS:x`.
5. **`/tmp`** is `TMP:`, which Install assigns to T: (unix-userland.md).
6. **`/bin/sh`** is `/gg/bin/sh` = vsh (ledger :1018). `system()`/`popen()` in every port run vsh.
7. **Stack**: vsh honours `$STACK:` cookies (ledger P8). GNU tools with deep recursion (grep's regex, find, gawk) need one.
8. **Licences**: the GG coreutils precedent ships COPYING plus the complete source in the kit (install.dos:99-101).
   BebboSSH and curl do the same (`dist/net/*/SOURCE.txt`).

## 6. Where I looked

- vtcon: RULES.md; thoughts/shared/plans/2026-09-28-vtcon.md (P5 :877, P6 :838-911, P7 :916-1045, N1 :362, U1-U3
  :1045-1058, Q1 :1063); plans/2026-10-02_unix-userland.md; research/2026-09-29_68k-unix-ports-for-testing.md,
  2026-09-29_unix-tty-layer-and-shell.md, 2026-10-03_ssh-for-up-term.md, 2026-10-04_python-on-amigaos.md; Makefile;
  dist/install.dos, dist/Install.installer, dist/vshrc; shell/sh_exec.c; build/rig (listed); build/dist.
- Siblings: tmux-amiga, screen-amiga/src, neovim-amiga, ixemul-vtcon, ~/opt/amiga.
- Web, 2026-10-04:
  - aminet.net/dev/gg pages 1-10 (411 rows);
  - Aminet searches: grep, bzip2, xz, lha, lzx, curl, git, rsync, lua, jq, htop, which, zsh, bash, ripgrep, wget, tar, gzip, unzip, less, nano, vim, micro, tree, ncdu, top, file, busybox, telnet, ping, ssh, perl, python;
  - readmes: busybox, curl-8.22.0, curl-bebboget, grep-1.68, grep-2.5-bin-m68k, am-git, UnZip-6.0, lua-5.4.7, amiworp-lua550-os3, atop, xz-utils, bzip2_68k, nano_234_68k, AvePING, wget-1.11.4-amigaos-ssl, lhasa-0.3.1, olha-0.4.4-amiga, lha_68k;
  - codewiz.org/pub/systemshock/fileareas/GeekGadgets/ (top-level bin list);
  - github.com/bdgscotland/amiport (README summary).
