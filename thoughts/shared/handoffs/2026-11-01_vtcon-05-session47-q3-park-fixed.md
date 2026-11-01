---
date: 2026-11-01
session: vtcon-05 s47
status: phase done
---

# vtcon-05 session 47 handoff

## Done
- Q3-PARK fixed: e60396fc on feature/native-splits (pushed). Root cause: wb_tabs_park / wb_tabs_unpark guarded on `c->ntabs < 2` (tab slots), so one tab split into 2 panes returned early; the second pane process kept its public-screen lock and CloseWorkBench failed (IPrefs Retry/Cancel). screen_switch had the same slots-vs-panes guard. All three now use `tabslot_all(c->slots, c->ntabs)`. Regression test in tests/test_wbpark.c (source-text check). Rig6: 2 fresh boots, 1 tab x 2 panes, ScreenMode USE aga16 and back to aga256: PASS. Evidence thoughts/spot/term-findings/q3-park-s47-rootcause.md.
- DIST-USERDATA-FAILS: 63d08084 on main (pushed). All 5 failures were stale tests after e2c36886 / a276b6f5 / a2f4f97d / 42ac0a39; exemptions are exact paths. Findings thoughts/spot/term-findings/dist-userdata-s47.md.
- rig7 deleted (owner OK); rig6 kept, stopped.

## Open in vtcon-05 s43 queue
- AM-RIG-TEXTMENU-LEFTOVER: Amarchy session's row.
- s28 WASABI-SHORTHELP-MUI3: A1200 final check only.
- AMIBERRY-RIGS-GAPS: needs a rebuilt Amiberry rig and owner key tests.
- feature/native-splits is still a side branch; no merge to main planned.

## Gotchas
- `rig.py stop` once took over 60 s; a second stop finished it.
- Splits rig harness reboots the rig on its REFRESH step.
- Do not touch tools/rig/rig.py, thoughts/shared/research/s28-evidence/flat-visible.md, tools/rig/-.
