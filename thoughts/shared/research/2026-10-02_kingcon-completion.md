---
date: 2026-10-02
topic: KingCON's Tab completion, as a spec for UP-Term's "kingcon" completion style (H8)
tags: [kingcon, completion, lineedit, h8]
status: final
---

# KingCON completion: the behaviour UP-Term's `completion = kingcon` reproduces

Sources: `reference/kingcon/KingCON-handler.asm` (line numbers below), read by a
research agent; and the owner's own KingCON install on the rig (KCON: from
`DEVS:KingCON-mountlist`, default options), driven by `tools/rig` (screenshots in the
session scratchpad, not kept). Logic is re-written in our own C, credited "KingCON,
David Larsson"; no assembler is copied (reference/kingcon/README.md).

## Measured on the rig (default FNCMODE = W)

| Typed | Key | Result |
|---|---|---|
| `dir S:Sh` | Tab | `dir S:Shell-Startup ` (one match: name + space) |
| `list S:` | Tab | "Select filename" window: GadTools list (sorted, case-insensitive), string gadget with the selection, OK / Cancel |
| (window) | Tab, Tab, Shift+Tab | selection moves down, down, up |
| (window) | Return | `list S:Amico8.prefs ` inserted (name + space), window closes |
| `list S:Hip` | Tab | window with the 3 HippoPlayer.* names (no common-prefix step first) |
| (window) | Esc | window closes, line unchanged |
| `dir SYS:Pre` | Tab | `dir SYS:Prefs/` (directory: `/`, no space) |
| `lis` | Tab | nothing visible (Tab is file names everywhere; see below) |
| `dir SYS:Zzq` | Tab | nothing visible |

## From the source

Keys (cooked mode; NOFNC or RAW turns all of it off):
- Tab = file-name completion wherever the word is (5819, 6373-6384); during an inline
  cycle: next.
- Shift+Tab (`CSI Z`) = device / volume / assign completion, `:` appended (6343-6354);
  during a cycle: previous.
- Alt+Tab = command completion (6020-6026): path dir of the word, else the CLI path,
  every dir of a multi-assigned C: (SameLock de-dup of dirs), residents (seg_UC >= 0 or
  -2); executables only (E or S bit), no dirs, .info hidden.
- Ctrl+D on a non-empty line = list the directory of the word (or the current dir) in
  columns (8215-8256); on an empty line it is the normal break.
- Ctrl+S during a cycle opens the selection window.

Word: an odd number of `"` before the cursor = quoted word starting after the last `"`;
else back to space , > < or backtick (`=` `|` `;` are not delimiters). Only the file
part (FilePart) is replaced; text after the cursor is kept. Quoting on insert: quoted
when the word was opened with `"` or the result has a space; a file's trailing space
becomes `" `; an opening quote is inserted when needed (and removed again if a later
cycle step no longer needs it); a directory stays open (`"My Dir/`). Line cap 511.

Matching: case-insensitive, real case inserted. The typed part as a pattern when it has
wildcards (whole-name match, no implicit `#?`), else a prefix. `.info` hidden by default
(menu "Show .info"). File scan: ExAll of PathPart; dirs `/`, files a space; lock failure
= silent; zero entries = fall back to device completion of the whole word (whose no-match
beeps -- the rig's "nothing visible" was DisplayBeep(NULL), a screen flash on 3.1 that a
single screenshot can miss). Empty word + W opens an ASL file requester.

Order: insertion sort by kind descending, then Stricmp: files (2) before directories (1);
volumes (3) before assigns (2) before devices (1).

Styles (FNCMODE letters, default W; W clears L and B): W = window when several match;
L = print the list; B = cycle inline (Tab next, Shift+Tab previous, silent wrap); C =
common prefix first; S = silent. Any other key ends a cycle.

Selection window: titled "Select filename" / "Select device" / "Select command"; Tab /
Shift+Tab next / previous with wrap, up / down (Shift or Alt jump), Return / Enter /
double-click accept, Esc cancel.

Printed list (L, Ctrl+D): newline, 19-character columns ((width+1)/19, at least 1),
names over 18 shown as 15 + `...`, then prompt and line redrawn.

## What UP-Term builds (H8)

`completion = unix | kingcon` per profile (Prefs; later the H9 Session menu). unix is
today's behaviour (common prefix, list under the line, cycling). kingcon is KingCON's
default style W: the keys above, the selection window, its quoting and order, Ctrl+D.
One engine (complete.c) serves both; only the keys and the presentation differ.
