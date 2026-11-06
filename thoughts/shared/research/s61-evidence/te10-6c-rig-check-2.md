# TE-10.6c hooks rig check 2 (rig 175, OS 3.2, 99db9a6d on te10-6c-3), 2026-11-06

Helper commit 99db9a6d: tests/amiga/pubscreen.c gets SHAREPENS (SA_SharePens), FreeArgs moved after CloseScreen. Built, 0 warnings, cookie `$STACK: 16384` present.
Start: `--allow-stale-base` (branch lacks 6 origin/main commits); `[WARN] the Workbench stayed PAL (W43)`.
Rules written before the single `--setup`, read back before and after: unchanged
(default all / TE10A = gruvbox / TE10N = gruvbox / TE10T = gruvbox takeover / TE10B = exclude). `--setup` printed the known
`FAIL flat prefs off ... STATUS: flat=on calls=0`. Workbench theme = tokyo-night (1a1b26 / 7aa2f7); gruvbox.theme exists (dist/amarchy-themes).
Expected gruvbox: button face = GR_SURFACE = background 282828, border = GR_MUTED = muted 665c54.

| Check | Verdict | Measured |
|---|---|---|
| C1a case 1 (TE10A depth 8 SHAREPENS) | text gate PASS, pixels FAIL | `screens=1 applies=1` (was 0/0); button face (110,57) = 1a1b26, border = 7aa2f7, histogram 1a1b26/7aa2f7/a9b1d6 only; expected 282828 / 665c54. dripens palette (256 entries) has no 282828 or 665c54: no gruvbox colour written to a free pen. Product finding. |
| C1b case 2 (TE10N depth 4) | text gate PASS, pixels = nearest-pen not shown | `applies` 1->2, screens=0; face 1a1b26 (expected nearest of own palette to 282828 = 1a1b26: MATCH), border 7aa2f7 (expected nearest to 665c54 = 555555: NO MATCH, though 555555 may not be in the 16-colour set read). Flat draw not seen (rectfill hits=0). |
| C1c case 3 (TE10T takeover depth 4) | FAIL | `screens=0` (needs >=1), applies 2->3; pixels = A look, palette unchanged (pens 0 1 2 0 0 3 0 0 2 2 0 13). |
| C1d screen B / Workbench hash | UNVERIFIED | applies unchanged on B (3->3, correct for Exclude). Workbench md5 drifts without any test screen (9ec5.. -> 3b32.. -> 4a3c..), so the hash is not usable; B pixels equal A/N/T. |
| C2 sentinel | calls moves, flat draw not | amarchy-flat calls 0 -> 30; frame calls 22 -> 94 but `frame flat=0`, rectfill `hits=0 shits=0` for every screen. |
| C3 SHINE==BACK | FINDING | dripens pens on A, N, T, B: SHINEPEN (3) = 0 and BACKGROUNDPEN (7) = 0, both 1a1b26: equal. flat_dopus_decide skips when not held and equal, so it fires on every foreign screen that holds no pens (N, T, B here). |
| C4, C5, C6, C7, C8 | UNVERIFIED | C4-C6 need a themed look first (C1a failed). C7: VTCX:DOpus5 not installed. C8 not reached. |

Screens kept: ~/Desktop/upterm-shots/te10-6c-rig175-caseA-shared.png, te10-6c-rig175-caseT-takeover.png.
Open question for the owner of the hook code: with SHAREPENS and 256 colours, why does the plan (applies=1, screens=1) write nothing to the pens or the draw?
