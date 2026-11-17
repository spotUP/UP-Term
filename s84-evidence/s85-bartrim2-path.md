# S85 BARTRIM 2 path + wallpaper not updating (3.1, static read of patina/, no rig)

## Part 1: writers of ENV:Sys/palette.prefs reachable on 3.1 (all in patina/desk_dos.c)
Every Patina writer ends in a 3.1-safe form (desk_palette_31safe, desk_core.c:487-498, sets slot 11 = slot 10 when the list has >= 12 words):
- dk_put_palette :249-265 -> 31safe -> dk_put(0,...) -> dk_write(ENV) :184. Callers: dk_apply :571 (GUI pick AND hotkey, both via gt_dos_apply_saved gthemes_dos.c:207 -> desk_dos_apply :1320), dk_flat_pens :945, start apply :1854.
- dk_restore_targets :1500-1560 (GTA_REVERT / Revert / marker recover / dk_orig_off): dk_palette_31_fix(name) :1524 on the snapshot copy, then dk_env_fix_file :1529 and again :1536 (dk_stream_copy :1475). Copy source was made safe first.
- dk_palette_31_fix :1123 (start :1149-1154, dk_orig_restore_arc :1260): writes only safe form; skipped (DK_P31_NONE) when desk_palette_31safe returns 0 or bytes equal (desk_core.c:507-509).
- dk_editor("Palette") is reachable only when dk_palette_editor_ok() (>=47): :264 and :1531. Not on 3.1.
- Copies ENV->ENVARC (:1458, :1762, marker :1393) read ENV, never write it.
GUI and hotkey share dk_apply; GUI differences are only gt_dos_desk_restore -> desk_dos_flat_sync (gthemes_dos.c:152-158, :203) before the apply, and Keep/Revert requester afterwards (dk_confirm_ask). Neither writes the palette except via the paths above.

## Verdict
No Patina code path writes BARTRIM 2 on 3.1 (checked by tracing every dk_write/dk_stream_copy/dk_editor/dk_put target, not only grep for the filename). The only thing that forces 2 is Palette 40.2 USE itself (desk_core.c:224 comment), so the 2 in ENV came from:
 (a) SYS:Prefs/Palette run by someone/something else (owner, Prefs drawer, or an old Patina binary on the GUI side: s85-pal31-direct.md says 413460 B md5 053479e1 was deployed; confirm the owner's rig runs that one, the GUI run may be an older build that still calls dk_editor("Palette")), or
 (b) IPrefs rewriting ENV on its own notify (unverified), or
 (c) the 31safe skip: desk_palette_31safe returns 0 for a file with a pen list shorter than 12 words or unterminated -> dk_palette_31_fix leaves it. UNVERIFIED but cheap to check.
Discriminator: compare T:patina-desk/palette.prefs (work copy, 434 B 15:12:25) vs ENV:Sys/palette.prefs. Work file slot 11 = 0 and ENV = 2 => an outside writer after our write (a/b). Work file already 2 => 31safe skipped (c).
Also the live pens `... 2 2 0 2` are read from the DrawInfo; if IPrefs 40.7 only reads the file on reset, ENV and live agree, so the ENV 2 was there BEFORE the reset.

## Proposed root-cause fix (shared code)
Enforce the invariant at the single sink, not per caller: in dk_put_palette (desk_dos.c:249) after the dk_put(0,..) write, and after the IPrefs idle wait, read ENV back and, if slot 11 != slot 10 (or bytes != safe), rewrite via the same safe bytes (dk_write ENV). Same read-back at the end of dk_restore_targets for DT_PALETTE. Plus S85: log the work file tail and ENV tail in an am_sentinel so the next rig run shows which side holds the 2.

## Part 2: wallpaper (wbpattern.prefs) not updating from GUI on 3.1
S85-PAL31-DIRECT did not touch the WBPattern path: still dk_put("WBPattern",...) -> dk_editor (desk_dos.c:584, :170-186). Findings:
1. LIKELY (not rig-verified): ordering regression. Palette used to run through dk_editor, which waits for IPrefs idle (desk_editor_waits_iprefs, desk_core.c:255-258 lists Palette; dk_editor :126-146). Now dk_put(0,..) is a plain file write: no dk_wait_iprefs. dk_apply only waits for the reset when `reset` is set (:574-575). So the WBPattern editor starts at :584 while IPrefs 40.7 is still handling the palette change notify and Workbench reset; its ENV write (or the IPrefs re-read) is lost/raced (cf. S57-START-POINTER-HANG, :572 comment; "object is in use" Error 202 note :170). Fix: after dk_put(0,...) in dk_put_palette, when the write really happened (dk_put returned without skip), run the same idle wait the editor path did: idle = dk_iprefs_idle_mask() BEFORE the write, dk_wait_iprefs(idle) after (the dk_flat_pens site :938-947 already does this pair; dk_apply :571 does not).
2. S82 equal-bytes skip (dk_put :176-181): only skips when work == ENV byte-for-byte; a theme change yields different bytes, so not the cause unless the theme pattern equals the current one.
3. WALLPAPER-KEPT (desk_core.c:717-): if ENV already holds a picture chunk (ROOT/DRAWER/SCREEN, WBPF_PATTERN clear) it is kept byte for byte; only pattern/none gets the theme tone. If the owner's wallpaper on the rig is a picture, "not updating" is by design. Check: T:patina-desk/wbpattern.prefs vs ENV:Sys/wbpattern.prefs, and whether ENV has a PTRN chunk with flag bit 0 clear.
4. gate: desk_gate_mask (desk_core.c:1495-1503) keeps the WBPATTERN bit on 3.1 and sets ws_reset=1 so windows close around the editor; unchanged by S85.
Discriminator: if work file differs from ENV wbpattern.prefs after the pick => race (1) or editor refused; if equal and ENV is a picture => (3).
