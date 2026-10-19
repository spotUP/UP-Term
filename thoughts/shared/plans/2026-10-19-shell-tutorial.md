---
date: 2026-10-19
status: plan, decisions answered
topic: tutorial, an interactive shell tutorial for Amiga users new to a Unix shell
---

# tutorial: an interactive shell tutorial in vsh

## Goal

A new UP-Term user who knows the AmigaDOS Shell (Dir, Copy, Type, DH0:) and
has never used bash opens the tutorial, types real commands at a prompt, and
leaves knowing how to move around, handle files, use pipes, find help and
install packages in vsh. Every lesson starts from what they already know in
AmigaDOS and shows the vsh way next to it.

Done (reachability): `tutorial` started through the real launch paths (the
command, the Help menu item, /tutorial) runs lesson 1 against the host vsh
with scripted input, the step checks pass on correct answers and fail on
wrong ones, and progress is written and resumed (L-T1, L-T2).

## Owner decisions (answered 2026-10-19)

- D1 Command name: `tutorial` (owner's pick over `learn`). The slash command
  is /tutorial, the Help menu item "Shell tutorial".
- D2 Ships in the base archive (about 40 KB of text; a new user has not met
  upkg yet).
- D3 First-run hint: yes. One line under the banner in an interactive vsh
  ("New to a Unix shell? Type tutorial") until lesson 1 is finished, or
  until $HOME/.tutorial-hint-off exists.

## What exists to reuse

- dist/upkg-ask: POSIX sh, plain prompts from stdin with `read -r`, a
  `while IFS='|' read -r` line-record parser, and a plain fallback with no
  terminal. The lesson reader and prompt loop copy this pattern.
- mdv (UP-Term:bin): Markdown drawn for the window. Lesson text is
  Markdown, so headings, code spans and lists render the same as README
  pages and tldr.
- vshrc banner block (`case $- in *i*`): where the first-run hint goes.
- Help menu (handler/vtcon_handler.c menu_help, MENU_HELP_ITEMS) and the
  slash table (handler/slash.c): a "Shell tutorial" item and a /tutorial
  command, dispatched by menu_run() the same way as MENU_DEMO, which starts
  C:UPDemo in a tab (tab_spawn TAB_DEMO). The tutorial is a tab running
  `vsh UP-Term:bin/tutorial`, so its table row follows the UPDemo row (command,
  name, missing-file message, minimum size).
- Help menu "Slash commands" item (feature/help-menu): lands first; the
  tutorial item goes below it.
- build/vsh_host and tests/test_vshrc_editor.py: the host test pattern
  (real script, real vsh, scripted stdin).

## Design

### Program

`UP-Term:bin/tutorial`, a vsh script (POSIX sh plus nothing vsh-only, so the
host vsh and a real bash both run it for tests).

    tutorial           the menu of lessons, with a mark on finished ones
    tutorial N         start lesson N
    tutorial next      the first unfinished lesson
    tutorial reset     forget progress

Each lesson runs in a sandbox directory, `T:tutorial/` (made fresh per lesson,
removed at the end), so `rm` in a lesson never touches the user's files. The
script `cd`s there and tells the user they are in a practice drawer.

### Lesson files

`UP-Term:share/tutorial/NN-name.lesson`, plain text, one record per line
start, so the parser is the upkg-ask style `case` on the first character:

    # Title of the lesson
    > Markdown paragraph shown through mdv (consecutive > lines join)
    $ setup command run silently in the sandbox before the next task
    ? Task text the user sees: "List the files here"
    = check: a sh test run after the user's command; status 0 is a pass
    ~ hint shown after a wrong try ("Try ls")
    ! answer shown after the third wrong try, then the step moves on

A step is one `?` with its `=`, `~` and `!` lines. Text and checks stay in
data, not in the script: adding a lesson is adding a file.

### The step loop

1. Show the `>` text through mdv (plain `cat` when mdv is missing).
2. Show the task, prompt `tutorial> `, read a line with `read -r`.
3. Built-in words: `skip`, `hint`, `quit` (progress kept), `help`.
4. Otherwise run the line with `eval` in the sandbox, output to the window
   as a normal command, then run the `=` check.
5. Pass: a short "Right." line and the next step. Fail: the `~` hint; after
   three fails, the `!` answer is shown and run for them.

Checks look at the result, not the spelling, so `ls`, `ls -l` and
`command ls` all pass "list the files". Where the command itself is the
point (a pipe), the check can also match the typed line (`$LEARN_LINE`).

### Progress

`$HOME/.tutorial` holds one finished lesson number per line. `tutorial` with no
argument marks them; `tutorial next` picks the first one missing.

## Lessons (AmigaDOS first, vsh next to it)

- 01 Where am I: `pwd`, `ls`, `cd`. Dir, CD. `$PWD` is an AmigaDOS name
  (Work:dir). Paths: /Work/dir is Work:dir, `..` is the parent (AmigaDOS
  `/`), `/` alone lists the volumes, `~` is $HOME (SYS: by default).
- 02 Looking at files: `cat`, `less`, `head`, `tail`. Type, More. `type` in
  vsh is the bash builtin (which command is this), not C:Type.
- 03 Making and moving: `mkdir`, `touch`, `cp`, `mv`, `rm`, `rm -r`.
  MakeDir, Copy, Rename, Delete ALL. No trash: rm is final.
- 04 Wildcards and quotes: `*` is AmigaDOS `#?`, `?` is the same, quotes
  for names with spaces.
- 05 Pipes and redirection: `|`, `>`, `>>`, `<`; `grep`, `sort`, `wc`.
  AmigaDOS `>` is the same idea; a pipe has no AmigaDOS counterpart
  except PIPE: files.
- 06 Getting help: `help`, `man`, `cmd --help`, `tldr cmd`, the /help
  command and the Help menu.
- 07 Keys that save typing: Up/Down history, Ctrl-R, Tab completion, the
  grey suggestion, Ctrl-C. (Checks here are informational: no test.)
- 08 Variables and PATH: `$VAR`, `export`, `echo $PATH`, how vsh's PATH
  relates to the Shell's Path and C:.
- 09 Editing a file: `joe` and `nano`, $EDITOR, $HOME/.vshrc for your own
  settings.
- 10 Packages: `upkg list`, `upkg search`, `upkg install git`. The check
  only reads `upkg list` (no network in the lesson).
- 11 Mixing both worlds: running AmigaDOS commands from vsh (C:Dir, Assign,
  `Version`), and starting vsh from the AmigaDOS Shell.

Each fact in a lesson is checked against vsh and the README before it is
written (L-C1), since a tutorial that teaches a wrong thing is worse than
none.

## Checklist

- [x] L-0 Owner decisions D1, D2, D3 answered (2026-10-19).
- [x] L-1 dist/tutorial: menu, `tutorial N`, `next`, `reset`, sandbox in T:tutorial,
      plain fallback without a terminal (upkg-ask pattern).
- [x] L-2 Lesson file parser (#, >, $, ?, =, ~, !), mdv with a cat fallback.
- [x] L-3 Step loop: eval in the sandbox, check, hint, answer after three
      fails, skip / hint / quit / help.
- [x] L-4 Progress file $HOME/.tutorial, resume with `tutorial next`.
- [x] L-5 Lessons 01-11 written (share/tutorial/*.lesson).
- [ ] L-C1 Each lesson's facts checked on vsh (host and rig).
- [ ] L-6 Kit and dist: learn to UP-Term:bin, lessons to UP-Term:share/tutorial,
      in parts.txt and the base (not slim.txt-removed), uninstall removes them.
- [ ] L-7 Help menu "Shell tutorial" item and /tutorial slash command, a tab
      running `vsh UP-Term:bin/tutorial` (UPDemo row pattern), after the
      "Slash commands" item.
- [ ] L-8 First-run hint in dist/vshrc (D3: yes), off by
      $HOME/.tutorial-hint-off or after lesson 1.
- [ ] L-9 README.txt section: "New to a Unix shell" pointing at tutorial.
- [x] L-T1 Reachability test: tests/test_tutorial.py drives dist/tutorial through
      build/vsh_host with scripted stdin. Lesson 1: right answers pass, a
      wrong answer shows the hint, three wrong answers show the answer;
      $HOME/.tutorial gets "1"; a sentinel lesson proves the lesson files are
      the ones read.
- [x] L-T2 Lesson lint: every .lesson parses, every step has a check and an
      answer, and each answer passes its own check in a sandbox (run in
      `make test`).
- [ ] L-T3 Menu and slash wiring test next to the existing menu tests: the
      Shell tutorial item and /tutorial reach the tab spawn with the tutorial
      command line.
- [ ] L-R Rig check (owner or rig 4): tutorial from the Help menu on the
      Amiga, lesson 1 to the end, quit and resume.

## Risks

- `eval` runs what the user types. The sandbox keeps the lesson's files
  apart, not the whole machine; `rm -r /Work` still works. Lesson 03 says
  so before teaching rm.
- vsh runs subshells and pipelines as processes (no fork): `eval` of a pipe
  in the script must behave as at the prompt. L-C1 covers it on the rig.
- mdv's width follows the window; a narrow window wraps the text. The
  minimum size goes in the tab row (L-7), as UPDemo's does.
