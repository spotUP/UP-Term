# GUI-BROWSE-SCROLL (2026-10-28)

Question: does the drawn Browse fail to scroll on Down past the last visible row, or is only AMARCHY_ROWS stale?

## Code (amarchy/gui.c)
- Down -> key() -> set_sel(): mg_scroll(sel, top) moves g->top (host-tested in tests/test_keymap.c) and then draw_list(). Scroll runs; no product bug in the code path.
- sentinel_rows() (AMARCHY_ROWS) is called only from draw_full(), and it joins the WHOLE level (idx 0..count, cut at 256 bytes), not the drawn window. draw_list() (the scroll path) published nothing. So AMARCHY_ROWS cannot change on Down by design: observability gap, not a scroll bug.
- Fix: sentinel_cursor() publishes AMARCHY_TOP (first drawn row) and AMARCHY_SEL on every list draw and every selection move.
- Rig case: amarchy_gui_rig.py --list-scroll (Apps level, Down len+2 times, asserts SEL on last row and TOP moved).

## Rig 5 (2 runs, both stopped)
- Sentinel works: AMARCHY_SEL read 3 of 3 after Down past the end on the Apps level (3 rows).
- Apps (3 rows) and Settings (6 rows) are shorter than one window, so no scroll could be seen; Browse needs a staged feed (28 rows, pkg_install).
- UNVERIFIED on screen: that TOP moves on a long list. Run `amarchy_gui_rig.py --list-scroll <RootRow>` with a long level (default Style) or read AMARCHY_TOP in the Browse of the pkg_install feed.

## Long-level run
- Result: PASS. Packages > Browse of the 28-row pkg_tags feed (staged by local_feed + refresh_to_browse, as pkg_install): AMARCHY_TOP 0 -> 20, AMARCHY_SEL 27 (last of 28) after Down len+2 times. Down past the last visible row scrolls and SEL follows.
- Command: UPTERM_RIG=5 python3 tools/rig/amarchy_gui_rig.py --list-scroll Browse (rig 5, --max). Log: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/4882f64a-b46b-4894-85ec-a5daf2c65294/scratchpad/ls-run2.log
- Case fix: the first run failed (12 rows) because AMARCHY_ROWS is cut at 256 bytes and lists only a window; --list-scroll only walked root rows so Browse was unreachable. Now scroll_probe takes the feed row count, and Browse stages the feed.
- Parser: --help and the argument parse ran fine; no parser bug.
