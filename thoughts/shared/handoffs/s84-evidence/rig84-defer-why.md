# rig 84: why reset deferred after DOpus Hide (S85) -- PARTIAL
Deploy: build Patina 412772 (21:04) put to VTC:Apps/bin/Patina AND Rushhours:Tools/UP-Term/bin/Patina (size 412772 both). patina-gui 582424 equals rig. Running Patina = Rushhours:Tools/UP-Term/bin/Patina (the dir that holds patina-gui/patina-menu; VTC:Apps/bin has only Patina; UP-Term: = VTCX).
NOTE: quitting a Patina started from VTC:Apps/bin killed the menu hotkey (no patina-menu there); restart from Rushhours bin brings the menu back (plus a "Welcome to Patina" window, first run).
Window list BEFORE pick (2 DOpus listers open, wasabid window dump, flag field only has a=active b=backdrop; raw Flags hex / WBENCHWINDOW NOT obtainable from wasabid):
- dopus_lister x2 (title "", 196x173 @0,0, not backdrop) -> foreign by desk_window_foreign unless WBENCHWINDOW
- dopus_start_menu (title "", 39x15 @12,456, not backdrop, task dopus_start_menu) -> foreign (never hidden by lister Hide: LIKELY THE LEFTOVER)
- DOpus5:DirectoryOpus (800x573, backdrop) -> not foreign
- Patina bar -> not foreign (titled)
Foreign count before pick = 3. Hide hides listers only; dopus_start_menu window has no reason to vanish => count stays >= 1 => deferred. HYPOTHESIS, NOT CONFIRMED.
Pick NOT completed: menu harness (patina_theme_save_rig one_select 1) failed 3x ("no menu window", then "Up in theme list: no stable state"); sentinels PATINA_DEFERRED/DOPUS_HIDES/HIDE_POLLS/HIDE_LEFT all unset (-) since no apply ran.
Step 3 not done (out of budget).
Rig 84 left running; purpose "vtcon S85: why reset deferred after DOpus Hide". 2 DOpus listers open at 0,0.
Scratch: p5.py in session scratchpad (small). Log: rig84-defer-why.log
