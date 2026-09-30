# KingCON-handler source (reference only)

`KingCON-handler.asm` -- 68020 assembler source of KingCON-handler (the CON:/RAW:
replacement), copied byte for byte from `~/Code/KingCON-handler.asm` (file date
2005-08-22, 314787 bytes, 19697 lines, Latin-1). The owner found it on 2026-09-30 and
asked to keep it in the repo for later.

What it is (read from the file, not checked further):
- A resourced source (labels such as `lbW004130`) carried on by a later maintainer: the
  change log at the top runs from 0.16 (1.4, 1999-03-07) past 0.18 (1.5), with build
  switches for KS 3.x ROM, VisualPrefs, screennotify, NewMouse, OS4 and MorphOS.
- Copyright string: "Copyright (c) 1993,1994 David Larsson" (line 19484).
- Status (owner, 2026-09-30): KingCON is freeware. Its sources were lost until someone
  found them and published them on a public forum; this is that source. The file itself
  states no licence text.
- Use: study it freely. Where vtcon code follows its logic (for example the completer),
  write it in C in our own structure and credit "KingCON, David Larsson" in that file's
  header comment. No verbatim copying of the assembler.

Where it helps: ledger item H8 (KingCON-style Tab completion) -- the completer
(C: scan, multi-assigns, resident commands, quoting; change log l.23-94) -- and
console.device plan H5 (a CON:/RAW: handler's packet handling).

Not built, not in any Makefile target.
