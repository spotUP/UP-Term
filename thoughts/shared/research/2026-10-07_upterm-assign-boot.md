---
date: 2026-10-07
topic: UP-Term: assign at boot and requester safety (UPTERM-VOLUME-REQUESTER)
tags: [install, assign, requester, boot, ixemul, vsh, handler]
status: draft
---

# Problem, in the domain's terms

Invariant violated: "nothing of UP-Term asks AmigaDOS for a volume it does not
have, and UP-Term: exists as early as anything of UP-Term reads it."

Install makes `Assign UP-Term:` at install time and writes a marked block into
S:User-Startup. XCON:, PTY: and console.device are mounted by the system
(DEVS:DOSDrivers, Startup-Sequence) before User-Startup runs, and the CON:/RAW:
switch makes console.device UP-Term's. A window opened in that gap, or on a
boot where the block is missing or wrong (old Install over new, hand edit),
reads UP-Term:unifont/ with no assign. Owner confirmed the assign is not there
when the requester shows. Which exact consumer raised it on the Replay is NOT
confirmed (needs the owner's Assign LIST line and Type S:User-Startup tail, per
the todo); the design below closes every row of the table, so it does not
depend on that answer.

# Consumers (counted from `grep UP-Term:`, `/UP-Term`, UF_DIR/CE_DIR/BMSG_KIT_BIN,
`pr_WindowPtr` over vtcon and ixemul-vtcon; ixemul itself names no UP-Term path)

| Consumer | Before User-Startup? | Requesters off before this change? |
|---|---|---|
| render/vtwin.c uni_load/emo_load via vo_read (UF_DIR, CE_DIR) in handler AND console.device | yes, any early window | yes (outline.c worker sets -1), but then the page is "not installed" for that window: no Unifont/emoji until a new window |
| device/upconsole.c:569 VERSIONS | no (user command) | yes (-1) |
| shell/vsh.c resolve() $PATH loop, os_stat | no | yes (-1) |
| shell/vsh.c lock_name() (cd, redirections, `/UP-Term/bin/x`, `/vol/x` fallback) | no | NO: Lock of the Unix reading unguarded. Found and fixed |
| handler/complete.c:594, 944 (command cache, theme drawer) | no | yes (-1) |
| vtcon_handler.c workers 710, 744, 4881 | possible | yes (-1); read ENV:/ENVARC:, not UP-Term: |
| dist/vshrc PATH=/UP-Term/bin:... | no | per-process: vsh guards its own search; the PATH value is exported to ixemul children |
| ixemul programs (tmux, screen, nvim, python, coreutils) path search, `/UP-Term/..` to `UP-Term:` | no | only through ix_flags & ix_no_insert_disk_requester (library/__plock.c get_device_proc, __open.c). Default flags include it (library/ix_settings.c:35); a saved ixprefs without it, or a program setting its own flags, gets requesters. Not changed here (ixemul-vtcon repo) |
| tty/bmsg.h BMSG_KIT_BIN, shell/sh_exec.c:386 | no | a message string only, no DOS access |
| dist/install.dos, Uninstall | Install makes the assign itself; Uninstall reads it (`Assign EXISTS`, `CD UP-Term:` only when it exists) | n/a (scripts, guarded by EXISTS) |
| dist/screenrc, tmux.conf | no | use /C/vsh, no UP-Term path |
| ClaudeCode, IXPIPE, PTY, XCON, up-term.conf | no | no UP-Term: access |
| prefs/upprefs, demo, claude/, net/, zm/ | no | no UP-Term: access (strings only) |

# Options

1. Assign early. Stock OS 3.1/3.2/3.9 give three places before DOSDrivers
   mounts: edit S:Startup-Sequence (fragile: Replay/Workbench updates and
   other kits replace it; restore must be byte for byte; a bad edit stops the
   boot), a DOSDrivers-mounted helper (mount order inside DEVS:DOSDrivers is
   alphabetical and not a guarantee, and a mount entry that runs code is a
   handler we would have to write and keep resident), or the handler
   assigning itself from a stored path. Only the last needs no edit of a
   system file. It covers every UP-Term program but not a third party.
2. Make every consumer requester-safe (-1 around each access; ixemul path
   search fixed). Covers "nothing asks", but leaves an early window without
   Unifont/emoji and a vsh without /UP-Term/bin until the next boot: the
   assign is missing and the kit silently degrades. Fixing ixemul means a
   change in another repo that user prefs can still switch off.
3. Both. Chosen.

Cost/coverage: option 3 is one 70-line header (config/upassign.h), four
include sites, one Install line and one Uninstall line; no system file besides
S:User-Startup (already handled, byte-for-byte restore already tested) is
edited. It guarantees: UP-Term: exists whenever UP-Term's own code reads it
(handler and device via the glyph worker, vsh at startup, UPConsole), because
that code makes it from ENVARC:up-term/Dir (written by Install, removed by
Uninstall) before the first access; and when it cannot be made (not
installed, Uninstall ran, drawer deleted) every access is under
pr_WindowPtr = -1, so the answer is "not found". Not covered: a third-party
ixemul program that reads /UP-Term/... before any UP-Term program ran on a
boot with a lost block AND with ixprefs clearing the no-requester flag.
Nothing of UP-Term puts that program ahead of itself (vshrc's PATH is read
by vsh, which has made the assign by then); the residual is listed in the
todo for the ixemul repo (suppress requesters for Unix-slash-translated
names regardless of the flag, as Unix has no such requester).

# Design as built

- `config/upassign.h`: `upassign_parse` (pure, tests/test_upassign.c) and
  `upassign_ensure` (UPASSIGN_DOS): look in the DOS list (no requester), else
  read ENVARC:up-term/Dir and `AssignLock("UP-Term", Lock(dir))`, all under
  pr_WindowPtr = -1. Header-only so the four builds need no new object.
- `render/outline.c` VO_READ (the one choke point of handler and device for
  Unifont and emoji): ensure before an `UP-Term:` path.
- `shell/vsh.c`: ensure at startup; `lock_name` Unix-reading Lock under -1.
- `device/upconsole.c`: ensure before the VERSIONS read.
- `dist/install.dos`: `Echo >ENVARC:up-term/Dir "$updest"` right after the
  assign (ENVARC:up-term goes whole at Uninstall); `dist/Uninstall` deletes
  Dir BEFORE removing the assign (else a window opened meanwhile makes the
  assign again and the drawer is in use) and falls back to Dir when the assign
  is gone. S:User-Startup block unchanged (still serves third-party
  programs); S:Startup-Sequence is not touched, and install_rig now checks
  that byte for byte.
- Rig: `tools/rig/assign_rig.py` (not run), install_rig extended.

# Verification state

Host: `make test` suites green except an untracked probe of another agent
(invocation/type_missing_volume, not in a ratchet file). `make amiga` builds.
Reachability on the Amiga code (handler worker, vsh startup, UPConsole) is
only provable on the rig: assign_rig cases lazy-handler and lazy-vsh drive the
real entry points and assert the assign appears; noassign asserts no
"System Request" window. UNVERIFIED until the owner triggers phase 2.
