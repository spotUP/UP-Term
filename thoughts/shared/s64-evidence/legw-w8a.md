# W8a: legw on KS 3.2 (rig 81) and KS 3.2 + P96 (rig 82; --os 32 is already uaegfx/P96, rig.py has no separate P96 flag)
Both: 1 ok (UP-Term.version), 9 FAIL (1, 3-10), then [ABORT] reboot did not come back in 240 s. No verdict; 22 checks not reached.
Root: FAIL 1 base Install: at install.dos "vsh unstartup.sh Amarchy" (~line 569) vsh prints "rm: not found" and "grep: not found", rc 127 -> Install aborts mid-way (after Copy Rexx). In the guest vsh $PATH is empty when run bare from the Install script (T:t.sh: PATH=). SYS:UP-Term/bin/rm exists; grep missing in the stub kit (slim.py moved it to an upkg package; stub kit has no grep).
Unresolved: why KS 3.1 (22/22) did not hit it (cannot tell from here; kit rebuilt, original kit gone).
Empty-bin after Uninstall: not reached.
Logs: w8a/32.log, w8a/32p96.log
