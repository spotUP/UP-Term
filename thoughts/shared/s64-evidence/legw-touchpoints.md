# LEGW touchpoints: amarchy-flat to Startup-Sequence, Amarchy + indexer to WBStartup (s64, read-only)

Method: grep over dist, tools, tests, Makefile, amarchy; line ranges read. Counts of "every" come from grep of the
three marker spellings (";BEGIN UP-Term Amarchy", ";BEGIN UP-Term Amarchy index", ";BEGIN Amarchy-Flat") plus
"amarchy-flat BOOT", "bin/Amarchy", "bin/amarchy-index", "unstartup". Not yet verified from a structure that produces the
set (no manifest of blocks exists; dist/install.dos is the only writer).

## A. Places that write / strip / check / document the three blocks

### Writers (single source)
- dist/install.dos:544-546  creates S:User-Startup if missing (stays: other blocks use it).
- dist/install.dos:547-566  Amarchy block: Search for guarded line "  Run >NIL: UP-Term:bin/Amarchy"; if WARN, backup
  S:User-Startup.before-UP-Term, `vsh unstartup.sh Amarchy`, copy back, Echo the 6-line block (;BEGIN UP-Term Amarchy ...).
- dist/install.dos:569-585  Amarchy index block, same pattern, `vsh unstartup.sh "Amarchy index"`.
- dist/install.dos:587-603  Amarchy-Flat block (";BEGIN Amarchy-Flat", no UP-Term in name), `vsh unstartup.sh "Amarchy-Flat"`.
- dist/install.dos:510-511  Copy of Amarchy amarchy-menu amarchy-gui amarchy-index amarchy-flat ... to UP-Term:bin (stays;
  WBStartup icons still need the binaries in UP-Term:bin or icons point to them via default tool).
- dist/install.dos:355      copies unstartup.sh to ENVARC:up-term/unstartup.sh (Uninstall copy; old kits keep an OLD copy there).
- dist/install.dos:829-837  `vsh unstartup.sh UP-Term` strips every ";BEGIN UP-Term" block on a move/reinstall (this already
  removes the Amarchy and "Amarchy index" blocks, NOT Amarchy-Flat).
- dist/install.dos:1324-1335  `unstartup.sh console` / `device` (not ours; shows the by-name pattern).
- dist/install.dos:862, 1034 comments mention unstartup.sh (no change).
- dist/reassign.sh          rewrites the ";BEGIN UP-Term assign" block only (no Amarchy blocks).

### Uninstall
- dist/Uninstall:81-85      finds unstartup.sh (updest copy, else ENVARC:up-term copy).
- dist/Uninstall:108-124    plain `$unvsh "$unst"` strips all ";BEGIN UP-Term" blocks (covers Amarchy + index), copies T:User-Startup.up-term back.
- dist/Uninstall:118-124    `$unvsh "$unst" Amarchy-Flat` strips the flat block by name. Must become the S:Startup-Sequence line removal.
- dist/Uninstall:127-134    PRECEDENT: `$unvsh "$unst" "" S:Startup-Sequence` + Copy T:Startup-Sequence.up-term back (RemLib block).
- dist/Uninstall:632ff area  (tests/test_dist_installer.py:632-641 checks it) waits for Amarchy process to end before drawer delete.
- dist/Uninstall: no code deletes anything from SYS:WBStartup today; icons + the two WBStartup entries need explicit Delete.

### unstartup.sh
- dist/unstartup.sh:1-45    the stripper (see B).

### Installer front ends / docs
- dist/Install.installer: no Amarchy block text (lines 59, 215, 376, 407-430, 582, 783 talk about User-Startup for other blocks; 215 "S:User-Startup that Uninstall takes out" is generic). Check wording at 215 once Startup-Sequence is also edited.
- dist/common/update.installer:76  "the blocks in S:User-Startup" (Update description; generic wording, add Startup-Sequence line + WBStartup icons).
- dist/common/oldopts.installer:8-18  reads UP-Term option blocks with `Search S:User-Startup ";BEGIN UP-Term <block>"`; Amarchy blocks not among the options (no change; verify block list there).
- dist/README.txt:63        Uninstall description "the blocks in S:User-Startup" (add: one line in S:Startup-Sequence, WBStartup icons).
- dist/README.txt:732       "a block in S:User-Startup starts UP-Term:bin/Amarchy" -> WBStartup icon.
- dist/README.txt:999       "a line in S:User-Startup" starts the index scan -> WBStartup icon.
- dist/README.txt:1009      "Amarchy starts it through WBStartup itself" (index rows launched by Amarchy; unrelated, same word).
- dist/up-term.conf:35, dist/keymap:200  mention amarchy-index only (no change).
- dist/parts.txt:35,60,61   ships unstartup.sh, amarchy-index, amarchy-flat. New .info files + a Startup-Sequence helper script need rows here.
- dist/packages.txt:53      uhc add-on uses Files/unstartup-uhc.sh (a COPY of unstartup.sh built per add-on: Makefile rule; a change to unstartup.sh flows there).
- dist/addons/{python,uhc,vbcc}/uninstall-body.dos and uhc/install.dos:26-32  call ENVARC:up-term/unstartup.sh <name> (arg contract must stay compatible).
- dist/iconlib/instremlib.sh, SOURCE.txt:18  the RemLib precedent (see B).

