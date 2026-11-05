---
date: 2026-11-05
topic: TE-8-STOCK-CONTRAST P1 - inactive title text gets its own dim floor 150 and a default pushed to it
verdict: fixed. GTC_DIM / GT_FLOOR_DIM 150 added; the default of title-text-inactive is pushed off the inactive bar in gt_resolve_rgb; 20 of 23 stock themes changed, 3 already read 150 or more.
---

# P1 inactive title text

## Invariant
The inactive title text is dimmed on purpose, so its pair is held to 150 (GT_FLOOR_DIM), not 220. The DEFAULT value (element has no own ref, so it is the Muted Text group) must read at least 150 on the inactive title bar. A value the model sets itself is returned as set and only warns under 150.

## Where the default lives
`gt_elem_defs[GTE_TITLE_TEXT_INACTIVE]` (gthemes_core.c:528) names group GTG_MUTED_TEXT. `gt_resolve` (about :893) falls back element normal, then group state, then group normal; `gt_resolve_rgb` evaluates the ref. Every reader goes through `gt_resolve_rgb`: the contrast table (`gt_contrast_pairs`), the pen budget (`gt_pen_budget`), `gt_elem_rgb`, the preview. Single place changed: `gt_resolve_rgb`, when elem is GTE_TITLE_TEXT_INACTIVE and neither the state nor the normal slot of the element holds a ref, the result goes through the existing `push_off(c, toward, bar, GT_FLOOR_DIM)` (toward black on a light bar, white on a dark one). No second push routine.

## Kind consumers (counted from the code)
`GTC_*` kinds are read in 2 places: the table `gt_cpair_def[]` (gthemes_core.c, pair 1 now GTC_DIM) and the kind-to-floor mapping in `gt_contrast_pairs` (one ternary, now with GTC_DIM). grep of amarchy/, tests/ shows no kind-to-label switch. gthemes_core.h: enum + GT_FLOOR_DIM + comment.

## Per-theme (old default, new, contrast on the inactive bar)
| theme | old | contrast | new | contrast | bar |
|---|---|---|---|---|---|
| catppuccin | 585b70 | 120 | bcbdc5 | 160 | 6c7087 |
| catppuccin-latte | acb0be | 103 | 686a72 | 152 | a6a9b6 |
| ethereal | 6d7db6 | 119 | 98a4cb | 152 | 76625e |
| everforest | 475258 | 141 | b5b9bc | 151 | 77766c |
| flexoki-light | b7b5ac | 122 | 5c5b56 | 153 | 94928b |
| gruvbox | 665c54 | 113 | b2ada9 | 154 | 756b5a |
| hackerman | 2d3450 | 192 | 2d3450 | 192 | 69757e |
| jade-horse | 64887b | 101 | b1c3bd | 150 | 6f7f78 |
| kanagawa | 54546d | 126 | babac4 | 160 | 747169 |
| last-horizon | 584e51 | 140 | bcb8b9 | 150 | 777777 |
| lumon | 304860 | 163 | 304860 | 163 | 6c7983 |
| lupine | 9e9e9e | 103 | 5f5f5f | 153 | 999999 |
| matte-black | 333333 | 168 | 333333 | 168 | 5f5f5f |
| miasma | 666666 | 101 | a3a3a3 | 150 | 6a6a61 |
| nord | 4c566a | 142 | c9ccd2 | 154 | 7a808c |
| retro-82 | 2a6b78 | 122 | a9c3c9 | 161 | 717066 |
| ristretto | 72696a | 110 | c6c3c3 | 156 | 7f7676 |
| rose-pine | cecacd | 115 | 676567 | 164 | b1acb9 |
| solitude | 4b4e55 | 126 | a5a6aa | 157 | 636667 |
| starship | 8e8e93 | 112 | c6c6c9 | 152 | 7d7d7f |
| tokyo-night | 414868 | 125 | a0a3b3 | 162 | 5a5e75 |
| vantablack | 7a7a7a | 137 | 878787 | 151 | 555555 |
| white | 808080 | 109 | 4d4d4d | 171 | 8d8d8d |

Unchanged (already at or above 150): hackerman 192, lumon 163, matte-black 168. Changed: 20 of 23.

## Side effect, tests
The pen budget counts distinct resolved colours, so the pushed default adds one colour: nord 9/27/3/3 became 10/26/4/4 (colours/shared/needing/fold, depth 3 and 4; text "4 of 10"), jade-horse 8/28/2/2 became 9/27/3/3 (depth 3 and 8; text "3 of 9"). The nord "1 warning as shipped" check is now 0. All counted from the failing output of the first run (19 failed), not guessed.

## Runs
- RED (fix disabled, FINAL test code): RUNTEST gthemes rc=2 [FAIL] gthemes: 6279 checks, 78 failed
- GREEN (fix, final test code): RUNTEST gthemes rc=0 [OK] gthemes: 6279 checks, 0 failed
- desk: RUNTEST desk rc=0 [OK] desk: 2311 checks, 0 failed
- m68k build (Amarchy amarchy-gui amarchy-menu amarchy-index amarchy-icons amarchy-flat upprefs): rc=0

## Drawing code (not changed)
The drawn inactive title text does NOT read the model element GTE_TITLE_TEXT_INACTIVE. flat.c:318-345 `c2_get_pens` uses `flat_inactive_pen` (flat_core.c:661-666): pen_inbar (the GR_INACTIVE bar tone, obtained from inbar_rgb) at depth 4..8, else pen_dimtext (muted text pen). Only `themeprev_core.c:21` (the editor preview) reads GTE_TITLE_TEXT_INACTIVE; grep over all .c/.h finds no other reader. So this fix changes the contrast table, the warning, the preview and the pen budget, not the live window. Whether the live inactive title text should follow the element is a separate question.
