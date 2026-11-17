# Master refresh (Patina candidate) - NOT STARTED

Gate failed: distcheck2.log last line is "rc=2" (make dist-check non-zero), so no candidate was made and no rig was started.
Context seen in older logs (fa/dist.log, fa/distcheck.log, may be pre-rerun):
- dist.log: tools/gen_jbmono.py fails, ModuleNotFoundError: No module named 'PIL' (python3 lacks Pillow).
- distcheck.log: "kitsize: Files/Amarchy: no line of parts.txt claims it" (stale old-name file in Files/ of the dist tree).
distcheck2.log itself holds only "rc=2" (no output captured).

## Candidate built and smoked (13 Nov, second agent)
Candidate: /Users/spot/Code/UP-Term/images/nyhd2-patina-candidate.hdf (cp -c clone of nyhd2.hdf; master untouched). Scripts/logs: scratchpad master/ (refresh.py imports rig.py and reuses strip_user_startup_blocks, sys_hdf_kit_upterm, KIT_WBSTARTUP_ICONS; xdftool for delete/write).
Removed (DH1 Rushhours:Tools/UP-Term): bin/Amarchy, bin/amarchy-gui, bin/amarchy-index, bin/amarchy-menu, Amarchy/ (+Themes/), share/man/man1/amarchy.1, share/tutorial/14-amarchy-keys.lesson. No amarchy names existed on DH0 (S:, ENVARC:, Libs, C).
Startup: S:User-Startup blocks ";BEGIN UP-Term Amarchy" and ";BEGIN UP-Term Amarchy index" removed, NOT replaced: since Leg W the kit (install.dos ~l.722) starts Patina and patina-index from SYS:WBStartup icons, not User-Startup. Added SYS:WBStartup/ (new dir) + Patina.info + patina-index.info from build/dist/up-term/Files/wbstartup. NOT added: the S:Startup-Sequence ";BEGIN UP-Term Patina-Flat" block (installer inserts it before IPrefs via instssblock.sh; the master had none; rig flat_boot covers it) - add it if the owner wants the full install state.
Written: all UP-Term: kit files via rig.sys_hdf_kit_upterm (md5 read-back ok); UP-Term:MANIFEST replaced with build/dist/up-term/Files/MANIFEST (old one listed 6 amarchy paths).
Smoke (rig 9, purpose "up-term-ba session: Patina master candidate smoke"):
- boot completes: PASS (rig.py start printed "up", wasabid ping ok)
- Patina runs: PASS-ish - no process named Patina left, but Status shows Rushhours:Tools/UP-Term/bin/patina-gui and patina-menu (started by it) and UP-Term:bin/patina-flat; boot.log "early UP-Term shadows ... before Patina"
- address PATINA 'STATUS': PASS "rc 0" (result string empty: "0 RESULT")
- ports: PASS "1 1" (show(P,PATINA.MENUSRV) show(P,PATINA.GUISRV))
- no amarchy left: PASS, 0 matches over full recursive listing of DH0 and DH1 and 0 in S:User-Startup
Rig 9 stopped; rig.py stop saved 262 files (40 MB) to build/rig-saved/9/20261113-140829 (left, mine).
