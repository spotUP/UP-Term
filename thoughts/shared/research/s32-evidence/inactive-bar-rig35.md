# S32-INACTIVE-BAR eye check, rig 35 (main 27607459), 2026-11-03
md5 on rig (UP-Term:bin, read back via ami get): Amarchy b93c6695..., amarchy-flat 9efa2579..., amarchy-gui 8d5a47e5..., amarchy-menu 901295b5... = build/amiga.
STATUS: flat=on calls=288; pens text=2 line=3 back=0 outline=3 button=4 ptext=0; pens-held 16 refs=2 dim=249 knob=250 inbar=251.
Screens: ~/Desktop/upterm-shots/vtcon16-inactive-bar.png (active window zoomed to 80x40 over inactive), vtcon16-inactive-bar-full.png (inactive in front).
Measured: inactive band #323f5f (expected ~#63687f: FAIL on value); ring #323f5f (= band, PASS consistent); inactive title text #1a1b26 = active title text #1a1b26 (PASS); active band #7aa2f7 (PASS); title strip rows 21-24 under bar at right gadget box x=625,632 (inactive) and x<=74 (active) #1a1b26 surface (PASS).
Note: derived value gt_mix(bg,fg,45) with Tokyo fg c0caf5 gives about #656a83; rig shows #323f5f.
