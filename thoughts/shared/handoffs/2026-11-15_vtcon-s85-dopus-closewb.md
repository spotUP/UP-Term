---
date: 2026-11-15
session: S85
topic: Workbench theme reset deferred while DOpus is open; DOpus scroller colour
status: fix built, rig check pending
---

# S85 handoff: DOpus CloseWorkBench cooperation

## Root cause (measured on rig 84)
DOpus `command wait Hide` uses HIDE_CLOCK by default. It opens a window owned by
process `dopus_hidden_clock` (0,0, 177x14, not backdrop). Patina counts it as a
foreign window, so `desk_defer_map` defers the Workbench reset. Hide is the wrong
approach.

DOpus already cooperates with CloseWorkBench (source ~/Code/Reference/dopus5/source):
- Library/wb.c L_WB_CloseWorkBench sends DN_CLOSE_WORKBENCH (patches active, wb.c:2625).
- Program/event_loop.c:614 starts "dopus_closewb" when DOpus has a window on the
  Workbench screen. misc.c:441 check_closescreen waits up to 4x250 ms for only
  Opus, WBENCHWINDOW and "« IPrefs »" windows to remain.
- MAIN_CLOSEWB (event_loop.c:481) closes all its windows, calls CloseWorkBench()
  again, reopens after 1 s (event_loop.c:924) or on DN_OPEN_WORKBENCH.
- When hidden, GUI->window is null, so the path is skipped and the clock stays.

## Fix (uncommitted, built)
- Hide feature reverted in patina/desk_dos.c (saved: scratchpad dopus-hide.patch).
- patina/win_core.c/.h: `desk_task_dopus(task)`: true for tasks named `dopus_*`
  except `dopus_hidden_clock`.
- patina/desk_dos.c dk_wb_windows (~304): DOpus-owned windows no longer count as
  foreign (except in splash_only mode). Needs the `(struct Task *)` cast for vbcc.
- tests/test_desk.c test_desk_task_dopus. `make test ONLY=desk`: 2408 checks, 0 failed.
- build/amiga/Patina 412540 bytes, built via buildq.
- tools/rig/patina_theme_save_rig.py: wb_pens_check role order ('bg','dim','fg','accent').

## Scriptable theme apply (for the rig check)
The theme menu automation is unreliable on rig 84. Use a hotkey instead:
keymap binding `gtheme:<name>` (config/keymap.c:222, KA_GTHEME) runs
`gt_dos_select(name,0,0)` in the commodity (patina/patina.c:425). Add a binding to
the rig keymap, restart Patina, send the key. Note: this path does not publish the
PATINA_WBPENS/... sentinels; only the menu path does (patina/leaf.c:536 lf_gt_select).
Read pens from the screen (rig palette/pens tools) instead, or route patina.c:425
through a shared helper that publishes them (preferred: move lf_gt_select's sentinel
block to gthemes_dos.c so both paths publish; one source).

## Next steps
1. Rig 84 check (sonnet rig agent, purpose "vtcon S85: DOpus closewb reset check").
   Deploy build/amiga/Patina to Rushhours:Tools/UP-Term/bin/Patina and
   VTC:Apps/bin/Patina, restart Patina. With 2 DOpus listers open, apply a theme.
   PASS: PATINA_DEFERRED not incremented; pen indices change; BARTRIM = bar fill
   (no light line at y13); listers close and reopen with new pens; no IPrefs
   requester left. Risk: 3.1 IPrefs may need a click on its requester.
2. Scroller fix (not measured). On 3.1 DOpus knob is stock propgclass;
   FRAME_PROPKNOB only on Intuition >= 47.44. Rig showed knob and arrows 8D8D8D
   (pen 3, vantablack). Arrows use c2_get_pens p->knob = flat_knob_state_pen
   (flat.c:412, flat_core.c:1088). Compare DOpus vs a Workbench drawer in a
   blue-accent theme (1A1B26 414868 A9B1D6), active and inactive. Planned 3.2 fix:
   record frameiclass FRAME_PROPKNOB objects at OM_NEW (like img_note_new,
   flat.c:540), let smart_knob_of (flat.c:762) accept them; `make flat-budget`;
   unit test. Notes: s84-evidence/dopus-scroller-findings.md,
   thoughts/shared/plans/2026-11-09-dopus-scroller-look.md.
3. Commit by name, no AI attribution: patina/desk_dos.c, patina/win_core.c,
   patina/win_core.h, tests/test_desk.c, tools/rig/patina_theme_save_rig.py.
4. Owner eye check rig 84 (LOCAL): DOpus open, pick vantablack, then starship.
5. Push and release (memory vtcon-release-recipe).

Do not touch: thoughts/shared/research/test-register.md, worktree
agent-a81292c12d8643813, rig 81. Local main has unpushed 04603265.

## Update late 2026-11-15 (S85k, runs 4 and 5)
Status: theme apply with DOpus open still FAILS on rig 84; no fix found yet.
- Uncommitted, built (Patina 412908 bytes, deployed to rig 84): patina/wbreset_patch.c close_hook retries a failed
  original every 10 ticks for 2 s (WBR_RETRY_TICKS in patina/wbreset_core.h). Run 4: no help. Keep or drop after the
  blocker is known; likely drop.
- Run 4/5 evidence: thoughts/shared/s84-evidence/s85-reset-blocker.md sections "Run 4" and "Run 5". DOpus never closes
  its windows. In run 5 even a Retry click on the IPrefs requester did not make DOpus close (run 1 did, 0.5 s). No
  `dopus_closewb` process seen, but wasabid has a 0.04-1.4 s blind spot after a click.
- Ruled out: the Patina "Patina desktop theme" confirm (desk_confirm_dos.c) opens only after dk_wait_reset gives up
  (+6 s), so it is not the first blocker.
- Open: (A) a window fails DOpus check_closescreen (dopus5 Program/misc.c:441: not WFLG_WBENCHWINDOW, not a DOpus
  window, UserPort task not "« IPrefs »", or UserPort NULL), or (B) DOpus never runs its close (patch chain order,
  GUIF_CLOSE_PENDING stuck, DOpus main busy). wasabid cannot read Window->Flags / UserPort.
- Next step (decided, cheapest decisive probe): extend diag_census in patina/wbreset_patch.c. At the original call,
  for each window on the Workbench screen record the first one that breaks DOpus's rule (Flags lacks 0x02000000, task
  name does not start with "dopus"/"DOpus" and is not "« IPrefs »", or UserPort NULL): publish its title and task
  name via am_sentinel (PATINA_WBR_BLOCKER). Also publish whether intuition LVO -78 JMP target equals our gate or
  DOpus code (who is on top of the chain) and ipatch_saved(&g_pc). Build via buildq, deploy to rig 84, rig agent
  (sonnet, purpose "vtcon S85: closewb blocker census") reboots, opens 2 listers, runs `PATINA RUN 72`, reads sentinels.
- Rig 84 state: IPrefs System Request standing (do NOT click Cancel), DOpus + 2 listers, Patina bar and Keep|Revert
  confirm open. Needs `rig.py reboot --purpose ...` before the next run.
- Still unpushed local main (04603265..6b97f707, faf40ea0, 677af64a, 1bae5f75). Owner eye check pending (ping when a
  run PASSES). Ledger: thoughts/spot/todos.md rows S85k/S85l/S85m.
