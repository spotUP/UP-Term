# s80: IPREFS-REQ-ON-THEME-SWITCH, code research (read-only, nothing run, nothing reproduced)

## 1. Call path (patina/)
1. Keymap row `gtheme:<name>`: patina.c:422 `case KA_GTHEME: gt_dos_select(b->arg,0,0)` (leaf.c:334/538 are the menu/GUI entries to the same function).
2. gthemes_dos.c:217 `gt_dos_select`: saves AM_GTHEME via amconf_save, then `gt_dos_apply_saved(name)` (gthemes_dos.c ~200).
3. `gt_dos_apply_saved`: `desk_dos_apply(&th,0,0)` (desk_dos.c:1245). Fallback only when palette part off/refused: `wb_pens_apply` (gthemes_dos.c:39), which is live SetRGB32 on pens 0-3, no IPrefs.
4. `desk_dos_apply` -> `dk_apply` (desk_dos.c:439): plan mask from desk switches (font, pointer, icontrol, wbpattern, palette).
   - Palette bit: `reset = desk_pens_reset(live,...)` (desk_dos.c:479, desk_core.c:211). TRUE only when the live pen NUMBERS (DrawInfo pen map) differ from the wanted map, i.e. pen assignment, not colour values.
   - If reset (or old_ws): `dk_windows_close()` (:483, def :281) closes OUR bar/windows only (own_close, BAR OFF), then `dk_put_palette` (:237) -> `dk_put` (:166) -> `dk_editor` (:125): `SYS:Prefs/Palette FROM <work> USE`, which writes ENV:Sys/palette.prefs; IPrefs gets the notify and resets Workbench (CloseWorkBench / reopen); `dk_wait_reset` (<=6 s, DK_RESET_SECS) then `dk_windows_open`.
5. Then `desk_dos_apply` -> `dk_flat_pens` (desk_dos.c:820): same pattern at :863-875 (RL15-IPREFS-SYSREQ). Reset when `sync` (live pens != map, or stale file, or flat toggle/restore).

Which writes trigger CloseWorkBench (from code, IPrefs behaviour itself unverified):
- Palette.prefs whose pen map differs from the live one: yes (that is what `reset` predicts).
- Palette.prefs colours only (pen map already equal): code treats as NO reset (reset=0, windows not closed, no wait).
- Font.prefs: yes, resets the screen (3.1 twice); comments :531, :1779. dk_apply does NOT close windows for font unless `reset||old_ws` -> font change is another foreign-window trigger.
- Pointer, WBPattern, Workbench.prefs (icon border): not noted as resets (only wait on IPrefs idle).
- Screen mode: not written by the theme.

## 2. Can the switch avoid the reset?
- Colours only: yes already. Once pens are in the Patina/flat map (reset=0), a theme switch changes only palette colours; no screen reset, no requester. The requester appears when the pen MAP changes: first switch to flat-pens (map differs), a non-flat <-> flat toggle, and font changes.
- Live alternative: apply colours with SetRGB32/LoadRGB32 (`dk_live_pens`, `wb_pens_apply` exist) and write palette.prefs WITHOUT USE through the editor (write ENV:+ENVARC: file directly). Loses: the Palette editor is how IPrefs is told; a direct ENV: write is picked up by IPrefs notify anyway, so IPrefs may still reset (the s56/rig notes say direct ENV: writes collided with the editor: "object in use"). Pen-map changes cannot be live: DrawInfo pens are fixed at screen open (needs reset). So the reset cannot be avoided when the map must change; it can be avoided when only colours change.
- ENV-only (no ENVARC) would still notify IPrefs (it watches ENV:). Lost on reboot, no gain.

## 3. Options
A. Detect foreign windows, skip the pen-map part (S, ~25-40 lines): before `reset` close, walk `scr->FirstWindow` of the Workbench screen (IntuitionBase lock, `dk_wb_screen` already reads it) and count windows not ours (Backdrop/WBWindow excluded). If foreign and reset needed: write colours only for the CURRENT pen map (desk_palette_theme with the live map), skip font; log "pen map kept: close windows" once. User sees: theme colours change, no requester; pen-map/font change deferred until they close lister windows. Risk low; theme not fully flat until later.
B. Suppress requester via `pr_WindowPtr=-1` (XS, ~5 lines): wrong layer: the requester is IPrefs' (its own process), our pr_WindowPtr does not affect it. Rejected unless proven otherwise.
C. Foreign windows: ask DOpus to close/iconify listers (ARexx `lister close all` / `dopus hide`), reopen after (M, ~80-120 lines, DOpus only; lister positions/paths must be saved and restored, other foreign apps untouched). User sees listers flash/reopen. Medium-high risk (state loss, timing, 6 s wait).
D. Pre-flight message plus confirmation (S, ~40 lines + string): when reset is needed and foreign windows exist, show a Patina requester "Theme needs to reset Workbench; close other windows first" with Continue/Colours only/Cancel. User sees a Patina prompt instead of IPrefs' generic one. Low risk, one more dialog.
E. Make the case rare: on first apply pen map is set once (reset), later switches colour-only (already). Keep a "dk_pens_ok" fast path so the map is never re-derived to differ (e.g. font-driven resets only when font actually changes). XS-S, only reduces frequency.

## 4. Recommendation
A (with D's message as the one-line notice in the ARexx/status result, not a modal): detect foreign windows on the WB screen, and when a reset would be needed, apply colours live/for the current map and defer pen-map/font. It fixes the stall at the layer we control, needs no DOpus-specific code, and the common case (colours only) is already reset-free. Verify first on a rig with DOpus (rig.py dopus, reference tools/rig/amarchy_dopus_toolbar_rig.py): read the requester text and confirm whether the first switch (map change) or every switch triggers it; s75 saw it on switch 2 (white) after switch 1 (nord), which suggests EITHER a map flip OR a font/other reset, so the premise "map changes every switch" is UNVERIFIED and the log of `desk_dos_wb_closes`/`reset` per apply should be read before building A.

Unverified: IPrefs internals; that DOpus windows are what blocks CloseWorkBench; whether colour-only palette writes reset on 3.2 (code assumes not).
