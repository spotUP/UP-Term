# Amarchy -> Patina rename evidence (s76)
vtcon: /Users/spot/Code/UP-Term/vtcon-patina branch patina-rename, commit 7d6bdb09 (base 98c58e32). 961 text files edited, 357 paths git-mv'd.
upterm-ports: /Users/spot/Code/UP-Term/upterm-ports-patina branch patina-rename (base main 10fa13a), 10 text files, 0 paths.
Spellings found: amarchy 6648, Amarchy 3974, AMARCHY 1533 (vtcon); amarchy 13, Amarchy 5 (ports). No others. No prior "patina" hits.
Script: scratchpad rename.py. Length literals fixed by hand: demo.c strncmp 7->6, 18->17; gui.c memcpy/offset 12->11; tests iconapply 27->26, keymap 17->16, shot_core 14->13.
Absence 1: `git grep -il amarchy` -> only tests/data/desk/workbench-wbtf-sample.prefs (binary, offset 92 "Amarchy P1a title %v", captured probe, only cited in docs); `git ls-files | grep -ic amarchy` = 0 both repos.
Absence 2: `strings -a` on all 51+ executables in build/amiga: 0 hits; `find build -iname '*amarchy*'` = 0 (clean build). Kits NOT built: make dist stops at dist-base check "upterm-ports/build/kit/userland was staged before the stamp: make kit-stage in upterm-ports" (main checkout state, not touched).
Tests: make test: shot_core, keymap, iconapply failed on the length literals; fixed, reran those suites green. No other failures in the log.
ARexx port: patina.c `rexx_host_open(&rexx, "PATINA")`; prefs: "ENV:Patina/prefs, else ENVARC:Patina/prefs" (amconf_core.h), BARCONF_ENV "ENV:Patina/bar.conf".
