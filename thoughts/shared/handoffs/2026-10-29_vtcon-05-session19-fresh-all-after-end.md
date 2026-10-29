---
date: 2026-10-29
session: vtcon-05 session 19
topic: FRESH-ALL-AFTER-END (rig 9 fresh-all fails 3, 32, 38, 39, 40) and owner-question round
status: 38/39/40 committed, 3 fixed uncommitted, 32 open
---

# Handoff: vtcon-05 session 19

Ledger: thoughts/spot/todos.md row FRESH-ALL-AFTER-END (grep it) and the s19 sub-bullets under it.
Repro: `UPTERM_RIG=9 nice -n 19 python3 -u tools/rig/installer_rig.py ONLY fresh-all FORCE --purpose "<session>: <task>"`

## Done
- 9cfa774b: dist/Uninstall waits (Status COM UP-Term:bin/Amarchy, at most 10 s) before Delete ALL (fail 38).
  Test: tests/test_dist_installer.py WasabiDaemonOption.test_uninstall_waits_for_amarchy_to_end_before_the_drawer_goes.
- e67ad678: install_rig.stock_libs(); prepare() uses it (fails 39/40). Test tests/test_installer_rig_reboot_libs.py.
  NOTE: the call `ir.stock_libs()` at the end of installer_rig.reboot_amarchy_check is still UNCOMMITTED
  in tools/rig/installer_rig.py; commit it with the items below.

## Uncommitted in tools/rig/installer_rig.py
- reboot_amarchy_check: end calls ir.stock_libs() (belongs to e67ad678).
- reboot_amarchy_check: when no Amarchy window, logs `screen [<title>]: <windows>` for every screen (fail 32 diagnostic).
- layout_judged(tree, why): a window without Proceed 90 / Abort 91 (half-drawn page) is not layout-judged (fail 3,
  "text in the bottom border strip (y 204..205) on [None]"). Test tests/test_installer_rig_layout_judged.py (untracked;
  fails with the fix reverted).

## Open: fail 32 "no window titled Amarchy in 20 s"
- Rerun 3 log: /private/tmp/claude-501/-Users-spot-Code-UP-Term-vtcon/fc13214b-1e0f-4893-b597-80b8915bda65/scratchpad/fa-s19c.log
  (grep "screen \[" for the diagnostic).
- Hypothesis: tabstate.open_host('f1host') leaves a terminal in front; menu-in-terminal defaults to text, so
  Left Amiga + Space opens amarchy-menu in an untitled terminal, not the drawn "Amarchy" window. vtcon-32 hit the same
  in amarchy_window_rig (ledger "Amarchy s19 window rig DONE ca4e50e2").
- Fix if confirmed: reuse amarchy_window_rig.drawn_menu_prefs() / restore_prefs() around the keypress (no new code
  path). Regression test on the host like test_installer_rig_reboot_libs.py (prefs set before key, restored after).

## Rules for the next session
- Rigs 9-11 only. Rig 8 belongs to vtcon-32 until it says its DOpus probe is done.
- vtcon-32 owns OMARCHY-AI, AMARCHY-DESK-*, AMARCHY-PLANS, AMARCHY-DOPUS-DESKTOP, AMARCHY-PLUGINS, D12, D13, B2,
  D14/D11, OWN-PREFS M3. vtcon-05 owns D21, D9/D10 (--no-dopus), base kit DOpus filetype, Apps add-ons (rows marked
  "OWNED BY vtcon-05").
- Do not commit other sessions' dirty files (amarchy-session10 handoff, omarchy-ai plan, untracked `-`, scratchpad_*).

## Still open from s18
- Local main holds original commits, origin/main cherry-picked copies: rebase later.
- make dist-check fails on dist/wasabi/SOURCE.txt pin (be4c0559 vs pin; I657 other session).
- Plain `make dist` wipes build/dist/up-term without the ports kit (use UPTERM_PORTS=.../kit-main).
- Debug FS-UAE patch uncommitted in /Users/spot/Code/Emulators/fs-uae-debug.
- RIG-SLOW-BOOT waits for an owner decision.
