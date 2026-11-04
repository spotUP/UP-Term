# palette.prefs: what a 3.1-safe PALT is, who writes it, and the fix shape (s56, host-only)

Status: Q1 is partly unproven. The 3.1-saved reference file was not kept and no NDK palette.h is on this host (a disk-wide find timed out), so the layout below is derived from our own writer (amarchy/desk_core.c:215-285) and the golden bytes, not from the NDK header. The cause inside intuition is unknown.

## Q1 Format

Layout, confirmed by the writer constants PAL_4PENS 16, PAL_8PENS 80, PAL_COLORS 144 and DK_PALETTE_SIZE 434: FORM, PRHD(6), PALT(400). The PALT body is struct PalettePrefs: pap_Reserved[4] (16 bytes), pap_4ColorPens[32] WORD (64), pap_8ColorPens[32] WORD (64), pap_Colors[32] ColorSpec of 8 bytes (index, r, g, b; 256). 16+64+64+256 = 400.

The golden tests/data/desk/palette-gui-save.prefs (3.2 Palette v47 save), body at file offset 0x22: reserved all zero; 4-colour pens at 0x32 are 0,1,1,2,1,3,1,0,2,1,2,1,2 then ffff at 0x4c (13 pens, then terminator); the 8-colour list at 0x72 is identical, ffff at 0x8c; colours at 0xb2: entry 0 = 1315 a9a9 07c1, 1 = 5555 dddd 5555, ... entry 7 = eeee 9999 0000, terminator ffff at 0xf2, rest zero. So the structure is well formed: both pen lists end in ffff, no pen at or above the colour count in the 8-colour list, ColorSpec list terminated, reserved zero. Our own writer files (palette-flat-*, palette-p1a-*) have the same shape (13 pens + ffff, 8 colours + ffff).

Differences from what 3.1 writes, as far as bytes can show: (a) 13 pen words before the terminator. The 3.1 stock pen set has 12 entries (DETAILPEN..BARTRIMPEN, NUMDRIPENS 12); the golden's first 12 words are exactly the 3.1 stock set 0,1,1,2,1,3,1,0,2,1,2,1 and the 13th word (2) is not a 3.1 pen. This is the strongest suspect and matches the "26 bytes differ" count only partly (2 lists x 4 bytes = 8 bytes; the remaining 18 would be colour words, e.g. entry 0 is 1315/07c1, not the nibble-replicated values 3.1 stores). (b) Colour words that are not nibble-replicated (1315, 07c1; the flat files use 1313/0707). Both are hypotheses. How either leads to AddHead on a NULL-based list in intuition is unknown; nothing in the repo or evidence reads the 3.1 ROM code.

A cheap way to settle it without the missing file: take the golden, truncate both pen lists to 12 entries plus ffff (zero the 13th), requantise colours to nibble replication, and run the Copy split on one 3.1 rig; then bisect the two changes.

## Q2 Writers of palette.prefs

Amarchy code (all via dk_put to DK_WORK/palette.prefs, then Palette USE or the editor applies it to ENV:/ENVARC:):
- amarchy/desk_dos.c:425 desk apply (theme pick). Gated: desk_dos.c:397 desk_gate_mask(mask, dk_palette_editor_ok(), ...) drops the DT_PALETTE bit below Palette 47. Cannot reach 3.1 today.
- amarchy/desk_dos.c:718 flat-pens sync/restore (dk_flat_pens). The gate for this path was not read; unverified whether it checks dk_palette_editor_ok. Check before relying on it.
- amarchy/desk_dos.c:1474 start-time write (DESK_START_PALETTE plan, FLAT-PENS-STALE-FILE part 2). Unverified whether the plan is gated on 3.1.
- Revert/original restore (dk_orig_save/dk_orig_restore, desk_dos.c:885-1010, the file table at desk_dos.c:1226 and desk_core.c:1003): copies the user's saved original back into ENV:/ENVARC:. If the original was saved by 3.2 Palette on a shared disk, restoring it on 3.1 re-creates the crash file. Reaches 3.1.
- Builders: desk_palette (desk_core.c:~273), desk_palette_theme, FLAT-PENS-LIVE rewrite (desk_core.h:126). They produce the 13-pen layout.
Not writers: no Installer script, theme pack (dist/amarchy-themes holds .theme files only), dist/ or kit file ships a palette.prefs (git grep over tracked files). Test data tests/data/desk/palette-*.prefs and the rig scripts tools/rig/amarchy_desk_rig.py:148,181,190,457, amarchy_marker_rig.py:15, amarchy_persist_rig.py:11 only read or delete it (the desk rig's own trigger `Palette FROM stock USE` at about line 181 wrote it on 3.1 and fired DESK-31-CRASH first).
The user's own 3.2 Palette save (disk shared between 3.2 and 3.1 boots) is the other real way a 3.2 file reaches a 3.1 IPrefs.

Decision today: desk_palette_editor_ok = desk_ver_major(Palette binary) >= 47 (desk_core.c:1190); desk_gate_mask drops DT_PALETTE and DT_ICONTROL below it (desk_core.c:1195); desk_iprefs_idle (desk_core.c:208) is only the wait-for-IPrefs-idle test, not a version gate.

Ledger (thoughts/spot/todos.md): DESK-31-CRASH line 3171 (confirmed IPrefs 40.7 freeze, trigger `Palette FROM stock USE`); FLAT-31-PALETTE-HANG line 3264 says ANY write freezes 3.1, "flat file or stock file, and a bare Copy". It does not say which stock file or who saved it. It is already contradicted: line 4057 (S32-31-FREEZE-CULPRIT) records an owner GUI Save in the 3.1 Palette editor leaving the guest alive, and the 21:46 silence as a wasabid snoop-stream hang; line 4085 (FSUAE-31-PALETTE-UPSTREAM) ties the crash to a 3.2-saved file. So "any write" is wrong; the stock file in that row was probably a 3.2-era file. Unverified, the row's source is scratchpad palette-hunt.md, not read.

## Q3 Fix shape

1. Add a pure function desk_palette_31safe(in, inlen, out, cap) in amarchy/desk_core.c next to desk_palette_theme: parse with the existing PALT locators (desk_core.c:374, :432), copy pens, truncate each pen list to the 12 pens 3.1 knows plus ffff and zero the rest, drop the 13th pen, nibble-replicate colours, zero reserved, keep 434 bytes. Reuse put_pen_tables/palette_fill so the single source of the layout stays in one place (make the pen count a parameter instead of DK_SLOTS).
2. Host tests in tests/test_desk.c: golden gui-save in, byte-compare against a hand-made 3.1 layout; idempotent; a 3.1-format input passes unchanged.
3. Route every writer at desk_dos.c:425, 718, 1474 and the original-restore copy through it when the Palette editor major is below 47; then lift the palette gate on 3.1 (desk_gate_mask palette bit stays) and apply colours through the safe file. Also sanitise a 3.2 file found in ENV:/ENVARC: at Amarchy start on 3.1.
4. Rig checks needed (not host-provable): first the bisect above on one 3.1 rig (recipe from fsuae-31-palette-upstream.md "Split runs": fresh boot, Copy to ENV:Sys/palette.prefs, 30 s wait, check guest alive), golden vs 12-pen variant vs requantised variant; then one 3.1 desk-apply run with the gate lifted via tools/rig/amarchy_desk_rig.py (sentinel: desk_dos_start_writes / desk_dos_flat_pens_writes counters), plus a 3.2 rig run to prove the 3.2 path is unchanged.
5. Step 1 must not be coded before the bisect names the offending field; the pen-13 theory is a guess.
