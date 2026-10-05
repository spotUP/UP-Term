---
date: 2026-10-05
topic: C:Claude /loop and ScheduleWakeup (A4 gaps 3, loop branch)
tags: [claude, a4, parity, loop, cron]
status: implemented
---

# A4 gaps 3: /loop and ScheduleWakeup -- progress ledger

Branch feature/a4-gaps3-loop, cut from main 07517cb. Audit rows:
thoughts/shared/research/2026-10-05_claude-parity-audit.md line 77 (/loop) and line 202
(ScheduleWakeup). A parallel agent owns tui.c/input.c/keys.c and new slash.c entries for
/color /focus /keybindings: my slash.c edits stay on the /loop entry and its handler; tui.c
gets one hook and one fold function, additive.

## What Claude Code does (sources, fetched 2026-10-05)

- code.claude.com/docs/en/scheduled-tasks.md: /loop is a bundled skill; interval + prompt ->
  fixed cron schedule (the model calls CronCreate); prompt only -> self-paced, ScheduleWakeup
  picks 1 min .. 1 h, chosen delay and reason printed each iteration; nothing (or interval
  only) -> built-in maintenance prompt or loop.md (.claude/loop.md, then ~/.claude/loop.md;
  >25000 bytes truncated; edits take effect on the next iteration). Esc while waiting clears
  the pending wakeup (CronCreate tasks unaffected). stop:true ends it. An iteration that
  neither reschedules nor stops gets one fallback wakeup ~20 min later; the loop ends when
  that one does not reschedule either. Self-paced loop: listed with the scheduled tasks, no
  jitter, seven-day expiry applies, not restored on resume. CLAUDE_CODE_DISABLE_CRON: the
  cron tools and /loop unavailable. Alias /proactive (commands.md).
- tools-reference.md: ScheduleWakeup, no permission; between one minute and one hour; stop
  cancels; the pending wakeup appears in Stop's session_crons.
- hooks.md: session_crons entries {id, schedule, recurring, prompt}, sourced from CronCreate,
  ScheduleWakeup and /loop; UserPromptSubmit runs on a /loop iteration.
