---
date: 2026-10-18
topic: base Install finds add-on archives beside it and installs them
status: open
---

# Base Install finds the add-on archives beside it

Owner request (2026-10-18): the base UP-Term Install should detect the optional
packages in the same directory and offer to install them; it should also find
their .lha files, unpack them and then install.

## Decisions (made, do not re-litigate)

- D1. Where to look: the drawer that holds the base kit's drawer (the parent of
  `(pathonly @icon)`), for every non-base row of dist/packages.txt: a drawer
  `<archive>/` with an `Install` in it, or a file `<archive>.lha` /
  `<archive>-*.lha` (the release names carry a commit hash). A drawer wins over
  an lha of the same archive.
- D2. When: after the base install has finished (UP-Term: assigned for this
  session), before the final page. Update runs too.
- D3. A page lists what was found, each ticked by default (askoptions); Help says
  each add-on runs its own Install with its own pages and record.
- D4. An lha is unpacked beside itself (same parent drawer) with C:lha. Owner
  (2026-10-18): assume every Amiga user has lha in C:; no missing-lha path, no
  fallback page.
- D5. Each ticked add-on runs its own Install through the real Installer
  (`Installer SCRIPT <drawer>/Install APPNAME <archive> ...`, check the exact
  CLI on the rig), one after the other; UP-Term-nvim after the base (always true
  here). A failed add-on is reported on the final page; the base stays installed.
- D6. Probes use `(exists ... (noreq))` or are guarded so no "insert volume"
  requester can appear (see commit d29434f1 and FirstInstall in
  tests/test_dist_assemble.py).

## Open questions (measure on the rig first)

- Q1. Can Installer 43/47 start a second Installer from a `(run ...)` while its own
  window is open? Memory, screen, and whether the inner one's pages show.
- Q2. Exact `lha` invocation that unpacks quietly into a given drawer.

## Done (reachability)

A rig case in tools/rig/installer_rig.py: base kit drawer plus one add-on `.lha`
beside it; the user path (Install icon, defaults) ends with that add-on's record
present (its MANIFEST under UP-Term:pkg/<archive>/ or ENVARC: for stand-alone) and the
final page naming it. Static tests in tests/test_dist_installer.py for the page,
the noreq/guard rule and the packages.txt-driven list.

## Ledger

- [x] L1 Q1 measured on rig 2 (060): YES. Outer started `Run >NIL: Installer SCRIPT RAM:outer APPNAME X MINUSER AVERAGE DEFUSER AVERAGE NOLOG`; inside it `(set rc (run "Installer SCRIPT RAM:inner APPNAME X MINUSER AVERAGE DEFUSER AVERAGE NOLOG"))` (template from `Installer ?`: `Installer [SCRIPT] filename [APPNAME name] [MINUSER level] [DEFUSER default] [LOGFILE] [LANGUAGE] NOPRETEND NOLOG NOPRINT`, v43.3). Inner opens its own window (title RAM:inner, over the outer's 'Press Esc to Abort'), shows its own mode page + message page (Proceed id 90 each); inner ran its (run) body; both ended, no window left; rc = 0 (inner `(exit (quiet))`). Caveat: the inner shows its Install-mode page first (own pretend/real choice), so pass the same user level and expect an extra page unless the inner script skips it.
- [x] L2 Q2 measured: `System:C/lha` (LhA 2.15 68020+, on the path as `lha`). `lha -q x <archive.lha> <drawer>/` unpacks quietly with full paths (archive's top dir, e.g. UP-Term-foo/, created under the drawer; trailing slash on drawer), rc 0, no output. Drawer must exist (MakeDir first).
- [x] L3 (2a67fa6e) search + page in dist/Install.installer (list built from packages.txt by tools/mkinstaller.py, no hardcoded names)
- [x] L4 (2a67fa6e) unpack + run each
- [x] L5 (2a67fa6e) static tests
- [ ] L6 rig reachability case
- [x] L7 (2a67fa6e) README.txt install section names it

Notes (L3-L7): @icon under `Installer SCRIPT <abs path>` is not in any source in this repo (Installer is closed); every add-on and the base already take #files from (pathonly @icon), so the nested run passes an absolute script path and relies on the same derivation. UNVERIFIED on a rig: L6 must show the add-on's #files resolves (its install.dos runs). If @icon is empty there, add a common fragment falling back to the script path. If the wasabi final page (14 lines) has an add-on failure, the note gets a page of its own before it. test_dist_addons legacy-manifest test (python) already failed at HEAD before this work.
