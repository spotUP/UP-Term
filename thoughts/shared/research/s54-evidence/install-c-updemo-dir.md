# INSTALL-C-UPDEMO-DIR proof on rig 91 (main 1b2abd22 + 48baf073)

Build: `make amiga`, `make dist UPTERM_PORTS=.../upterm-ports/.claude/worktrees/kit-main` (rc 0; kit-stage already current).
`strings build/dist/up-term/Files/UPDemo | grep VER:` -> `$VER: UPDemo 1.0 (03.11.2026) UP-Term`

Rig: `UPTERM_RIG=91 rig.py start --max --purpose "vtcon-ee s54: C:UPDemo drawer install check"`.
Steps run through install_rig.prepare(None) + run_long('runinstall') (the base kit's Files/install.dos, as the base case; no Uninstall afterwards).

Before install (rununinstall first; then MakeDir C:UPDemo, Echo >C:UPDemo/x hi):
    List C:UPDemo -> x  3 ----rwed Today 16:55:07 / 1 file - 2 blocks used     (C:UPDemo is a drawer)

Install: rc 0, ends "UP-Term installed."

After install:
    List C:UPDemo        -> Directory "C:" ... UPDemo  51804 ----rwed Today 16:52:33 / 1 file - 103 blocks used   PASS (file, not drawer)
    Version C:UPDemo FULL-> UPDemo 1.0 (05/29/21) / UP-Term                                                      PASS (name + base version 1.0)
    C:UPDemo ?           -> SCENE/N,ANSI/S,FULL/S,SOLID/S,BENCH/S                                                PASS (template)

Note: Version shows date (05/29/21), not 03.11.2026: AmigaOS Version reads the $VER date as dd.mm.yy and
mis-parses the 4-digit year; the name and version number are right. Not part of this fix.
Rig 91 stopped with plain `rig.py stop`.
