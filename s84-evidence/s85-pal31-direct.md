# S85 3.1 palette direct-write check, rig 84

Build: build/amiga/Patina on host is 413460 bytes, md5 053479e18b1572bb804aacc03243d818 (NOT the 413436/75edcd15... named in the brief; newer, 13:27). Deployed that one; rig size 413460.
Rig 84 needed stop+start once (first start timed out at wasabid).

1. Live DrawInfo (T:dripens Workbench, deployed from build/amiga/dripens): pens 0 1 2 0 0 3 0 0 2 2 0 0 -> pen10 = 0, pen11 = 0 (rgb 1C1C1E both). PASS. Read after starship cycle and after nord repeat run.
2. ENV:Sys/palette.prefs vs T:patina-desk/palette.prefs (work.py): identical 4-colour and 8-colour pen lists
   4: 0 0 2 0 0 3 0 0 2 2 0 0 ffff 0 ; 8: 0 fefd 2 0 0 3 0 0 2 2 0 0 ffff 0 (same in both files; entries 10,11 = 0,0). PASS.
3. rp_run.py 1 cycle (starship, nord, starship), press to Keep requester seen: 0.81 s, 0.87 s, 0.75 s. PASS (<2 s). Log: scratchpad/fix_run3.log
4. PATINA_DESK_PUT_SKIPS: 4 after cycle; repeat nord presses x3: 4->5, 5->7, 7->9. Bumps on repeat (+2 per press). PASS.
   (ARexx 'GTHEME x' returns rc 10; used keymap row gtheme:nord + key, as rp_run does.)
Other: PATINA_DESK_PAL31_FIXES = 0; PATINA_DEFERRED unset; RAM:pat.log only "[CLI 3]".
Requesters: none; final screen windows: 'Patina bar' only. keymap.user deleted, Patina reloaded. Rig 84 left running.
