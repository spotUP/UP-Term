---
date: 2026-10-25
topic: Full session handover (account switch). 0x34 NULL read, test_dist_stack, MuForce reruns, HARDVOL-FSSM, STACK-AUDIT landed; owner checks and follow-ups open
branch: main (my last merge 176c572d; main since moved to 79d5a212 from the Amarchy session)
---

# Handover: UP-Term session 2026-10-25 (account switch)

Start a FRESH session on the new account. Do not resume this one: the prompt
cache belongs to the old account. This file plus the ledger is the whole state.

## Where things live
- Repo: `/Users/spot/Code/UP-Term/vtcon`. It is a git repo even when the
  environment line says otherwise. The session root is `/Users/spot/Code/UP-Term`.
- Ledger: `thoughts/spot/todos.md` (git-ignored, 1660+ lines). Read by section:
  `grep -n "^## " thoughts/spot/todos.md`, then `sed -n` the range. Sections
  touched this session: `## UNINSTALL-WASABI-BLOCK`, `## HARDVOL-FSSM`,
  `## RIG-FRESH-SYS`, `## STACK-AUDIT` (end of file), and the
  XCON-NULL-READ-0x56 / 0x34 entries.
- Stack table: `thoughts/shared/research/2026-10-25_stack-audit.md`
  (untracked, not committed yet).
- This handover: untracked, not committed yet. Commit both docs on the next
  push (add by name).
- GitHub: account `spotUP`, origin `https://github.com/spotUP/UP-Term.git`.
  ssh `serveraliveinterval 60` is set.

## Done this session (all on origin main)
1. **0x34 NULL read**: `le_resized` in `handler/lineedit.c` read `vt_cols`
   before its guard; added `le_unbind`. Also fixed the Makefile
   `MENU_ICONS_INC`/`SYMBOLS_TTF` ordering bug. Commits 141d68a7, 77e62eec,
   merge e524e7ff.
2. **test_dist_stack red on main**: 2523115c added an Amarchy cookie but left
   Amarchy in `AUDIT_PENDING`. Fixed befdf652, merge 83c34d1d.
3. **UNINSTALL-WASABI-BLOCK, wasabi-owned rig case**: PASS 14/14 on rig 3,
   fresh `make dist` from 83c34d1d.
4. **XCON-NULL-READ-0x56 MuForce rerun** (rig 6, `--mmu`): 0 hits at 0x56 and
   0x34; the `muhit` liveness probe hit, so MuForce was live. Triggers:
   `closecrash_rig.py 6`, `bind_rig.py`, `closeinactive_rig.py`.
   pane_close NOT covered (see open item 2).
5. **HARDVOL-FSSM** (rig 6, MuForce on): `UPBigVol LIST` marks `* VTCX:`
   correctly; `amarchy_menu_rig` app_index 19/19; MuForce 0 hits.
   `installer_rig` bigvol 3/4: askdir showed `VTCX:Apps`, test expects
   `VTCX:UP-Term`, because `rig.py install` leaves an `UP-Term:` assign and
   `P_ASKDEST` offers that drawer first. Test precondition gap, not an fssm bug.
   Not exercised: BSTR/network startup, the ROM-range branch.
6. **STACK-AUDIT** (RULES.md rule 6): every kit program sets vbcc `__stack`
   and a `"$STACK: n"` cookie; `AUDIT_PENDING` in `tests/test_dist_stack.py`
   is empty. Commit 49c83f38, merge 176c572d, pushed, ls-remote matched.
   `strings` shows the cookie in all 16 binaries in `build/amiga`.
   - Method: scratchpad helper (`stk/stkrun.c` + `runon.s`) ran each program
     on a 256 KB stack filled with 0xA5 and read the high-water mark.
     `wasabi ps` FREE is current SP only, useless for peaks.
   - Sizes: hl 24576 (peak 10466), mdv 20480 (8182); 8192 for ps (726),
     upsame (1146), wasabikey (880), Reboot (not run); 16384 for rz/sz,
     uptelnet, UPTerm, UPConsole, top, UP-Term Prefs, UPDemo, upicon, upupdate.
   - hl/mdv stack lines sit under `#ifdef __VBCC__` (they also build on the
     host with `-std=c89 -pedantic -Werror`).

## Open, in order
1. **STACK-AUDIT heavy-path remeasures.** 16384 is headroom, not measured:
   rz/sz real transfer (idle peaks 2658/3186), uptelnet live session (1214,
   refused connect), UPTerm with window open (1162), UPConsole as handler
   (1288, STATUS only), top interactive (846, batch), Prefs requesters (2700),
   UPDemo scene (unmeasured), upicon write path (1450), Reboot by static
   reading. One sonnet agent, one rig. The scratchpad helper is gone with the
   old session; the method is in the table doc, rebuild the helper from it.
2. **pane_close MuForce rerun** after `feature/native-splits` merges
   (`splits_rig.py` exists only there; worktree `../vtcon-splits`). That merge
   conflicts lightly with 691ed139 in `render/amiga_render.c` and
   `tests/test_render.c`.
3. **installer_rig fixes**: takes `ONLY <case>`, silently ignores `--only` and
   runs every case; bigvol needs the `UP-Term:` assign removed first; the run
   leaves the rig without `VTC:Apps`, so `amarchy_menu_rig` then fails with
   "ERR: cannot create the temporary file" until `rig.py install` reruns.
4. **make dist** stops at packaging: "upterm-ports/build/kit/userland is older
   than these checked builds: make kit-stage in upterm-ports first". Other repo;
   not touched.
5. **Pre-push hook did not block** a push while rig 6 ran (earlier this
   session). The machine-load gate misses rig/fs-uae runs.
6. **SessionStart hook** looks for `next-session.md` under the session root
   (`UP-Term/thoughts/spot/`), not `vtcon/thoughts/spot/`. The start prompt is
   copied to both.

## Blocked on the owner
- **RIG-FRESH-SYS**: copy `nyhd2.hdf` to `~/Code/UP-Term/images`.
- **UNINSTALL-WASABI-BLOCK A1200 check**: install with an unmarked
  `C:wasabid` already present, run Uninstall, reboot. PASS: `C:wasabid` stays,
  starts at boot, network mounts.

## Hands off (other sessions' work)
- Unstaged `tools/rig/rig.py` hunk on main (another session).
- Untracked `thoughts/shared/releases-backup/2026-10-23_vtcon-v1.0-v1.6/`.
- Amarchy session items: AMARCHY-*, RESCAN-FEEDBACK,
  TERM-ROWS-ONLY-OVER-TERMINAL, INDEX-SKIP-NETWORK, app_index check 7, I4.
  That session committed 79d5a212 on main after my merge.
- Worktrees not mine: `vtcon-demo-perf`, `vtcon-splits`, and the three under
  `vtcon/.claude/worktrees/` (am-idx, agent-a4a9..., fix-addon-install-disk-full).

## Working rules that bit this session
- One test/rig run on the machine at a time, all sessions counted. Check
  `LC_ALL=C ps -Ao pid,pcpu,command` for fs-uae, `*_rig.py`, and the
  MusicPlayer session's `selecta` before any test, build sweep or push.
- `nice -n 19`, at most 2 workers. Build logs to the scratchpad; print errors
  only. Note: grep for "error" matches `-warnings-as-errors` on every vc line;
  filter out lines starting `vc `.
- `make dist` failing does not mean binaries failed: check `build/amiga/<prog>`.
- Cookie regex in the test: `"$STACK: <number>"` literal or a macro-call form.
- No AI attribution in commits. Never `git add -A`.
