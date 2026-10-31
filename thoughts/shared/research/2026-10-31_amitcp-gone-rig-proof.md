# AMITCP-GONE / IXNET-UG-NOREQ rig proof (vtcon-05 s38, 2026-10-31, HEAD 351a0382)
Fresh rig 5 (build/rig5 moved aside, sys.hdf cloned anew), start --max.
Start WARN: "[WARN] Directory Opus 5.104 is not installed in this rig's VTC: drawer (no current dopus5.stamp)" (no build-drift line).
AMITCP-GONE: Assign AmiTCP: EXISTS -> rc 5 "AmiTCP: not assigned"; List LIBS:usergroup.library -> rc 20 object not found (LIBS: = VTCX:ixp6 + System:Libs + ... no amitcp);
 System:Libs/ixnet.library md5 9247e6d423a976f8d99bd100fe768eec == build ixnet (ixemul-vtcon/build295/ixnet/68020/amigaos/ixnet.library);
 curl -s -o RAM:t.bin http://127.0.0.1:8765/test.bin rc 0, md5 281c6b7bc28148aac8d54e548f05ead0 == served file;
 upkg_rig.py --only updatelist --force: "passed 6 of 6" (update exit 0).
IXNET-UG-NOREQ: nano typed in an UP-Term window (no AmiTCP: assign): Status shows UP-Term:bin/nano running, window list = UP-Term, Amarchy bar only, ami.py requesters empty; Ctrl-X ended it (0 nano tasks).
Residue: copy of rig 1's VTC: drawer brings build/rig5/vtc/amitcp (db, libs/usergroup.library, dated 29 Sep), unassigned and not in LIBS:.
Anomaly: watchdog logged "reset loop (10+ CPU resets in 60 s)" twice (19:44:33, 19:48:57), each time shortly after an UP-Term/vsh or upkg_rig action; cause not investigated.
