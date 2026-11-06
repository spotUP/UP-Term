# TE-ROWS-255-CUT: AMARCHY_ROWS cut at 255 characters

## Q1: where is the cut
The two caller buffers, nothing else. amarchy/gui.c:190 `char b[256]` (sentinel_rows) and amarchy/menu.c:347
`char rows[256]` each passed `sizeof` to mn_rows_join (amarchy/menu_core.c:37), which stops before a label that no
longer fits whole (`used + len + 2 >= n`, line ~45): 255 usable bytes. SetVar (gui.c) and am_sentinel_str
(sentinel_dos.c:17, a plain SetVar) pass the string through uncut. Readers not checked on the Amiga (UNVERIFIED).

## Q2: largest joined length
MN_MAX_ITEMS 64 (menu_core.h:10) rows, MN_LABEL 96 (menu_core.h:28, so 95 characters + NUL) =
64 * 95 + 63 separators = 6143 characters, 6144 with the NUL. Caveat: menu.c sizes a level by ix_level_cap
(index_core.c:427 = MN_MAX_ITEMS + KM_MAX_MENU + index apps + packages), so an index level with many apps can exceed 64 rows;
mn_rows_join then still stops at a whole label (never a half label). Not changed.
A label that holds '|' would be indistinguishable from a separator; mn_item labels are not filtered for it. Format unchanged.

## Fix
`#define MN_ROWS_MAX (MN_MAX_ITEMS * MN_LABEL + 2)` = 6146 in menu_core.h, used by both callers as `static char [MN_ROWS_MAX]`.
Static is safe: each is called from one task's loop (gui sentinel_rows, menu's level build), never re-entered, the join is
finished before SetVar returns. It keeps 6 KB off the stack. `__stack`/`$STACK` untouched: gui 65536, menu 262144 (grep of built programs).

## Failing first
- tests/test_amarchy_rows_buffer.py on the unfixed code:
  `FAIL: gui.c:190: char b[256];` and `FAIL: menu.c:347: char rows[256];` (rc 1); passes after the fix.
- tests/test_keymap.c `the_rows_join_buffer_holds_a_full_page_whole` (64 rows, join > 255, last label whole; worst case all
  95-character labels; MN_ROWS_MAX covers it). On the unfixed code it did not compile (MN_ROWS_MAX undefined); not run with a
  256 buffer separately.

## Checks
- `make test-one SUITE=keymap`: 21375 checks, 0 failed (4 checks new; before = 21371 by subtraction, not run).
- Amiga build of amarchy-gui, amarchy-menu, Amarchy: rc 0.
- Cookies: amarchy-gui `$STACK: 65536`, amarchy-menu `$STACK: 262144`.
- Runtime on the Amiga: UNVERIFIED (no rig run).