### Tests (existing, will break or need extending)
- tests/test_flat_startup_block.py (whole file, 50 lines): asserts the exact Echo lines, `vsh unstartup.sh "Amarchy-Flat"`, Uninstall `$unvsh "$unst" Amarchy-Flat`, by-name strip. Rewrite for the Startup-Sequence line.
- tests/test_dist_installer.py:930-957 (`blocks()` helper + `self.assertEqual(seen, 5)` at 955: "assign line, Amarchy, Amarchy index, Amarchy-Flat, GG") count drops to 2.
- tests/test_dist_installer.py:958-964 test_old_unguarded_block_is_replaced_on_update: keys '  Run >NIL: UP-Term:bin/Amarchy' / amarchy-index and `vsh unstartup.sh Amarchy\n` / `"Amarchy index"` must still exist as the strip-old-block calls (Update strips old blocks, per decision 2).
- tests/test_dist_installer.py:976-1000 test_unstartup_strips_every_guarded_block (skips non-UP-Term names; Amarchy-Flat note at 997).
- tests/test_dist_installer.py:632-641 uninstall waits for Amarchy (unchanged logic).
- tests/test_amarchy_docs.py:85-91  asserts install.dos contains 'Run >NIL: UP-Term:bin/Amarchy', ';BEGIN UP-Term Amarchy', ';BEGIN UP-Term Amarchy index', and `d.count('Run >NIL: UP-Term:bin/amarchy-index') == 2` (line 92). Update.
- tests/test_amarchy_docs.py near 85: also asserts Copy line for the binaries (stays).
- tests/test_rig_a500_user_startup.py:15-24  fixture with both UP-Term Amarchy blocks; tests rig strip (see rig.py 1052-1056). Rig-only, but its meaning changes (see E).
- tests/test_dist_iconlib.py:80-92  test of instremlib.sh + `unstartup.sh '' <file>` on a Startup-Sequence: the pattern to copy.
- tests/test_sh_exec.c:1927-2072  unstartup.sh semantics driven through vsh (name, plain, ";UP-Term off:", file arg). Extend if unstartup.sh gains a line mode.
- tests/test_dist_addons.py:462-478  python add-on unstartup usage (contract stays).
- tests/test_dist_solidwin.py:54-62  asserts unstartup.sh source text `';BEGIN UP-Term'|';BEGIN UP-Term '*) skip=1`: keep that text intact.
- tools/test_rig_upterm.py:182, tools/test_rig_start_guards.py:51-68, tools/test_rig_uhc_stays.py, tools/kitsize.py:48: only mention User-Startup in generic terms or other blocks (python, UHC, DOpus, wasabi). No Amarchy block text; kitsize.py has no Amarchy/WBStartup entry (new .info files add bytes, check its file list).

### Rig scripts that know the blocks
- tools/rig/rig.py:339-351  KIT_AMARCHY_BLOCK=";BEGIN UP-Term Amarchy"; comment RIG-BOOT-STALE-BIN.
- tools/rig/rig.py:386-402  inserts the rig's shadow block (BOOTX:shadows-up, user_startup.RIG_SHADOWS_BLOCK) directly BEFORE the kit's Amarchy block; no block => `go` runs the pass. With Amarchy in WBStartup (runs after User-Startup, after LoadWB) the shadow pass in User-Startup still precedes it, but the anchor disappears: falls to the "no Amarchy block" path.
- tools/rig/rig.py:631-633, 1051-1056  `--no-amarchy-block` strips ";BEGIN UP-Term Amarchy" from the clone; with WBStartup it must delete the WBStartup icon(s) instead.
- tools/rig/rig.py:1029-1064  strip_user_startup_blocks(image, name) (reusable; plain "UP-Term" strip at 1064 also hits Amarchy/index).
- tools/rig/amarchy_gui_rig.py:56-59, 1224-1264, 1307-1326  FLAT_BOOT = 'Run >NIL: UP-Term:bin/amarchy-flat BOOT' "the kit's S:User-Startup line, word for word"; comment at 57 names the User-Startup block. Update text + source of the line.
- tools/rig/install_rig.py:673-674, 884, 895-896, 1245, 1365  startup_before/sseq_before byte-for-byte compares. 674 comment "no part of UP-Term edits [S:Startup-Sequence]" becomes false: after Install the sequence has one extra line; after Uninstall it must equal sseq_before (884, 1365 already assert this: they become the regression check for the removed line). 875 `Search S:User-Startup ";BEGIN UP-Term"` is for GG.

