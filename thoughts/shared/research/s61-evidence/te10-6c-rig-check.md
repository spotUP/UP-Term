# TE-10.6c hooks rig check (rig 173, OS 3.2, e3f6c456), 2026-11-06

Verify-only. Result: the precondition never held (screen A was not themed), so C1, C2 (themed part), C4-C7 are UNVERIFIED. Nothing was fixed.

Setup: `make wasabi`, `rig.py start --max --os 32` (start log: `[WARN] the Workbench stayed PAL (W43)`), `amarchy_gui_rig.py --setup`
(known `FAIL setup: flat prefs off ... STATUS: flat=on calls=0`). Rules written to ENV:Amarchy/screen-themes:
`default all / TE10A = gruvbox / TE10B = exclude`. Screen A = `RAM:pubscreen TE10A DEPTH 4`.

Freeze finding: a SECOND `--setup` (Break of amarchy-flat + Amarchy, then Run Amarchy) after screen A had been opened and
Broken made the guest vanish (wasabi refused; guest back with a fresh process list = CPU reset). Same shape as the rig 171
hunt (Exception 3 on Run Amarchy after Break). Host load then: tsc 122%, fs-uae 27% (not starvation). A third setup on the
rebooted guest worked. Not hunted.

Measured, screen A, rules file present before Amarchy started (setup 3):
- STATUS `pens-held 16 refs=1 screens=0 applies=2` after 15 s and after a close/reopen of A: no non-Workbench screen held.
- A pixels (raw rows x 40..340, y 30..180): colours 1a1b26 (39417 px), 7aa2f7 (4761), a9b1d6 (822) = the Workbench theme
  (tokyo-night). Button face (110,57) = 1a1b26, window border 7aa2f7. Not gruvbox.
- Workbench md5 before A: 6a6610a89660.
- amarchy-flat `calls` moved 2 -> 32 -> 85 while A opened/redrew, but with screens=0 that is the Workbench path, not ipd_screen_roles.
- Side note: A's screen title read back as garbage on the second open (first open 'TE10A'). Not checked.

C3 (code only): flat_dopus_decide / flat_frame_decide (flat_core.c:194-211) return SKIP only when enabled, not held and
pen_shine == pen_back; a stock new-look DrawInfo has SHINE != BACK so the gate does not fire there. A's real DrawInfo pens
were not read (no themed screen, no probe run): UNVERIFIED on the rig.

C7: `VTCX:DOpus5` absent on the rig: UNVERIFIED. C8: not reached.

Open question: why `screens=0` with a matching rule (candidates: ENVARC copy not written; pens_theme_named for a foreign
screen; ip_screens_sync plan on a 16-colour clone). Screenshot kept: ~/Desktop/upterm-shots/te10-6c-rig173-screenA-unthemed.png.
