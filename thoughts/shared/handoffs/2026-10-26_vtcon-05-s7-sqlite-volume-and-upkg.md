---
date: 2026-10-26
topic: vtcon-05 session 7. Dist Makefile archive-drawer fix (5d163df5), new ixemul 086c15cf on the A1200, sqlite3 AmigaDOS volume-path fix (ports PR 7, 5da6599c), upkg-ui rig 8 suite (browse PASS, cli/ask rerun)
branch: main (origin 5d163df5); ports main 5da6599c
---

# Handover: vtcon-05 session 7 (UP-Term side)

Ledger: `thoughts/spot/todos.md`, section "### 2026-10-26 (session 7, vtcon-05)" and the rows after it. Trust the ledger over this summary.

## Landed
- vtcon 5d163df5: the dist Makefile stages each slim package into the drawer of its packages.txt archive (tools/slim.py `kits` mode); test `test_each_package_stages_into_the_drawer_of_its_archive`.
- ports PR 7, 5da6599c: sqlite patch 0002. Under `__amigaos__`, `unixFullPathname` returns a name whose first element holds a ':' (`RAM:x.db`, `Work:dir/x.db`) unchanged instead of prefixing getcwd(). Check case `volume`. On the A1200 the old binary fails with `unable to open database file`; the new binary opens volume, relative, `/RAM/` and journal databases.
- A1200: LIBS:ixemul.library is 086c15cf (old one at LIBS:ixemul.library.bak-20261026).

## Findings
- thoughts/spot/term-findings/upkg-ui-suite-rig8-2026-10-26.md: upkg_browse_rig and upkg_browse_extra_rig PASS 13/13 on rig 8. upkg_rig and upkg_ask_rig were BLOCKED because rig 8's UP-Term: base (83199a4) has no upkg; a rerun with the base installed was running at handoff time (see the ledger row).
- sqlite WAL answers `delete` on this build (no shared memory); expected.
- Scratch evidence: session scratchpad sqlite-volume-fix.md, sqlite-a1200-install.md, upkgui/.

## Done since
- v1.7 published (28 packages, up-claude 1.2 via vtcon 7bbe4385): https://github.com/spotUP/vtcon/releases/tag/v1.7. releases/latest/download/Packages serves 28 entries.
- Worktrees vtcon-dist05 and upterm-ports-vtcon05 removed; ports branch fix/sqlite-volume-paths deleted.
- A1200 C:sqlite3 deleted (backup C:sqlite3.bak-20261026 kept); UP-Term:bin/sqlite3 is the one vsh runs.

## Open, in order
1. Owner: `upkg update` then `upkg list` on the A1200; expect 28 packages and up-claude 1.2 as an upgrade.
2. upkg_ask_rig pass C fix: a sonnet agent on rig 8 in worktree vtcon-askfix (typewin types by US key position on a Swedish keymap). upkg_rig.py PASS 0/9; ask A and B PASS.
3. Rig 8 run of the pkgs/sqlite check cases (tools/rig/userland_rig.py), including `volume`. After the pass C agent frees rig 8.
4. CI rolling feed: plan thoughts/shared/plans/2026-10-26-upkg-rolling-feed.md (F1-F13) with owner answers. F1 first: measure cold/warm build time and peak disk (8.8 GB free now).
5. CP6 creep walk steps 5-6 (rig install of a built figlet .ipk), then delete local branches ports creep/walk-figlet and vtcon creep/walk-rows.

6. NEW: launch installed packages from `upkg browse` (Enter = Launch, Delete = Remove, launch column). Plan: thoughts/shared/plans/2026-10-26-upkg-browse-launch.md, steps L1-L7.
7. wasabi keymap: queued brief thoughts/spot/brief-wasabi-keymap.md: measure default vs console keymap on rig 8 first; upstream PR to creep-ltx/wasabi only if wasabid is at fault.
8. upkg_ask_rig passes A/B: re-run FAILED only because the agent hand-copied a stale upkg (main ports checkout, 25 Oct, no Files staging to T:upkg-stage). Install the base from a current ports origin/main build on rig 8, then rerun the full script (no --only A/B exists). Earlier A/B PASS with base 5d163df5.

## Constraints
- Rig 8 only. The main vtcon checkout is far behind origin: never pull; commit through temporary worktrees on origin/main.
- tools/rig/rig.py in the main checkout is modified by other sessions; leave it alone.
- On the A1200, `ami.py long` scripts that redirect `>>` and `*>>` to one file hang; use `ami.py exec "Execute RAM:<script>"` with `>out *>err`.
