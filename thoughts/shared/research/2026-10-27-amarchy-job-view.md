---
date: 2026-10-27
topic: Amarchy Phase J shared job view - facts for planning (RJ.1-RJ.7)
status: research
---

Read from branch prefs-inline (main has the Phase J text, roadmap lines 303-317; gap research section 4 is on main only, lines 107-143).

## 1. How commands run today
- act_run (amarchy/act.c:19-37): SystemTags async, SYS_Input and SYS_Output both NIL:, 32768 stack. Nobody reads output.
- act_in_window (act.c:69-96): XCON: window as input, SYS_Output 0 (console is the window). Output goes to a window, not read.
- run_in_uterm (amarchy/leaf.c:57-84): Open(UP-Term window spec), SystemTags async, input = window, output 0, stack pl_stack(prog,65536). Used by `run:` rows (leaf.c:534) and lf_open_index (leaf.c:315-325). Window is the job's console; Amarchy never sees bytes.
- start_detached (leaf.c:362-370) -> pl_start (amarchy/proclaunch.c:57-77): NIL: in/out, async, stack = max(floor, program $STACK cookie).
- pl_run (proclaunch.c:79-87): SYNCHRONOUS, caller passes `out` BPTR. The only place output is read: pk_dos_read (amarchy/pkgs_dos.c:19-36) runs `upkg --tsv` into T: temp file, waits, reads whole file (64 KB). Not streaming.
- No existing code reads a child's output live. A job runner is new; pl_run's "out BPTR" shape is the closest base.

