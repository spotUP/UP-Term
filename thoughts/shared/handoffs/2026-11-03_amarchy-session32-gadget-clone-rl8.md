---
date: 2026-11-03
session: vtcon-16 (Amarchy session 32)
status: in progress
---

# Amarchy session 32: gadget clone root cause, RL8 flat colour

## Done
- RL8 step 1 (role table gt_roles_of in amarchy/gthemes_core.c): pushed c1068af3.
- S32-STALE-GADGETS cause 2 root cause found and fixed (592ac082, merged 921a1ec8, pushed).
  3.2 sysiclass (Kickstart 3.2.3, intuition 47.53) bakes imagery at OM_NEW from SYSIA_DrawInfo pens;
  IM_DRAW never reads imp_DrInfo. The clone now draws a private sysiclass image created with a static
  marker DrawInfo (dri_Depth 8), cached in a 16-slot table (flat_clone_slot), disposed in clone_free.
  Host test test_clone_private_slot (SUITE=flat_core). STATUS splits fallback by cause (busy/nodri/nobm/empty).
- Rig 96 folder cleared with owner OK (disk was 1.4 GB free, host build hit ENOSPC).
- S32-TITLE-STRIP fixed and pushed (6c34f5b9). Owner eye check still pending.
- S32-INACTIVE-BAR merged and pushed (27607459), plan thoughts/shared/plans/2026-11-03-inactive-dragbar.md.
  New theme colour `inactive` (GR_INACTIVE, gt_mix surface/text GT_INACTIVE_PCT), pen_inbar in flat_state
  (FLAT_VERSION 3, 32 bytes). Inactive bar, gadget boxes, arrows, knob and outer outline take pen_inbar;
  title text and glyphs take pen_ptext. STATUS prints ` inbar=`. Evidence s32-evidence/inactive-bar.md.
- S32-GHOST-MENU probes rounds 1 and 2 (254f98c6, 0967fc51; s32-evidence/ghost-menu*.md):
  Intuition ROM offset 102120 calls RectFill with AreaPtrn {0x4444,0x1111}, AreaPtSz 1, FgPen 2, JAM1.
  Items: rp->Layer NULL, off-screen menu bitmap (BytesPerRow 192). Titles: bar layer, Layer->Window NULL.
  Gate goes before the Layer/Window branch; replacement recolours non-background pixels to the dim pen
  (a solid fill erases the label).

## Open, in order (ledger thoughts/spot/todos.md: S32-STALE-GADGETS ~line 3839, RL8 rows)
1. Gadget rig proof on 3.2: PASS 2026-11-03 rig 18 (KS 47.115, draws 96/51, fallback 0 after install and reboot;
   thoughts/shared/research/s32-evidence/gadget-clone-proof-32.md). a017b5e8 was wrong (doubled flags), fixed 3880f642.
2. 3.1 rig check of the private-image path (not run).
3. RL8 step 2: DONE, on origin (c91ea918), host suites pass. Owner: palette entry 6 follows the theme.
4. Rig 96 owner eye-check (setup agent started 2026-11-03 from 3880f642): fresh full install from pushed main,
   UPTERM_RIG_FLAGS='--max --os 32 --idle-minutes 0 --purpose "<session>: owner eye-check full install (3.2)"', --keep on stop.
   Message vtcon-ee at start and end. Test Left Amiga+Space via `ami.py key 40 40` BEFORE any drift
   (ami.py gate() refreshes VTC: and reboots a drifting rig). Ask owner to check gadgets, pointer, title line.
5. RL8 step 3 (menu and bar), step 4 (icons, logo, flat pointer generated from the role table: owner chose "Generate from theme").
6. RL18, RL19-21.
7. Worktree agent-a57f03ea92143ba93 (unmerged fd04ceb3): owner approval needed to delete.
8. Rig 35 owner eye check of inactive bar + title strip (proof agent result: s32-evidence/inactive-bar-rig35.md).
9. S32-GHOST-MENU merged (49c3ac3b, proof 3a004133, rig 37 mhits 10). Refresh rig 35 to see it themed.
   Remaining ghost cases: 3.2 classes/ReAction, BltPattern, icon.library.
9a. S32-STALE-HELD-PENS (s32-evidence/pen-overwrite.md): after a WB reopen dim/knob/inbar pens keep old
   numbers (rig 35 pens 249-251 = icon colours 36-38). Fix: sonnet worktree agent (pens_sync returns the
   ip_sync action; new screen forgets shared pens without release and re-runs dim_refresh). Then rebuild all
   four binaries from main, update rig 35, reopen WB, reprove inactive band/ring = #5A5E75.
