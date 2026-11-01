# ICON-AA-WRONG-BG + ICONPEN-THEME-SWITCH-RECOLOUR (s29, branch s29-icon-aa)

Cause: ip_colours blended the AA ramp (FG/ACCENT/MUTED 33/66 %, spare slots) from desk_role(DK_BG), pen 0, the panel tone. Icons are
drawn on the Workbench backdrop pen, whose colour is desk_pen_colour(t, desk_backdrop_pen(), own) = desk_darker when flat owns the
backdrop, else the muted role. So edge pixels were blended toward a different (darker/lighter) tone than the backdrop: a fringe.

Fix: ip_ground() = the backdrop pen colour; ip_colours(t, ground, rgb) blends from it (ip_fold keeps the panel tone: it picks role pens).
ip_holder keeps the colours of the held pens; ip_recolour() re-reads the theme and SetRGB32s only the changed held pens (no release,
no obtain). ipd_recolour() (iconpen_dos.c) runs it under the public screen list lock; gt_dos_select calls it after desk_dos_apply.

Host test: tests/test_iconpen.c test_theme_switch_recolour (spare slot == backdrop RGB, not pen 0; switch A to B on an unchanged
screen rewrites 16 pens, sentinel c.sets == 16; second recolour pokes 0).

## Rig 14 (OS 3.2, this branch's build, flat OFF so the backdrop pen is the muted role)

Path: `amarchy-icons APPLY`, then the real theme pick (menu Style > Amarchy theme > row > Return, gt_dos_select -> ipd_recolour), `RAM:iconpens` read after each.
- vantablack (muted #7a7a7a): pen 44 (spare slot, ramp zero point) = 7a7a7a, pens 32-47 = 909090 bebebe bebebe 8d8d8d 808080 a7a7a7 868686 878787 7a7a7a ...
- white (muted #808080), picked live: pen 44 = 808080, pens 32-47 = 565656 000000 000000 6e6e6e 7a7a7a 2c2c2c 414141 747474 808080 ... (all rewritten, owners=6 unchanged, no reboot, no icon file rewrite)
- vantablack again: pens back to the first set byte for byte.
Before the fix pen 44 held the pen-0 panel tone (theme background), not the backdrop colour.
Needed for amarchy-menu (gcc build): it links iconpen_dos.c through gthemes_dos.c, so menu.c now defines and opens GfxBase.
