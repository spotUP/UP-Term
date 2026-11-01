# DESK-RIG-FLAT-LEFTOVER rig check (rig 8, worktree agent-a2b195889e71261ee, dcb0d3ad + f35880d5)

1. PASS (with note): rig start needed `make wasabi` and a build/amiga dir (first start crashed in kitset.py: no build/amiga). After `make amarchy-flat build/amiga/Amarchy`, start worked; `amarchy_gui_rig.py --setup` -> "ok setup: Amarchy runs from VTCX:Apps/bin, port AMARCHY answers", rc 0.
2. PASS: `amarchy_desk_rig.py --flat` run twice, both: "[OK] flat off: the block starts disabled", "[OK] flat restored: the resident amarchy-flat is gone (STATUS flat=absent)", "[OK] 0 failed".
3. Stale refusal PASS: after `touch amarchy/note.c`, `--setup --no-build` -> "[ERROR] stale: build/amiga/Amarchy", "[ERROR] stale: build/amiga/amarchy-gui", rc 1, nothing copied.
   `make -q` returns 0 FAIL for build/amiga/amarchy-gui (q=1 even after `make build/amiga/amarchy-gui`, 0 compiles run). 0 for amarchy-flat, Amarchy, amarchy-menu.
   Cause (make -d): build/symbolsnerd/NerdFontsSymbolsOnly-3.5.1.tar.xz is newer than the extracted SymbolsNerdFontMono-Regular.ttf (tar -xJf keeps the archive mtime, so the rule reruns every make and never settles) -> menu_icons.inc -> amarchy-gui permanently out of date. So --setup --no-build always refuses amarchy-gui; the default --setup path (builds) is unaffected.
   Also: plain `make` stops at the test target: tests/test_prefs.c:1201 pg_help(it) <= 76 failed (prefs 1 failed), unrelated to this fix.
