---
date: 2026-11-16
session: vtcon S85 (Patina flat-ask / DOpus backdrop)
status: S85-FLAT-ASK, S85-DOPUS-BACKDROP, S85-KEEP-USER-SETUP done (owner rig 84 PASS)
---

# Handoff: S85 DOpus desktop picture

## Done
- Commit 032456f8 "Ask about the Directory Opus desktop picture on theme apply" (on main).
  desk_dopus_picture_parse in patina/desk_core.c (+ unit test in tests/test_desk.c);
  patina/desk_dos.c queries `dopus query background desktop` over DOPUS.1, asks Keep / Use theme colour,
  saves the path to T:patina-revert/dopuspic, clears + refreshes; revert sets it back; confirm deletes the file.
- Owner test on rig 84 (DOpus 5 as Workbench, 800x600x24): question shows, Keep keeps picture and mode,
  Use theme colour gives flat colour, Revert restores the picture. All PASS.

## Rig 84 finding (not a Patina bug)
- No IFF picture loaded: ilbm.datatype v43.10 sat on picture.datatype v40.4 (dated 01-Nov-26, installer unknown).
  MultiView said "Unknown data type"; DOpus "can't load picture".
- Fixed with owner OK: picture.datatype 43.41 (from ~/Library/CloudStorage/Dropbox/WB/Classes/DataTypes)
  copied to rig 84 SYS:Classes/DataTypes; old one kept as picture.datatype.v40. Evidence: scratchpad dt84.md (may be gone).
- Open question: who installed the v40.4 file on 01-Nov, and do other rigs with the same system disk have it.
- SnoopDOS "can't open ENV:Patina/screen-themes" is expected: optional file, defaults when missing.

## Open
- S85-DOPUS-THEMES (ledger, after this): can Patina write/apply a DOpus 5 theme so DOpus desktop and lister colours follow the Patina theme.
- Rig 84 leftovers: WBPattern ROOT = Rushhours:Graphics/2D/ImageFX/BackDrop.iff (test leftover, ENV+ENVARC);
  DOpus desktop background set by owner to Welcome.iff; temporary `Assign DOpus5: SYS:Opus5` (gone after reboot).
- Unverified: palette wait may run the full 6 s on deep screens with 3.1.
- Not ours, uncommitted: thoughts/shared/research/test-register.md (never touch), s84-evidence/ files.
