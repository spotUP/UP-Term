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
- (superseded by the Rig 22 section below) Expected: backdrop pen 7 RGB = darker tone, pen 5 = muted, pen 4 vs pen 0 >= 1.5, icon ground = backdrop. wbpens already prints RGB of pens 0-7 and top-4..top at depth > 4; icon ground pen is 32 + IP_SPARE12 slot (read via iconpens).
- Muted vs selection readability guard: not cheap, not done.
- Depth 2 shares pen 1 with stock icon outlines.

## Rig 22 check (fs-uae, 3.2, AGA 256 colours depth 8, branch rebased on origin/main b2e0aabf, flat-system-look and flat-window-gadgets on, FP-1 Keep clicked after each pick, amarchy-icons APPLY done)
Host after rebase: desk 2033/0, iconpen 2113/0, make amiga rc 0. Contrast below = desk_contrast (the guard's metric: (luma_hi+13)/(luma_lo+13)); WCAG ratio in brackets.

desktop-theme OFF (flat only; the backdrop pen is "own"):
- vantablack: pens 0-7 000000 222222 bebebe 8d8d8d 1a1a1a 7a7a7a b6b6b6 222222. Pen 7 = pen 1 = darker tone PASS; pen 5 = 7a7a7a = theme muted PASS; pen4 vs pen0 = 3.00 (WCAG 1.21) PASS on the guard's metric (selection #1a1a1a already passed, not stepped); icon ground pen 44 = 222222 = pen 7 PASS.
- white: pens 0-7 ffffff e8e8e8 000000 6e6e6e 9a9a9a 808080 3a3a3a e8e8e8. Pen 7 = darker e8e8e8 PASS; pen 5 = 808080 muted PASS; pen4 vs pen0 = 1.60 (WCAG 2.81; theme selection c0c0c0 stepped to 9a9a9a) PASS; icon ground pen 44 = e8e8e8 PASS.

desktop-theme ON (pattern themed, so `own` = 0 in desk_dos_apply): pen 7 = pen 5 = muted on both themes (white 808080, vantablack 7a7a7a), icon ground = that muted tone. This is the design (desk_pen_colour: the themed pattern keeps its muted tone), not a regression, but the backdrop is NOT the darker tone then. Other pens in that run: white pen4 9a9a9a (1.60 guard metric); vantablack pen4 1a1a1a (3.00).

Odd: vantablack pen 7 = 222222 although the theme darker_background is 070707 (desk_darker falls back when the theme value is too close to bg).
