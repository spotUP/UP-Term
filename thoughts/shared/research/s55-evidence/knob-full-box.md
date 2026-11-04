---
title: XCON-KNOB-FULL-BOX trace
---
# XCON scroller knob fills the box with 60 lines of scrollback

Verdict: the math is right; no defect found in the knob arithmetic or the gadget feed.

Chain: vt_scrollback_lines(w->t) -> vtwin_knob_sync (render/vtwin.c:2148) -> sbar_from_view (render/sbar.c:6)
-> host->knob = h_knob (handler/vtcon_handler.c:6443) -> sbar_gad_set (handler/sbar_gad.c:153) ->
SetGadgetAttrsA(PGA_Total, PGA_Visible, PGA_Top). The handler never sets VertBody itself; propgclass derives it.

sb=60, rows=24: total 84, visible 24, top 60, body = 24*0xFFFF/84 = 0x4924 (28.6%), not 0xFFFF.
Pinned by host test `make test-one SUITE=sbar` (sixty_lines_of_scrollback_in_24_rows_is_not_a_full_knob).

The knob is full (total==visible) only when sbar_from_view gets sb<=0 or alt screen (render/sbar.c:13), or
vtwin_knob_sync returns early (!w->t || !w->win || w->r.off || !host->knob, vtwin.c:2151), or never runs.

Rig 96 repro (rig96-knob-reproof.md): the 150 px window holds ~13 rows, so 60 lines must give scrollback ~47.
The note itself says "no scrollback ... presumed": it was not measured. Candidates, in order:
1. vt_scrollback_lines()==0: scrollback profile key "scrollback = 0" disables it (handler sb_size, vtcon_handler.c ~1593; -1 none).
   Check the rig 96 profile and `c->st.scrollback` (vtcon_handler.c:4919).
2. `type` output never reached the terminal grid as scrolling lines (cleared/alt screen).
3. vtwin_knob_sync not called after output (called per frame at vtwin.c:626 only when the frame path runs).
4. sbar_gad_set skipped by sbar_equal against a stale `shown` (sbar_gad.c:156), e.g. after reopen with shown_valid set.
Next probe (needs a rig, not done): read scrollback count and the PGA_Total/PGA_Visible of gadget id 0x5B10 via wasabi after the fill.
