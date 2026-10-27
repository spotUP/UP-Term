---
date: 2026-10-27
topic: Amarchy shared job view (roadmap Phase J): run a command, stream its output into the menu, Done/Failed footer
status: approved scope (owner, 2026-10-27), not started (0 of 11)
ledger: thoughts/spot/todos.md row AMARCHY-JOB-VIEW
research: thoughts/shared/research/2026-10-27-amarchy-job-view.md
roadmap: thoughts/shared/plans/2026-10-26-amarchy-roadmap.md Phase J (RJ.1-RJ.7)
---
# Amarchy shared job view

## Owner decisions (2026-10-27)
- RJ.3 scope: output-only first. The job view streams output; upkg-ask keeps taking defaults or recorded answers
  (its stdin/stdout are not a terminal, dist/upkg-ask:55-62). The two-way question protocol (tagged `@ASK` lines,
  PTY: channel, choose-one/confirm rows) is a later item, not part of this plan.
- Done/Failed colours: new theme roles (MT_OK, MT_FAIL) with theme-file keys and fallbacks, not ACCENT/DIM reuse.
- Both styles: the drawn menu (gui.c) and the text menu (menu.c, libupkgui) both get the job view.

## Design
- Channel: the job's output goes to a T: file (`T:amarchy-job.<task>`); Amarchy reads new bytes on a timer.device
  tick (TR_ADDREQUEST, the handler/pty_handler.c:185-200 timer_start/timer_stop pattern). PIPE: is out (its Read
  blocks, RULES.md:67). Input is NIL:. upkg output is line buffered into a file (patch 0024), so lines arrive per line.
- Exit code: the job runs as an async SystemTags child; it ends by a wrapper that records the return code. Default:
  run `<line>` through the shell with `SYS_UserShell` and an end marker the job writes after the command
  (`<line>` then `Echo "@JOBRC $RC"` into the same file). The pure parser takes the `@JOBRC n` line as the end and the
  code; the marker line is not shown. Chosen because it needs no child-process completion message on the libnix/ixemul
  sides; if the rig shows `$RC` does not expand in the shell the menu uses, the fallback is a NP_ExitCode hook, decided
  at RJ.1 with evidence.
- Pure core `amarchy/job_core.c` (host tested): append bytes with partial-line carry, CR and CRLF handling, tab
  expand, ring of the last JOB_MAX_LINES lines, `@JOBRC` detection, footer state (RUNNING, DONE rc 0, FAILED rc != 0,
  CANCELLED), scroll position that follows the tail until the user scrolls up.
- DOS side `amarchy/job_dos.c`: jb_start(line, prog, title) (pl_stack for the stack, rule 6), jb_sigmask(), jb_poll()
  (Seek/Read the new bytes into job_core), jb_cancel() (Signal CTRL_C to the child task), jb_end() (close, delete file,
  stop timer). One job at a time.
- Drawn menu: mode M_JOB in gui.c beside M_VIEW. Header band = job title (the logo slot stays empty until Phase L RL.2,
  which reads the logo file). Body = output lines, MT_TEXT. Footer = "Running...", "Done", "Failed (code n)" or
  "Cancelled" in MT_DIM / MT_OK / MT_FAIL. event_loop ORs jb_sigmask() into its Wait in every mode, so nested modes do
  not starve the job.
- Text menu: menu.c drives the same job_core from its ui_on_tick hook (menu.c:145, 243), draws through libupkgui. If
  libupkgui has no scrolling text view that can be refreshed from a tick, that API is a request to the UP-Term session
  (ledger row), and RJ.2t waits on it.
- Keys in the view: Up/Down/PageUp/PageDown scroll; Esc while running = cancel (CTRL_C to the child); Esc or Enter after
  the end = close back to the menu row.
- Keymap: a new row action prefix `job:<line>` (beside `run:`). `install.upgrade` and `install.update` move from `run:` to
  `job:`. `upkg browse` stays `run:` (curses, needs a terminal).
- upkg contract (upterm-ports feat/uhc-repo a3a9752, pkgs/opkg/doc/upkg-api.txt): rc 0 ok, 5 nothing to do, 11 usage,
  12 not found, 13 network, 14 package script failed, 15 needs answers (no tty), 20 fail. With stdout not a tty it prints
  `[PROGRESS n/m] <phase> <name>` lines. The footer names these codes in words (rc 5 counts as Done, "Nothing to do";
  rc 15 = "Needs answers: run it from a terminal"); job_core keeps the last `[PROGRESS n/m]` for the header
  ("3 of 7"). Codes live in one table in job_core, not in gui.c or menu.c.
- Sentinels: AMARCHY_JOB_LINES (lines appended) and AMARCHY_JOB_DONE (end seen, value rc + 1), published like
  AMARCHY_DRAWS (gui.c:515).

## Checklist
- [x] RJ.6a pure core job_core.c + tests/test_job.c: carry across chunks, CR, CRLF, tab, empty lines, ring overflow,
  `@JOBRC 0` / `@JOBRC 5` / marker split across two chunks, marker never shown, follow-tail vs scrolled. Done 5e91f921, suite job 70 checks 0 failed.
- [x] RJ.6b footer state machine in job_core (RUNNING to DONE/FAILED/CANCELLED, cancel after end ignored), upkg code
  table (0, 5, 11-15, 20, unknown), `[PROGRESS n/m]` capture, host tested. Done 5e91f921.
- [ ] RJ.1 job_dos.c: jb_start/jb_sigmask/jb_poll/jb_cancel/jb_end, timer.device tick (pty_handler pattern), stack via
  pl_stack, T: file removed at the end. `$RC` marker proven on the rig (or the fallback chosen with evidence).
- [ ] RJ.4 theme roles MT_OK and MT_FAIL: menu_theme.h, gthemes_core (GUI themes), text-menu roles, every theme file
  gets keys or a documented fallback; host test that each theme resolves both roles.
- [ ] RJ.2 drawn view M_JOB in gui.c: header title, body, footer, scroll keys, jb_sigmask in every event_loop Wait.
- [ ] RJ.2t text view in menu.c through libupkgui on the ui_on_tick hook (or the libupkgui API request ledgered).
- [ ] RJ.5 cancel and close: Esc sends CTRL_C while running, Esc/Enter closes after the end, menu returns to the row.
- [ ] RJ.K keymap `job:` action in leaf.c, host test in test_keymap (row action parsed, run: rows unchanged);
  dist/keymap install.upgrade and install.update move to `job:`.
- [ ] RJ.S sentinels AMARCHY_JOB_LINES and AMARCHY_JOB_DONE.
- [ ] RJ.7 rig reachability (rig 7, 060, both styles): hotkey, Install > Update lists, the view shows upkg output lines,
  footer Done with code 0, sentinels move; a failing job (`job:Failat 1 ; Fault 205`-style line, no network) shows
  Failed with its code; Esc during a long job cancels. Against the local P7 feed, never the real feed.
- [ ] RJ.D docs: dist/keymap comment for `job:`, Amarchy user doc row on the job view.

## Deferred (owner, 2026-10-27)
- Two-way questions in the job view (RJ.3 full): upkg-ask tagged-line mode in upterm-ports, PTY: channel, choose-one and
  confirm input rows. Own plan after this one lands.

## Results
