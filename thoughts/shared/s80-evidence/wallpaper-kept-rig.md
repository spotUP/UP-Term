# Wallpaper kept on a Patina theme apply: rig 9 (AmigaOS 3.2, --max), 2026-11-14

Verdict: file check PASS (ROOT picture chunk and file name kept byte for byte, DRAWER got the theme tone); on-screen picture check INCONCLUSIVE (see Screen).

Build: build/amiga/Patina 411780 bytes 19:18, newer than patina/desk_core.c (18:27). Staged with patina_gui_rig.setup() minus patina-menu (its make target was stale and is not part of make amiga); Patina ran from VTCX:Apps/bin.

How applied: (1) hand-made ENV:Sys + ENVARC:Sys/wbpattern.prefs, 358 bytes: ROOT = picture chunk (flags 0, depth 3, name SYS:Prefs/Presets/Backdrops/default_pal.iff, 44 bytes with NUL), DRAWER = 16x16 checker pattern (flags 1), SCREEN = none (flags 1 depth 0). (2) START apply: patina_desk_rig.patina_restart (QUIT, start). (3) THEME PICK: Style > Patina theme > vantablack through patina_desk_rig.pick() on the original file put back (PATINA_DESKS 0 -> 1, DESKMASK 15). The Keep click failed (no 'Patina desktop theme' window), file read 15 s later and again later: unchanged.

## Hex (chunk header bytes: PTRN size, which, flags, revision, depth, datalen)
Before ROOT   : 5054524e0000004400000000000000000000000000000000000000000003002c5359533a50726566
Before DRAWER : 00000078000000000000000000000000000000000001000100030060aa55aa55aa55aa55aa55aa55
After start   ROOT  : 5054524e0000004300000000000000000000000000000000000000000003002b5359533a50726566
After start   DRAWER: 00000078000000000000000000000000000000000001000100000060000000000000000000000000
After pick    ROOT  : 5054524e0000004300000000000000000000000000000000000000000003002b5359533a50726566
After pick    DRAWER: 00000078000000000000000000000000000000000001000100000060000000000000000000000000
Name before: 000000000003002c5359533a50726566732f507265736574732f4261636b64726f70732f64656661756c745f
Name after : 000000000003002b5359533a50726566732f507265736574732f4261636b64726f70732f64656661756c745f

Differences before vs after (start and pick identical): chunk size 0x44 -> 0x43 and datalen 0x2c -> 0x2b in the ROOT chunk (offsets 34, 58; the name bytes and total file size 358 are the same), and DRAWER depth 3 -> 0 with all-zero data (pen 0 flat tone). desk_core copies the picture chunk verbatim, so the length 44 -> 43 is most likely the stock 3.2 WBPattern/Workbench rewrite after the write (patina_desk_rig.py documents that stock editors normalise ENV:Sys after USE; depth 0 on an all-zero chunk is the same case). Not proven which process did it. ENVARC:Sys/wbpattern.prefs stayed unchanged (persist is not part of the apply).

## Screen
Workbench in backdrop mode (Right Amiga + B) shows flat light grey (e5e5e5) in the empty area after the apply, and the same flat grey after the ORIGINAL file is put back without Patina (control). So the screen cannot tell the picture from the pattern here: either default_pal.iff is a uniform grey, or Workbench did not render my hand-built chunk. Needs an owner/real picture check: set a coloured ILBM through the WBPattern editor, then apply.

PATINA_DESKS stayed 0 after the start apply (it rose only on the pick); the start apply's file rewrite came with no counter bump: unverified whether Patina's start path or the stock editor wrote it.
