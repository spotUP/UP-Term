---
date: 2026-09-29
topic: 68k Unix-style full-screen programs to exercise the xterm-compatible console
tags: [research, aminet, geek-gadgets, ixemul, ncurses, termcap, test-corpus, irc, screen]
status: final
---

# 68k Unix-style ports for testing the console

Web-only survey (Aminet + Geek Gadgets mirrors + upstream sources). Nothing was
downloaded or installed. Sizes are Aminet's packed size unless noted.
"Unverified" means that the readme or listing did not say, so the value is inferred or unknown.

## Bottom line

- **GNU screen: there is no 68k port.** It is not in Aminet `dev/gg` (all 9 pages were
  listed), not in the GG 980523 binary mirror, and a web search found nothing.
  The cause is structural: the ixemul `__open.c` maps only `/dev/tty` and `/dev/null`
  and has **no pty support** (checked in the ixemul 48.0 source mirror; later
  62.x/63.x releases were not checked). The same applies to **tmux**, which has 0 Aminet hits.
- **irssi: there is no 68k build.** The two Aminet builds (1.1.1, 1.2.2) are
  **ppc-morphos only**. The closest terminal IRC clients on 68k are **BitchX 1.1 final**,
  **BitchX 1.0c18**, **EPIC4 1.1.7**, and **ircII 2.8.2 (AmIRCii)**, all ixemul-era.
- **bash: only source is available** (`dev/gg/bash-src.lha`, 2.01, a tarball). No 68k bash
  binary was found. The Unix shells that do have 68k binaries are **pdksh 4.9** and **tcsh 6.12**.
- **vim: no ixemul/termcap build was found.** The modern native build (vim 9.1,
  `-noixemul`) contains a *builtin* `xterm` termcap table, so `vim -T xterm` is the
  way to exercise xterm sequences with it (see the notes below).
- The best curses corpus is **ncurses 5.5-1 (68020+, ixemul 48+)**. It ships the
  classic test programs (`ncurses`, `worm`, `firework`, `rain`, `tclock`, `hanoi`,
  `lrtest`, `bs`, `knight`, `demo_forms/menus/panels`) plus `tic`, `infocmp`, `tput`, `toe` and `tack`.

## Table

DL = `https://aminet.net/<path>` (the `.readme` is next to it).

