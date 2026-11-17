# S85 why rig 84 had no UP-Term: assign (2026-11-15)

## Finding
The assign WAS made at boot and was removed LATER by something that ran on the live rig. The boot files are not at fault.

Evidence (rig 84, read-only, no reboot):
- S:User-Startup lines 52-56 (kit block, written 20:52): `If EXISTS "Rushhours:Tools/UP-Term"` / `Assign UP-Term: "Rushhours:Tools/UP-Term"`. File reached its end (boot.log "UP-Term shadows done").
- Lines 57-58 `Assign Python3: UP-Term:Python3` and 75-83 `Assign GG: UP-Term:` (only when GG: was missing and UP-Term: EXISTS): the live assign list shows
  `Python3 Rushhours:Tools/UP-Term/Python3` and `GG Rushhours:Tools/UP-Term`. Both resolve through UP-Term:, so UP-Term: was assigned during boot.
- BOOTX:go (real path BOOTX:go, DH2) lines 72-75: `Assign >NIL: UP-Term: EXISTS` / `If WARN` / `Assign >NIL: UP-Term: VTC:Apps` is only a fallback; it did not fire.
  go lines 76-87 then made Rushhours:Tools/UP-Term/Patina/Themes (dir stamped 20:59:51, during boot): UP-Term: existed then.
- S:Startup-Sequence has no UP-Term line; BOOTX:s/startup-sequence has none either.
- Now: `Assign UP-Term: EXISTS` -> VTCX:Apps (the hand-set VTC:Apps, VTC: is an assign to VTCX:). Kit target Rushhours:Tools/UP-Term is no longer in it (not even ADDed).

## Generator
go is written by tools/rig/rig.py (the text around rig.py:541-576, fallback UPTERM_ASSIGN at rig.py:418-422). Rig 84's go (5036 B, 20:56:16) was written by the current start; lines 72-75 match UPTERM_ASSIGN. Not byte-diffed.

## Who removes an assign (repo, bare / REMOVE forms)
- tools/rig/install_rig.py:197 drop_upterm_assign (bare `Assign UP-Term:`; restore_upterm_assign puts it back)
- tools/rig/assign_rig.py:140,157 (bare)
- tools/rig/claude_datadir_rig.py:127,158; histfile_rig.py:74; amigapi_rig.py:364,373,575,695,784 (REMOVE, restore with a fixed VTC target, not the kit's)
- tools/rig/standalone_rig.py:679 (REMOVE)
- tools/rig/patina_menu_rig.py:497 x9_end: `Assign UP-Term: REMOVE` when x9_upterm_target() found none at x9_begin
- tools/rig/patina_gui_rig.py:1281-1283 --setup re-sets VTCX:Apps then ADDs the kit (the live single VTCX:Apps is NOT this form, it lacks the ADD)
No Patina/installer C code or dist/install.dos line removes the assign (grep of .c/.h/.dos found none).
Not decidable which one ran on rig 84: no command log on the rig (no Shell history) and the evidence files do not name a harness run.
The Patina theme-save harness (patina_theme_save_rig.py) and the helper scripts dep.py/st.py/ls.py contain no assign-removing call.

## Proposed fix (not applied; no source edits allowed)
- Any harness that drops the assign must use install_rig.drop_upterm_assign/restore_upterm_assign (RIG96-ASSIGN-LOST), restoring the KIT target, not a hardcoded VTC drawer: amigapi_rig, claude_datadir_rig, histfile_rig, assign_rig, standalone_rig, patina_menu_rig x9_end.
- Cheap guard: rig.py start/ensure-up check `Assign >NIL: UP-Term: EXISTS` after boot and re-run UPTERM_ASSIGN (rig.py:418) plus the kit target from the User-Startup block (rig.py:940 parser) when absent.
- To find the culprit on a repeat: add `Echo >>BOOTX:boot.log` after the `Assign UP-Term:` guard in go, or poll `Assign UP-Term: EXISTS` between harness steps.

## Rig state left
Rig 84 not rebooted. UP-Term: = VTCX:Apps (hand-set, untouched). Nothing changed on the rig.
