# S85 DOpus closewb rig check (rig 84), 2026-11-15

Build: build/amiga/Patina 412540 bytes deployed to Rushhours:Tools/UP-Term/bin/Patina and VTC:Apps/bin/Patina (rig had 412772 before).
Keymap: ENV:up-term/keymap.user (ENV only) with super+ctrl+f9 = gtheme:vantablack, super+ctrl+f10 = gtheme:starship. Patina restarted (74 rows).
DOpus 5 on the Workbench screen, 2 Device List listers. Logs: s84-evidence/s85-run{1,2,3,4}.log. Scripts: scratchpad step.py/step2.py.
Tools on rig T: dripens, wbpens (from build/amiga).

## Runs
- Run 1 (F9 vantablack, no Keep click): screen title went Directory Opus -> Workbench Screen at +3s with "Patina desktop theme"
  (Keep/Revert) window; at +15s DOpus screen back. Pens/palette back at tokyo-night values: the unanswered requester reverted.
  PATINA_DEFERRED not set. No IPrefs requester after this run's revert at +18s, but ...
- Run 2 (F9 again, Keep clicked at once): start state was mid-revert (palette 7F007F garbage) with an IPrefs "System Request"
  (Retry|Cancel) open on the DOpus screen. After the Keep click PATINA_DEFERRED=1 and the requester stayed 18+ s. Cancel clicked by hand cleared it.
- Run 3 (F9) and run 4 (F10 starship), Keep clicked at 0.1-0.4 s after the requester showed: PATINA_DESK_APPLY_R=15, CONFIRM_ASKS rose,
  PATINA_DEFERRED stayed 1, no IPrefs requester, DOpus listers stay. But pens/palette did NOT change: DrawInfo rgb stayed
  1A1B26 414868 A9B1D6 ... (tokyo-night) for both vantablack and starship; ENV:Sys/palette.prefs not rewritten (mtime unchanged),
  PATINA_THEME stays tokyo-night; only ENV:Patina/prefs patina-gui-theme changed (now starship). WBPattern.prefs was rewritten.

## Verdicts
- PATINA_DEFERRED not incremented: PASS in runs 1,3,4 (no defer); FAIL in run 2 (0 -> 1, after the unanswered revert left an IPrefs requester).
- Pens/palette changed: FAIL/UNKNOWN. No change on the hotkey path in runs 3,4 (cause not found; gtheme: sets patina-gui-theme and
  desk apply returned 15 but palette.prefs was not written; suspect the apply uses the terminal theme tokyo-night or a no-op on identical palette).
- BARTRIM = bar fill: UNKNOWN (no pen change achieved; slot 10 == slot 0 in tokyo-night; vantablack never took).
- DOpus listers closed and reopened: PARTIAL. Run 1 showed the screen close and reopen (~12 s, DOpus returned) with listers; no pen change to confirm.
- No IPrefs requester left: FAIL in run 2 (Retry|Cancel stuck, needed Cancel); not seen in runs 1,3,4.
- Finding: the hotkey/revert path leaves the rig in a mid-state (palette 7F007F) if the 15 s requester is not answered.
