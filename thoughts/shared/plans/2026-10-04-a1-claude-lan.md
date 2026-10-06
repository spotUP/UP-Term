---
date: 2026-10-04
topic: A1 Claude from the Amiga -- uptelnet (native telnet client), uptelnetd (LAN-only Mac end), Claude Code's screen in UP-Term
tags: [vtcon, network, telnet, claude, a1]
status: implemented
---

# A1: Claude from the Amiga (owner chose option A)

Claude Code stays on the owner's Mac; the A1200 (network card + Roadshow) is its terminal
over the LAN. Branch `feature/a1-claude-lan` (not pushed, not merged).

Done = uptelnet builds clean in `make amiga`, its protocol module is host-tested; the Mac
end runs from one documented command, refuses non-LAN binds and peers, needs a password
and rate-limits failures, with tests driving it through its real entry point; a captured
Claude Code session replays through the engine with the gaps fixed or listed; the owner
has exact rig and A1200 steps (A1.4 is the owner's).

## Checklist (13 of 14 done; A1.4 is the owner's)

- [x] A1.1a telnet protocol module, portable C89 (`net/tn.c`, `net/tn.h`) -- ffa32a4
- [x] A1.1b host tests (`tests/test_telnet.c`, 28 checks, `make test ONLY=telnet`) -- ffa32a4
- [x] A1.1c `net/uptelnet.c`: bsdsocket.library, async ACTION_WAIT_CHAR + WaitSelect, raw
      termios via ACTION_VTCON_TCSETA (SetMode 1 elsewhere), GWINSZ polled, Ctrl-] quits -- ffa32a4
- [x] A1.1d Makefile (`amiga`, `dist`), install.dos / Uninstall (C:uptelnet), vshrc `telnet` -- ffa32a4
- [x] A1.2a `tools/uptelnetd.py` (LAN bind, allowlist, password, lockout, pty+ctty) -- f0eb88d
- [x] A1.2b `tools/test_uptelnetd.py` (27 tests, 5 through main() in a subprocess), in `make test` -- f0eb88d
- [x] A1.2c interop: `build/tn_host` (net/tn.c on a POSIX socket) logs in to uptelnetd -- f0eb88d
- [x] A1.3a capture (`tools/capture_claude.py`, `build/vtreply` answering) -- 7b47548
- [x] A1.3b measured (below)
- [x] A1.3c fixes with tests: glyph stand-ins + titles (0adb744), focus reports + termios
      routing of paste/mouse/focus (aa4d28f); stream regression in test_xterm + refdiff
- [x] README section CLAUDE FROM THE AMIGA, RULES.md commands
- [x] make test green; make amiga clean (warnings are errors)
- [x] commits atomic, ending with the co-author line
- [ ] A1.4 rig + A1200 run (OWNER: steps below; this session must not start FS-UAE) (2026-10-06 NOT the same as replay-hardware-tests H1: H1 ran `Claude` to the NAS on the Replay and passed, but A1.4 steps 4-7 (uptelnet to the Mac end, `stty size` and resize follow, paste block, focus reports, Ctrl-], 3 wrong passwords) and the 020 spinner timing are not covered by it)

## Decisions

