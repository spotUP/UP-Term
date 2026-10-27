---
date: 2026-10-27
topic: vtcon-05 session 8. upkg browse launch (L1-L6 done), install-all PR 9 rig run and upgrade re-ask fix, resize/EINTR PRs 10-11, ixemul mkdir-root fix, UHC as a second upkg repository (plan decided, U1+D4 in PR 2, rig check not run)
branch: main (vtcon origin), ports PRs 8-11, vtcon PR 2, ixemul-vtcon PR 3
---

# Handover: vtcon-05 session 8 (UP-Term side)

Ledger: `thoughts/spot/todos.md`, rows UPKG-BROWSE-LAUNCH, UPKG-INSTALL-ALL-RIG8, IXEMUL-MKDIR-ROOT, IXEMUL-EINTR, UPKG-BROWSE-UHC and "UHC U1+D4" (the last rows of the file). Trust the ledger over this summary.

## Constraints
- Rig 8 only. Start with `make wasabi` then `UPTERM_RIG=8 tools/rig/rig.py start --max --purpose "vtcon-05: <task>"`. Stop it at the end.
- The main vtcon checkout is dirty and behind origin: never pull, stash, reset or commit there. Commit through temporary worktrees on origin/main. `tools/rig/rig.py` in the main checkout is another session's edit; leave it.
- The owner merges PRs. Merge order for ports: PR 8, 9, 10, 11 (merge commits).
- The wasabi MCP reaches rig 8, not the A1200; A1200 checks go through tools/rig/ami.py (AMI_HOST=192.168.0.74, key in ~/.config/upterm/a1200-wasabi.conf; check `route -n get 192.168.0.74` shows en0 first).

## Done
- Browse launch L1-L6 (plan thoughts/shared/plans/2026-10-26-upkg-browse-launch.md). L7 [~]: dopus launch value in vtcon 9ff0479b; the feed rebuild waits for the PR 8 merge.
- Ports PR 9 (install-all, patch 0041): rig 8 run 13 of 17 pass (tools/rig/upkg_install_all_rig.py, findings thoughts/spot/term-findings/upkg-install-all-rig8.md). Upgrade re-ask fixed in ports b4e7694: `upkg upgrade` runs postinst unattended (upkg_xsystem_unattended(1) around opkg_cmd_exec), test check/install_all.sh revert-proven.
- Ports PR 10 (resize blank rows, clearok on KEY_RESIZE) and PR 11 (EINTR retry in libopkg/xsystem.c, patch 0042) opened.
- ixemul-vtcon PR 3: mkdir() on an existing volume root returned EIO (nnn "10161: input/output error"). Fix built in worktree /Users/spot/Code/UP-Term/ixemul-mkdir.
- UHC as a second repository: research thoughts/shared/research/2026-10-27-upkg-uhc-repo.md, plan thoughts/shared/plans/2026-10-27-upkg-uhc-repo.md (U1-U8, owner decisions D1-D4 recorded, do not re-ask). U1+D4 in vtcon PR 2 (branch fix/uhc-env, 1ccd40b2, up-uhc 1.1): ENV:UHC/ENVARC:UHC setup, boot copy, uhcupdate/uhccheck disabled stubs.

## Open, in order
1. Rig 8 check of vtcon PR 2 (checks 1-6 in the ledger row "UHC U1+D4"; blockers in thoughts/spot/term-findings/uhc-env-rig8.md). Reuse upkg_install_all_rig.py's make_feed/serve/need/run_typed to get upkg and a local feed on rig 8 instead of installing by hand. Also check the review risks: `Copy ENVARC:UHC ENV:UHC ALL QUIET` + `If WARN Quit 20` on an empty ENVARC:UHC, and whether an upgrade's uninstall-body deletes the user's ENVARC:UHC.
2. UHC U2-U8 in upterm-ports (libupkg UHC source reader, row src field, install runner, remove via Copy lines, CLI `uhc:<n>`, browse source tag, rig reachability).
3. PR 9 rerun items: rig confirms the up-iaask answer is kept on upgrade (questions file may need a `<PATH` default); Shift+U said "Update all 4" where 2 were expected (remove the 9 real add-ons on rig 8 and rerun, else suspect chrome.c:296-297 / upkg_all.c); Shift+I check needs the fixtures removed via CLI and the screen read before pressing I, then `--only 4`.
4. Rig 8: PR 10 even-row height; PR 11 slow-postinst install with a mid-install resize, without and with the fix.
5. ixemul mkdir: run compat/test/mkdirprobe.c with the new library (7/7 OK) and the old one (fail), check nnn starts; the A1200 install needs owner approval; clean up the worktree, build295 and docker image ixemul-gcc295. Findings thoughts/spot/term-findings/ixemul-mkdir-root.md.
6. Later: L7 feed rebuild after PR 8; NVIM-PERF (`go nvim-perf`); upkg_ask_rig A/B rerun; CI rolling feed F1-F13; creep CP6 steps 5-6.

## Owner decisions waiting
- `upkg upgrade` now answers every postinst question with its default. A question new in the upgraded version is therefore never asked. Keep, or ask only new questions?
- UHC licence: up-uhc 1.1 ships modified UHC files; confirm the redistribution permission covers that before release.
