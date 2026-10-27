# Why rig scripts select all text in the Amarchy text menu (2026-10-28)

Cause: tools/rig/rig.py install (_install loop, ~line 1021) refreshed only files already present in the rig's
VTC:. UPTerm was added to the quiet reader (typewin.quiet_screen_text, commit f4e96be9) after rigs such as rig 3
had their VTC: seeded, so VTC:UPTerm did not exist: `VTC:UPTerm ACTIVE screen FILE ...` answered rc 10
"Unknown command" (the shell's, not the handler's), quiet_screen_text returned None and screen_text() fell back to
Select all + Copy (typewin.py ~line 107). The handler side (upterm.c send_active, keymap_act.c slash:) is fine.

Fix: rig.py VTC_REQUIRED = ("UPTerm",): install copies it into VTC: even when absent.
Test: tests/test_rig_kit_shadow_keymap.py test_install_adds_upterm_a_rig_vtc_lacks (fails with the fix reverted).
Verified on rig 3: with a menuhost UP-Term window, the quiet command rc 0, screen_text() sent 0 keys.
Note: with no UP-Term window the command is rc 20 "no UP-Term window to ask" and the fallback still runs
(by design, for non-UP-Term windows).
