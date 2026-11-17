# UPKG-REMOVE-NOOP: why `upkg remove up-git` printed "No packages removed."

Code-only analysis, nothing run. Not decided by code alone.

## Where
upkg is opkg 0.9.0 + patches (upterm-ports/pkgs/opkg/patches/0001..0027). Message at
upterm-ports/build/src/opkg/libopkg/solvers/internal/opkg_action.c:152 (opkg_solver_remove), printed whenever `done == 0`.
`done` becomes 1 only after opkg_remove_pkg() returns 0 (lines 143-148), so this message is also the tail of every failed or skipped remove.

## Remove path (opkg_action.c:50-155)
1. available = all pkgs with state INSTALLED/UNPACKED/HALF_INSTALLED from the status file (/UP-Term/pkg/upkg/status, dist/upkg.conf); feeds are NOT read for remove (src/opkg.c:~663).
2. Match: `fnmatch(pkg_names[i], pkg->name, 0)` (line 69), exact case-sensitive name, no version/arch suffix. If nothing matches, patch 0021 prints "up-git is not installed." on STDERR (line 135-137) and sets err=1.
3. A match is skipped (no pkgs_to_remove entry, so "No packages removed.") by: pkg_hash_fetch_installed_by_name NULL (line 76, ERROR "Package X is not installed."); dependents of the package without --force-depends (line 105-118, ERROR + err=-1); or opkg_remove_pkg failing (essential / prerm failure, opkg_remove.c:244-281, ERROR "not removing package ... prerm script failed").
4. Success path prints NOTICE "Removing up-git (ver) from root..." (opkg_remove.c:266) BEFORE the prerm.

## Install side (both paths identical)
Kit-local (`upkg install VTC:pk/<ipk>`) and online (`upkg install --force-reinstall up-git`) both go through the same opkg install and write the same status record; pkgs.py (tools/pkgs.py:~355) writes `Package: up-git` in the control, renamed packages add Replaces/Conflicts/Provides: UP-Term-git (dist/package-renames.txt). No name, case, suffix or path mismatch found on the install side: `upkg status` listed it as "install user installed" and `upkg install --force-reinstall up-git` found it by the same name. No RUNTIME_DEPENDS exists in any upterm-ports recipe (grep), so a dependents block is unlikely. The prerm (tools/pkgs.py:~228-236) always `exit 0`.

## Conclusion
No mismatch in name lookup or status writing is visible in code. The evidence line says only "No packages removed." was seen (evidence file line 259, side note, cause not looked at). The decisive unknown is what ELSE was on the output: the stderr line "up-git is not installed." (patch 0021) or an ERROR line. If the test script captured stdout only, the real reason was dropped.
Top candidates:
- A: remove ran when the status file did not (yet) hold up-git as installed (ordering inside that rig script, or a different state_file than the one install wrote); stderr "up-git is not installed." was not captured.
- B: dependents/essential/prerm refusal (ERROR lines on stderr, also not captured). Less likely: no "Removing up-git" NOTICE was reported, which a prerm failure would have been preceded by.

## Single on-rig check (do not run here)
On a rig with up-git installed: `upkg remove up-git 2>&1` (capture stderr) and `Type UP-Term:pkg/upkg/status` (grep the `Package: up-git` stanza: Status line must read `install user installed`). The stderr text splits A from B.

## Proposed fix
Once cause confirmed: if A/capture-only, no code fix, make the rig script use `2>&1`. If the status is correct and still no match, check the status stanza name vs `up-git`. Independently, make opkg_action.c:151 print the reason (e.g. only print "No packages removed." when nothing matched, otherwise "Nothing was removed; see errors above.") so the message is not misleading.
