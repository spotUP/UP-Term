# ICON-AA-WRONG-BG + ICONPEN-THEME-SWITCH-RECOLOUR (s29, branch s29-icon-aa)

Cause: ip_colours blended the AA ramp (FG/ACCENT/MUTED 33/66 %, spare slots) from desk_role(DK_BG), pen 0, the panel tone. Icons are
drawn on the Workbench backdrop pen, whose colour is desk_pen_colour(t, desk_backdrop_pen(), own) = desk_darker when flat owns the
backdrop, else the muted role. So edge pixels were blended toward a different (darker/lighter) tone than the backdrop: a fringe.

Fix: ip_ground() = the backdrop pen colour; ip_colours(t, ground, rgb) blends from it (ip_fold keeps the panel tone: it picks role pens).
ip_holder keeps the colours of the held pens; ip_recolour() re-reads the theme and SetRGB32s only the changed held pens (no release,
no obtain). ipd_recolour() (iconpen_dos.c) runs it under the public screen list lock; gt_dos_select calls it after desk_dos_apply.

Host test: tests/test_iconpen.c test_theme_switch_recolour (spare slot == backdrop RGB, not pen 0; switch A to B on an unchanged
screen rewrites 16 pens, sentinel c.sets == 16; second recolour pokes 0).
