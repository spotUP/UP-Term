---
date: 2026-10-03
topic: Slash commands (ledger C1) -- every UP-Term setting as a typed /command in the line, and C:UPTerm for scripts
tags: [vtcon, handler, settings, c1]
status: implemented
---

# C1: slash commands

Owner 2026-10-03: "add command line command equivalents to all the ui settings stuff we added..
like claude cli with a / command for quick commands to control all settings etc of the term ...
one gotcha is that slash is the equivalent of cd .. on amiga so a single slash should still do
that".

## Design (decided)

- **What is ours**: a line typed in UP-Term's line editor (cooked mode) that starts with "/",
  then a lower-case name that is in the command table, then a blank or the end. Everything
  else goes to the program as typed: "/", "//", "/Work", "/c/dir" (no such command), "//cursor".
  A leading blank passes any line on (" /cursor" runs a parent directory's "cursor").
  Names are the profile keys where there is one (cursor, cursor-blink, bell, ...), so a
  command, a profile line and a Prefs field say the same thing.
- **One table** (handler/slash.c, portable, host-tested): name, values, and what each value
  does -- for settings the menu's own item id (handler/menu_ids.h, the enum moved out of the
  handler), so a typed "/cursor bar" runs the same menu_setting() a menu pick runs. Kinds the
  menu has no item for (any scrollback size, a font by name, colours) carry an argument.
- **Running**: the line goes to history, the command runs, its answer ("cursor: bar", or what
  was wrong) is written to the window, and the reader gets an empty line -- a shell prints a
  fresh prompt. Settings change the window only (as the menus do); /save writes them to the
  profile.
- **Tab** on a /command line completes from the table: command names, then that command's
  values (profile names from the profile file in memory). No worker: the table is in memory.
- **C:UPTerm** (tools: upterm.c): `UPTerm cursor bar` sends the same line (without the "/") to
  the console of its output by ACTION_VTCON_COMMAND and prints the answer; return code 0 / 10.
  For S:Shell-Startup, scripts, and windows whose line editor is not in use.

## Commands

cursor block|underline|bar · cursor-blink on|off · bell none|beep|visual · bold-bright on|off ·
meta amiga|alt · copy-on-select on|off · wheel scroll|ignore · completion unix|kingcon ·
kingcon-mode LETTERS · kingcon-info show|hide · kingcon-cache on|off|reset|purge ·
scrollback N|none · font [NAME SIZE] (none: the requester) · font-fallback NAME|none ·
fg / bg / cursor-color / selection-fg / selection-bg RRGGBB|none · theme [NAME] (none: the
requester) · profile NAME · save · prefs · find TEXT · copy · paste · tab new|next|previous|close ·
help [NAME].

## Checklist

- [x] C1.1 handler/menu_ids.h: the menu enum, shared.
- [x] C1.2 handler/slash.[ch]: table, parse (ours / not ours / error), help text, completion;
      tests/test_slash.c (paths stay paths, every command and value parses, errors say why).
- [x] C1.3 handler: the line editor's Return runs a /command; Tab completes it; each kind
      wired (menu_setting, scrollback, font, fallback, colours, theme by name, profile, save,
      prefs, find, copy, paste, tab, help).
- [x] C1.4 complete.c: theme by name (no requester when a name is given).
- [x] C1.5 C:UPTerm + ACTION_VTCON_COMMAND.
- [x] C1.6 rig check tools/rig/slash_rig.py: "/cursor bar" changes the menu's checkmark,
      "/" still goes up a directory, "/help" lists, Tab completes, UPTerm from a script.
- [x] C1.7 README, ledger.

## Result (2026-10-03)

7 of 7. Host: slash 119 checks (paths stay paths, every kind parses, errors say what fits,
the help lists every command, Tab names then values). Rig (3.1): tools/rig/slash_rig.py
9 of 9 -- "/cursor bar" checks the menu's Bar and the shell runs on, "/" is still the parent
directory, Tab completes "/cursor-b", "/help" lands in the window (found through UPTerm find
with the words in a shell variable, so the typed line cannot be the hit), UPTerm cursor
block / round (0 / 10), a leading blank passes the line on. The command word shows green
(screenshot, not asserted by the check).

Decisions made while building: the reader gets an empty line after a command (a shell shows a
fresh prompt; a program reading a line gets an empty one); "/theme NAME" reads
ENVARC:up-term/themes/NAME.conf in the completion worker -- a name that is not there beeps
(the answer is already printed when the worker finds out). Not completed by Tab: theme and
font names (they need a directory scan: DOS work); profile names are.