| Program | Aminet path (.lha) | Version | CPU | Runtime | Terminal / TERM | Size | Known problems / notes |
|---|---|---|---|---|---|---|---|
| **GNU screen** | none | - | - | - | - | - | **No 68k port.** ixemul has no pty. |
| **tmux** | none | - | - | - | - | - | **No port on any Amiga arch** (0 Aminet hits). |
| **irssi** | `comm/irc/irssi.lha`, `comm/irc/irssi-1.1.1.lha` | 1.2.2 / 1.1.1 | **PPC only** | MorphOS | - | 5.2M / 2.3M | **No 68k build.** |
| BitchX | `comm/irc/BitchX-11.lha` | 1.1 final (2012) | 68k, CPU unverified | ixemul + ixnet 48+ | curses/termcap not stated (unverified); RunBitchX script | 3.8M (~1.3M binary) | Launch via `GG/local/bin/RunBitchX` |
| BitchX | `comm/irc/BitchX-BIN.lha` | 1.0c18 (2001) | 68k | ixemul + ixnet 48+ | "console only"; needs its fonts copied to FONTS:; started from a shell (`SShell C 8` in readme) | 1.1M | Built without TCL |
| EPIC4 | `comm/irc/epic4.lha` | 1.1.7 (2002) | 68k, CPU unverified | ixemul presumed (**unverified**, readme silent) | termcap/terminfo unverified | 978K (binary 1.09 MB) | readme gives only headers |
| ircII | `comm/irc/AmIRCii-2.8.2.lha` | 2.8.2 (1996) | 68k | **unverified** (listing does not mention ixemul) | termcap unverified | 471K | binary `bin/IRC-II/bin/irc-2.8.2` |
| (GUI, not for this test) | `comm/irc/apartee-0.8.1.lha` | 0.8.1 (2026-09-07) | 68020 | native, bsdsocket v4 | **Intuition GUI, not a console program** | 72K | listed only so nobody tries it as a terminal client |
| bash | `dev/gg/bash-src.lha` | 2.01 | - | (ixemul if built) | - | 1.4M | **Source only**; no 68k binary found |
| pdksh | `dev/gg/pdksh-bin.lha` | 4.9 | 68k | ixemul | line editing via termcap presumed (unverified) | 122K | usable Bourne-ish shell substitute |
| tcsh | `dev/gg/tcsh-6.12.00-b.lha` | 6.12.00 (2004) | 68k, CPU unverified | ixemul (unverified) | termcap (unverified) | 151K | executable only |
| vim (native) | `text/edit/vim-9.1.lha` | 9.1 (2026-03-27) | **68020+**, OS 3.0+ | **native** (bebbo gcc `-noixemul`) | default `amiga`; builtin `xterm`, `ansi`, `vt320` tables compiled in | 9.4M (2.2 MB binary; archive includes source) | no UTF-8, no pty/`:terminal`, no clipboard |
| vim (native, old) | `text/edit/vim60bin.lha` (+`vim60rt.lha`) | 6.0 (2002) | 68k | native | builtin `amiga` | 368K | whether old builds include builtin xterm is unverified |
| vim (native, old) | `text/edit/vim53bin.lha`, `vim53big.lha` (+`vim53rt.lha`) | 5.3 (1998) | 68k | native | builtin `amiga` | 254K / 267K | same caveat as 6.0 |
| vim (ixemul/termcap) | none found | - | - | - | - | - | **No ixemul build on Aminet/dev/gg** |
| less | `dev/gg/less-bin.lha` | 321 (1998, ADE) | 68k | ixemul | termcap (readme: "Less uses termcap") | 126K (inner `less-321-bin.tar`) | old; `less-mos.lha` 704 is MorphOS only |
| most | `dev/gg/most-490.lha` | 4.9.0 (2000) | 68k | unverified (probably slang/ixemul) | unverified | 188K | alternative pager |
| mc | `dev/gg/mc-4.6.0-bin.lha` | 4.6.0 (2004, Pavel Fedin) | 68k | ixemul 47+ | readme: "OS console, xterm, and other terminal emulators" | 2.7M | ncurses or slang unverified |
| mc | `dev/gg/mc-4.1.40-pre8-bin-m68k.lha` | 4.1.40-pre8 (2007) | **68020+**, OS 2.04+ | ixemul 48+ + `libncurses.ixlibrary` 5.5 | **TERM=amiga-f** ("the only good terminfo") | 854K | no VFS (fork), find recursion broken, no panel toggle, no GPM mouse |
| mc (slang) | `dev/gg/mc-4.1.40-pre8-s1-bin.lha` | 4.1.40-pre8 slang | 68020+ (same porter) | ixemul, slang | unverified | 519K | slang build of the above |
| ncurses runtime | `dev/gg/ncurses-5.5-1-usr-m68k.lha` | 5.5-1 (2009) | **68020+** | ixemul 48+, shared `libncurses.ixlibrary` | terminfo at `usr/local/share/terminfo`: amiga, amiga-f, amiga-8bit, ansi, linux, vt100/200/300 ... | 155K | remove any older ixlibrary before installing; ID changed 2008-12-28 for ixemul 48.3 |
| **ncurses test progs** | `dev/gg/ncurses-5.5-1-bin-m68k.lha` | 5.5-1 | 68020 / 68040 builds | ixemul 48+ | includes xterm-16color and xterm-88color entries; terminfo was **reduced** (no symlinks on AmigaOS) | 3.9M | demos in `demos/` use the shared lib; plus tic, infocmp, tput, clear, toe, tack |
| ncurses (old) | `dev/gg/ncurses-5.3.lha`; `ncurses-bin.lha` / `ncurses-term-b.lha` 1.9.9e | 5.3 / 1.9.9e | 68k | ixemul (unverified for 5.3) | full terminfo.src (5.3) | 6.0M / 501K+259K | 5.3 is mostly an SDK |
| Amiga terminfo pack | `dev/gg/custom-ti.lha` | 0.0.1 (2007) | - | - | adds `amiga-fb`, `iris-color`, `iris-mono`, `/etc/termcap`, keymaps `mcsl-amiga`, `mcsl-vt100` | 223K | readme: `set TERM amiga-fb` |
| termcap (GG) | GG mirror `termcap-1.3-bin.tgz` (codewiz mirror below) | 1.3 | 68k | ixemul | `/etc/termcap` | 270K | .tgz, not .lha |
| nano | `dev/gg/nano-1.2.5-bin-m68k.lha` | 1.2.5 (2006) | **68020+**, OS 2.04+ | ixemul 48.2+, ncurses 5.3 | **TERM=amiga-f**; readme says plain `amiga` misbehaves; terminfo included | 378K | mixed -O2/-O3 build (assembler bug) |
| joe | `dev/gg/joe-3.1-bin.lha` | 3.1 (2004, Pavel Fedin) | 68k | ixemul 47+ | termcap/terminfo not stated | 615K | includes jmacs, jstar, rjoe |
| jove | `dev/gg/jove-bin.lha` | 4.16 (1998) | 68k | ixemul | termcap (unverified) | 226K | small Emacs-like editor |
| emacs | `dev/gg/emacs-bin.lha` | 18.59 | 68k | ixemul | termcap | 2.2M | heavy but classic termcap client |
| lynx | `comm/www/lynx-2.8.7-bin-m68k.lha` | 2.8.7dev1 (2007) | **68020+**, OS 2.04+, 2 MB | ixemul 4.8+ / ixnet 4.8+ / AmiTCP 3.0b2+ | **slang** (not ncurses); **TERM=amiga**; set LINES/COLUMNS for 80x25 | 1.4M | no SSL; ~35 KB stack; prefers Unix paths |
| lynx | `comm/www/lynx.lha` | 2.8.5dev3 (2001, Fr3dY) | 68k | ixemul (bundles sh, cp, gzip...) | Amiga terminfo files included | 966K | |
| lynx | `dev/gg/lynx-bin.lha` | 2.6 (1998, ADE) | 68k | ixemul | curses/termcap | 352K | |
| links | `comm/www/Links2.12.lha` | 2.12 (2016, Tygre) | 68k, OS 2.04+ | **unverified** (readme silent) | text mode only; arrows patched in kbd.c | 6.8M (Aminet list) / ~7.2M (page) | `links -g` unsupported |
| top-like | `util/moni/top.lha` | 1996 | 68k | native (SAS/C source) | uses the Amiga console, not curses (unverified) | 24K | no Unix-style top/htop port exists |
| aview | `dev/gg/aview-1.2-bin-m68k.lha` | 1.2 | 68k | ixemul (aalib) | aalib text output | 648K | bonus: full-screen ASCII art |
| xaos (console) | `dev/gg/xaos-3.0-bin-m68k.lha` | 3.0 (2007) | 68k | ixemul (aalib) | full-screen text fractal zoomer | 400K | bonus |
| ncurses games | e.g. `game/actio` and `game/shoot` entries from 2012 (Aminet search "ncurses", m68k rows) | various | 68k | ixemul + ncurses (presumed) | - | 150K-1.6M | good extra curses load; one per 2012 uploader batch |

