# TE-10.5a: gt_screen_apply_plan (pure, host only)

Signature (amarchy/gthemes_core.h):

    typedef struct gt_plan_pen { short pen; uc_u32 rgb; short exclusive; } gt_plan_pen;
    #define GT_MASK_WORDS 8
    int gt_screen_apply_plan(const gt_model *m, int depth, const uc_u32 *free_mask,
                             int allow_owner_pens, gt_plan_pen *out, int cap);

- free_mask: GT_MASK_WORDS words, pen p = bit (p & 31) of word p >> 5 (256 pens). 1 = free (not the owner's).
- Returns the entry count written (never more than cap; extra entries are cut off, cap <= 0 writes nothing).
- One entry per colour gt_pen_budget calls "needing" (count == budget.needing; truecolour/exact = 0 entries).
- exclusive = 1: write rgb to pen (SetRGB32). exclusive = 0 (best): pen = gt_nearest_role_pen of the colour, no write.
- Exclusive pens are >= budget.held and < budget.pens, ascending: first pens in free_mask, then (only allow_owner_pens) the
  owner's pens. No pen planned exclusive twice. A colour with no candidate is best. Depth <= 3 has held >= pens, so every entry is best.

## Reused
- gt_pen_budget (counts, held, pens, exact), gt_nearest_role_pen, desk_role, gt_resolve_rgb. The "needing" walk was extracted
  from gt_pen_budget into the static needing_colours(), used by both (one source, no second table).
- Searched: gt_pen_budget / gt_budget / iconpen_core.h for an existing 256-pen set: none exists, hence the 8-word mask.

## Open points for part b (gt_dos_apply_screen)
- held is a count (lowest pens), not the real held set: at depth 5+ the pointer pens (17-19) and icon pens (8-15 / 16-23) are
  not contiguous with it. Part b should pass them out of free_mask (clear their bits) so they are never planned.
- Build free_mask from the screen's ViewPort ColorMap: pens the owner allocated (ObtainPen / ObtainBestPen) are 0 bits.
- Depth > 8 (hicolour/truecolour): exact, nothing planned.

## Results
- Red (stub returning 0 entries): RUNTEST gthemes rc=2 [FAIL] gthemes: 6647 checks, 56 failed
- Green: RUNTEST gthemes rc=0 [OK] gthemes: 6647 checks, 0 failed
- m68k build (7 Amarchy targets): rc=0
