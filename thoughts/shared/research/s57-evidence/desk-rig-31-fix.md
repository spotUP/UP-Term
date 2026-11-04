# Desk rig on 3.1: script fixes (s56-deskrig, s57)

Rig 89, `--os 31`. Test-side fixes only; no product file touched.

Fixed in tools/rig:
- Setup root cause: `flat_prefs_on()` in amarchy_gui_rig.py treated an absent key as off; flat-system-look and flat-window-gadgets default ON (amconf_core.h). Absent now reads on. `--setup` passes.
- amarchy_desk_rig.py builds build/amiga/wbpens and dripens itself (make).
- Off step writes `desktop-theme = off`, `flat-system-look = off`, `flat-window-gadgets = off` and verifies the read-back; baseline snapshot taken after the write.
- 3.1: palette expected in the desk_palette_31safe form (desk_expect arg 31); ENVARC:Sys/palette.prefs must equal ENV:Sys's; fields Palette 40.2 USE rewrites (pen8[1], pen8[4], pen4[11], pen8[11], color[7]) skipped.
- 3.1 DrawInfo want (pens31 hunt, deb1a23f): desk_expect `pens <desk|flat> 31 <depth>`: pens >= 4 through desk_palette_pen, slot 1 = 1, slot 11 = 2, all 12 slots compared. Depth constant WB_DEPTH = 8.
- Font: desk_expect takes the icon-text setting (default plain = 0), no longer hardcoded outline.

Result run f (3.1, after setup): the DrawInfo-pens and font checks no longer fail. 27 OK, 6 FAIL lines (5 distinct):
- off: AMARCHY_DESKS did not rise: was 3 now '0'
- off: no desktop prefs file changed: ENV/ENVARC palette, wbpattern, font changed
- on: AMARCHY_DESKS +1: was 3 now '1'
- on: ENVARC:Sys/wbpattern.prefs unchanged
- on: ENVARC:Sys/font.prefs unchanged
Likely stale rig state: the previous run (open menu timeout right after setup) left Amarchy running with counters 3 and files written; run d on a cleaned rig showed only the ENVARC:pointer off failure. Not re-run: budget spent. 3.2 (rig 88) not run.
Next: clean ENV:/ENVARC: Sys prefs and ENV:Amarchy/prefs, restart Amarchy, run desk rig on a fresh rig twice (first run times out at open menu).
