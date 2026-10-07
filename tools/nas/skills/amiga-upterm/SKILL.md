---
name: amiga-upterm
description: Use when working on or with the user's Amiga (AmigaOS 3.x on a Replay / FS-UAE) through the amiga MCP tools (amimcp/amiagent), or when the user asks about AmigaDOS, UP-Term, vsh, XCON:, assigns, ENV:/ENVARC:, Installer scripts, or files on the Amiga.
---

# The Amiga and UP-Term

You are Claude Code running on the user's NAS. The user talks to you from an
Amiga, through UP-Term (a terminal) and uptelnet. The `amiga` MCP tools
(amimcp -> amiagent on the Amiga) let you act on that same Amiga: run AmigaDOS
commands, read and write files, list drawers, see the screen, click and type.

## Safety rules (read first)

- amiagent runs whatever you send, as the user, with no undo. Read before you
  write; never delete or overwrite outside what the user asked for.
- A command that waits for input or a requester ("Please insert volume ...",
  "System Request", "Volume Request") blocks amiagent until it is answered.
  Before running anything, prefer commands that cannot ask: give full paths,
  check an assign exists with `Assign >NIL: NAME: EXISTS` (never asks), and
  redirect input from NIL: (`cmd <NIL:`). If a requester is up, look at the
  UI tree / screen and tell the user, or cancel it, before doing anything else.
- Never kill or reboot during an install or a copy. Long copies show no output.
- Never paste or log secrets (ENVARC:Claude/key, amiagent tokens).

## AmigaDOS in one page

- Paths: `Volume:dir/file` or `Assign:dir/file`; `/` alone means the parent
  drawer (not root); `:` alone is the root of the current volume. Names are
  case-insensitive, case-preserving.
- Assigns are logical names: `SYS:` boot volume, `C:` commands, `L:` handlers,
  `LIBS:` libraries, `DEVS:` devices and `DEVS:DOSDrivers` (mounted at boot),
  `S:` scripts (`S:Startup-Sequence`, `S:User-Startup`), `ENV:` (RAM, this
  session) and `ENVARC:` (disk, survives reboot), `T:` temp. `Assign LIST`
  shows them; `Assign NAME: path` makes one, `Assign NAME:` removes it.
- Common commands: `List`, `Dir`, `Type`, `Copy FROM TO ALL CLONE`,
  `Delete ... ALL QUIET`, `MakeDir`, `Rename`, `Protect`, `Execute script`,
  `Version name`, `Avail`, `Info`, `Search`, `Which`, `GetEnv`, `SetEnv [SAVE]`.
  `Status` lists processes; `Break N C` sends Ctrl-C to process N.
- Return codes: 0 ok, 5 warn, 10 error, 20 failure. Scripts use `If WARN`,
  `If ERROR`, `FailAt`, `Skip`/`Lab`.
- Text files use LF line ends and ISO-8859-1 (Latin-1).

## UP-Term on this Amiga

- Installed by the kit's Installer into a drawer the user chose, reached as the
  assign `UP-Term:` (made at every boot by a marked block in S:User-Startup, and
  from `ENVARC:up-term/Dir` by UP-Term's own programs if missing).
- `XCON:` is UP-Term's console handler (an xterm-class terminal in a window);
  `PTY:` gives Unix pseudo-terminals; CON:/RAW: may be switched to UP-Term
  (`C:UPConsole STATUS` shows what is active and the kit version).
- `vsh` (UP-Term:bin/vsh and C:vsh) is a bash-compatible shell; its startup
  file is ENVARC:vsh/vshrc; $PATH is in Unix form (`/UP-Term/bin:...`).
  GNU coreutils, tmux, screen, nvim, python3 live in UP-Term:bin and drawers.
- `C:Claude` is the Amiga client that connects here (ENVARC:Claude/remote holds
  `host port`; with an API key it talks to Anthropic directly instead).
  `Claude SETUP` / `/setup` runs its setup wizard.
- Uninstall (beside Install in the kit) removes everything and restores
  S:User-Startup byte for byte.

## Working style

- Prefer text over screenshots: command output, `Assign LIST`, `List`, the UI
  tree. Take a screenshot only when the question is visual.
- Say what you will run before running anything that writes, and show the
  result after.
