# Why DOpus opens text files in its own viewer on the A1200 (2026-10-26, read-only)

Cause: nothing ever installs the UP-Term filetype into DOpus5:Filetypes. It exists only as a generator (tools/gen_dopus_filetype.py) and golden-byte test.

Evidence (A1200, ami.py exec):
- `Assign`: DOpus5 -> System:Opus5; UP-Term -> Rushhours:UP-Term. `Version DOpus5:DirectoryOpus` = 5.91 [OS3].
- `List DOpus5:Filetypes ALL`: only stock types (Text, ASCII, All, Archive LHA ...). `PAT=#?(erm|UP)#?` matches nothing: no "UP-Term" type.
- `List UP-Term:bin PAT=uterm#?`: empty. uterm-edit, uterm-view (and uterm-start/open) are absent; kit VERSIONS = vtcon b66f953c, a8108aff (the launchers) is NOT an ancestor of b66f953c (on origin/main only).
- Stock Text type bytes (fetched): name Text, priority 0, match `text`/`asci` content. ASCII: priority -1.
- Repo (origin/main): grep "filetype|gen_dopus" in dist/addons/dopus, tools/mkinstaller.py, Makefile install steps = no install step. Filetype command is `Run >NIL: Execute UP-Term:bin/uterm-edit "%s"` (double-click), uterm-view on User1.
- Ledger todos.md: line 2210/2336/2338 "D13 base-kit filetype install when DOpus5: exists" = still open ([ ] "Code follow-ups").

Secondary problems (unverified, found in the generator):
1. Priority -10 is below stock Text (0) and ASCII (-1); if DOpus tries higher priority first, a content match on Text wins over our name match. Check DOpus 5.91 ordering in source before choosing the number (use >= 1).
2. Our match is by name only: `#?.(c|h|txt|md|rexx)`; extensionless text files (README, Makefile) still go to DOpus Text.
3. Generated for the v5.104 FORM OPUS layout; 5.91 file layout vs 5.104 filetype_tool encode is unchecked (stock 5.91 files parse with the same TYPE header offsets: name32, id8, flags, prio byte at +41).
4. DOpus reads Filetypes at start or on "Filetypes" edit/reload; after install a restart or `dopus ... ` reload is needed.

Fix level: installer/kit (D13): the base-kit installer step copies a generated "UP-Term" file to DOpus5:Filetypes when `Assign DOpus5: EXISTS` and the launchers are in bin; plus filetype priority above Text. Not a DOpus config fix.

Test idea (fails before): host test that drives mkinstaller/the install script (dist install.dos path) against a fake tree with a DOpus5: assign and asserts `Filetypes/UP-Term` == gen_dopus_filetype.generate() and uterm-edit/uterm-view in bin; and a priority assertion > stock Text. Rig: A1200-like rig with DOpus 5.91/5.104 installed, run installer, then `dopus` ARexx or double-click a .txt and check the UP-Term window.
