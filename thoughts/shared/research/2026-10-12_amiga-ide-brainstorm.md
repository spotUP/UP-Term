---
date: 2026-10-12
topic: Amiga IDE (VS Code-like) - decomposition, approach, spike gate
tags: [ide, editor, scintilla, amiga-pi, brainstorm]
status: draft
---

# Amiga IDE - brainstorm decisions

Owner goal (2026-10-12): a VS Code-like experience on the Amiga, all three forms: a
terminal IDE in UP-Term, a native Workbench GUI, and an agent-first (Cursor-like) review
of amiga-pi's edits. Porting VS Code/Electron itself was ruled out (no 68k V8, GBs of RAM).

## Decisions (owner-approved)

1. Decomposition and order, each with its own spec, plan and build cycle:
   1. Editor core: buffers, undo, search, highlighting, file tree model, diff/hunk model.
   2. Agent review: amiga-pi edits, per-hunk accept/reject (needs 1, amiga-pi F/K).
   3. Terminal IDE in UP-Term: tree, tabs, editor, amiga-pi panel, shell pane (needs 1, 2).
   4. Workbench GUI: the same core in an Intuition window (needs 1, 2).
2. Baseline machine: accelerated Amigas (030/040/060, PiStorm). A stock A1200 may be slow.
3. Languages: C/C++, 68k assembly, scripts (AmigaDOS, ARexx, bash/vsh, Python, Lua),
   data/docs (Markdown, JSON, Makefile, AmigaGuide).
4. Approach A: Scintilla + Lexilla as the core; Scinterm (curses, over our ncurses port)
   for sub-project 3; an Intuition platform layer for sub-project 4. Fallback if the spike
   fails: B, neovim headless over RPC (ported; 9.7 MB, ~22 s start on an A1200). Own C89
   core (C) only if both fail.

## Spike (approved, not started) - the gate before spec 1

Question: can Scintilla 5 + Lexilla + Scinterm be built with our Amiga toolchain and run
well enough on an 040?

- Build: which Amiga C++ compiler handles Scintilla 5's C++17 (string_view, optional);
  bebbo gcc 6.5 likely not, the gcc 16 track (W39) has one hang open. If neither, try
  Scintilla 3.x (C++11) and record what that loses.
- Run: Scinterm demo on ncursesw under UP-Term (ixemul), rig with --060 or --max.
- Measure on a 100 KB C file: open + full highlight time, one keystroke to screen,
  scroll a page, RAM used. Proposed pass bar (owner may change): open <= 2 s,
  keystroke <= 100 ms, RAM <= 4 MB.
- Lexilla lexers present for the listed languages; ARexx, AmigaDOS scripts and
  AmigaGuide counted as lexers to write.
- Output: a findings note with the numbers and a go/no-go; everything built is throwaway.
