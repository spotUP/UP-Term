# TE-9.1 rig run 2 (build b4fb736e, te9-1): NOT GREEN, asserts not reached

Rig 170 os32 060 --max (start needed --allow-stale-base: origin/main moved to d79cf2bd). Host load fine on the freezes (only other rigs, one pre-push vitest during the third boot).

## Runs
1. Script without `amarchy_gui_rig.py --setup`: "step: menu path Settings > Theme Editor not reachable"; asserts 1-4 [FAIL] (file MISSING, sentinel None, pixel #000000). Cause: Amarchy not running from VTCX:Apps/bin; the script needs --setup first.
2. After --setup (ok: "Amarchy runs from VTCX:Apps/bin, port AMARCHY answers"; FAIL "flat prefs off, amarchy-flat BOOT left nothing resident, STATUS: flat=on calls=0 chained=0", same as run 1): steps as run: Edit tokyo-night (child 4, Base Theme: tokyo-night), Presets and Files (child 16) > Duplicate Theme... te-test ("copy present: True"), Edit te-test (child 4), Apply to Desktop (child 8, AMARCHY_SEL=8). Sentinel AMARCHY_EDIT_APPLIES before = None (even though leaf.c:332 publishes it after the apply). Next step walk Settings failed once, then the menu reopened to Theme Editor, then "auth failed: the Amiga took the connection but its daemon did not answer for 150 s - frozen, or stuck on something". Same point as run 1: guest frozen right after Apply to Desktop. Later ami.py calls: daemon still silent (30 s, 15 s).
3. Fresh rig boot, --setup: the guest froze during setup itself (TimeoutError after "ok setup: the earlier amarchy-flat resident unloaded on Break"); rig stopped and cleared (no third script run; tool-call budget reached).

## Asserts
1 [NOT RUN] te-test.theme holds background=#ff8800 and amarchy-theme = 2 (Save never reached; copy file exists after Duplicate)
2 [NOT RUN] both prefs name te-test
3 [NOT RUN] sentinel after Save = before + 1 (before = None)
4 [NOT RUN] title strip #ff8800 (swatch step never reached)

## Findings
- Apply to Desktop on 3.2 freezes the guest (daemon silent 150 s) in both runs; sentinel AMARCHY_EDIT_APPLIES never readable before it. Candidate: the apply writes the palette (cf. s32-evidence/31-freeze-culprit.md, 3.1 palette.prefs write freezes the guest). Not hunted.
- The guest also froze during --setup after the amarchy-flat Break on a fresh boot (once of two fresh boots).
- No dialog or requester text was captured (daemon silent, no screenshot possible).
- Script needs amarchy_gui_rig.py --setup before --case te91; it does not call it.