## 2. Event loops and waiting
- Drawn menu: `event_loop` amarchy/gui.c:983-990, `Wait((1UL << g->win->UserPort->mp_SigBit) | SIGBREAKF_CTRL_C)`; CTRL_C = hotkey toggle, closes. Modes enum gui.c:74 `M_LIST, M_INPUT, M_MSG, M_VIEW`. Each mode (ask_value gui.c:1132, refused gui.c:1160) re-enters the same event_loop, so a job view = one more mode + a signal bit OR-ed into this Wait and a `if (got & jobsig)` branch. Warm server loop gui.c:1393 and menu.c:513 wait on the server port + CTRL_C only.
- Text menu has no Wait of its own: it runs libupkgui (upterm-ports) ui_run with `ui_on_tick(on_tick, 0)` (menu.c:145, 243, 386, 426; on_tick returns 1 to end ui_run with UI_TICK). Polling hook, used today for the index scan watcher (ixw_poll, amarchy/ixwatch.h). Re-set after every ui_start.
- Multi-signal Wait precedent: commodity loop amarchy/amarchy.c:329 `Wait(cxs|rxs|nts|lps|wrs|cws|CTRL_C)`; clipwatch.c:66 `cw_sigmask()` shows the sigmask-from-IO-request pattern (cw_io is a timer/notify IO).
- timer.device precedent with TR_ADDREQUEST: handler/pty_handler.c:185-200 (`timer_start/timer_stop` helpers, SendIO) and handler/vtcon_handler.c:3196, 6615. None in amarchy/ yet. Reuse: copy the pty_handler timer_start pattern.
- PIPE: in the repo: only tools and RULES.md:67 (a PIPE: Read blocks / "no answer"; PTY: pipe answers with what it holds). No Amarchy use. So PIPE: is NOT suitable for non-blocking read; it blocks the reader.
- ixpipe-handler (Makefile:844-849) = ixemul's IXPIPE: handler, builds from $(IXEMUL_SRC)/utils/ixpipe-handler.c, mounted as IXPIPE: (install_rig.py:592). It exists so execve can hand a pipe/socket to a native program; it is for ixemul vfork+exec children, not a general async-read channel. Not a fit.
- Implication: the proven non-blocking options are (a) job output to a T: temp file plus timer.device tick that Seek/Read-s new bytes (what the roadmap's alternative says), (b) PTY: pipe (answers with what it holds, RULES.md:67; handler/pty_name.h). PIPE: is out. A PTY: pipe as the job's output also makes isatty-style checks behave differently (see 4).

## 3. Reusable pieces
- Line splitter: gui.c `lines_split` (1100-1112, splits on \n into g->ln/g->ll, MAX_LINES) and `lines_wrap` (1114, wrap to pixel width). Both static in gui.c, operate on a complete buffer, no partial-line carry, no CR handling. A pure splitter for RJ.6 must be new (or extracted to a *_core.c) with partial-line carry; pk_parse in amarchy/pkgs_core.c already handles CR/last-line-without-newline for one format.
- Scrolling text view: M_VIEW mode (gui.c:74, 899, 952, 1283: key viewer) draws g->ln lines with scroll via g->top. Closest base for the log pane; is a fixed buffer, not append.
- Logo drawing: none in amarchy/ (grep logo: no hits). RL.1/RL.2 logo file not built yet; Phase L plan says RJ.2 and RL.1 share the file.
- Header/footer: draw_header gui.c:368, draw_border 357, draw_list 488, draw_full 504 (header, list, no footer). No footer/status line exists.
- Theme roles: amarchy/menu_theme.h:12-19 `MT_BG, MT_TEXT, MT_DIM, MT_ACCENT, MT_SELBOX` (5 roles; header = DIM, selected = ACCENT). GUI themes: amarchy/gthemes_core.h, `menu_theme_roles_gui` (menu_theme.h:38). No error/success role: Done vs Failed colours need a new role or reuse ACCENT/DIM. Decide in plan (RJ.4).
- ask_value (input requester): callback in lf_ui (amarchy/leaf.h:171-174), both styles: drawn = gui_ask_value (gui.c:1132, M_INPUT mode, header prompt, first row edit line, re-enters event_loop); text = ask_value (menu.c:79, libupkgui K_INPUT). Signature `int ask_value(ctx, label, usage, out, n)`; string only. NO choose-one or confirm requester exists (leaf.c callers: 112, 210-212, 238, 281, 521 all string/number/hex). RJ.3 needs choose and confirm new. Note a nested event_loop inside the job view means the job signal bit must also be served inside M_INPUT, or output queues until the answer.

## 4. upkg interactive questions
- upkg-ask (dist/upkg-ask in this repo; installed to UP-Term:bin/upkg-ask, Makefile:1357; called from postinst built in tools/pkgs.py:177-190). Questions file: `key|type|prompt|default|choices`, types string, yesno, choice (example dist/addons/claude/questions). Answers written as shell `KEY='value'` file for postinst.
- It asks through upkg-ui (curses, libupkgui dialog subset) ONLY when stdin AND stdout are a terminal (dist/upkg-ask:55-62). With output into a pipe/file it takes defaults silently (unless recorded); comment at line 55 states this on purpose for `upkg browse` programbox. So a job view that captures stdout via a file or PIPE: gets NO questions asked; today they are defaults. Recorded answers: UPKG_ASK_STATE (info_dir/NAME.answered, KEY='value' lines), UPKG_ASK_UNATTENDED=1, UPKG_ASK_PLAIN=1; upgrade runs scripts without the terminal and keeps recorded answers (upterm-ports commits b4e7694, 9cc0d27, merged PR #14; PR #4 in upterm-ports is an unrelated m68k-cc change, so "PR #4 record every answer" was not found there; the recording is in dist/upkg-ask lines 22-26).
- Machine-readable: `upkg --tsv` (package list, 9 tab fields, amarchy/pkgs_core.h:2-5; doc pkgs/opkg/doc/upkg-tsv.txt in upterm-ports not present at the checked branch, unverified). No question protocol exists. Patch 0024 (line-buffered output through a pipe) means upkg output streams per line into a pipe: good for RJ.1.
- A job view question needs a NEW protocol. Minimum: upkg-ask (new mode, e.g. UPKG_ASK_PROTOCOL=1 or a `--job` style) prints on stdout one tagged line per question, e.g. `@ASK<TAB>key<TAB>type<TAB>prompt<TAB>default<TAB>choices`, then reads the answer line from stdin; so the child needs stdin as a readable channel (pipe/file) from Amarchy, not NIL:. The job runner therefore needs TWO channels (output read, input write), which PIPE: and a plain file cannot give non-blocking; PTY: pair can. Parser needed on Amarchy side = the questions-file field split (key|type|prompt|default|choices) reused from dist/upkg-ask, one more reason for a pure core function with host tests.

## 5. The three rows (dist/keymap)
- Install add-ons: `run:upkg browse` (keymap:110, 237); Upgrade all: `run:upkg upgrade` (109, 238); Update lists: `run:upkg update` (239). All `run:` -> run_in_uterm -> an UP-Term window (leaf.c:534), vsh as shell, own window, Amarchy blind. Packages rows (installed add-on launches) start in an UP-Term window too (commit f44cf618). `upkg browse` is its own curses UI and must stay in a terminal; only Update and Upgrade fit a job view.

## 6. Host test patterns
- Suites are registered via `void suite_<name>(void)` in tests/test_<name>.c (tests/test_pkgs.c:40-45 pattern: static test_x functions, CHECK_INT/CHECK_STR from tests/harness.h), compiled into $(BUILD)/vttest_host with sources listed in Makefile:198-200 (add amarchy/<x>_core.c to both link lines and the TESTS list at Makefile:55). Run one: `make test ONLY=pkgs` (name = suite; Makefile:105-110 checks `vttest_host --has`). Closest copies: tests/test_pkgs.c (parser, CR/partial line), tests/test_menu_text.c (state machine, menu_text_core.c). Pure core with no OS calls, DOS side in a sibling _dos.c (pkgs_core.c/pkgs_dos.c split).
- Rig reachability: sentinel counters via sentinel_publish("AMARCHY_DRAWS", ...) in gui.c:515-519; add AMARCHY_JOB_LINES / AMARCHY_JOB_DONE the same way. Rig script style: tools/rig/upkg_ask_rig.py, own_prefs_rig.py.

## Open questions for the owner
1. Question channel: new upkg-ask protocol mode (tagged lines on stdout, answer on stdin, needs upterm-ports change + two-way channel) versus job view showing output only and questions staying defaults / in a terminal.
2. Output channel: T: temp file + timer.device poll (simple, no stdin) versus PTY: pair (two-way, needed for RJ.3). Recommend PTY: if RJ.3 is in scope.
3. Done/Failed colours: add roles to menu_theme (affects GUI-themes mapping) or reuse ACCENT/DIM.
4. Text menu: job view only in the drawn menu (RJ.2 says drawn), text menu keeps terminal windows - confirm.
