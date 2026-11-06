# TE-9.6 helper: pubscreen (built, not run)

Source tests/amiga/pubscreen.c, binary build/amiga/pubscreen, branch te9-6-helper. Never run on an Amiga: everything below is host-build only.

Usage: `pubscreen NAME/A,DEPTH/N,TITLE/K,SECONDS/N`. Opens public screen NAME (depth default 4, SA_LikeWorkbench first, fallback 640x256 hires; standard 3D pen list so the DrawInfo is new-look), one window at 40,30 size 300x150 with drag bar, depth, close and size gadgets, five GadTools gadgets and a menu "Project / Quit". Waits for CTRL-C, the close gadget, a menu pick or SECONDS. Exit 0 ok, 5 screen still has visitors (after about 10 s of CloseScreen retries), 20 failure with a plain reason line.

Ready line: `pubscreen ready name=<NAME> screen=0x.. window=0x.. depth=<n> win=l,t,w,h button=l,t,w,h string=.. scroller=.. checkbox=.. cycle=..`; gadget boxes are in SCREEN coordinates (window left/top plus gadget edge, taken from the created Gadget structs, so GadTools adjustments are included). Requested window-relative boxes: button 20,20 100x14; string 120,40 150x14 (prefilled "text"); horizontal scroller with arrows 120,60 150x12; checkbox 120,80 (GadTools size); cycle 120,100 120x14 (Alpha/Beta/Gamma).

Reused: btnbgprobe.c (GadTools open, NewGadget fill, __stack plus cookie form), scrprobe.c do_open (public screen tag list, PubScreenStatus), shotscreen.c (poll loop), gadprobe.c (stack form), amnotify.c/flat.c ($VER form). No existing file opened a named public screen with gadgets, so a new file was needed.

Rig: not in the `amiga:` target, dist or kit (shotscreen is the same). stage_rig.TARGETS has `pubscreen`, so `stage_rig.stage_build(('pubscreen',))` builds it and copies it to UPTERM_BIN with a SHA check; run it as `Run >NIL: <UPTERM_BIN>/pubscreen NAME SECONDS 600` style and read the ready line from a redirected file or ami.py long.

Unverified: all runtime behaviour (screen opens, SA_LikeWorkbench on the RTG Workbench rig, depth honoured, ready-line coordinates, string/scroller sizes after GadTools layout, refresh, clean close, Wait granularity 0.1 s polling).
