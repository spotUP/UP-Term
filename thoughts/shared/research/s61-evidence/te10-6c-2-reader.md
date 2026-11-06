# TE-10.6c brief 2: ipd_screen_roles (worktree vtcon-te6c-2, branch te10-6c-2)
Commit: dc5968a3 (one local commit, not pushed).
Files: Makefile, amarchy/iconpen_core.c/.h, amarchy/iconpen_dos.c/.h, tests/test_iconpen.c, tools/rig/amarchy_iconpens_rig.py (docstring name only).

API: `int ip_screens_slot_of_screen(const ip_screens*, const void*)`; `int ip_holder_plan_copy(const ip_holder*, short*, uc_u32*, int cap)`;
`int ipd_screen_roles(const void *scr, ipd_roles *out)`; `ipd_roles` as briefed except `role[IPD_ROLE_COUNT]` (see deviation).
ipd_rec gained `ipd_slot_roles roles[IP_SCREENS]` {ok, screen, name[GT_NAME], role[7]}.

Answers:
- "Theme read succeeded": pens_theme_named and gt_load_model are void; gt_load always fills the gtheme (defaults on a missing file). So success = the call returned; ok=1 is set right after it for i >= 1. flat_role_rgb_of does NOT need t.model (reads gt_elem_rgb on the gtheme only).
- Locking: copied ipd_role_write (find_rec(0) + ObtainSemaphore(&r->ss), no list lock). Record is freed only by the last ipd_owner(off), which takes r->ss, so it lives while held. No DOS call inside.
- Layout: IPD_SEM_NAME bumped to "Amarchy.IconPens.6" (as .5 was for screen_applies); updated the pin in tests/test_iconpen.c, comments in iconpen_dos.h and the rig docstring.
- Makefile: flat_role_core.c added to AMARCHY_SRC, AMARCHY_MENU_SRC, AMARCHY_GUI_SRC, amarchy-flat (prereq + link), amarchy-icons (prereq + link). No missing symbols.
- Host harness for iconpen_dos: none exists; the reader has only a header-pin check in test_iconpen.c. Unverified at runtime: ipd_screen_roles itself (no rig by order).

Deviation: iconpen_dos.h cannot include flat_role_core.h: its FLAT_ROLE_TEXT/LINE/BACK collide with flat_glyph.h's enum (flat.c includes both; vbcc "redeclaration" errors). The header defines IPD_ROLE_COUNT 7; iconpen_dos.c has a compile-time typedef check == FLAT_ROLE_COUNT. Brief 4 (flat.c) must not include flat_role_core.h next to flat_glyph.h either.

Failing first: tests/test_iconpen.c:1185:19: error: implicit declaration of function 'ip_screens_slot_of_screen' [-Werror,-Wimplicit-function-declaration]

Checks: iconpen 2554 checks, 0 failed; gthemes 6953 checks, 0 failed; flat_role_core 48 checks, 0 failed; m68k 7 binaries rc=0;
flat-budget _c2_disp_reg 20/308/76, _c2_disp_stk 4/292/92 (rc=0).
Python guards pass: restore_original_wiring, flat_pens_per_screen, screens_page_name_guard, wb_lock_guard. tests/test_ipd_no_dos_under_list.py: 2 failures that exist on HEAD (pens_sync lacks IP_SYNC_NEED_THEME text; pens_theme lacks pr_WindowPtr since the pens_theme_named refactor); not fixed here.
Cleanup: scratch dir te6c2 116K left; worktree and build tree kept.
