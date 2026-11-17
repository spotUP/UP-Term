---
date: 2026-11-15
topic: S84 DOpus hide around Workbench reset + DOpus blue scrollers
status: in progress (uncommitted work)
---

# S84 handoff: DOpus hide around WB reset, DOpus scroller colours

Release (vtcon) still waits on: Workbench title bar FAIL after a theme apply (light line at y13), then owner eye check.

## State of the tree
- Committed (local main, NOT pushed): 04603265 "Fix Workbench title text drawn in muted grey after a theme apply" (BARTRIM = BARBLOCK in desk_core.c:89/476, goldens, tests).
- UNCOMMITTED:
  - patina/desk_dos.c: S84-DOPUS-HIDE, `dk_dopus_hide()` (~line 320) sends `command wait Hide` to DOPUS.1, polls dk_foreign_windows() up to 150 x DESK_IPREFS_POLL_TICKS; dk_apply calls it after dk_windows_close() when `reset`, sends `command Reveal` on the deferral path and before dk_windows_open(). Counter `desk_dos_dopus_hides` (not exposed as sentinel yet).
  - tools/rig/patina_theme_save_rig.py: wb_pens_check expects role order ('bg','dim','fg','accent'). Commit with the next fix.
- build/amiga built with the hide (make amiga rc 0).

## Rig 84 result (evidence: s84-evidence/rig84-hide-scroller.md, probe s84-evidence/probe4.py)
- vantablack pick: palette RGB applied, DOpus listers closed and reopened (new window addresses), BUT pen INDICES unchanged and PATINA_DEFERRED=1: the reset was STILL deferred. So BARTRIMPEN stays 2 (pen map `0 1 2 0 0 3 0 0 2 2 0 2`), which is the light line at y13 (BEBEBE = pen 2). The 04603265 BARTRIM fix never lands while the map change is deferred.
- Open question (next step): WHY still deferred after Hide. Candidates: (1) a non-DOpus foreign window on the WB screen (rig shell/CLI, probe window) counted by desk_window_foreign; (2) Hide not finished within ~3 s; (3) DOpus keeps a window after Hide (button bank, app window). Measure: dump WB screen window list (title, flags) right before desk_defer_map, plus desk_dos_dopus_hides and the poll count, as sentinels. Fix depends on which.
- Harness: probe4 timed out on "reopen theme list"/"open menu" so select 2 (starship) not tested. `lister query all` returns rc 20 on DOpus 5.104; count listers from the window list.
- rig.py bug seen: `rig.py rtg` relaunched the rig with a stale purpose ("vtcon-05: DOpus 5.104 install ...") from the dopus install step. Not fixed.

## DOpus blue scrollers (owner report: knob/arrows blue = window border colour; want lighter grey than bg like Workbench)
- Source cause (s84-evidence/dopus-scroller-findings.md): DOpus lister scroller = boopsi propgclass (GTYP_CUSTOMGADGET), PGA_KnobImage = frameiclass FRAME_PROPKNOB (dopus5 lister_dual.c:1010-1090, common.h:143-170). Patina smart_knob_of (flat.c:762-775, test flat.c:770) needs GTYP_PROPGADGET + GadgetRender/NextImage, so it misses; stock frame draws FILLPEN.
- Rig counters: frameiclass draws 42, flat only 4 (knob not recognised); clone draws rise on arrow scroll with fallback 0 (arrows go through c2_draw); track 000000 ok. Pixels in vantablack: knob AND arrows = 8D8D8D = FILLPEN, theme knob pen 233 = 1A1A1A. So arrows ALSO draw FILLPEN although recognised: check c2_get_pens/flat_knob_state_pen (flat.c ~412) for the arrow pen choice on these images.
- Planned fix (Patina layer, no DOpus fork): record FRAME_PROPKNOB frameiclass objects at creation (like img_note_new for arrows), let smart_knob_of accept them; fix arrow pen; unit test; `make flat-budget`. Confirm with a theme whose FILLPEN is blue.

## Next steps in order
1. Measure why the reset is still deferred (sonnet rig agent, rig 84, DOpus already installed there; purpose text "vtcon S85: why reset deferred after DOpus Hide"). Fix, host test, buildq make amiga, rig recheck (PASS = pen indices change, BARTRIM = bar fill, no y13 line, listers back).
2. DOpus scroller knob + arrow fix as above.
3. Commit by file name (desk_dos.c, patina_theme_save_rig.py, scroller fix), no AI attribution. Owner eye check: rig 84 LOCAL, DOpus open, pick vantablack then starship.
4. Push + release per memory vtcon-release-recipe.

## Other open ledger items
thoughts/spot/todos.md: S84-DOPUS-HIDE, S84-DOPUS-SCROLLER, rig checker raw-hex vs derived role colours.
Partyline audit (AmiExpress doorserver) is a separate session: start prompt in ~/Code/AmiExpress/amiexpress-doorserver/thoughts/spot/next-session-partyline.md.
Do not touch: thoughts/shared/research/test-register.md, .claude/worktrees/agent-a81292c12d8643813, rig 81.
