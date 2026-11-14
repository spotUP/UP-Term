# W8b: SYS:UP-Term/bin left behind after reboot + Uninstall (PARTIAL FIX)

Branch s81-w8b (from main 07b1e6a0). Not merged, not pushed.

## Causes
1. patina-flat runs under the ENVARC:up-term/Dir name (SYS:UP-Term/bin/patina-flat). Uninstall deleted Dir first and never matched that name.
2. patina-gui and patina-menu ignore Ctrl-C; they end only on MS_QUIT from Patina's shutdown (patina.c ~771).
3. A WBStartup Patina is no CLI process: Status and Break miss it. It needs the ARexx QUIT.
4. OPEN: after Patina QUIT, patina-flat still runs and ignores Break. Rig 81 showed two DOS "System Request" (Retry|Cancel) windows, one owned by patina-flat, one by IPrefs. patina-flat is blocked in a DOS call on a vanished volume/assign, so it cannot take Ctrl-C (flat.c Wait loop at ~3764 never runs). Requester text not read (no screenshots). Not yet known which path it opens; no UP-Term: use in flat.c.

## Fix
- dist/Uninstall: copy Dir to ENV:upflatdir before deleting it; `Patina QUIT` before the Patina stop block; patina-flat Status/wait also try $upflatdir/bin/patina-flat.
- patina/patina.c: `Patina QUIT` CLI verb (ARexx QUIT to PATINA, waits up to 10 s for the port to go).
- tools/rig/install_rig.py: removed the rebooted=True skip of the two compares.
- tests/test_patina_docs.py: test_uninstall_stops_a_wbstartup_patina_and_the_flat_patch_by_its_dir_name. Errors on the unfixed files, passes with the fix (python3 -m unittest tests.test_patina_docs).

## legw
- Before (rig 77, Amarchy-era tree): 24/25, FAIL 21 (see w8b-upterm-bin.md).
- After (rig 81, KS 3.1): 23/25. Patina, patina-gui, patina-menu now gone. FAIL 21 remains (bin Dir empty, held by patina-flat). FAIL 15 "resident patch answers STATUS: flat=absent" new/unexplained (possibly flake).
- make amiga rc=0 (dist-base built). Kit built with a shim ports tree because kit-main staged userland was stale: test-only kit.

## Next
Find the volume patina-flat's requester waits on (read requester text on a rig), then stop patina-flat before the Assign UP-Term: removal or fix its access. Re-run legw.
