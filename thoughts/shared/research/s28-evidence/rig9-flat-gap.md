# rig 9 flat gap (s28), measured 2026-10-31
Note: ami.py refreshed stale files into VTCX:Apps/bin on first call (md5 differed for all four); recopied from worktree build with AMI_HOST=127.0.0.1 set (that env var disables the refresh). md5 now match: Amarchy e71c9e27, gui 78cc8d30, menu 4158ffc8, flat 1ea13ff0. Running processes were not restarted (images already in memory).

Q1 dripens Workbench: pens 0 1 2 0 0 0 2 0 2 2 0 1 (rgb AAAAAA 000000 FFFFFF AAAAAA AAAAAA AAAAAA FFFFFF AAAAAA FFFFFF FFFFFF AAAAAA 000000). SHINEPEN(3)=0 == BACKGROUNDPEN(7)=0: yes. Screen 640x256 depth 8. dri_Depth: not measurable with existing tools (dripens does not print it); unmeasured.
Q2 Palette.prefs (ENV:Sys; no copy in ENVARC): data at 0x22, pap_4ColorPens at 0x32, pap_8ColorPens at 0x72; BOTH arrays hold the same 12 flat words 0,1,2,0,0,0,2,0,2,2,0,1 (the earlier 0x34-0x4F range was offset by one word). Screen depth 8 uses the 8-colour array; live pens equal the flat map. Palette entries 0-3 now AAAAAA 000000 FFFFFF 6688BB.
Q3 fresh drawer "System" (150,80 361x150), shot ~/Desktop/upterm-shots/s28-rig9-fresh-drawer.png:
 left border 170,170,170 (not white); right border 170 (x=507..509); top 170; bottom 170 (with blue 0,0,255 run on the size-gadget row y=222..227);
 scroller track AND knob both (0,0,255) solid blue over x=497..506 y=92..196 (not dithered, not black/white); interior most common 170 (27077 px), black 4836, white 3359, 6688BB 953.
 pure-white pixels inside window: 4375 (3359 in interior, rest gadget/icon imagery).
Q4 amarchy-flat STATUS: flat=on calls=613 chained=0 / pens text=2 line=3 back=0 outline=3 button=4 ptext=0 / dopus=absent / rectfill installed=yes calls=4142 hits=148 shits=658 / text installed=yes calls=2113 hits=0 / frame=hooked calls=0 flat=0
Prefs restore: ENVARC:Amarchy/prefs and ENV:Amarchy/prefs before = header + flat-system-look=on + flat-window-gadgets=on only (prefs.orig = header only). After = plus amarchy-gui-theme = white, desktop-theme = on (both files; ENV: written via Copy from T: because put's rename fails on ENV:). Nothing else toggled.

## Q5 backdrop
(a) root backdrop samples (20,100),(600,100),(320,40),(580,200),(60,200) = all (170,170,170). Outside the System window, rows 11..245: 88480 of 95738 px = 92.4% are 170,170,170 (others: black 2170, blue 2000, white 1796, 6688BB 1292 = icons).
(b) No wbpattern.prefs / Pattern prefs in ENV:Sys (Palette, Workbench, pointer, screenmode), ENVARC:Sys (= Empty-12MB-FFS:Prefs/Env-Archive/Sys) or SYS:Prefs/Env-Archive/Sys; ROOT pattern type/pen could not be read from a file (Workbench.prefs not decoded). With no pattern the backdrop is the plain BACKGROUNDPEN fill.
(c) 34,34,34 is held only by palette pen 183 (not a system pen); 170,170,170 by pens 0, 26, 184. Pen 1 is now 000000.
(d) Yes: backdrop 170,170,170 == pen 0 == BACKGROUNDPEN colour AAAAAA. Backdrop and window interior/borders are the same colour because the flat map maps both to pen 0; the s27 222222 backdrop was palette pen 1, now black.