## B. unstartup.sh and S:Startup-Sequence

- Format (dist/unstartup.sh:20-45): reads file line by line (IFS='', read -r), prints to T:<file minus "S:">.up-term. Args: $1 = block name ("" = all ";BEGIN UP-Term"/";BEGIN UP-Term <x>" blocks), $2 = file (default S:User-Startup). Named: skips from ";BEGIN UP-Term <name>" or ";BEGIN <name>" to matching ";END ...". Plain mode also turns ";UP-Term off: <line>" back into <line> (takecon.sh).
- With $2=S:Startup-Sequence the output is T:Startup-Sequence.up-term (precedent: Uninstall:127-134).
- It does NOT support a single marked line. A one-line marker has no BEGIN/END pair. Options that reuse the mechanism: (a) a third mode: line-level marker, e.g. the inserted line carries a recognisable fixed text and unstartup.sh gains a case that drops lines matching it (the marker must be part of the executable line, AmigaDOS scripts have no trailing comments: e.g. `Run >NIL: UP-Term:bin/amarchy-flat BOOT` itself is unique and can be the match key, or put `;UP-Term line` on the previous line and drop pair); (b) a BEGIN/END pair of two comment lines around the Run line (works with the existing script unchanged, but the owner said ONE line). Decision for owner/implementer; (a) needs a regression in tests/test_sh_exec.c plus test_dist_iconlib-style python test.
- Only existing editor of S:Startup-Sequence: dist/iconlib/instremlib.sh (insert RemLib block before SetPatch, writes T:Startup-Sequence.up-term, idempotent via grep of the marker, writes nothing if the anchor is missing) driven from dist/install.dos:1146-1161 (backup S:Startup-Sequence.before-UP-Term if absent, vsh script, Copy back CLONE QUIET, Delete temp) and stripped in dist/Uninstall:127-134. Test: tests/test_dist_iconlib.py:80-92. Template for a new `dist/instflat.sh` (anchor: line after AddDataTypes / before IPrefs; case-insensitive match as instremlib does at line 14).
- Other grep hits for "Startup-Sequence" are rig-only (tools/rig/rig.py bootcap wrapper, Startup-Sequence.os), not product code.
- RISK to check (not measured): S:Startup-Sequence runs BEFORE S:User-Startup, and the UP-Term: assign is made in User-Startup (";BEGIN UP-Term assign", reassign.sh). The old flat block was guarded `Assign >NIL: UP-Term: EXISTS` / `If NOT WARN`. A single line in the Startup-Sequence cannot rely on UP-Term:; it needs the absolute drawer path (the installer knows $updest, reassign.sh then must also rewrite this line on Move) or a guarded form, and `amarchy-flat BOOT` must not need other assigns (LIBS:, ENV:) at that point. Also the Updates/Move path (reassign.sh) is a touchpoint the decision list does not yet name.

## C. Icon generation

