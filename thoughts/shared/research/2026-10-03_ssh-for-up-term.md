---
date: 2026-10-03
topic: SSH on the Amiga for UP-Term -- BebboSSH, ssh2fs, openssh-bin (owner's Aminet links)
tags: [vtcon, ssh, network, a1]
status: final
---

# SSH for UP-Term

Owner 2026-10-03: "these should help connecting to the amiga and connect to unix terminals from
the amiga? can we bundle them with the terminal? or is there a better solution?" Links:
comm/net/bebbossh, disk/misc/ssh2fs.m68k-amigaos, comm/tcp/openssh-bin, franke.ms wiki.

## The three packages

- **openssh-bin** (OpenSSH 4.5p1): `Architecture: ppc-amigaos` -- AmigaOS 4 only. Not usable.
- **BebboSSH 1.45** (Stefan Franke; source franke.ms/git/bebbo/bebbossh): client `bebbossh`,
  server `bebbosshd`, `bebboscp`, `bebbosshkeygen`, `libcryptossh.library` (68000 / 020 / 060
  builds). Needs bsdsocket.library only (Roadshow, AmiTCP; FS-UAE emulates it). Ciphers
  curve25519, ed25519, aes128-gcm, chacha20-poly1305 (a modern OpenSSH server accepts them).
  Licence: GPLv3+, a few files public domain.
- **ssh2fs 53.1** (Fredrik Wikstrom): an SFTP file system (`L:ssh2-handler`, a DOSDriver),
  libssh2 1.10. Needs filesysbox.library 54.3+, z.library 2.1+, AmiSSL 5.1+, a TCP stack;
  optional ReqTools. 3.0+.

## Measured on the rig (3.1, FS-UAE, 68020, UP-Term handler fa53884+)

A throwaway sshd on the Mac (127.0.0.1:2222, key login, files in the session scratchpad,
stopped after):
- `bebbossh -i KEY -p 2222 spot@127.0.0.1 uname -sm` -> `Darwin arm64` in 3.6 s, connection
  included. The first connection asks to trust the host key (answer "yes").
- Interactive, in an XCON: window: colours, UTF-8 (cafe with e-acute), box drawing, the remote
  window title (OSC 0) all right; `stty size` = the window's 35 x 84.
- **TERM**: bebbossh sends `xterm-amiga` unless the local variable TERM is set.
  `SetEnv TERM` (global) is not read; `Set TERM xterm-256color` (local) is. With xterm-amiga the
  Mac has no terminfo entry, zsh cannot move the cursor left and redraws every keystroke after
  the last ("eecchecho"; the owner's screenshot). With xterm-256color: clean.
- **LANG**: on the Amiga bebbossh always sends `LANG=C` (ENV_LANG_C in bebbossh.cpp); the Mac's
  sshd does not accept env, so the remote LANG stayed unset. UP-Term shows UTF-8, so a
  remote UTF-8 locale is right for it.
- Not yet checked: a window resize reaching the remote (bebbossh asks the console for raw
  events with `CSI 2;11;12 {` and sends window-change on event 12 -- an Amiga-personality
  request; UP-Term's xterm personality may not report it), Ctrl-C, the Amiga-key mouse.

## bebbosshd (logging in to the Amiga)

src/shellchannel.cpp: the server answers a session with its own mini shell -- a prompt, local
echo, completion, CRLF normalisation -- and commands run with the channel as their DOS
streams. No PTY, no termios: a full-screen program (vim, nvim, less through ixemul) gets no
terminal. `pty-req` dimensions are kept, `window-change` and `env` are ignored, `sftp` is a
subsystem (works).

## Decision (lead), pending the owner's bundling call

- Client: BebboSSH is the one. Bundle `bebbossh`, `bebboscp`, `bebbosshkeygen` and the 020
  library as an optional Installer component, with COPYING and the source URL (GPL beside our
  code: aggregation). UP-Term side: vshrc and the Installer set a local TERM for it; or
  upstream a change to bebbossh (GPL, its author is active): on an UP-Term console (the
  ACTION_VTCON_GWINSZ packet answers) send TERM from the variable or `xterm-256color`, LANG
  from the variable, and the window size and its changes through the vtcon packets. Prefer
  upstream; carry a patch only if it is not taken.
- Server: bebbosshd's shell is not a terminal. The UP-Term way is the serial login's: a
  session runs `upgetty` in a stdio mode -- the channel's streams bridged to a PTY: pair with
  vsh on the slave, as upgetty does for serial.device -- so ssh into the Amiga gets termios,
  vsh, vim, nvim. Needs: upgetty STDIO mode (ours), and bebbosshd running it as the login
  shell (a config option upstream, or exec "upgetty STDIO" from the client).
- ssh2fs: point to it in the README (it needs AmiSSL, filesysbox, z.library -- not ours to
  bundle).
