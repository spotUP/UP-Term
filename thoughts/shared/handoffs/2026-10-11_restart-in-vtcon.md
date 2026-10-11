---
date: 2026-10-11
topic: Restart in ~/Code/vtcon - amiga-pi Phase E and AmiSSL bundle paused as WIP
tags: [handoff, amiga-pi, amissl, kit, restart]
status: final
---

# Start here (restart, 2026-10-11 late)

Supersedes the "Update 20:15" next steps in `2026-10-11_session-switch.md`. Read that file
for the older state (kit `~/Desktop/UP-Term-83199a4.lha`, open queue, working rules) and the
"Owner answers" sections of `thoughts/spot/todos.md` before briefing any agent.

The previous session ran from `~/Code/ixemul-vtcon` by mistake. Run sessions from `~/Code/vtcon`.

## State at writing

No agents are running and no rigs are up. Disk: 6.9 GB free (tight; check `df -h /` before builds).

### 1. AmiSSL bundle (first job of the next kit): WIP, nothing verified

- Worktree `/Users/spot/Code/vtcon/.claude/worktrees/amissl`, branch `feature/amissl-bundle`,
  WIP commit `ca48808` on top of main `4919240`. Not pushed.
- Done: draft patch (`thoughts/spot/notes/2026-10-11-amissl-draft.patch`) applied to Makefile,
  `dist/Install.installer`, `dist/LICENSES.txt`, `dist/README.txt`, `dist/parts.txt`,
  `dist/amissl/LICENSE-Apache-2.0.txt`.
- Uncommitted, gitignored in that worktree: `build/rig-inputs` copy, `vendor/` symlinks to the main
  tree (ndk-3.2r4-Include_H, ndk-3.2r4-netinclude, amissl-5.27), real copy of
  `vendor/AmiSSL-5.27-OS3.lha`.
- Open, in order:
  1. `make dist` in the worktree fails: it looks for the sibling
     `.claude/worktrees/ixemul-vtcon/utils/ixpipe-handler.c`. Point UPTERM_ROOT / the ixemul path at
     the real `~/Code/ixemul-vtcon` checkout (fix the Makefile path logic, not a symlink hack).
  2. `tools/rig/installer_rig.py` clicks Proceed (id 90) on every page. The new AmiSSL askbool now
     appears on clean images. Find its gadget ids on clean 3.2.2 (`--os 322 --max`), add an
     `amissl` case that answers Yes and drives the nested Install-AmiSSL pages; all other cases
     answer No.
  3. Rig 1 checks: AmiSSL-absent and AmiSSL-present on clean 3.1 and 3.2.2, plus Update. TLS check
     after install (curl --version / Claude SETUP shows "AmiSSL 5").
  4. `make test` (claude-tls-check, claude_fixture stay green), then kit to
     `~/Desktop/UP-Term-<sha>.lha`.
- Spec: `thoughts/spot/todos.md`, line starting "- FIRST JOB OF THE NEXT KIT: bundle AmiSSL".

### 2. amiga-pi Phase E: 0 of 18 ticked (E1-E13, G1-G4), WIP

- amiga-pi worktree `~/Code/amiga-pi/build/wt-phase-e`, branch `phase-e`, WIP `b3f0cd9`.
- vtcon worktree `.claude/worktrees/phase-e`, branch `feature/amiga-pi-phase-e`, `47af1e4`
  (progress-file pause note). Untracked `vendor/` there is local data: never commit it.
- Done: `session3.c` compiles under -Werror (two dead-code fixes), added to PI_CORE in the Makefile;
  11 fixtures in `tests/pi/sessions/` with generator `tools/pi_oracle/mkfixtures.py`; pi's
  session-format and compaction docs saved in `~/Code/amiga-pi/build/pi-oracle/docs/`.
- Not done: no test has run; `tests/test_pi_session.c` not written.
- Next, in order: write `tools/pi_oracle/gen.ts` (runs pi's SessionManager over each fixture,
  produces expected outputs plus a normalised writer scenario) -> `tests/test_pi_session.c` and
  register the suite in `tests/main.c` -> `context.c`, then continue in the progress-file order
  (`thoughts/shared/plans/2026-10-07-amiga-pi.progress.md`, Phase E section).
- Build: `make` in the amiga-pi worktree needs
  `VTCON_DIR=/Users/spot/Code/vtcon/.claude/worktrees/phase-e`.
- pi oracle (node, pi f10993bc): `~/Code/amiga-pi/build/pi-oracle/` (gitignored).

## Still waiting on the owner

- Replay test of `~/Desktop/UP-Term-83199a4.lha`: Install, then Install again and choose Update.
  PASS: only changed files get new dates; nothing asks for a missing volume.

## Not ours

- Worktree `/private/tmp/claude-501/-Users-spot-Code-ixemul-vtcon/bf545e39-.../scratchpad/wt2`
  (detached `85a76db`) belongs to another session. Left alone; owner decides.

## Rules that bit before

At most 2 agents; agents editing vtcon use their own worktree; rig 1 = AmiSSL, rig 2 = amiga-pi;
always `rig.py --max`; `caffeinate -dims` while agents run; main session on Sonnet.
