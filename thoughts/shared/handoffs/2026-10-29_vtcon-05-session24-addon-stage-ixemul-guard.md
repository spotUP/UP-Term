---
date: 2026-10-29
session: vtcon-05 s24
status: closed
---
# vtcon-05 session 24: add-on staging, ixemul probe guard, stale Uninstall row

Ledger: thoughts/spot/todos.md, grep "s24 RIG-AGENT-STAGE", "s24 UNINST", "s24 SIGRC", "s24 CLAUDE-SETUP", "s24 RIG-PYTHON".

Done (all pushed to origin main):
- fbef94a8 rig.py stages current C:Claude and C:amiga-pi at every start through kitset.ADDONS shadow rows
  (amiga-pi comes from the sibling repo ../amiga-pi/build/amiga). Rig 10 md5 proof. tests/test_rig_addon_stage.py.
- e2decb83 ixpty_rig.use_ixemul no longer copies libs into VTC:ixp6; booted_ixemul() stops every probe whose rig
  booted a different ixemul/ixnet than IXEMUL=/IXNET=. Fix: restart the rig with the same IXEMUL=/IXNET=.
  tools/test_rig_booted_ixemul.py, make test ONLY=rig_booted_ixemul (python-only row).
- UNINST-EMPTY-DRAWER row was stale: 46d66840 already fixed it; rig 9 base check 92 and all check 73 pass.

Open (new rows):
- CLAUDE-SETUP-SWALLOW: line typed at the Claude setup/login prompt on a rig is swallowed.
- RIG-PYTHON-UHC-FAIL: install_rig --only all fails python add-on and uhc (amisslmaster.library v5 missing).
- --orig path of use_ixemul (LIBS: DH0:Libs) has no booted-lib guard.
- test_rig_kit_shadow_keymap amnotify failure belongs to the Amarchy session.

Rigs 9 and 10 stopped by their agents.
