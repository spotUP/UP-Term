---
date: 2026-10-20
topic: Which terminal (TUI and essential CLI) apps to port next to UP-Term
status: draft
---

# TUI / CLI port candidates for UP-Term (revision 2)

Evidence key. "verified" = fetched this session from the cited source; "unverified" = from memory, not checked.
Limits of the verification: the GitHub API gives the language and the REPOSITORY size (includes history, so it is NOT source size) and last push date. Source line counts are unverified for every row; the sizes below are repo KB only where a URL is given. Version numbers are verified only where stated.

## Already shipped (do not list)
- upterm-ports pkgs (verified by ls): awk bc bzip2 column coreutils ctags diffutils ed file findutils fzy gawk git grep gzip iconv ixcompat jq joe less libarchive lua make mandoc mg nano nc ncdu ncurses opkg patch perl script sed tree upkg-ui vim watch xz zip zlib.
- Add-on packages (dist/packages.txt): claude, pi, nvim, python3, git, perl, vim.
- vtcon `amiga:` target (Makefile line 444, verified) builds: vsh, sz, rz (ZMODEM), uptelnet (telnet), upgetty, ps, top, hl, mdv, vtshow, winbox, sizewatch, upprefs, upicon, upupdate, UPTerm, UPDemo, ixkill, ixpipe-handler, pty-handler, Claude and test/probe tools. hl and mdv by name are a highlighter and a markdown viewer (I did not read their sources; treat as covering those two jobs).
- Sibling repos in ~/Code/UP-Term (verified by ls): amiga-pi, cpython-amiga, neovim-amiga, tmux-amiga, screen-amiga, ide-spike (a Scintilla based IDE spike, contents not read), ixemul-vtcon*, amiga-gcc*.
- Base kit (dist/LICENSES.txt, README.txt): curl 8.22, BebboSSH 1.46.1 (ssh, scp, sshd, keygen), bebboget, asynergyc.
- Removed from the first list because covered: telnet + lrzsz (uptelnet, sz, rz), a serial login getty (upgetty), a syntax-highlighting cat and a markdown viewer (hl, mdv). htop is kept only as a possible replacement for the existing `top`.

## Platform constraints
No real fork (vfork+exec only), slow 68k, 8-64 MB, console select() unverified in ixemul (test once with a small program before any event-loop app), sockets exist (ssh, curl, nc work). A port of anything glib2-based needs glib2 built first.

## Essential (top 10, ordered by value x low risk)
| # | Name | What | Language / deps / evidence | Difficulty and obstacle | Native alternative, still worth it |
|---|------|------|-----------------------------|-------------------------|-------------------------------------|
| 1 | dash | POSIX sh; runs configure/install scripts, nnn plugins | C, no deps; latest 0.5.13.5 (Wikipedia caption, search result) verified-version; language/size unverified (gondor.apana.org.au unreachable) | low-medium: pipeline subshells via vfork | vsh and AmigaShell exist; yes, as script engine |
| 2 | nnn | file manager with previews and plugins | C, "minimal library deps" (README); verified language, repo 7993 KB, pushed 2026-10-07, BSD-2 (api.github.com/repos/jarun/nnn); dep list not stated: unverified | low-medium: plugins need sh | none in terminal; yes |
| 3 | lynx | text web browser incl. gopher/ftp | C; latest stable 2.9.3, dev snapshots current (lynx.invisible-island.net) verified-version; deps (ncurses or slang, optional ssl) unverified | medium: sockets, console select, TLS via amissl | IBrowse/AWeb are GUI; yes for ssh/serial use |
| 4 | tig | git TUI (log, blame, diff, tree) | C; deps: git, ncursesw, iconv; optional readline, PCRE2 (raw INSTALL.adoc) verified; repo 7279 KB, pushed 2026-10-13 (api.github.com/repos/jonas/tig) | low-medium: runs git through popen (vfork ok) | none; yes, git is shipped |
| 5 | mutt 2.x | mail client | C; ncursesw runtime dep verified via distro packaging; latest unclear (2.2.14 Artix, 2.3.2 OpenCSW, mutt.org unreachable): unverified; size unverified | medium: SMTP/IMAP over TLS via amissl, no local MTA | YAM/Thor GUI; yes for ssh/serial |
| 6 | irssi (fallback bitchx) | IRC client | C; needs glib >= 2.32, optional OpenSSL, ncurses/terminfo (raw INSTALL) verified; repo 8767 KB, pushed 2026-09-23 (api.github.com/repos/irssi/irssi). bitchx: unverified | high: glib2 must be built first; bitchx medium | AmIRC GUI; marginal, but glib2 also unlocks mc |
| 7 | hexedit | hex editor | C, ncurses; repo 91 KB, pushed 2025-10-18 (api.github.com/repos/pixel/hexedit); deps unverified | low | GUI hex editors only; yes |
| 8 | NetHack 3.6.7 (tty/curses) | roguelike, also a terminal stress test | C; 3.6.7 is the stable line, 3.7 is not a release and moves level compiling to Lua (search result, github.com/nethack/nethack); repo 166 MB (history) | medium: save/lock files, memory; avoid 3.7 | Amiga NetHack exists; still worth it as tty in UP-Term |
| 9 | htop (Amiga backend) | task monitor, would REPLACE the shipped `top` | C, ncurses 6 wide; libnl, libsensors, libcap optional (htop README via Debian) verified; repo 6944 KB, pushed 2026-10-19 (api.github.com/repos/htop-dev/htop) | medium-high: no /proc, write a platform dir; only worth it if `top` is weak | `top`, `ps` shipped; low priority |
| 10 | picocom | serial terminal client (modems, null-modem) | C; repo 1325 KB, pushed 2023-10-27 (api.github.com/repos/npat-efault/picocom); deps unverified | medium: termios on serial.device through ixemul | uptelnet/upgetty cover network and login, not an outgoing serial session |

