---
date: 2026-10-14
topic: Inventory of Amiga development tooling that already exists, for making amiga-pi useful for Amiga development
tags: [amiga-pi, amimcp, amiagent, toolchain, skills, inventory]
status: draft
---

# Amiga dev tool inventory (2026-10-14)

Read-only. Verified = I ran or listed it myself (command named). Unverified = inferred from docs.
Rig 1 was queried with `UPTERM_RIG=1 python3 tools/rig/ami.py exec 'List/Which/Assign/Version'` only.

## Summary

| Need | Exists | Missing | Smallest step |
|---|---|---|---|
| 1 Mac agent drives Amiga | amiagent 0.13.2 (Amiga half), `tools/rig/ami.py` (Python client of every protocol call), NAS Claude container already wired with an `amiga` MCP server | The Mac-side amimcp MCP server itself (not installed, not vendored) | Wrap `ami.py` as a stdio MCP server, or install amimcp from its GitHub source |
| 2 Build code | Mac: vbcc 0.9hp3 + vasm + vlink (brew), vtcon cfg `build/vbcc-aos68k.cfg`, NDK 3.2R4; gcc 15 built but not installed. Amiga: nothing | Any compiler/make/git/ssh on the Amiga; no remote-build tool for pi | Remote build: pi `bash` runs a script that uses bebbossh from the kit to ssh to the Mac, or amiagent PUT/EXEC from the Mac side |
| 3 Knowledge as skills | amiga-pi loads Agent Skills (`skills.c`, dir `UP-Term:var/amiga-pi/skills/`); NDK 3.2R4 autodocs on disk (111 files, 5.9 MB); one NAS skill `amiga-upterm` | No Amiga-dev skill for pi; no skill content for AmigaDOS/ARexx/gcc/ASM-One | Write SKILL.md files, put autodocs beside them, `skills/` loads them as is |
| 4 Crash feedback | MuForce, MuGuardianAngel, MuTools in NDK on the Mac | Nothing installed on rig 1 (Which: no MuForce/Enforcer/SegTracker); no capture path | Install MuForce on a rig, tee its output to a file (`MuForce_Window` / `> file`), have the agent `read` it |
| 5 ARexx control | amiagent `AREXX` (0x12) and `REXXPORTS` (0x13) calls; `ami.py rexx`; SYS:Rexxc and RexxMast present on rig 1 | An ARexx tool inside pi itself (pi only has read/write/edit/bash/grep/find/ls + MCP) | Via need 1 (MCP) or `bash` running `SYS:Rexxc/RX` (unverified that it runs under vsh) |
| 6 git / transfer | Amiga: bebbossh 1.45 (ssh, scp, sshd, keygen), bebboget 1.11, curl 8.22 vendored in dist/net; amiagent GET/PUT/GETRANGE | git on the Amiga (none); rsync | Transfer: bebboscp or amiagent PUT. git stays on the Mac |

## Need 1: Mac agent drives an Amiga

