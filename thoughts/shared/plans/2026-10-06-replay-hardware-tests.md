---
date: 2026-10-06
topic: UP-Term kit on the Replay (FPGA Arcade board, Amiga core amiga_db060) -- everything not yet tested there
tags: [replay, a1200, hardware, kit, serial]
status: draft
---

# Replay checklist (FPGA Arcade Replay, Amiga 060 core, 192.168.0.57)

Access: `AMI_HOST=192.168.0.57 AMI_TOKEN=<from Install-Agent> python3 tools/rig/ami.py`;
Pi 4 `ssh amigrabber` (serial log ~/amiga/serial.log, grabber owned by the
Up_Rough_Demo_System session -- don't touch its services or the Replay SD card).
Owner-only checks (eyes, hands) are marked OWNER.

- [x] H1 Claude from the Amiga to Claude Code on the NAS (`Claude` in an XCON
      window, NAS password, Claude replies). OWNER PASS 2026-10-06.
- [ ] H2 Install runs to the end (W50: froze four times; PINNED to L393, Copy LIBS:ixemul.library LIBS:ixemul.library.orig CLONE; earlier on-disk line
      L255 FixFonts, but FixFonts alone does not freeze -- on-disk log lags).
      Next: Install with each step echoed to RAM:inst.log, polled every 2 s
      from the Mac, so the last step before a freeze is known.
- [ ] H3 ixemul 80.1 and ixnet 80.1 installed (`Version LIBS:ixemul.library`),
      after H2 (today 48.2).
- [ ] H4 `python3 -c "print('hello', 1+1)"` prints `hello 2` (after H2).
- [ ] H5 `nvim --version`, and nvim opens and quits (OWNER for the screen).
- [ ] H6 `ls -l` (coreutils) lists.
- [ ] H7 UP-Term icon in SYS:Utilities opens a window (after H2).
- [ ] H8 UPDemo: euro sign and emoji (OWNER).
- [ ] H9 command colouring, white while typing then green (OWNER).
- [ ] H10 tmux and screen start and detach/attach.
- [ ] H11 Serial: a line sent from the Amiga (`Echo >SER:`) reaches the Pi.
      2026-10-06: 0 bytes arrived -- which port is the cable on? (OWNER)
      12:39: Pi bridge (FT232, 115200 8N1) has logged 0 bytes since 11:35, across the Replay's
      reboot after the L393 freeze; Enter and ? got no reply. Neither the ARM console nor the
      Amiga core reaches the cable: check which header it is on and TX/RX (OWNER).
- [ ] H12 Serial login: `upgetty LOOP` on the Amiga, a terminal on the Pi gets
      vsh; `sz` a file to the Pi's `rz` byte for byte (needs lrzsz on the Pi:
      sudo, OWNER). After H11.
- [x] H13 Synergy: Mac mouse/keys reach the Amiga past the Mac's right edge -- OWNER PASS 2026-10-06
      (asynergyc connected 2026-10-06). OWNER.
- [ ] H14 Uninstall leaves no UP-Term block in S:User-Startup (W51 fix; needs a
      kit rebuilt after 518e65b).

## Log
- 2026-10-06: asynergyc in SYS:UP-Term/asynergyc, started from S:User-Startup
  (;BEGIN asynergyc block), connects to Deskflow on the Mac (192.168.0.70).
