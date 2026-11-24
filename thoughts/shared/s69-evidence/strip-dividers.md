---
topic: DOPUS-GADGET-DIVIDERS, lister strip ("<", popup arrow, "Off") frames
status: NOT FIXED. Rig 8 could not show the strip; nothing measured on it. No code changed.
---
# What was done (2026-11-24, rig 8, main checkout build)
- Rig 8 default (KS 40.68 / WB 40.42, RTG 800x600x24) booted with DOpus5 assigned but DirectoryOpus NOT running and
  Patina's patina-flat running (STATUS flat=on). Started DOpus by hand (DOpus5:DirectoryOpus), STATUS then
  `dopus=68.16 patched=3 calls=1 flat=1 sums=05a329c5,aa2c535e,00000000` (3 of the DrawBox/FieldBox/bevel entries patched).
- `rx "ADDRESS DOPUS.1; command DeviceList NEW"` opens a 196x173 lister. Its grid (rows grabbed, run-length): title bar,
  list text, path field at the BOTTOM (x8..174, y150..160), arrows/size at the bottom right. It has NO "<" parent button,
  NO popup arrow and NO "Off" field and no toolbar strip: the default config of this rig has no such strip.
- Conclusion: the strip the owner means exists only in the owner's DOpus config (lister with path/title row + toolbar).
  Reproducing needs that config copied to the rig (DOpus5:System/ or ENVARC lister settings) or a lister layout command
  that shows the "<"/popup/"Off" row. Not found in this session.
- Static facts for the fix (no DOpus source in vendor/, binaries only): the frames of DOpus buttons are drawn through
  DrawBox / DrawFieldBox (patched, patina/flat.c d4_flat_pens, ring decided by flat_dopus_ring: raised DrawBox and
  FieldBox = no ring, tests/test_flat_core.c:702-730) and through the bevel body (d4_bevelbody, FLAT_EL_BEVEL, ring KEPT).
  The "Off" field is a FieldBox/DrawBox raised: no ring already. A frame that survives therefore comes from (a) the bevel
  body path (class-drawn "<" / popup button: flat_dopus_ring(FLAT_EL_BEVEL,0) == 1), or (b) inline Move/Draw in the
  DirectoryOpus program (lister_title.c, unpatched). Which one is UNVERIFIED.
# Next step
1. Get the owner's DOpus config (or `command` that shows the lister path row) onto a rig, open that lister.
2. ami.grab rows/columns around "<" popup "Off"; compare to flat pens (accent 7aa2f7 / ground 1a1b26 here).
3. If (a): make flat_dopus_ring return 0 for FLAT_EL_BEVEL when the rect is a lister-strip button (needs a discriminator,
   O7 says Environment class buttons keep the ring); if (b): a new patch point is needed.