- `dist/net/amiagent-0.13.2/` (SOURCE.txt, PROTOCOL.md, amiagent, amiagent.020; Aminet comm/net, Apache 2.0, upstream https://github.com/thomas-luebker/amimcp). Read.
- Protocol, from PROTOCOL.md lines 41-66: PING, EXEC (rc + output, deadline), GET, PUT, LIST, INFO, SHOT, INPUT (keys, mouse), BREAK, SCREENS, POINTER, HASH, UITREE, UIACT (click, dclick, settext, menushort), MENUS, AUTH, HELLO, AREXX, REXXPORTS, GETRANGE. So exec, files, screenshots, UI tree, menus and ARexx are all there. Token auth (TOKEN=...), default port 7846.
- `tools/rig/ami.py` implements the client: `ping | break | info | exec | menus | uitree | ui | shot | rexx | get | put | key | screens | gclick` (header lines 3-9). Verified: `exec` works on rig 1 (Version, Assign LIST, List, Which all returned). Works against a real Amiga with `AMI_HOST`/`AMI_TOKEN`. Not an MCP server: a CLI.
- The MCP server (amimcp proper): NOT on this Mac. Checked: `pip list` (amitools only, a different thing), `npm ls -g` (only pi-coding-agent 1.2.0), `brew list` (no amimcp), `~/go/bin ~/.cargo/bin ~/.local/bin`, `find ~/Code ~/Downloads -iname '*amimcp*'` (nothing). `~/.claude.json` mcpServers names: sequential-thinking, context7, playwright, morphllm-fast-apply, tavily, chrome-devtools, figma (no amiga). `~/.pi/agent/mcp.json` does not exist.
- The NAS Claude container does use it: `tools/nas/mcp.json` has one server named `amiga` (`command: sh`; contents not read further), and `tools/nas/skills/amiga-upterm/SKILL.md` says "the `amiga` MCP tools (amimcp -> amiagent on the Amiga)". So amimcp is installed inside that Docker image, not on the Mac (unverified: Dockerfile not read).
- Smallest step: either build amimcp from upstream source for the Mac and add it to `~/.pi/agent/mcp.json` (amiga-pi supports pi's mcp.json, stdio and http, `amiga-pi/mcp.c`), or wrap `ami.py` (already tested against rig 1) in a small stdio MCP server like `amiga-pi/tools/fake_mcp.py`. Prefer upstream amimcp: no reinvention; check its licence/build first (unverified).
- Security note from SOURCE.txt: amiagent runs whatever it is sent; never start without TOKEN.

## Need 2: Build code

Mac side (verified by listing):
- vbcc 0.9hp3: `/opt/homebrew/Cellar/vbcc/0.9hp3/` (bin: vc, vbccm68k, vbccm68ks, vprof; config aos68k, aos68km, aos68kr, kick13*; targets m68k-amigaos and m68k-kick13 with include+lib; its own `NDK3.2R4/Include_H`). vasm and vlink in `/opt/homebrew/bin` (vasmm68k_mot, vasmm68k_std, vlink). Running `vc` bare prints "No config file!" (needs `+cfg`; the env VBCC is unset).
- vtcon builds with it: `Makefile:325-345` generates `build/vbcc-aos68k.cfg` (exists) from `tools/vbcc-aos68k.cfg.in` and runs `vc +cfg -I$(VTCON_NDK) -cpu=$(CPU) -O2 ...`. This is the proven flow; amiga-pi uses it too.
- NDK headers: `vtcon/vendor/ndk-3.2r4-Include_H`, `ndk-3.2r4-netinclude`; full NDK 3.2R4 at `~/Downloads/NDK3.2R4` (also `.lha`): Autodocs, Include_H/I, FD, SFD, lib, Examples, DeveloperDocumentation, Tools (Autodoc, AG2HTML, AGuideCheck, bumprev, Catcomp, Localize), MuTools, MuManual, SANA+RoadshowTCP-IP.
- Bebbo gcc: `/Users/spot/Code/UP-Term/amiga-gcc` (gcc 6 era, sdk dir with .sdk files) and `amiga-gcc15` (built 2026-10-04: `build-Darwin-m68k-amigaos/gcc/gcc/{xgcc,cc1,cc1plus}` exist). NOT installed: `/opt/amiga/bin` holds only 15 binutils (m68k-amigaos-as, ld, ar, objdump, strip, ...), no `m68k-amigaos-gcc`; `/opt/amiga/m68k-amigaos/lib` has only ldscripts, no libamiga/libnix. So there is no usable gcc on PATH today. (Counted from `ls /opt/amiga/bin | wc -l` = 15 and a `grep gcc` that matched nothing.)
- Also on PATH: lha, vamos, xdftool, romtool (amitools 0.7.1.dev9), vamiga in /opt/homebrew/bin.

Amiga side (rig 1, `Which` returned rc 5 = not found for each): vc, vbcc, vasmm68k_mot, vlink, gcc, m68k-amigaos-gcc, sc, dcc, make, git, ssh, scp, rsync, curl, python, python3, lua, nano, vim, ne, cc. Found: `C:lha`, `C:Ed`. Installed non-compilers: `SYS:Python3` (lib, bin, c3-sentinel.py; python3 not on path), `VTCX:upterm/` (UP-Term, Claude, ClaudeCode, amiga-pi, bin/vsh, var), `VTC:pkgs` (nano, ncurses, less, BitchX, shells, man, bin). Developer tools on DH1:Coding (listing, not run): ASM-One (V1.48, V1.49-RC1, Rescuer, Docs), Asm-Pro (AsmPro + readme), Amiblitz2, Resource v5.0 (2 copies), StoneCracker, Hex, Sources, Player6.1con. So an assembler IDE exists on the Amiga, no C compiler.
- upterm-ports pkgs (`/Users/spot/Code/UP-Term/upterm-ports/pkgs`): awk bzip2 coreutils diffutils file findutils fzy grep gzip ixcompat less libarchive mandoc nano ncdu ncurses patch script sed tree watch xz zip zlib. No make, git, gcc, ssh.
- Remote-build pieces that exist: `dist/net/bebbossh-1.45` (bebbossh client, bebboscp, bebbosshd, bebbosshkeygen, libcryptossh.library 000/020/060; source tarball included; notes in SOURCE.txt) and `dist/net/bebboget-1.11`, `curl-8.22.0`. `dist/ClaudeCode` plus `tools/nas` (Docker) reaches the NAS with `uptelnet`. These are in the kit (`dist/packages.txt` has 26 lines; grep for ssh/git/gcc matched none, so the net tools are installed by the Install script, not listed there; unverified).
- Whether the bebbossh binaries are installed on rig 1: not found by `Which ssh`/`scp` (they may be named bebbossh/bebboscp: NOT checked; run `Which bebbossh` next).
- Smallest step: no Amiga-native compiler needed. Let the Mac build (vc recipe from vtcon Makefile) and let pi on the Amiga call it through ssh (bebbossh) with a one-line script, or let the Mac agent push (amiagent PUT) and run (EXEC).

## Need 3: Amiga knowledge as pi skills

- Loader exists: `amiga-pi/amiga-pi/skills.c/.h`, pi 1.1.0 rules (Agent Skills, SKILL.md with name+description). Places: `<agent dir>/skills` = `UP-Term:var/amiga-pi/skills/` (ARCHITECTURE.md D10, line 56), `<cwd>/.pi/skills` (trusted), `.agents/skills` up the tree, settings.json `skills`, `--skill`. Host tests `tests/test_pi_skills.c`, golden cases in `amiga-pi/tests/pi/skills`.
- Skills on disk today: `~/.pi/agent/skills` does not exist. `~/.claude/skills`: port-demomaniac-effect, port-explorer-effect, synced, transpile-debug-68k-replayer (not generic Amiga). `vtcon/tools/nas/skills/amiga-upterm/SKILL.md` (UP-Term, vsh, XCON:, amiagent usage; also copied in `vtcon-demo-perf/tools/nas/skills`). Others under ~/Code, not generic: AmiExpress door skills (`AmiExpress/amiexpress-web/.claude/skills`), `Up_Rough_Demo_System/.claude/skills/port-amiga-effect`, `DEViLBOX/.github/skills/transpile-debug-68k-replayer`, amiberry skills (emulator, not dev).
- Raw knowledge on disk: `~/Downloads/NDK3.2R4/Autodocs` (111 .doc files incl. exec, dos, intuition, rexxsyslib, rexxsupport, arexx_cl, graphics, ...; 5.9 MB), `~/Downloads/NDK3.2R4/{Include_H,Include_I,FD,SFD}`, `MuTools/MuForce.guide`, `MuTools/MuGuardianAngel.guide`, `DeveloperDocumentation`, `MuManual`; ASM-One `Docs` and `Asm-Pro` readme on DH1:Coding; `/Users/spot/Downloads/AGA.guide`; `amiga-gcc/sdk/*.sdk` (library SDK definitions); `ixemul-vtcon/ixprefs.guide`; `/Users/spot/Code/creep-AmigaTools/*.doc` (small tool docs); AmigaGuide viewer tool `Tools/AG2HTML` in the NDK (converts guides to HTML).
- Missing: no skill for AmigaDOS, ARexx, gcc/vbcc flags, ASM-One/Asm-Pro. pi's system prompt has no docs section (COMPATIBILITY.md line 76).
- Smallest step: one skill dir per topic, `SKILL.md` short (a few KB) pointing to autodoc files by name, autodocs copied beside it only for the libraries needed (grep tool covers lookups). Context cost: autodocs are 5.9 MB, never in the prompt, only read on demand. vbcc flags: lift from `vtcon/Makefile:339` as the worked example.

## Need 4: Crash feedback

- On the Mac: NDK MuTools (`~/Downloads/NDK3.2R4/MuTools`: MuForce, MuForce_Off, MuForce_Window, MuGuardianAngel(+_Off), MuOVLYMGR, MuMapRom.debug, disassembler.library, guides). Needs an MMU CPU (060 rigs: memory note says rig runs as 060).
- On rig 1: `Which MuForce`, `Which Enforcer`, `Which SegTracker` all rc 5. Not installed. Only SnoopDos (`DH1:Tools/SnoopDos`) and `SYS:S/MCP.gurudat` (MCP's Guru data) seen.
- Missing: any capture path. amiagent EXEC captures stdout of a command only; MuForce writes to the serial port or a window.
- Smallest step: install MuForce with output redirected to a file (its guide documents the option; unverified, read `MuForce.guide`), then pi `read`s the file after a crash. SegTracker is a separate Aminet tool (not on disk; no web check made).

## Need 5: ARexx control

- Amiga side verified on rig 1: `SYS:Rexxc` (HI, RX, RXC, RXLIB, RXSET, TCC, TCO, TE, TS, WaitForPort), `SYS:System/RexxMast`, libs `rexxsyslib`, `rexxsupport`, `rexxmathlib` in SYS:Libs. `Which rx`/`Rexxc`/`RexxMast` rc 5 only because Rexxc is not on the C: path. Whether RexxMast is running at boot: not checked.
- Remote control: amiagent AREXX (runs the payload as a program via rexxsyslib, returns rc + RESULT; needs the REXX port) and REXXPORTS (lists live ports); amiagent itself has an `AMIAGENT` port (QUIT, TASKPRI, TASKLIST per readme). `ami.py rexx "program"`. Not run (rule: no programs beyond List/Assign/Version/Which/Type).
- Missing: no ARexx tool in pi. amiga-pi tool list is `read write edit bash grep find ls` (`tools_pi.c:98`) plus MCP and Lua extensions (`ext_lua.c`).
- Smallest step: expose AREXX/REXXPORTS through the amimcp MCP server (need 1); on the Amiga itself, `bash` calling `rx` once Rexxc is on the path. A native pi extension (Lua) is the other route, but it duplicates what the MCP tools already do.

## Need 6: git / file transfer

- Transfer exists: amiagent GET/PUT/GETRANGE/LIST (`ami.py get/put`, files over 16 MiB by chunks), bebboscp/bebbosshd, bebboget, curl (all vendored in dist/net), `lha` on C: (verified), IXPIPE/PTY/XCON handlers in the kit.
- git: none on the Amiga (Which git rc 5; absent from upterm-ports pkgs; no bebbo git in kit). Nothing on disk. Not checked: Aminet for a git port (no web).
- Smallest step: keep git on the Mac. Amiga edits reach the repo by amiagent GET or bebboscp; an agent on the Mac commits.

## Open points (unverified)

- Does `Which bebbossh` / `Which bebboscp` succeed on rig 1 (I tested `ssh`, `scp` only)?
- amimcp upstream: language, build, licence, whether it exposes AREXX/UITREE as MCP tools (the NAS SKILL.md suggests exec, files, screenshot, click, type).
- NAS Dockerfile: how amimcp is installed there; may be copyable to the Mac.
- vbcc cfg uses a flat `-I` NDK; vbcc target libs in brew (`targets/m68k-amigaos/lib`) not listed in detail.
- Whether MuForce logs to a file (read `MuForce.guide`).
