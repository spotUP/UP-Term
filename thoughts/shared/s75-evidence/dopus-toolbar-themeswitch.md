# s75: DOpus toolbar after a theme switch (main 98c58e32, rig 9, OS 3.2, purpose "s75: DOpus toolbar after theme switch test")

Verdict: PASS for "new lister gets the flat toolbar after a theme switch"; the set cannot differ between themes, so the test is "set survives switch".

Set choice: dopus_pick_bank (amarchy/menu_theme.c:307) -> dt_pick_set(font height, pixel aspect, icon-style outline) (amarchy/dopustb_core.c:7). The theme is not an input, so every theme maps to the same set on one screen (here Workbench 640x256: filled-32t).

Non-GUI theme path: keymap rows `gtheme:<name>` (config/keymap.c:222, amarchy/amarchy.c:422 gt_dos_select) in ENV:up-term/keymap (rows 0..2 = nord, white, gruvbox), then ARexx `AMARCHY RELOAD` and `AMARCHY RUN <row>`. Theme picker GUI not used.

Rig lines (ARexx text, no screenshots):
- fresh DOpus, before any apply: handle 272330324 -> PROGDIR:Buttons/toolbar (upstream default)
- RUN 0 (nord, ENV:Amarchy/prefs gui-theme = nord), 6 s later: 272330324 -> DOpus5:Buttons/Amarchy-toolbar-filled-32t (pre-existing lister moved)
- `lister new` +5 s: 277949820 -> Amarchy-toolbar-filled-32t  PASS
- RUN 1 (white, gui-theme = white): existing listers stay filled-32t; a `lister new` +5 s: 278056532 -> PROGDIR:Buttons/toolbar (NOT yet); +15 s later the same handle -> Amarchy-toolbar-filled-32t. A System Request from task IPrefs (Retry|Cancel) was open during this apply, which stalled the commodity.
- after the apply settled: `lister new` +5 s: 278215196 -> Amarchy-toolbar-filled-32t  PASS

Caveat: the +5 s miss after theme B coincided with an IPrefs "System Request" left open by the Workbench pen reset (rig artefact or product, not isolated); once the requester no longer blocked, the follow tick caught the lister. The Amarchy ticks counter was 66 at that time.
Not tested: a theme switch that changes the set (needs icon-style outline or another screen font/aspect, not a theme).
