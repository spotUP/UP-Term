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

## Counter lines: real cause (s57 second pass)

AMARCHY_DESKS is `desk_dos_applies`, a per-process counter, published to the ENV: global only after a theme pick (leaf.c lf_gt_select). The global outlives the Amarchy process. The old baseline read the previous process's last value (3); a restarted Amarchy published 0 after the off pick and 1 after the on pick (3 -> 0 -> 1: not stale files, a stale global against a fresh counter). Fix in amarchy_desk_rig.py: delete the AMARCHY_* globals, QUIT and restart Amarchy before the baseline (baseline 0, every published value this run's own). WB_DEPTH constant removed, wb_depth() measured. pick(): second hotkey when the first after an Amarchy start is lost (run a timed out at open menu).

Runs on a cleared rig 89 (3.1), commit "second hotkey": run c 27 OK / 4 FAIL, run d 28 OK / 3 FAIL. Counter lines OK in both.
Remaining FAIL lines:
- off: no desktop prefs file changed (theme vantablack): ENV palette, wbpattern, font (run d also ENVARC palette, wbpattern, font) -- with desktop-theme = off, flat off, a theme pick still rewrites desktop prefs. NOT hidden, not yet diagnosed (product or Amarchy restart/boot restore; next: diff the bytes, read dk_orig_restore/Amarchy-start path desk_dos.c ~932).
- on: ENVARC:Sys/wbpattern.prefs unchanged, on: ENVARC:Sys/font.prefs unchanged (run c also pointer). Product: desk_dos_confirmed (Keep, R1.10, desk_dos.c:1235) copies ENV: to ENVARC: for every target pair, the rig pick() clicks Keep, so ENVARC changing is by design since 1c74d07d; the script expectation is stale. NOT yet edited (should become ENVARC == ENV after Keep).
3.2 run (rig 88) not done: budget. Rebase onto origin/main conflicts in test-register.md (aborted); rig started with --allow-stale-base.