## Notes that matter for the console

1. **ixemul asks the console for its size with a native Amiga sequence, not xterm's.**
   In the ixemul 48.0 source mirror, `library/__tioctl.c`, `TIOCGWINSZ` writes
   `9B 20 71` (CSI SPACE q, the *Window Status Request*). It then reads the reply and
   parses `CSI 1;1;<rows>;<cols> r` (8-bit CSI 0x9B). If there is no reply, it falls back to 80x24.
   **Every ixemul curses program (mc, nano, lynx, BitchX, less ...) gets its size this
   way.** The console must answer `0x9B "1;1;R;C r"`, and must do so in the raw-mode
   path (`SetMode(fh,1)`). `TIOCSWINSZ` is a no-op.
2. **Vim's native build takes its size from the ConUnit struct, not from an escape sequence.** In vim `os_amiga.c`, when
   `TERM` is the default `amiga`, `mch_get_shellsize()` sends `ACTION_DISK_INFO` to the
   console handler. It then reads `id_VolumeNode` as a `struct Window*` and `id_InUse` as
   an `IOStdReq*` whose `io_Unit` is a `ConUnit`, and uses `cu_XMax+1` and `cu_YMax+1`.
   A console handler that does not fill in those fields makes vim treat the window as
   "not an amiga window". With `TERM=xterm` (or `vim -T xterm`), `term_console` is
   false and vim uses its builtin xterm table. No size query is made, so the size comes
   from defaults or `LINES`/`COLUMNS` (**unverified** which one wins on Amiga). The AROS/MorphOS branch
   of the same function uses the `CSI 0 q` / `CSI 1;1;R;C r` exchange instead.
3. **TERM values these ports expect:** `amiga` (ncurses readme: mono, non-standard
   cursor keys), `amiga-f` (ANSI, recommended by the mc and nano porters), and `amiga-fb`
   (custom-ti). The 5.5-1 terminfo was cut down, so a full `xterm` entry is **not**
   guaranteed in the Amiga tree. Only `xterm-16color` and `xterm-88color` are listed. A
   proper `xterm` or `vt100` entry may need compiling with the shipped `tic`.
4. **No ptys means no multiplexers or terminal-in-terminal programs.** screen, tmux,
   dtach, `script`, and vim `:terminal` all depend on ptys. Testing them would first need a
   pty implementation in ixemul, or a pty-capable replacement runtime.
5. Most of the Aminet readmes are thin. Rows marked "unverified" should be read from the archive
   itself after download.

## Other archives

