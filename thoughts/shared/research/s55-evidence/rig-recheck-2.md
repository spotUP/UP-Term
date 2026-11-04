# s55 rig recheck 2 (rig 102, main 4705d533 >= d873ce13)

Build: make wasabi, make amiga, make dist (UPTERM_PORTS=kit-main) all rc 0. Rig 102 started --max --idle-minutes 0.
Install: installer_rig.py ONLY fresh-all (every option ticked, INSTALLER_KEEP=1): PASS, 36 checks, includes reboot + Amarchy port + menu.

## Boot verdicts (serial.log "87000004" count 0 each time; ami.py requesters empty; "CPU reset" count 0 in fs-uae.log.txt)
- boot 1 (installer's own reboot): clean, Workbench + Amarchy bar, no requester.
- boot 2 (rig.py reboot): clean, "Welcome to Amarchy" window only.
- boot 3 (rig.py reboot): clean, same.
PASS: no Program failed #87000004, no reset loop on 3 boots.

## UD2 (UPDemo 15): NOT RUN (no verdict)
UPDemo runs in C: (System:C/UPDemo), scenes 1-based. After `UPDemo 14` + Return the demo sits on scene 14 (Sixel);
one Space then lands on "Demo 16/35 Kitty truecolour": scene 15 (Kitty animation) never stays on screen.
Shot ud2-a showed scene 16 with the stale scene-15 heading text ("a globe turns over a fjord..." garbled on row 2), so
scene 15 starts to draw and is left at once. Cause unknown (extra key event consumed by scene 15, or scene failing).
B / Left (raw 0x35, 0x4F) from 16 did not move back. Fjord/earth region was therefore never captured; no pixel verdict.
Also seen: the demo does not auto-advance on the rig (scene 16 stayed 30+ s).
UD3, UD5, UD6: NOT RUN (stopped at the budget).

Shots: ~/Desktop/upterm-shots/s55-102/ud2-*.png (ud2-r = scene 14, ud2-t/v = scene 16).
