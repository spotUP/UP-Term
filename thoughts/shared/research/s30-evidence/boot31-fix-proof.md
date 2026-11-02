# BOOT-31-WB-DEADLOCK fix proof, OS 3.1 RTG cold boots (rig 50, fs-uae)

Setup: make amiga ok; rig 50 `--max --emu fsuae` (no --os): Kickstart 40.68, Workbench 40.42, intuition.library 40.85 (OS 3.1).
ENVARC:Sys/screenmode.prefs = mode 0x50041303 (uaegfx RTG 800x600); Workbench screen 0x320x0x258 after every boot.
Amarchy md5 ed7b63c55034606ad4ea5992ba2c3978 on the rig (UP-Term:bin/Amarchy, shadowed from build/amiga/Amarchy by `rig.py install`) == build/amiga/Amarchy.
NOTE: a rig.py stop/start clones sys.hdf anew, so the fresh-all kit install on DH0 (done once, passed 32 checks, one reboot via
ami reboot passed) is gone after a cold start; the cold boots run Amarchy from the VTC: kit drawer (Rushhours:Tools/UP-Term).
The install itself had a stale kit Amarchy (build/dist 11:03, pre-fix md5 56830d1a...); the proof boots use the post-fix build.

| boot | result |
| 0 (first start, fresh clone) | no wasabid answer after 180 s, boot.log stopped at "go started"; rig.py auto-retried (stop/start) itself, retry came up. Cause unknown, not examined (rig auto-reset). |
| installer reboot (warm) | PASS, Amarchy STATUS answered, menu opens |
| cold 1 | PASS, up in ~2 min, no retry |
| cold 2 | PASS, 12:01-12:03, no retry |
| cold 3 | PASS, 12:05-12:07, no retry |
Per cold boot: Workbench 800x600 RTG, wasabi answers, AMARCHY STATUS answers (rows=69 enabled=1 bar=1), IPrefs state wait (not running), Amarchy processes wait.

rig.py stop at the end also removed build/rig50 (the tool's own stop behaviour for numbered rigs); rig 50 is stopped, directory gone.
