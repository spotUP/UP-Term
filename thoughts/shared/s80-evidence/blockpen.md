# BLOCKPEN pen-reset hunt (S81, branch s81-blockpen)

## Cause (proven on rig 8: os 32, AGA 256 colours, depth 8, Palette editor of OS 3.2)
Two separate mismatches made `desk_pens_reset()` true forever:
1. OS 3.2 never adopts BLOCKPEN (slot 1). Patina's 8-colour palette.prefs (pens 0 5 2 2 5 3 0 0 2 2 0 5 2) was USEd through
   `SYS:Prefs/Palette FROM T:bp.prefs USE` with slot 1 set to each of 5, 0xFEFD, 253, 1, 0 (the file ENV:Sys/palette.prefs after the
   USE holds 5 -> -259 (0xFEFD), 253 -> -11, 1 -> 1, 0 -> 0, i.e. the file does take the value):
   live DrawInfo pens after every one of them = `0 1 2 2 253 3 0 0 2 2 0 253`. BLOCKPEN reads back 1 whatever is written. The screen did
   reset each time (address changed 100AE698 -> 100521A8 -> 100517B0 -> 100103A8), so this is not "no reset": the OS ignores/forces the slot.
   Answer to the candidates: (b), not (a); no pen-map value reaches live BLOCKPEN, so the map cannot be fixed on the Patina side.
2. `desk_pens_reset` compared the raw map (pen 5) with the live pens, but on a screen of 4+ planes IPrefs resolves map pens 4-7 to the last
   four screen pens (5 -> 253, `desk_palette_pen`, the WB-TITLE-GREEN rule). Slots 4 and 11 (value 5) read 253 live: another permanent mismatch,
   hidden behind the BLOCKPEN one. (At depth 2/3 the old compare was right, which is why the unit tests passed.)

WB-TITLE-GREEN / FILENAME-PEN are untouched: pen 5 stays in the written map (BARTRIM, SHADOW, BLOCK as before); only the reset verdict changed.

## Fix (patina/desk_core.c desk_pens_reset)
Skip slot DK_BLOCK; for depth > 3 translate wanted pens 4-7 through `desk_palette_pen(pen, depth)` before comparing.
Same commit: PATINA_FOREIGN probe removed from patina/desk_dos.c (dk_foreign_windows).
Unverified: BLOCKPEN on OS 3.1 / depth < 8 (skip is OS-wide; harmless there because BLOCKPEN always changes together with slots 4 and 11).
Pre-existing, not fixed: tools/rig/desk_expect.c fails to compile on main (`prefs_paths_patina` redefinition), so patina_desk_rig `expect()` is broken.

## Tests
tests/test_desk.c: `test_desk_pens_reset_deep` (new; depth-8 live map in force -> 0 reset and DESK_DEFER_NONE; flat map -> reset; moved
slot 2 or unresolved pen 5 -> reset), `test_desk_pens_reset` knob loop now expects slot 1 not to move the verdict.
## Results
- Fix in place: make test ONLY=desk -> [OK] desk: 2372 checks, 0 failed. make test ONLY=win -> [OK] win: 289 checks, 0 failed. make amiga rc=0.
- Fail-before (desk_core.c reverted to HEAD, tests kept): [FAIL] desk: 2372 checks, 3 failed:
  test_desk.c:601 knob loop (slot 1 moved the verdict), :617 desk_pens_reset(live,...,8)=1 want 0, :618 desk_defer_map=-2 want -1 (DESK_DEFER_NONE).
- Rig re-measure of a full theme pick (PATINA_WBCLOSES / PATINA_DEFERRED): NOT done. Patina did not start on rig 8 (build/amiga not installed there). Unverified live.
