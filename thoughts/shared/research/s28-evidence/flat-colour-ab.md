# s29 FLAT-COLOUR-OVERHAUL: backdrop pen and selection contrast guard

Branch s29-flat-colour. Host test evidence only; the rig 22 check is NOT done (see Open).

## Change
- Backdrop no longer uses pen 5 (muted). desk_pen_plan_for gives it pen 7 (DK_RED, drawn by nothing, no collision with the icon ramp 32-47) at depth >= 3, pen 1 (DK_DARKER) at depth 2. desk_wbpattern uses desk_backdrop_pen_at(depth). Under flat the backdrop pen holds desk_darker(theme); pen 5 is plain muted again. ip_ground() reads desk_pen_colour(t, desk_backdrop_pen(), own) so the icon ramp ground follows.
- desk_selection(): selection is stepped toward black (light bg) or white (dark bg) in 10 percent steps until plain and nibble-cut contrast vs bg >= 150 (1.5). desk_role(DK_SELECTION) returns it, so palette, live pens and the icon selection plate all follow.
- desk_dos.c: one line (palette entry for the backdrop pen).

## Host tests (nice 19, ONLY=<suite>)
- desk 2017/0, iconpen 2113/0, iconapply 137/0, flat_core 693/0, make amiga rc 0.
- Revert proof 1 (backdrop back to DK_MUTED): desk FAIL test_desk.c:514 x6, :518, :524 x4; iconpen FAIL test_iconpen.c:52 x7.
- Revert proof 2 (desk_selection returns t->selection): desk FAIL test_desk.c:523 x10 (23 shipped themes, selection contrast < 150).

## Open
- Rig 22 live pen read (flat ON, white and vantablack) not run: budget. Expected: backdrop pen 7 RGB = darker tone, pen 5 = muted, pen 4 vs pen 0 >= 1.5, icon ground = backdrop. wbpens already prints RGB of pens 0-7 and top-4..top at depth > 4; icon ground pen is 32 + IP_SPARE12 slot (read via iconpens).
- Muted vs selection readability guard: not cheap, not done.
- Depth 2 shares pen 1 with stock icon outlines.
