# RL8 steps 3-4: menu dim contrast and flat pointer

Status: code and host tests done; rig pixel proof OPEN (tool budget spent).

- gt_roles gains `dim`: t->muted pushed (push_off, 10 % steps) until contrast against bg >= GT_FLOOR_MUTED (220). GR_MUTED itself unchanged (desktop palette pins stay). menu_theme_roles_gui sets MT_DIM = r.dim.
- desk_pointer: colours from gt_roles_of (text, surface, accent); for depth-2 pointers plane1 &= ~plane0 so colour 3 (accent shading) is gone: one fill, one outline.
- Tests: tests/test_gthemes.c (MT_DIM floor over 23 themes, 11 failures when reverted), tests/test_desk.c test_desk_pointer (no colour-3 pixels in 41 chunks, 535 left when reverted; no drawn pixel lost).
- Gates: suites gthemes and desk green, make flat-budget PASS, make amiga builds.
- Open: rig 125 pixel measure of disabled menu item vs menu bg and pointer colours; test-register row.