9b. DONE 525bc62f (STALE-HELD-PENS) + bd1c1ddc (S32-PEN-EPOCH-LEAK: epoch bumps only on a new screen identity).
   Rig 35 reproof after both (s32-evidence/rig35-reproof.md): inactive bar STILL #323F5F on a fresh boot, no 5A5E75
   in the palette. Cause open; round 3 trace agent (sonnet) appends "## Round 2" to rig35-reproof.md, also measures
   the dim disabled menu rows (proof b). Limitation: hold off + WB reopen at same address = no epoch bump.
   rig.py install pi-llmprobe `_strtod` FIXED 094c7a49 (-lmieee).
   ROUND 3 RESULT: colour maths right. 323F5F = boot-time stale pens 249-251 (icon client's colours);
   flat_pens_follow never re-obtained after the boot WB reopen. Break 6 D on rig 35 forced dim_refresh:
   inbar=243 = 5A5E75 measured (so rig 35 shows the right colour NOW, until the next reopen).
   NEXT (first task of next session): opus hunt + fix - identity screen address + ipd_epoch misses a same-address
   reopen when icon-pen hold is off. Ledger S32-INACTIVE-COLOUR. Then proof b: map Workbench menu rows with
   `ami.py menus`, hold right button on a known title, measure disabled rows (round 2 saw only 1A1B26/A9B1D6 in
   the dropdown: possible FAIL if that was the Window menu).
9c. OWNER EYE CHECK rig 35 (2026-11-03 evening): a) inactive bar PASS, d) disabled menu text PASS (proof b done).
   b) scrollbar knobs invisible (same colour as track). Owner decided S32-KNOB-STATE: knob = pen_outline (accent)
   in the active window, pen_inbar in inactive windows. Sonnet worktree agent implementing (helper in flat_core,
   test, four-binary build); merge + rig 35 pixel proof after it reports.
   NEW S32-ARROW-OUTLINE: clicking the scroller up/down arrows erases the outer outline next to them. Opus hunt on
   rig 47 (read-only), findings to s32-evidence/arrow-outline.md; fix after.
   Still open: pen identity independent of icon-pen hold (S32-INACTIVE-COLOUR follow-up).
9d. S32-KNOB-STATE DONE 1206a2c3 (pushed), rig 35 pixel proof PASS: active knob 7AA2F7, inactive 5A5E75, track 1A1B26
   (s32-evidence/rig35-reproof.md "Knob state"). Only amarchy-flat restarted on rig 35; other three binaries new on disk.
   S32-ARROW-OUTLINE FIX fb6aa600 (pushed): c2_draw refills the outer ring (flat_outer_ring_overlap) under a clone box
   that reaches the window edge. Host test fails on revert. Rig 35 proof PASS (ring 206/206 7AA2F7 after arrow clicks,
   inactive 5A5E75; rig35-reproof.md "## Arrow outline (fb6aa600)"). Rig 35 runs all four binaries from main. Owner eye check pending.
   Found: amarchy-flat did not autostart on fresh rig 47 (ledger S32-FLAT-NOAUTOSTART).
10. S32-UPDEMO-CDIR: fixed by peer (1b2abd22, proof 78b8a13d). Rig 35 keeps the old C:UPDemo dir until reinstall.
11. Knob brightness, front box filled vs hollow, pointer, Left Amiga+Space window, RL15 on rig 96.
12. AMARCHY-THEME-EDITOR gated until Amarchy + flat UI done. Decided: Simple + Advanced (Advanced sets each
    role, Simple groups them), live preview, apply on prompted Workbench reset, page with subpages in amarchy-gui.

## Gotchas
- Rig 96 hold: after the setup agent reports, fast-forward main to origin (644091de + f308aa4f fix `rig.py hold`),
  then `UPTERM_RIG=96 rig.py hold --hours 4` and verify `cat build/rig96/hold.pid` + `ps -p <pid>` alive.
- Disk tight: check `df -h /` before every rig start or build. Own rigs: stop + clear at once.
- Host test names have no test_ prefix: `nice -n 19 make test-one SUITE=flat_core`.
- One build/test on the machine at a time: check `LC_ALL=C ps -Ao pcpu,command` first.
- Agents overran budgets (43, 59, 33, 107 calls): keep briefs to one item, one rig, one proof.
- FLAT_VERSION 3: readers refuse a mismatch; all four binaries (amarchy-flat, Amarchy, amarchy-gui,
  amarchy-menu) must come from one build.
- amiga MCP tools default to rig 1: use `UPTERM_RIG=N tools/rig/ami.py`.
- Plain `rig.py stop` deletes build/rigN; `--keep` on rigs to keep. dist-base is blocked (upterm-ports kit-stage).
