# S32-INACTIVE-BAR evidence

Plan: thoughts/shared/plans/2026-11-03-inactive-dragbar.md

## Revert proof

Seven fixes were reverted together in one build of build/vttest_host (a build
takes about 4 minutes under nice 19, one build per fix was too slow). Each
fix is attributable by its own failing check. Suites flat_core and gthemes
were run, then every fix was restored and the suites rebuilt and rerun.

Reverted:
1. gthemes_core.c gt_roles_of: `r->c[GR_INACTIVE] = t->bg;`
2. flat_core.c flat_frame_pen: inactive branch returns -1
3. flat_core.c flat_title_decide: inactive band_pen -1
4. flat_glyph.c: inactive TEXT/LINE glyph back to the role pen only
5. flat_core.c flat_edge_pen: always pen_outline
6. flat_core.c flat_scroller_pen: no inactive knob pen
7. flat.c STATUS dump: `" inbar="` renamed

Result with all seven reverted:

```
[FAIL] flat_core: 1927 checks, 12 failed
  FAIL tests/test_flat_core.c:1150: t.band_pen = -1, want 5          (title_decide inactive, fix 3)
  FAIL tests/test_flat_core.c:1151: t.text_pen = 5, want 0           (fix 3)
  FAIL tests/test_flat_core.c:1152: t.bg_pen = -1, want 5            (fix 3)
  FAIL tests/test_flat_core.c:1156: t.band_pen = -1, want 9          (fix 3)
  FAIL tests/test_flat_core.c:1157: t.bg_pen = -1, want 9            (fix 3)
  FAIL tests/test_flat_core.c:1158: t.text_pen = 5, want 1           (fix 3)
  FAIL tests/test_flat_core.c:1339: flat_frame_pen(&s, 4, 0) = -1, want 9   (fix 2)
  FAIL tests/test_flat_core.c:1373: flat_edge_pen(&s, 4, 0) = 5, want 9     (fix 5)
  FAIL tests/test_flat_core.c:1382: flat_scroller_pen(... active 0 ...) = 6, want 9   (fix 6)
  FAIL tests/test_flat_core.c:1362: strstr(src, "\" inbar=\");  put_num(line, &n, copy.pen_inbar)") != 0   (reachability, fix 7)
  FAIL tests/test_flat_core.c:1438: cv[0][0] == 'B' && cv[4][4] == 'T'   (flat_clone_draw inactive on-band, fix 4)
  FAIL tests/test_flat_core.c:1453: cv[4][4] == 'T'                      (fix 4)
[FAIL] gthemes: 1289 checks, 7 failed
  FAIL tests/test_gthemes.c:765: r.c[GR_INACTIVE] = 1710886, want 1193046   (explicit key, fix 1)
  FAIL tests/test_gthemes.c:772: r.c[GR_INACTIVE] = 0, want 4478310
  FAIL tests/test_gthemes.c:776: r.c[GR_INACTIVE] = 0, want 5921370
  FAIL tests/test_gthemes.c:804: r.c[GR_INACTIVE] = 1710886, want 5922421   (role table, default mix)
  FAIL tests/test_gthemes.c:804: r.c[GR_INACTIVE] = 0, want 5592405
  FAIL tests/test_gthemes.c:804: r.c[GR_INACTIVE] = 15725045, want 10922422
  FAIL tests/test_gthemes.c:804: r.c[GR_INACTIVE] = 16776432, want 9736843
```

## With all fixes in place

See the final lines appended below.

```
[OK] flat_core: 1927 checks, 0 failed
[OK] gthemes: 1289 checks, 0 failed
```

m68k builds (vbcc, -warn): amarchy-flat, Amarchy, amarchy-menu, amarchy-gui all link clean.