- The /loop skill text itself (the harness's bundled skill, loaded 2026-10-05): parsing rules
  (leading ^\d+[smhd]$ token, trailing "every N unit" clause), the cron conversion table,
  "execute now", dynamic mode with ScheduleWakeup {delaySeconds, reason, prompt "/loop
  <input>", noop}, stop: {stop: true} alone.
- Changelog: wakeups display as "Claude resuming /loop wakeup"; Esc/Ctrl+C cancel a pending
  wakeup while idle; consecutive noop wake-ups fold into a single line.
- UNVERIFIED (no doc states it; from the tool description as remembered): the sentinels
  `<<autonomous-loop-dynamic>>` (ScheduleWakeup prompt) and `<<autonomous-loop>>` (CronCreate
  prompt) that the runtime resolves to the default prompt at fire time; the 60..3600 clamp
  wording. Print mode's /loop message: Claude Code's is not documented.

## Decisions

- D1 /loop is a bundled skill (commands.c, like /simplify) so the menu, the Skill tool,
  UserPromptExpansion and the skill counts work as for Claude Code's. The typed /loop goes
  through a slash.c handler first: CLAUDE_CODE_DISABLE_CRON and print mode refuse; an empty
  prompt appends the default prompt (loop.md or the built-in one) to the expansion.
- D2 The pending wakeup is a one-shot job in the existing cron table (tasks.c), flagged
  wakeup: it fires through tasks_cron_due / sched_wake / the tui.h wake hook -- no second
  timer. Exact second (no minute rounding, no jitter); its expression pins the fire minute
  for CronList/session_crons. One at a time: a new ScheduleWakeup replaces it. Not written to
  the session's .cron file (not restored on resume).
- D3 Turn end (sched_loop_end): a wakeup-fired iteration that called no ScheduleWakeup gets
  one fallback wakeup in 1200 s; the next one that also calls none ends the loop. An
  iteration the user stopped (Esc) ends the loop instead of a fallback.
- D4 Noop fold on the screen only (the line mode cannot take lines back): the tick's lines
  are dropped from the Ctrl+L ring and the screen redrawn with one line (tui_fold).
- D5 Esc / Ctrl+C on the idle empty box with a wakeup pending cancel it (tui.h esc_idle hook);
  a typed prompt does not.
- D6 The built-in maintenance prompt: the docs' three steps, the pull-request step replaced
  by what this machine has (no git here).

## Checklist

- [x] L1 tasks.c: wakeup jobs (set, cancel, get, loop state, seven days, not persisted)
- [x] L2 tools.c: ScheduleWakeup declared (as the cron tools), never asks, header, summary
- [x] L3 commands.c: the bundled loop skill
- [x] L4 slash.c: /loop + /proactive handler (disable-cron, print mode, default prompt)
- [x] L5 sched.c: fire as "Claude resuming /loop wakeup", sentinels resolved, turn-end fallback
- [x] L6 tui.c/tui.h: esc_idle hook, tui_fold; repl.c wiring
- [x] L7 tests (fake clock): expansion, clamp, fire, stop, fallback, default prompt + loop.md,
      sentinel, print mode, disable-cron, Esc cancel, typed prompt keeps it, noop fold, expiry,
      session_crons, CronList, reachability through repl_screen
- [x] L8 fixture: tests/claude/tool_loop.sse, "loop test" route, RULES.md row
- [x] L9 audit rows 77 and 202, make test, make amiga, commit

- D7 loop.md is read whole up to 1 MB and cut at 25000 bytes (sys.h has no partial read); a
  file over 1 MB is skipped (deviation, documented in sched.c).
- D8 Wording that Claude Code does not document is ours: the ScheduleWakeup result, the fallback
  and end notes, the Esc note, print mode's refusal, the folded line's text after "Claude
  resuming /loop wakeup".

## Log

- L1-L6: tasks.c (wakeup jobs, loop state, ScheduleWakeup tool), tools.c/h (declared as the cron
  tools, never asks, header "Wakeup(reason)", summary = the result's first line), commands.c
  sk_loop, slash.c loop_ + run_def (slash_custom's tail, shared), sched.c (fire, sentinels,
  default prompt, sched_loop_end, sched_esc_idle), tui.c/h (esc_idle hook in handle() for Esc and
  Ctrl+C on the idle empty box; tui_takeback), repl.c (turn end, hook, tick reset).
- L7: test_gaps3_loop L1-L11 + test_loop_screen (repl_screen + repl_run, fake clock moved by the
  script's idle ticks; sentinel r.n_loop_ticks). Probe: with sched_loop_end and the esc hook
  unwired, 10 checks fail; restored, all pass. Expected deltas updated: 19 tools, "Skills: 10
  (+1)", print_stream.jsonl golden (ScheduleWakeup, loop -- nothing else moved).
- L8: tests/claude/tool_loop.sse + loop_final.sse, "loop test" routed first in pick(); RULES.md row.
- L9: audit rows 77 and 202 -> have; also test_claude_config's bundled-skill count (10). make test:
  43 suites OK; timeout 900 make amiga (vbcc, SDKs from the main checkout's vendor/): rc 0.
  Committed on feature/a4-gaps3-loop (the commit that adds this ledger).

## Open for a human (rig / screen)

- Rig check with the fixture: `python3 tools/claude_fixture.py`, on the Amiga
  `Claude URL=http://<Mac>:8080/v1/messages ROOT=SYS:`, type `/loop loop test`. PASS: "Looked:
  nothing new." and "Wakeup(quiet: another look in a minute)"; about a minute later
  "Claude resuming /loop wakeup" and the same again, not echoed as "> /loop loop test"; after the
  second wakeup the two quiet iterations collapse into one line "Claude resuming /loop wakeup (2 quiet
  wake-ups, nothing to do): quiet: another look in a minute" (the screen redraws once); Esc on
  the empty box prints "Cancelled the pending /loop wakeup" and nothing fires after.
- The redraw on folding clears the console window and draws the kept lines again (Ctrl+L's
  path): look for flicker or lost rows on the real console.
