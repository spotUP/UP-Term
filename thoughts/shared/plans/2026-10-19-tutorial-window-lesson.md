---
date: 2026-10-19
topic: Shell tutorial lesson 12 "The UP-Term window" with real checks of window state
status: plan
scope_report: session scratchpad window-state-scope.md (summarised below)
depends_on: feature/shell-tutorial merged (after the 1.2 release; peer holds vtcon main until "1.2 published")
---

# Lesson 12: the UP-Term window

## Goal
Lessons 12 and 13 teach tabs, menus, slash commands, copy and paste, the Right Amiga shortcuts and split panes, and
CHECK each step from the shell. A check passes only when the user really did the action in the window.

Done = `tutorial --selftest` passes for lessons 12 and 13 on the host and on rig 4, AND one rig 4 run of lesson 12
through the real entry point (Help > Shell tutorial tab, real key presses and menu picks) reaches "Lesson finished."
while the handler counters prove the actions ran (sentinel).

## Decisions (made, do not re-open)
- D1 Mechanism: extend the existing "ask the handler" channel. A new slash command `/state` goes through
  ACTION_VTCON_COMMAND (handler/vtcon_packets.h:60, vtcon_handler.c:6648), and the shell reads it with
  `UPTerm state [key]` (handler/upterm.c). No new packet, no ARexx port, no terminal query. Environment variables
  are stale inside a running script (sync_env runs only at a vsh prompt), and OSC 52 reads stay off.
- D2 State is monotonic counters, checked as a delta against a baseline that a `$` setup line saves. A snapshot
  would let a stale high value pass, and the tutorial tab itself already raised the tab count.
  Counters: tabs-opened, tab-switches, menu-picks, last-menu, slash-count, last-slash, copies, pastes, plus
  tabs (current count) and active (index).
- D3 Single source of truth: the counters live on the HOST con (it owns ntabs/active/tab_list,
  vtcon_handler.c:221). The host PUSHES the counter block to each tab con when a counter changes, using the
  existing host-to-tab message path (tab_post). A tab answers `/state` from its own copy, so the handler never
  blocks or waits on another process. This is a mirror for reading only; the host is the only writer.
- D4 Formatting `/state` output ("key value" lines) is a pure function in handler/slash.c (or a C89 header next
  to it) so tests/test_slash.c can drive it with fake counters.
- D5 A slash line never reaches `read` (vtcon_handler.c:4388-4400 consumes it and sends a bare newline). In
  dist/tutorial, an EMPTY typed line runs the step's checks silently: pass = "Right.", fail = no miss counted,
  prompt again. This is safe for every lesson, because --selftest already proves each check fails when
  LEARN_LINE is empty. The same lets a user do a menu pick or a tab switch and then press Return.
- D6 Paste is checked with no handler state: the lesson shows a random word, the user copies it with the mouse
  and pastes it at the prompt; the check compares $LEARN_LINE. The copies counter proves the copy.
- D7 Split panes are taught with tmux (the tmux addon; Ctrl-B % splits, `tmux list-panes`, Ctrl-B d detaches;
  tools/rig/tmux_rig.py:9-11,90-96). They go in their own lesson 13 "Split panes with tmux", so lesson 12 does not
  depend on an addon. Lesson 13's first text says to install the addon; its selftest answer is
  `tmux new-session -d && tmux split-window`. Native splits are a separate scope (ledger NATIVE-SPLITS).
- D8 Host selftest seam: checks call `${UPTERM_STATE:-UPTerm} state KEY` (the TUTORIAL_* override style). The
  host test points UPTERM_STATE at a fake script that answers from a file the test writes.

## Checklist
- [x] P1 DONE 92528f6f handler: counter block on the host con; bump points: tab spawn (tab_spawn/host_command ~:5812), tab_activate
      (:5559), menu_run/menu_pick (~:2363-2442, + last menu id), slash_run (+ last name), vtwin_copy/paste
      (render/vtwin.c:1347-1357).
- [x] P2 DONE 92528f6f handler: push the block to tab cons on change (tab_post); tab con keeps the mirror.
- [x] P3 DONE e48bf6a9 slash.c: `/state` row in the slash table + pure formatter; tests/test_slash.c cases (all keys, one key,
      unknown key = rc 5 and a message).
- [x] P4 DONE fd788ed8 C:UPTerm: `UPTerm state [key]` prints the answer (one key prints the bare value); test.
- [x] P5 DONE 3916391d on feature/shell-tutorial (test_empty_line_runs_the_checks_for_an_action_in_the_window, fails reverted). dist/tutorial: D5 empty-line check run; test in tests/test_tutorial.py (empty line passes a state step,
      does not count a miss on a plain step); D8 override documented in the header.
- [x] P6 DONE 680fc49a a12bafc6 dist/tutorial-lessons/12-the-up-term-window.lesson (name <= 30): new tab (Right Amiga T), switch back,
      Help menu item, a slash command (/help), copy + paste a random word, one more shortcut. Every step: `$`
      baseline, `=` delta check, `~` hint, `!` answer usable by --selftest with the fake.
- [x] P7 DONE 680fc49a (file 13-split-panes-tmux.lesson, tmux is base kit) dist/tutorial-lessons/13-split-panes-with-tmux.lesson (name <= 30), D7.
- [x] P8 NO CHANGE (no counter covers shell keys) lesson 7 key steps are echo stand-ins: replace with real checks where a counter now proves them.
- [x] P9 DONE host: make test ONLY=slash, tabprog, tutorial tests, test_tutorial_lessons (12 and 13 selftests),
      test_dist_manifest (30-char rule). Each new fix/feature test fails when its code is reverted.
- [x] P10 DONE make amiga rc 0 make amiga (vbcc, -warnings-as-errors).
- [x] P11 rig 4: tutorial_rig.py new case `lesson12`: open the tutorial tab from the menu, run lesson 12 with real
      keys and menu picks (reuse tabs_rig/menus_rig/slash_rig helpers), assert `UPTerm state` counters moved
      (sentinel) and "Lesson finished."; selftests 12 and 13 on Amiga vsh (13 needs the tmux addon installed).
- [x] P12 merge to main, push, delete branch, close rig 4, ledger.

## Risks
- R1 Push frequency: copy/paste/menu counters change rarely; tab_post per change is cheap. Measure that a paste
  of a long text does not post per character (count copies/pastes per action, not per char).
- R2 A tab opened BEFORE P2 lands (old kit) has no mirror: `/state` answers "unknown" and the lesson says to
  update UP-Term. Not a crash.
- R3 tmux on 68k is slow to start; lesson 13 text says so; rig timeout per tmux_rig.py.