- Resize: uptelnet polls ACTION_VTCON_GWINSZ every half second (the WAIT_CHAR timeout).
  The raw event report (CSI 12 {) arrives in-band as 0x9B..., also a UTF-8 continuation
  byte, so it cannot be told from typed text.
- Console wait: an async ACTION_WAIT_CHAR, not an async READ: it comes back by itself
  (timeout), so leaving never strands a packet in the console (upgetty closes its PTY
  master to end a READ; a window cannot be closed under the user).
- Roadshow headers: `VTCON_NETINC`, default `vendor/roadshow-netinclude`, else DCTelnet's
  copy (`~/Code/dctelnet-v2/src/third_party/netinclude`, "Freely Distributable").
- Mac end in Python 3.9+ stdlib, one process, select() loop (macOS kqueue and ptys).
- Default allowlist = the bind interface's subnet with its real mask
  (`ipconfig getoption en0 subnet_mask`; /22 on the owner's LAN), not a guessed /24.
- No COLORTERM is passed: Claude Code then draws in 256 colours, which UP-Term maps to
  the screen's pens; true colour would only be reduced again.

## A1.3 measurements (tests/streams/claude-session.80x24.bin, Claude Code 2.1.289)

Claude Code in an 80x24 pty, TERM=xterm-256color, the engine answering its queries:

| Feature | Claude Code | UP-Term |
|---|---|---|
| Synchronized output ?2026 | every frame (74 pairs), DECRQM first | supported; answers 2026;2 |
| Colours | 38;5 / 48;5 only (no COLORTERM) | yes |
| Bracketed paste ?2004 | on | engine yes; handler fixed for termios mode (aa4d28f) |
| Focus ?1004 | on | engine kept the mode, nothing sent: fixed (aa4d28f) |
| ?2031 scheme updates, OSC 11 | asks | supported, answered |
| Kitty keyboard (CSI ? u) | asks | not answered: legacy keys (Enter = CR, Shift+Tab = CSI Z) |
| Kitty graphics (APC G) | probes | swallowed, not answered: no images |
| CSI 16t (cell pixels) | asks | not answered (only images need it) |
| XTVERSION, DA1 | asks | "vtcon 1.0", 62;22 |
| Window title (OSC 0) | spinner star + task | was "?"; now "*" (0adb744) |
| Glyphs | U+2500 box, quadrants, ✳✻✶✢✽ spinner, ❯ ⏺ ⏵ ⚠ ※ ◐◑ … — · | box/blocks drawn; the rest were "?" on a Latin-1 bitmap font: stand-ins now |

The grid matches libvterm cell for cell (`make test-ref ONLY=claude`). Bandwidth is no
issue (9.3 KB for the whole session); redraw time on an 020 is a rig/A1200 question.

Not done, deliberately: kitty keyboard protocol (Claude Code works without it; Shift+Enter
for a newline needs it or modifyOtherKeys -- `\` then Enter works), CSI 16t, images.

## A1.4 -- the owner's steps (NOT run here)

Mac end (either case), from the repo:

    (umask 077; mkdir -p ~/.config/uptelnetd; read -rs p; printf '%s\n' "$p" > ~/.config/uptelnetd/password)
    python3 tools/uptelnetd.py
    # or straight into Claude Code in tmux:
    python3 tools/uptelnetd.py --command 'tmux new -A -s claude claude'

It prints e.g. `listening on 192.168.0.58 port 2323 ... allowing 192.168.0.0/22`.

Rig (FS-UAE has `bsdsocket_library = 1` in tools/rig/rig.py; its connections come from
the Mac's own LAN address, which the allowlist covers):

1. `make amiga && python3 tools/rig/rig.py start && python3 tools/rig/rig.py install`
2. `cp build/amiga/uptelnet build/rig/vtc/`
3. In the rig: open an UP-Term window with vsh, run `VTC:uptelnet 192.168.0.58 2323`.
4. PASS looks like: the banner, `Password:` (typing not echoed), your zsh prompt.
   `echo $TERM; stty size` gives `xterm-256color` and the window's rows and columns;
   resize the window, `stty size` again follows within half a second.
5. Run `claude`: the box, the prompt `>`, the spinner as `*`, an answer's bullet `o`, the
   window title `* Claude Code`. Right-Amiga V with two lines in the clipboard: Claude
   Code shows a pasted block, not two submitted lines. Ctrl-C interrupts, Esc cancels,
   Shift+Tab cycles the mode. Click another window and back: no stray `[I`/`[O` typed.
6. Ctrl-] ends uptelnet: `uptelnet: connection closed`, and the window is a normal
   cooked vsh again (line editing works).
7. Wrong password three times: `Login incorrect` each after 2 s, then the line closes.

A1200 (Roadshow up): install the kit (`make dist`, build/UP-Term.lha; Install), or copy
`build/amiga/uptelnet` to C: and the new `build/amiga/vtcon-handler` to L: (the paste,
focus and title fixes are in the handler), reboot, then `uptelnet <the Mac's LAN
address> 2323` in an UP-Term window and steps 4-7. Note how long Claude Code's spinner
redraws take on the 020: that is the A1.3 redraw-speed number.

## Found

- The kit already ships BebboSSH (dist/net/bebbossh-1.45): `ssh` to the Mac's Remote
  Login would be the encrypted path (option B) with no new server. Not tried against
  macOS's OpenSSH (its key exchange and ciphers may be newer than bebbossh speaks).
- Termios mode lost paste text, paste marks, mouse and raw reports before aa4d28f: they
  went to the cooked buffer a termios read never serves. That also affected tmux's mouse
  and vim's paste over ixemul, untested on the rig.

## Progress log

- 2026-10-04: all code items done and committed; A1.4 handed to the owner.