- tools/mkicon.py (docstring 1-14, args 140-160): writes a DiskObject .info with default tool, tooltypes, stack. Options: --tool NAME, --plain (no WINDOW tooltype), --wbtool (WBTOOL icon, no default tool; used for the app index), --tooltype T (repeatable), --stack N, --drawer, --x/--y. It already writes tooltypes, so a WBStartup tool icon is `mkicon.py OUT.info --wbtool --plain --tooltype DONOTWAIT --tooltype STARTPRI=10 ...` (check: the icon type emitted for --wbtool is WBTOOL; for WBStartup the tool name is the icon's own file name, so the .info sits next to a binary or a stub in SYS:WBStartup; a WBStartup project/tool icon needs the program file ITSELF in the drawer, or a PROJECT icon with default tool; decide: project icon with --tool UP-Term:bin/Amarchy needs the UP-Term: assign at WBStartup time, available after User-Startup, OK).
- Callers: Makefile:2011 (UP-Term.info), 2013 (UP-Term-Prefs.info, --tool C:UP-Term-Prefs --plain), 2036 (userland/bin/upkg.info --wbtool --plain --stack 262144), 2093 (launch icons, --tool C:IconX); tools/mkkit.py:65-67 (Install.info with Installer tooltypes APPNAME/MINUSER/DEFUSER, Uninstall.info --tool C:IconX --plain). Kit install of .info files: dist/install.dos:909-914 (`Copy UP-Term UP-Term.info SYS:System CLONE`).
- No existing WBStartup icon in the kit (no dist file or install.dos line writes to SYS:WBStartup; grep "WBStartup|DONOTWAIT|STARTPRI" over dist/tools hits only the rexx wb-autoarrange (an AppMenuItem), amarchy/wblaunch.c (launching a WBStartup message) and tests/amiga/wbfake.c).
- Other icon code is runtime splicing, not generation of kit icons: amarchy/iconfile_core.c (parse/splice .info, tests/test_iconfile.c), amarchy/iconapply_core.c, dist/iconlib. tools/gen_lucide_icons.py / gen_menu_icons.py produce image data, not tooltype icons.
- Existing test of tool-icon tooltype output: tests/test_dist_launch.py and tests/test_dist_manifest.py touch mkicon outputs (not read in detail).
- dist/parts.txt has .info rows only for UP-Term.info/UP-Term-Prefs.info (lines 82, 84): new rows needed for the WBStartup icons plus Makefile rules next to 2011-2013.

## D. How Amarchy finds the Workbench screen

- amarchy/amarchy.c:285-306 desk_start_gate(): comment says it all: "Amarchy starts from S:User-Startup before LoadWB opens it; ipd_lock_wb_screen never opens a closed Workbench (BOOT-31-WB-DEADLOCK)". Takes `ipd_lock_wb_screen()` (amarchy/iconpen_dos.c:~95-205, uses LockPubScreenList, not LockPubScreen); if no screen it returns and retries each 1 s tick (line 680 call at start, tick hook retries); start work (flat ensure, pens refresh, marker recover, backdrop, start apply) runs once when the screen is up (desk_start_due).
- amarchy/amarchy.c:311-325 wb_gate(): W43 hold, windows open only once Workbench runs its saved screen mode (wbm_check), with a timeout message.
- So yes, it already handles "Workbench not yet open" by polling; nothing needs to change for WBStartup (which starts AFTER the screen exists, so the hold simply ends sooner). Owner note says "waiting for the Workbench screen itself (LockPubScreen)": the code polls the public-screen LIST, deliberately not LockPubScreen; confirm with the owner which is meant. Comment at 285-287 ("starts from S:User-Startup before LoadWB") becomes stale and should be edited.
- amarchy/amarchy.c:522-556, 749: Amarchy replies WBStartup messages for launches it makes itself; with Amarchy now started BY Workbench from WBStartup, it gets a WBStartup startup message from WB (check whether main() calls GetMsg on the process message port when argc==0, and replies before exit; startup from a CLI vs WB differs: argc==0 under vbcc/ixemul-less code, see the main path ~line 740-760). Not verified.

## E. Reachability test / rigs

- Closest existing reachability test for install.dos content: tests/test_dist_installer.py (reads dist/install.dos as text DOS and runs the shell scripts through `sh` with S:/T: rewritten: helper `sh()` at ~966; test_unstartup_strips_every_guarded_block 976-1000) and tests/test_flat_startup_block.py (same approach for the flat block). These are static/text plus script-run tests, not an Amiga run.
- Real entry-point rigs: tools/rig/install_rig.py (boots a rig, runs Install/Update/Uninstall; compares S:User-Startup and S:Startup-Sequence byte for byte after Uninstall: lines 673-674, 884, 895-896, 1245, 1365; add Startup-Sequence line check after Install and WBStartup contents check) and tools/rig/installer_rig.py (Installer-driven; D10 case failed on Amarchy blocks, see tests/test_rig_a500_user_startup.py docstring).
- Boot-started process checks: tools/rig/amarchy_gui_rig.py:358 `process_running('Amarchy')`, 1224-1326 FLAT_BOOT run by hand (`Run >NIL: UP-Term:bin/amarchy-flat BOOT` then process_running('amarchy-flat'); STATUS flat=on/off). That hand-run line becomes a lie once the line lives in the Startup-Sequence; a boot-level check (reboot, then Status shows amarchy-flat/Amarchy/amarchy-index) is the real reachability test. Also tools/rig/reboot_deaf_rig.py (reboot flow) and rig.py boot scripts (BOOTX:go beside the Startup-Sequence; bootcap wrapper; rig.py 339-345 shadows-up ordering needs re-thinking: the shadow pass must still precede Amarchy and flat; flat now starts earlier than User-Startup, so the rig's UP-Term: shadow may not exist when flat BOOT runs).
- rig.py:1052-1056 --no-amarchy-block flag must now also remove WBStartup icons (A, rig section).
- No existing rig asserts amarchy-index running at boot (grep of installer_rig/install_rig/amarchy_gui_rig: none).

## Ledger of decisions needed from owner (surfaced, not decided here)
1. Single-line marker in Startup-Sequence: how unstartup.sh recognises it (new line mode vs match key) and the exact guarded text given no UP-Term: assign yet (absolute path from $updest; reassign.sh on Move).
2. WBStartup icon type (PROJECT with default tool UP-Term:bin/X vs TOOL with binary copy).
3. Whether "waits for Workbench screen" stays the list-based poll.
