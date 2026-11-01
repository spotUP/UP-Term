---
date: 2026-10-31
topic: FLAT-PENS-NOT-LIVE root cause (rig 9)
status: cause found, fix blocked on owner decision
---

# FLAT-PENS-NOT-LIVE: where the flat pen set stops

## Code path
- amarchy/flat_core.c:198-206 `flat_frame_decide` draws flat only when DrawInfo pen_shine == pen_back (called at amarchy/flat.c:188).
- Flat pens are written only by `dk_apply` (amarchy/desk_dos.c:333-445), reached only from `gt_dos_select` (amarchy/gthemes_dos.c:205-235), i.e. on a theme pick. Startup never applies the desk (see ledger AM-THEME-BOOT).
- `flat_pens = amconf_desk_flat_pens(&ap)` (desk_dos.c:347; amarchy/amconf_core.c:143-146) needs `amarchy-flat-pens` on (prefs/settings_def.c:214, default off) AND the palette bit of `amconf_desk_mask`, which needs `desktop-theme` on: a hidden key (prefs/settings_def.c:242, default off, no Prefs gadget).
- `flat-system-look` only drives the amarchy-flat state block (`dk_flat_sync`); it never writes pens.
- Owner decision "interior same colour as border": already true in `desk_pens_flat` (amarchy/desk_core.c:140-147: SHINE=SHADOW=FILL=BACKGROUND=DK_BG, FILLTEXT=DK_FG). No change needed.

## Layer 1 (owner config on rig 9): pen write never done
- ENV:Amarchy/prefs: only `amarchy-gui-theme = vantablack`, `flat-system-look = on`, `flat-window-gadgets = on`. No desktop-theme, no amarchy-flat-pens. No ENV:Sys/palette.prefs.
- dripens `0 1 1 2 1 3 1 0 2 1 2 1` (SHINE 1 != BACKGROUND 0); desk wanted `0 1 2 2 1 3 0 0 2 2 0 2`.
- STATUS: `flat=on calls=958`, `frame=hooked calls=22 flat=0`.

## Layer 2 (added desktop-theme = on, amarchy-flat-pens = on, picked theme "white")
- AMARCHY_DESKS 1, DESKMASK 15, WBCLOSES 1.
- ENV:Sys/Palette.prefs written 22:35:15 (434 bytes). Pens: DETAIL0 BLOCK1 TEXT2 SHINE0 SHADOW0 FILL0 FILLTEXT2 BACKGROUND0 HIGHLIGHT2 BARDETAIL2 BARBLOCK0 BARTRIM1. Correct flat set.
- Live pens unchanged: `0 1 1 2 1 3 1 0 2 1 2 1`, `frame=hooked calls=96 flat=0`.
- IPrefs System Request (Retry|Cancel): "Intuition is attempting to reset the Workbench screen. Please close the following windows: Font Preferences".
- "Font Preferences" (task Font) opened 22:35:21 by the desk apply after the dk_wait_reset 6 s timeout (desk_dos.c:254-265, 380-381). Pointer.prefs and WBPattern.prefs written 22:35:21; no font.prefs.
- Retry clicked: requester stays, pens unchanged (Font window still open). Same family as WBRESET-EXT / R1.17-WBRESET.

## Rig 9 state left
Running, flat on, System Request and Font Preferences still open; prefs carry desktop-theme = on and amarchy-flat-pens = on; theme "white".
