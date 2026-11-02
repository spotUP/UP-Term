# ixemul boot race proof (rig 57, Amiberry, OS 3.2)

Question: with the kit installed, does the early S:User-Startup block `;BEGIN rig-upterm-shadows`
assign LIBS: VTC:ixp6 before `;BEGIN UP-Term Amarchy` starts amarchy-menu (fix e07358c9, merged 82746511)?

Verdict: PASS on both cold boots, all five checks. No ixemul requester, amarchy-menu running.

## Setup
- Install: `INSTALLER_KEEP=1 UPTERM_RIG=57 UPTERM_RIG_FLAGS="--max --emu amiberry --os 32" python3 tools/rig/installer_rig.py ONLY fresh-all FORCE --purpose "vtcon-16 Amarchy: ixemul boot race proof"`
- Cold boots: `UPTERM_RIG=57 python3 tools/rig/rig.py reboot --max --emu amiberry --os 32 --purpose "vtcon-16 Amarchy: ixemul boot race proof"`, then text checks (`ami.py exec`, `ami.py requesters`).
- boot.log is not proof (go's Echo > wipes it); RAM:rig-libs-done is the marker.

## Boot 1
```
--- List RAM:rig-libs-done
rc 0
Directory "RAM:" on Monday 02-Nov-26
rig-libs-done                     5 ----rwed Today     13:31:35
1 file - 5 bytes - 2 blocks used
--- Assign LIBS: EXISTS
rc 0
LIBS           VTCX:ixp6
             + Empty-12MB-FFS:Libs
             + Empty-12MB-FFS:Classes
             + VTCX:amissl
             + Work:AmiSSL/Libs
--- Version LIBS:ixemul.library
rc 0
ixemul.library 80.1
--- Type BOOTX:boot.log
rc 0
go started
early UP-Term shadows started (before Amarchy)
kit shadows done
kit shadows skipped
early UP-Term shadows done (before Amarchy)
early LIBS: assigns done (before Amarchy)
wait done
env ok
UP-Term shadows ran before Amarchy
UP-Term shadows done
XCON mounted
LIBS: assigned before Amarchy
assigns done
wasabid started
--- ps
Process  3: stk  4096, gv 150, pri   0 Loaded as command: UP-Term:bin/Amarchy
Process  7: stk 262144, gv 150, pri   0 Loaded as command: Empty-12MB-FFS:UP-Term/bin/amarchy-gui
Process 10: stk 262144, gv 150, pri   0 Loaded as command: Empty-12MB-FFS:UP-Term/bin/amarchy-menu
--- requesters
--- screens
17:;BEGIN UP-Term assign
22:;BEGIN rig-upterm-shadows
25:;BEGIN UP-Term Amarchy
31:;BEGIN UP-Term Amarchy index
37:;BEGIN Amarchy-Flat
43:;BEGIN UP-Term
52:;BEGIN UP-Term serial
55:;BEGIN UP-Term wasabi
58:;BEGIN UP-Term SolidWin
61:;BEGIN UP-Term console
64:;BEGIN UP-Term device
67:;BEGIN AmiSSL
```

## Boot 2
```
--- List RAM:rig-libs-done
rc 0
Directory "RAM:" on Monday 02-Nov-26
rig-libs-done                     5 ----rwed Today     13:51:08
1 file - 5 bytes - 2 blocks used
--- Assign LIBS: EXISTS
rc 0
LIBS           VTCX:ixp6
             + Empty-12MB-FFS:Libs
             + Empty-12MB-FFS:Classes
             + VTCX:amissl
             + Work:AmiSSL/Libs
--- Version LIBS:ixemul.library
rc 0
ixemul.library 80.1
--- Type BOOTX:boot.log
rc 0
go started
early UP-Term shadows started (before Amarchy)
kit shadows skipped
kit shadows done
kit shadows skipped
early UP-Term shadows done (before Amarchy)
early LIBS: assigns done (before Amarchy)
wait done
env ok
UP-Term shadows ran before Amarchy
UP-Term shadows done
XCON mounted
LIBS: assigned before Amarchy
assigns done
wasabid started
--- ps
Process  3: stk  4096, gv 150, pri   0 Loaded as command: UP-Term:bin/Amarchy
Process  7: stk 262144, gv 150, pri   0 Loaded as command: Empty-12MB-FFS:UP-Term/bin/amarchy-gui
Process 10: stk 262144, gv 150, pri   0 Loaded as command: Empty-12MB-FFS:UP-Term/bin/amarchy-menu
--- requesters
--- screens
17:;BEGIN UP-Term assign
22:;BEGIN rig-upterm-shadows
25:;BEGIN UP-Term Amarchy
31:;BEGIN UP-Term Amarchy index
37:;BEGIN Amarchy-Flat
43:;BEGIN UP-Term
52:;BEGIN UP-Term serial
55:;BEGIN UP-Term wasabi
58:;BEGIN UP-Term SolidWin
61:;BEGIN UP-Term console
64:;BEGIN UP-Term device
67:;BEGIN AmiSSL
```

## Notes
- Also in the first install run, the installer's own reboot check passed (Amarchy ARexx port answers, Left Amiga + Space opens the menu) and logged "early LIBS: assigns done (before Amarchy)".
- Installer check 10 ("a drawer on another disk leaves SYS:UP-Term as it was") FAILED in two runs; unrelated to this question.
- Installer reboot check once hit "daemon did not answer for 150 s - frozen" under host load ~22; kit stayed installed.
- Two install attempts failed at rig start ("boot lacks the assigns ETC"): rig 1's seed drawer build/rig/vtc/etc was transiently missing while another session restaged it.
- `rig.py stop` removes build/rig<n>; use `reboot` for cold boots. `Assign LIBS: LIST` breaks the assign; use `EXISTS` only.
