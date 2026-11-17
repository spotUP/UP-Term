# S85l+ Keep-mess rig check (rig 84, rebuilt build/amiga Patina/menu/gui via buildq, patina_gui_rig --setup)
Method: scratch script keep_run.py (adapted from patina_confirm_rig/patina_desk_rig.pick): 2 DOpus DeviceList listers, Patina menu opened to Style > Patina theme, different theme picked, Keep clicked by t.keep_requester, windows read (ami read_ui every_screen) at +3 s and +10 s, then menu closed and reopened to the same list level.
Verdicts (text only; screenshots NOT viewed, no image-capable agent allowed):
- (a) Panel clipped: INCONCLUSIVE/SUSPECT. Before apply the panel window is 575x504 @112,48. After Keep (+3 s and +10 s, both runs) it is 360x188 @220,206 (the smaller menu size), fresh reopen at the list level is 575x504 @112,48. Windows are not geometrically clipped (a different level may legitimately be smaller); needs the screenshot to judge drawing.
- (b) Lister titles empty: NOT REPRODUCED in the window list: both listers titled 'Device List' 196x173 at +3 s and +10 s. Gadget flatness / pens not measurable in text.
- (c) Patina bar not visible: FAIL to reproduce = bar PRESENT ('Patina bar' 0,587 800x13) at +3 s, +10 s, both runs.
Keep: requester seen and clicked, gone after click both runs.
Screenshots: s84-evidence/s85lplus-keep-run1.png, s85lplus-keep-run2.png
Raw: 
## Run 1
picked row 17: rose-pine
panel before apply: [('Patina', 112, 48, 575, 504), ('Patina bar', 0, 587, 800, 13)]
Keep clicked=[True] LAST_KEEP={'seen_at': 1.692793846130371, 'kept_at': 1.7148900032043457, 'gone': True}
### +3s after Keep (t=3.0)
screen 'Workbench Screen' 800x600
   win 'Patina bar' left=0 top=587 w=800 h=13
   win 'Patina' left=220 top=206 w=360 h=188
   win 'Device List' left=0 top=0 w=196 h=173
   win 'Device List' left=0 top=0 w=196 h=173
   win '' left=12 top=456 w=39 h=15
   win '' left=0 top=14 w=800 h=573
Patina bar window present: True
panel geometry: [('Patina bar', 0, 587, 800, 13), ('Patina', 220, 206, 360, 188)]
dopus listers: RESULT
### +10s after Keep (t=10.0)
screen 'Workbench Screen' 800x600
   win 'Patina bar' left=0 top=587 w=800 h=13
   win 'Patina' left=220 top=206 w=360 h=188
   win 'Device List' left=0 top=0 w=196 h=173
   win 'Device List' left=0 top=0 w=196 h=173
   win '' left=12 top=456 w=39 h=15
   win '' left=0 top=14 w=800 h=573
Patina bar window present: True
panel geometry: [('Patina bar', 0, 587, 800, 13), ('Patina', 220, 206, 360, 188)]
dopus listers: RESULT
SHOT /Users/spot/Code/UP-Term/vtcon/s84-evidence/s85lplus-keep-run1.png
panel fresh reopen (list level): [('Patina', 112, 48, 575, 504), ('Patina bar', 0, 587, 800, 13)]## Run 2
picked row 18: solitude
panel before apply: [('Patina', 112, 48, 575, 504), ('Patina bar', 0, 587, 800, 13)]
Keep clicked=[True] LAST_KEEP={'seen_at': 7.328207969665527, 'kept_at': 7.342854976654053, 'gone': True}
### +3s after Keep (t=3.0)
screen 'Workbench Screen' 800x600
   win 'Patina bar' left=0 top=587 w=800 h=13
   win 'Patina' left=220 top=206 w=360 h=188
   win 'Device List' left=0 top=0 w=196 h=173
   win 'Device List' left=0 top=0 w=196 h=173
   win '' left=12 top=456 w=39 h=15
   win '' left=0 top=14 w=800 h=573
Patina bar window present: True
panel geometry: [('Patina bar', 0, 587, 800, 13), ('Patina', 220, 206, 360, 188)]
dopus listers: RESULT
### +10s after Keep (t=10.0)
screen 'Workbench Screen' 800x600
   win 'Patina bar' left=0 top=587 w=800 h=13
   win 'Patina' left=220 top=206 w=360 h=188
   win 'Device List' left=0 top=0 w=196 h=173
   win 'Device List' left=0 top=0 w=196 h=173
   win '' left=12 top=456 w=39 h=15
   win '' left=0 top=14 w=800 h=573
Patina bar window present: True
panel geometry: [('Patina bar', 0, 587, 800, 13), ('Patina', 220, 206, 360, 188)]
dopus listers: RESULT
SHOT /Users/spot/Code/UP-Term/vtcon/s84-evidence/s85lplus-keep-run2.png
panel fresh reopen (list level): [('Patina', 112, 48, 575, 504), ('Patina bar', 0, 587, 800, 13)]