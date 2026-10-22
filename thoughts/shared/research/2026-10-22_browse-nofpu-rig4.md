# rig 4 report
Task A (upkg browse, upkg built 22 Okt 13:17):
- upkg_browse_rig.py: 11 of 13 PASS. FAIL: colour green pixels rise/fall (0 -> 0; the screenshots hold no green pixel anywhere, 800x600). Cause not investigated.
- New check: after Return at the install OK box, Status lists upkg: PASS. Steps 2 and q also need upkg alive.
- upkg_browse_extra_rig.py: B3 PASS (5), B4 PASS (3; SLOW-FIRST 7 s, SLOW-LAST 42-49 s). B5: script bugs fixed; last run no requester (PASS) but empty output / no exit line (FAIL, undiagnosed).
- 0026 revert proof: NOT DONE.
Task B: build/rig4/vtcon-rig.fs-uae has cpu = 68LC060, no fpu key (PASS). Boot failed 3x "no wasabid answer; boot.log: go started"; host had tools/test_dancehall_mt40.py at 91% CPU (other project), likely starvation; not rerun. Installer checks NOT RUN.
