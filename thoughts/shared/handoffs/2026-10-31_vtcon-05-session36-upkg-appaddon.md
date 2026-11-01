---
date: 2026-10-31
session: vtcon-05 s36
status: phase done
---
# vtcon-05 session 36: ledger sweep, upkg appaddon proof

## Done (ledger rows ticked with "s36 2026-10-31")
- MENU-TOGGLE / MENU-BACKSPACE, INSTALL-STUCK-TCPSTACK, PKG-SWEEP-STOP, KIT-IXNET-NOREQ, KIT-TCP-STACK, PR 2 rig 8 checks row.
- UPKG-CURL-SOURCE-BUILD (ports PR #25 merged), CURL-HTTPS-TLS (ports PR #16; TLS-SYSROOT stays open).
- Row "base Uninstall leaves empty SYS:UP-Term" ticked: master nyhd2.hdf carries the empty drawer; 1304261e compares against Before.
- 2fdfa52d: install_rig ports() drops nano/zip (now add-ons; base 92/92 on rig 4); upkg_rig case `appaddon` (up-nano install, `nano --version` sentinel, remove) passed 11/11 on rig 4; upkg_rig.ensure_base()/uninstall_base() shared with upkg_browse_rig, so upkg_rig no longer needs a pre-installed base.

## Known
- tests/test_rig_fixtures.py has 6 failures that also fail on HEAD before 2fdfa52d (AddonCases x3, InstallerCases add-on icon, RigStart x2). Not caused by s36; not yet ledgered as a row of their own.
- `make dist` needs UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main (main ports checkout lacks SOURCES.txt).

## Next
- New row UPKG-ZIP-RIG: add an up-zip case like appaddon.
- Ports session item: upterm-ports mk/common.mk:123 `seq 1 0` on macOS; guard with $(if $($(1)_LHA_URLS),...) plus test.
- Open vtcon-05 rows: 3203 RIG-RECHECK-STALE, 3103/3105 RJ7, 2848 NVIM-PERF, 2921, 3128 RIG-AMITCP-UNSTAGED.