- The **Geek Gadgets 980523 m68k binary set** (codewiz mirror) has `pdksh-4.9`,
  `termcap-1.3`, `ncurses-980117` (+dev, +term), `lynx-2.7.2`, `emacs-18.59`, and
  `ixemul-47.3`. It has **no** bash, screen, less, or vim binaries.
- The **GG 990529 snapshot** is on archive.org inside one 2.3 GB `geekgadgets.7z`. The
  online viewer lists only directories, so its bin/ contents are **unverified**.
  The back2roots and exotica FTP mirrors were not reachable from here.
- **amiport** (GitHub) reports vim 9.1, less 692, and mg 3.7 built against its own
  `console-shim` (ncurses API via console.device). Its README says these are "local builds"
  and that only small CLI tools are on Aminet, so there are no usable binaries yet (**unverified** beyond the README).

## URLs opened

- https://aminet.net/search?query=screen
- https://aminet.net/search?query=irc
- https://aminet.net/search?query=irssi
- https://aminet.net/search?query=tmux
- https://aminet.net/search?query=bash
- https://aminet.net/search?query=vim
- https://aminet.net/search?query=ncurses
- https://aminet.net/search?query=less
- https://aminet.net/search?query=lynx
- https://aminet.net/search?query=links
- https://aminet.net/search?query=abc-shell
- https://aminet.net/search?query=zsh
- https://aminet.net/search?query=shell&arch[]=m68k-amigaos
- https://aminet.net/search?query=top&arch[]=m68k-amigaos&type=all
- https://aminet.net/dev/gg and ?page=2 ... ?page=9 (full listing)
- https://aminet.net/comm/irc and ?page=2 ... ?page=5
- https://aminet.net/package/dev/gg/mc-4.1.40-pre8-bin-m68k
- https://aminet.net/package/dev/gg/mc-4.6.0-bin , https://aminet.net/dev/gg/mc-4.6.0-bin.readme
- https://aminet.net/package/dev/gg/nano-1.2.5-bin-m68k
- https://aminet.net/package/dev/gg/joe-3.1-bin , https://aminet.net/dev/gg/joe-3.1-bin.readme
- https://aminet.net/package/dev/gg/ncurses-5.5-1-usr-m68k
- https://aminet.net/package/dev/gg/ncurses-5.5-1-bin-m68k , https://aminet.net/dev/gg/ncurses-5.5-1-bin-m68k.readme
- https://aminet.net/package/dev/gg/ncurses-5.3
- https://aminet.net/package/dev/gg/custom-ti
- https://aminet.net/package/dev/gg/less-bin , https://aminet.net/dev/gg/less-bin.readme
- https://aminet.net/package/dev/gg/bash-src , https://aminet.net/dev/gg/bash-src.readme
- https://aminet.net/package/dev/gg/lynx-bin
- https://aminet.net/package/comm/www/lynx-2.8.7-bin-m68k
- https://aminet.net/package/comm/www/lynx
- https://aminet.net/package/comm/www/Links2.12 , https://aminet.net/comm/www/Links2.12.readme
- https://aminet.net/package/text/edit/vim-9.1 , https://aminet.net/text/edit/vim-9.1.readme
- https://aminet.net/package/text/edit/vim60bin
- https://aminet.net/package/comm/irc/BitchX-11 , https://aminet.net/comm/irc/BitchX-11.readme
- https://aminet.net/package/comm/irc/BitchX-BIN , https://aminet.net/comm/irc/BitchX-BIN.readme
- https://aminet.net/package/comm/irc/epic4 , https://aminet.net/comm/irc/epic4.readme
- https://aminet.net/package/comm/irc/AmIRCii-2.8.2 , https://aminet.net/comm/irc/AmIRCii-2.8.2.readme
- https://aminet.net/package/comm/irc/apartee-0.8.1
- https://aminet.net/package/util/moni/top
- https://codewiz.org/pub/systemshock/fileareas/GeekGadgets/
- https://archive.org/download/geekgadgets-mirror (+ view_archive listing of geekgadgets.7z)
- https://www.ranger.innolan.net/github/amiga-ixemul/src/branch/master/library
- https://www.ranger.innolan.net/github/amiga-ixemul/raw/branch/master/library/__tioctl.c
- https://www.ranger.innolan.net/github/amiga-ixemul/raw/branch/master/library/__open.c
- https://raw.githubusercontent.com/vim/vim/master/src/term.c
- https://raw.githubusercontent.com/vim/vim/master/src/os_amiga.c
- https://raw.githubusercontent.com/vim/vim/master/src/feature.h
- https://github.com/bdgscotland/amiport
- https://github.com/adtools/amigaos-cross-toolchain/blob/master/toolchain-m68k (GG mirror host only)
