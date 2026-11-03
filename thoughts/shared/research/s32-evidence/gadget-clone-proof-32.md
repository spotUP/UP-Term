# Gadget clone proof on AmigaOS 3.2 (rig 18, 2026-11-03)

Version: Kickstart 47.115, Workbench 47.2 (both readings)
start.args (before and after reboot): --max --os 32 --purpose "vtcon-16 Amarchy session: gadget clone proof 3.2"
Install: installer_rig.py ONLY fresh-all with INSTALLER_KEEP=1 (all ticked): [PASS]; the driver itself reboots once after install.

Reading A (after fresh install + driver reboot, f1host window open):
clone draws=96 fallback=0
Reading B (after a second rig.py reboot, window open):
clone draws=51 fallback=0
clone fallback busy=0 nodri=0 nobm=0 empty=0 which=0 | clone empty ids=0 last which=0 size=0 nonzero=0 marks=0

Note: reading A has no split line captured (fallback=0 implies every split 0). The install-time reading before the first reboot was not taken (the driver reboots by itself).
Shot: /Users/spot/Desktop/upterm-shots/s32-clone-proof-32.png (640x256 grab; 5 distinct colours in top-left and top-right 24-row corners; no block-glyph shape test done).
Verdict: PASS (draws>0, fallback 0, KS 47.115).
