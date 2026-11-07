# B5 stack frames / am_prefs by value: STATE, NOT DONE (nothing committed)

Stopped at the tool-call budget. Working tree holds an uncommitted partial change in /Users/spot/Code/UP-Term/vtcon-s61-rig96
(patch copy: /private/tmp/claude-501/-Users-spot-Code-UP-Term/59d7869f-00a7-443e-824b-0e3cb1d5d3aa/scratchpad/b5-partial.patch, tests/test_amarchy_frames.py is untracked and new).

## Counts
- `am_prefs x;` declarations by value: 39 lines in amarchy/*.c + handler/vtcon_handler.c (grep `am_prefs [a-z_]*;`); 3 are static (icons.c:207, index.c:52, none on stack), the rest on the stack.
- am_prefs = AM_NKEYS 29 * UC_MAX_VALUE 160 = 4640 bytes.
- Frame scan BEFORE (tests/test_amarchy_frames.py, ceiling 2048, binaries built 10:15-10:24): 132 frames over the ceiling:
  Amarchy 44, amarchy-gui 44, amarchy-flat 12, amarchy-index 11, amarchy-icons 16, AmTile 5. Named list (from vc -S of the Amarchy sources) in
  /private/tmp/claude-501/-Users-spot-Code-UP-Term/59d7869f-00a7-443e-824b-0e3cb1d5d3aa/scratchpad/frames-before.txt: mn_levels_t 8804, ix_drop_shadowed 8708, lf_setting_labels 7892, lf_setting_text/set_colour/pick 7788, win_active_terminal 6112,
  lf_setting_pick 6088, gt_roles_of 5660, desk_dos_* 4640-4964, gt_se_add_rule 4608-4760 (x6), gt_dos_* 4640-4684, amconf_migrate_with 4644,
  shot_default_dir 4640, shot_take 4040, lf_theme_apply 3308, lf_profile_new 3180, lf_reset / lf_palette_hex 3148.
- The ledger's three frames are mn_item it[MN_MAX_ITEMS] arrays (mn_levels_t, ix_drop_shadowed: 8.7 KB each); the same array sits in the lf_setting_* family (7788 = mn_item array + am_prefs).

## Analysis (invariant: no Amarchy function keeps a frame above 2048 bytes; 2048 = four 512-byte path buffers, nothing in an ordinary function needs more)
- Level 3 (shrink am_prefs): REJECTED. Only the path-list keys (music/demo/hide/skip/volumes/shot-dir/font) need 160; shrinking the stride would truncate those, and settings_def.c addresses fields by offsetof+k*UC_MAX_VALUE.
- Level 2 (one static owner): REJECTED. bar.c starts a Process that calls ipd_lock_wb_screen (iconpen_dos.c), which loads am_prefs: a shared static races with the main task. Sites also hold edited copies across calls.
- Level 1 as ONE helper pair (CHOSEN, not hand-rolled AllocVec): amconf_borrow()/amconf_return() in amarchy/amconf_dos.c/.h (heap, never NULL; static defaults on OOM). Core files (amconf_core.c, leaf_core.c use amconf_load_with(io,..)) need a malloc twin in amconf_core.c: not done.

## Done in the tree (uncommitted)
- amconf_borrow/amconf_return added; 28 DOS-side sites converted by script (desk_dos 10, gthemes_dos 4, gui 3, iconpen_dos 2, amarchy.c 2, flat 2, tile 2, shot 1, win 1, handler/vtcon_handler.c 2) with return-paths wrapped. Amarchy and amarchy-gui compile clean.
- Reviewed by eye only: diff of iconpen_dos, shot, flat, handler, desk_dos prefix. NOT reviewed: gthemes_dos, gui, tile, amarchy.c, win, rest of desk_dos (check every early exit gets amconf_return; the script handles return/tail, not break/continue/goto; desk_dos had one flagged block, handler two switch-case blocks fixed by hand).
- After this partial change the 4640 frames from those sites are gone in Amarchy/amarchy-gui (the remaining 4640 ones are leaf_core/amconf_core/shot_take paths).

## Still open (nothing below is done)
1. leaf_core.c (8 am_prefs sites, via amconf_load_with(io)), amconf_core.c (amconf_migrate_with, amconf_save_fields_with): need a core-level alloc pair.
2. mn_item it[MN_MAX_ITEMS] arrays: mn_levels_t, ix_drop_shadowed, lf_setting_*; check whether the callbacks re-enter mn_children before choosing static scratch vs heap.
3. gt_se_add_rule (4608-4760), gt_roles_of 5660, win_active_terminal 6112, shot_take 4040, flat/index/icons/AmTile binaries (shared files above).
4. Wire tests/test_amarchy_frames.py into Makefile dist-check beside test_amarchy_stack.py (line ~2073-2081); fail-before run: 132 over 2048 (verbatim output reproducible by running the test on the unmodified binaries).
5. C unit tests for touched files, new deepest-chain estimate, proposed stack size, git diff --stat, SHA.
Note: the frame scan on the binary also needs the same kept test; the scanner matches link.w/link.l/lea/suba.l openings in HUNK_CODE only.

# Part 2 (B5b): finish conversions, core pair, remaining frames, scan at 4096

## Invariant and design
No function holds a settings struct, a full menu or a parsed-file pair by value. am_prefs (4640 bytes) lives on the heap per call
through ONE helper pair. A static owner was ruled out: bar.c's Process calls ipd_lock_wb_screen, which loads am_prefs, so a shared
static would race. Shrinking the am_prefs stride was ruled out: it truncates path values.
- Core pair (amarchy/amconf_core.c): `amconf_borrow_with(io)` / `amconf_return_with(io, p)`. amconf_io gained optional `alloc`/`release`
  (0 = malloc/free). Never returns NULL (static defaults on OOM); returning NULL or the fallback is a no-op.
- DOS side: dos_alloc = AllocVec(MEMF_ANY), dos_release = FreeVec; `amconf_borrow()` / `amconf_return()` are wrappers over the core pair
  with dos_io (one implementation).
- Host builders (tests/test_amconf.c io_of, test_gthemes.c 3 builders + 4 memset builders, test_keymap.c tw_io) fill alloc/release with
  mf_prefs_alloc/mf_prefs_release (tests/memfs.h).

## Exit-path review (one exit per function, or return the borrow right after last use)
- amconf_core.c: amconf_migrate_with (borrow, save, return, map result), amconf_save_fields_with (single exit; OOM -> PREFS_INSTALL_WRITE).
- leaf_core.c: 7 sites converted; lf_setting_labels borrows lazily, returns at end; lf_setting_text clears its pointer after the return.
- desk_dos.c restructured to single exits; gthemes_dos.c (gt_dos_screens_save, gt_dos_select single return); amarchy.c/tile.c welcome_maybe
  and cfg_save braces fixed. gui.c, flat.c, iconpen_dos.c, shot.c, win.c, handler/vtcon_handler.c reviewed: no leaks found.
- ASan host build (-fsanitize=address,undefined) ran the suites below clean.

## Remaining frames fixed (not am_prefs)
- gt_roles_of: gt_model (~4.6 KB) on the heap (OOM -> all-black roles).
- gt_se_*: six Screens-page functions each held gt_screen_row[24] (4608 bytes); now one static scratch (gt_se is already single static state;
  se_rows rebuilds the same bytes on nested calls).
- ia_skip_note region (apply_one) 5072 and ia_restore 4864: two li_file each; now ia_ctx.lf_a/lf_b (ia_ctx is heap in icons.c).
- li_splice 4132: output entry list is uint16_t (index | LI_FROM_INS) instead of pointer+int.
- lf_load_listers 6068: ix_row[16] malloc'd.
- win_active_terminal 6112 and shot_take 4040: gone after the am_prefs conversion (not in the scan).

## Scan at 4096 (tests/test_amarchy_frames.py, default ceiling now 4096)
Amarchy: 2 over (8804, 8708). amarchy-gui: 2 over (8804, 8708). amarchy-index: 2 over (8708, 8804). amarchy-flat 0, amarchy-icons 0, AmTile 0.
All remaining are mn_item it[MN_MAX_ITEMS] frames (mn_levels_t, ix_drop_shadowed, lf_setting_*): part 3. The scan is NOT yet wired into
the Makefile dist-check (it would fail on those); wire it in part 3.

## Host tests (ASan build)
amconf 899 checks 0 failed; gthemes 8083/0; keymap 21387/0; iconapply 166/0; iconfile 170/0; iconpen 2554/0; flat_role_core 48/0; desk 2346/0.
