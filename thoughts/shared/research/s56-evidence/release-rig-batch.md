# Release rig batch, rig 113, OS 3.2, fresh-all from origin/main 7aaecfb6 (2026-11-04)

Budget ran out before every row was measured. Rows below say exactly what was and was not measured.

| Check | Verdict | Evidence |
|---|---|---|
| fresh-all install | PASS | installer_rig.py fresh-all finished, deps md5 current |
| STRUT-REFLOW-RESTORE (strut_prebar, BAR OFF part) | PASS | opened [20,40,600,216], bar top 246, clamped 1 -> 2, after BAR OFF [(20,40,600,216)], want 20 40 600 216 |
| strut_off | FAIL (probably rig helper, unconfirmed) | off box None, root [('',256)], on box None (bar top 246); the opener returned no BOX in both states, so the BAR OFF / BAR ON reflow was not measured. Look at opener() and root_ends() in tools/rig/amarchy_bar_rig.py (r_strut_off lines 291-312) before blaming the product |
| S56-LOCALE-SAVE-FILL | CITED, not re-run | verify-run-87.md: Save box 12..99 x 181..194 = 1148 px 1a1b26, 84 px a9b1d6; main since changed only rig scripts and docs |
| FLAT-PENS-NOT-LIVE | pen read only, NOT a visual PASS | amarchy-flat STATUS flat=on calls=350; dripens SHINEPEN=BACKGROUNDPEN=0 |
| FLAT-BUTTON-FILL | NOT MEASURED | btnbgprobe GAD ran but T:btnbg.out was locked ("object is in use") and unreadable; screenshots ~/Desktop/upterm-shots/rel-batch-btnbg.png, rel-batch-btnbg2.png taken without box coordinates. Expected FAIL against owner spec (lighter_background): flat_button_pen amarchy/flat_core.c:560 returns pen_back (pen 0) when pen_sel is unset at depth 8; design question |
| R1.17-WBRESET | NOT RUN | needs amarchy_gui_rig.py --setup then amarchy_desk_rig.py |
| AM-FLAT-WB icon border (3.2) | NOT RUN | needs desk rig (dk_wbborder, amarchy/desk_dos.c) |
| DESK-CHECK-INVERTED, AM-FLAT-WB 3.1 | NOT RUN | 3.1 batch |

## Batch 2 (rig 115, 3.2, fresh-all install, tested commit ed33fb2f = fix/strut-off-helper merged with origin/main; 2 commits behind origin/main, RIG-STALE-BASE overridden)

- fresh-all installer (all ticked, INSTALLER_KEEP=1): PASS (0 of 1 failed).
- strut_off: PASS after helper fix. Root cause: r_strut_off never staged RAM:struttest (opener gave BOX None). Fix: strut_put() in tools/rig/amarchy_bar_rig.py, branch fix/strut-off-helper c5216c4e (pushed). Product not implicated. Numbers: off box [100,106,200,150] root 256 clamped 0->0; on box [100,96,200,150] bar top 246 clamped 1.
- FLAT-BUTTON-FILL: measured, owner decides. amarchy_btnbg_rig.py depth 8: idle fill pixel (mid, row 3) 8d8d8d, held fill bebebe; pattern pens {0:000000,1:7a7a7a,2:bebebe,3:8d8d8d}; background samples (60,70)(61,70)(60,71)(60,83) all bebebe. Idle fill 8d8d8d (pen 3) is NOT the bebebe window background; rig row "a button is drawn (centre differs from pattern)" FAIL. Corner pixels carry background pen idle and pressed (PASS); press changed 631 px (PASS). Workbench primary-button comparison NOT measured (budget). Box coordinates are in the btn.log scratch output (not kept).
- R1.17-WBRESET / AM-FLAT-WB icon border: NOT VERIFIED. `amarchy_gui_rig.py --setup` FAILED ("flat prefs off, amarchy-flat BOOT left nothing resident, STATUS: flat=on calls=16 chained=0"), so amarchy_desk_rig.py ran on a bad setup: 12 failed (AMARCHY_DESKS not rising, no desktop prefs changed, T:amarchy-revert missing, palette pens 0..10 mismatch, WBCLOSES). Treat as setup failure, not a product verdict. Needs a rerun after the setup problem is understood.
- FLAT-PENS-NOT-LIVE visual pixel check: NOT RUN (budget).