## Cool (next ones)
| Name | What | Language / evidence | Difficulty / obstacle | Note |
|------|------|---------------------|-----------------------|------|
| vifm | vi-key file manager | C, repo 53 MB (history), latest 0.15 (vifm.info) verified; deps unverified | low-medium | second file manager if nnn is disliked |
| mc | Midnight Commander | C; glib >= 2.32 plus S-Lang 2 or ncurses, gettext (source.midnight-commander.org INSTALL) verified; repo 95 MB (history) | high: glib2, subshell via fork+pty | only after glib2 |
| Brogue CE | roguelike | C; repo 8820 KB, pushed 2026-10-18 (api.github.com/repos/tmewett/BrogueCE); deps unverified | low-medium | large map redraws stress the terminal |
| Angband | roguelike | C; repo 126 MB (history), pushed 2026-10-20 (api.github.com/repos/angband/angband) | low-medium | has a curses frontend |
| ne | modeless menu editor | C; repo 2251 KB, pushed 2026-10-02 (api.github.com/repos/vigna/ne) | low | overlaps nano/joe/mg |
| vis | vi-like editor with structural regexps | C; repo 6861 KB (api.github.com/repos/martanne/vis); needs Lua (shipped) and lpeg, libtermkey: unverified | medium | borderline |
| nudoku | sudoku in ncurses | C; repo 338 KB (api.github.com/repos/jubalh/nudoku) | low | cheap colour demo |
| ninvaders, moon-buggy, nsnake, bastet | small curses games | unverified (language, deps, size) | low | cheap wins |
| zile | emacs-lite | C, ncurses; unverified | low | alternative to mg |
| wget (GNU) | downloader | C; unverified | low-medium | curl and bebboget cover most; script compatibility only |
| sacc | gopher client | C; unverified | low | retro |
| dialog | scripted TUI boxes | C, ncurses; unverified | low | installers and menus |
| info (texinfo) | GNU docs reader | C, ncurses; unverified | low-medium | |
| units | unit converter | C; unverified | low | |
| unrar | rar extractor | C++; unverified (C++ level not checked) | low | only if gcc 6.5 handles it |
| openmpt123 | tracker module player CLI | C++ library; unverified, needs an AHI backend | medium-high | HippoPlayer exists natively |
| colordiff | coloured diff | Perl script (perl shipped); unverified | low | |
| xmlstarlet | XML CLI | C, libxml2/libxslt; unverified | medium | |
| pkgconf | pkg-config | C; unverified | low | helps on-device configure |

## Skip
- btop: C++ with threads and /proc (language not re-verified this pass).
- w3m: C, repo 33 MB, last push 2024-08-19 (api.github.com/repos/tats/w3m) verified: stale; also needs Boehm GC (unverified); lynx first.
- htop-alternatives, bottom, zellij, helix, yazi, lazygit, k9s, fzf, bat, eza, fd, ripgrep, delta, starship, zoxide, gitui, broot, nushell: Rust or Go (from memory, not re-checked here).
- weechat (plugins, threads), rsync (fork/socketpair design), emacs (unexec, fork), cmus (audio libs, threads), mpg123/ffmpeg/mpv (no audio/video path), gdb/valgrind/strace (no ptrace), dropbear/openssh (BebboSSH ships), dtach/abduco/dvtm (tmux and screen exist), rtorrent (C++ with threads), elinks (stale, lynx first), neovim stacks beyond nvim: all unverified, judged from design.
- telnet, lrzsz, serial getty, syntax cat, markdown viewer: shipped (see top).

## Counts
Rows in the Essential and Cool tables: 28. Verified on language via the GitHub API: 15 (nnn, tig, irssi, htop, picocom, hexedit, NetHack, Brogue CE, vifm, mc, Angband, ne, vis, nudoku, plus w3m in Skip). Deps verified from a source: tig, irssi, mc, htop, mutt (ncurses only). Versions verified: dash 0.5.13.5, lynx 2.9.3, vifm 0.15, NetHack 3.6.7 stable. Source line counts: unverified for every row. Remaining rows are unverified.

## Notes
- Highest-leverage gates: dash (scripts), glib2 (irssi, mc), a console select() test in ixemul (lynx, mutt, htop, irssi).
- Compiler: gcc 6.5 and the 15 toolchain both exist (amiga-gcc, amiga-gcc15); C++17 apps are not assumed.
