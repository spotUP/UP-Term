# TE-10.6c brief 4a: role-pen enum renamed FLAT_ROLE_* -> FLAT_RP_*

## Failing first (tests/test_flat_role_enum_names.py, before the fix)
- G-c: AssertionError: '#include "flat_role_core.h"' not found in amarchy/flat.c
- G-a: Lists differ: ['FLAT_ROLE_BACK', 'FLAT_ROLE_LINE', 'FLAT_ROLE_TEXT'] != []
- G-b: hits amarchy/iconpen_dos.c, amarchy/iconpen_dos.h
- Ran 3 tests, 3 failures. After the fix: OK.

## Files changed (renamed uses)
- amarchy/flat_role_core.h 7 lines (enum, DRI enum, comments), flat_role_core.c 18, tests/test_flat_role_core.c 42 (+1 stale comment reworded)
- amarchy/iconpen_dos.h: IPD_ROLE_COUNT -> FLAT_RP_COUNT (2 uses), define and its comment removed, includes flat_role_core.h
- amarchy/iconpen_dos.c: compile-time check typedef and its comment removed (its FLAT_ROLE_COUNT use gone with it)
- amarchy/flat.c: +#include "flat_role_core.h"
- Makefile: guard registered (TEST_PY_ONLY + test-py line, same form as ipd_no_dos_under_list). test_flat_pens_per_screen.py is registered nowhere (grep Makefile, RULES.md, tools).
- The DRI enum (FLAT_ROLE_DRI_TEXT/SHADOW/BACKGROUND/PENS) lives in the same header; renamed to FLAT_RP_DRI_* for consistency (no collision existed).

## Left unchanged
- flat_core.h:378 has a THIRD enum named FLAT_ROLE_* (PRIMARY, SECONDARY, PLAIN: button roles). No collision with either other enum; used by flat_core.c (3), flat.c (3), roundaa_core.c (2), flat_glyph.c, tests/test_flat_core.c.
- Glyph enum users: flat_glyph.c/.h, test_flat_core.c, desk_core.h and gthemes_core.h comments.
- Include guard AMARCHY_FLAT_ROLE_CORE_H untouched.

## Checks
- flat_role_core 73 checks 0 failed; flat_core 2705; iconpen 2554; gthemes 6953 (all 0 failed)
- test_flat_pens_per_screen OK; test_flat_role_enum_names OK
- test_ipd_no_dos_under_list FAILS 2 tests (pens_sync / IP_SYNC_NEED_THEME). Pre-existing: same 2 failures with HEAD's iconpen_dos.c. Not touched.
- make -j2 of the 7 Amiga binaries: rc 0 (flat.c compiles with both headers)
- make flat-budget: _c2_disp_reg own frame 20, deepest chain 308, headroom 76; _c2_disp_stk own frame 4, deepest chain 292, headroom 92
