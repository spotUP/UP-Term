# TE-7-SCREENFONT-TEST evidence

Test: test_screen_font_alone_and_icon_font_alone_each_replan_font_prefs (tests/test_desk.c, suite desk_start_apply).
Mapped font slots in desk_font_name: GT_FONT_ICON (WBFONT chunk) and GT_FONT_SCREEN (SCREENFONT chunk). Title, menu, shell: no font.prefs chunk, no case (TE-7-TITLE-FONT).

Unchanged product: `[OK] desk_start_apply: 56 checks, 0 failed`.

Mutation (a): SCREENFONT block of desk_font_name removed.
  FAIL tests/test_desk.c:1906: desk_start_prefs(DT_FONT, all, &t, 0, font_known, (void *)"topaz.font", fa, na, out, sizeof out) = 0, want 518
  [FAIL] desk_start_apply: 56 checks, 1 failed

Mutation (b): WBFONT font_chunk_set call replaced by r = 0.
  FAIL tests/test_desk.c:1910: (same call) = 0, want 518
  [FAIL] desk_start_apply: 56 checks, 1 failed

Case (c) (nothing differs, want 0) has no mutation run; it guards against an always-stale plan, shared with the existing test's "own bytes" check.
Note: a first (a) mutation `changed |= 0 * font_chunk_set(...)` stayed green because the chunk is still written and staleness is decided on bytes, not on the return value.
Product code restored with git checkout -- amarchy/desk_core.c.
