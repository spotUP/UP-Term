# ixemul v48 requester, rig 51 (OS 3.2, Amiberry, keep-sys)
Boot could NOT be snooped: wasabid starts at the end of BOOTX:go (boot.log "wasabid started" after "assigns done"); daemon answered only ~4 min after start.
State after boot (wasabi run):
- Version LIBS:ixemul.library FULL = ixemul.library 80.1 (10/13/2026)  (LIBS: first entry now VTCX:ixp6, 201256 bytes, same size as distkit/Files/libs/ixemul.library)
- Version SYS:Libs/ixemul.library = object not found; List SYS:Libs PAT=#?emul#? = empty; host build/rig51/os-32/Libs has no ix* files
- Assign LIBS: EXISTS = VTCX:ixp6 + Empty-12MB-FFS:Libs + :Classes + VTCX:pkgs/ncurses.../ixlibrary/sys/libs + VTCX:amissl ; IXEMUL: not assigned
- ncurses ixlibrary dir holds only libncurses.ixlibrary (no ixemul); no ixemul in Work:Apps/UP-Term
- window "startup message" (Abort button, frbuttonclass) still open, task Work:Apps/UP-Term/bin/amarchy-menu (survives Ctrl-C = boot instance blocked in the requester)
Snoop of a manual amarchy-menu rerun after boot (LIBS has ixp6):
12:04:50.649 ramlib FindResident("ixemul.library") = fail
12:04:54.485 ramlib LoadSeg("LIBS:ixemul.library") = ok
12:04:59.736 amarchy-menu OpenLibrary("ixemul.library", v48) = ok
Full log was in scratchpad snoop.log (deleted).
