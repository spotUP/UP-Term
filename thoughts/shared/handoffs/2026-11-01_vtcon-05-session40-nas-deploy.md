---
date: 2026-11-01
session: vtcon-05 s40
status: handoff
---

# vtcon-05 session 40: NAS deploy, image publishing, wbpark z-order, upkg curl messages

## Done (all pushed, origin/main = 36d99e42)
- RIG31-RESET-LOOP closed as the 060 quirk (edfdaee2 and earlier).
- WB-PARK-ZORDER: 04af5788, f050913f, c6851167 (wbpark_layers_in_front in handler/wbpark.h, tests/test_wbpark.c).
- NAS: 13c8cb2b image workflow (.github/workflows/nas-image.yml, tag `nas-v*`, ghcr.io/spotup/claude-amiga); b2dc5f56 user docs; 6f6968de `make nas-deploy` (tools/nas/deploy.py, ~/.config/upterm/nas.conf); fe777b08 site settings in .env (tools/nas/env.example); eda036ad never sends .env; eef66637 ssh BatchMode, key-login failure stops with a hint; bc804f98 copy with tar over ssh (Synology refuses rsync over ssh unless the DSM rsync service is on); 36d99e42 wasabi clone source in tools/nas/wasabi.repo (the pin 83acd19 is spotUP-fork only; Dockerfile cloned creep-ltx and failed "reference is not a tree").
- NAS-DEPLOY ticked: owner ran `make nas-deploy`, it ended `[OK] deployed`; port 2323 answers with the uptelnetd greeting.
- upterm-ports d94a685 (opkg patch 0061): status -1 probes PATH; missing curl prints "curl not found (is curl installed and in PATH?)". Rig 3 checked both ways; revert FAIL line in the ledger (case CURL-MSG).

## Open (ledger thoughts/spot/todos.md, grep the IDs)
- NAS-IMAGE-PUBLISH: owner pushes `nas-v1` tag, then makes the ghcr package public; Docker Hub secrets optional.
- NAS-USER-DOCS open; NAS-DEPLOY-AMIGA (new, later): deploy.py on the Amiga with kit python3 + bebbossh, pack with tarfile.
- WB-PARK-ZORDER leftovers: (c) late drawer reopen not exercised; user-activated stop rule has no revert check.
- RIG-BATCH-DAILY remaining items, NVIM-PERF, RIG-RECHECK-STALE.
- Unverified: Synology Container Manager reads .env; `rig.py rtg` left PAL; UP-Term `exit` closes its window on 3.1.

## Gotchas
- NAS: 192.168.0.198, ssh port 1337, user spotUP, key ~/.ssh/id_ed25519. docker needs sudo with a password: the owner runs `make nas-deploy`; the tar copy step alone can run from the session.
- upterm-ports: `make host-opkg` does not rebuild on a C-source-only change; rm build/state/host-opkg.built first.
- ~/Code/UP-Term/upterm-ports on branch port/htop has another session's untracked files and worktrees; leave them. Saved patch: an earlier common.mk seq guard (not needed; 232ca13 fixed it).
- Two untracked lucide preview files were identical to 52e0a326 and moved to the s40 scratchpad to let the rebase run.
