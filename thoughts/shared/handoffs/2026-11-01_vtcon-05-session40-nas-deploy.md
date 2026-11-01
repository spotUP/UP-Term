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

## Done later in s40 (origin/main = 4266b68a)
- NAS-IMAGE-PUBLISH: ghcr package claude-amiga public, tags v1 + latest, anonymous pull 200. Only Docker Hub secrets left (optional).
- NAS-USER-DOCS ticked; d934db97 Claude add-on Installer help says "Claude Code on your server", not "on this computer".
- NAS-DEPLOY-AMIGA ticked: e2c36886 c50f05d9 c1fd99cf (tarfile pack; plain tar on Amiga, no zlib; T:nas.tar spool default since a Popen stdin pipe to bebbossh hangs; bebbossh -p/-T, key ENVARC:ssh/id_ed25519; nas/ drawer in up-claude via --stage). Full deploy from rig 5 ran, owner typed sudo, rc 0, port 2323 open. Rig key upterm-rig-deploy in NAS authorized_keys.
- 4266b68a default NAS_DOCKER resolves docker with /usr/local/bin on PATH (Synology non-login PATH lacks it; bare "sudo docker" was command not found). Not yet run on the NAS with the default (owner's nas.conf overrides it).
- Wasabi upstream PRs creep-ltx/wasabi #2 #3 #4 (from spotUP, mergeable): each merged onto upstream main e8fc865 builds, host tests 332/0 (#3 had one TLS flake, rerun clean). Host tests skip wasabid.c, so daemon changes still need a rig check. UP-Term pins wasabi f59c839 meanwhile; fork branch fix/uiact-subitem-mutualexclude must stay there.

## Open (ledger thoughts/spot/todos.md, grep the IDs)
- NAS-IMAGE-PUBLISH: Docker Hub secrets only (optional).
- WASABI-UPSTREAM-PRS: rig check of the #2-4 daemon changes; after upstream merges, re-pin tools/nas/wasabi.rev + wasabi.repo to creep-ltx main, make wasabi, one rig check.
- NAS-DEPLOY-AMIGA leftovers unverified: make dist-claude + Installer copy of nas/, OS 3.2, bebbossh first-connect host-key prompt. Stale `tar -xf` pid 19218 on the NAS (kill was refused to the session).
- WB-PARK-ZORDER leftovers: (c) late drawer reopen not exercised; user-activated stop rule has no revert check.
- RIG-BATCH-DAILY remaining items, NVIM-PERF, RIG-RECHECK-STALE.
- Unverified: Synology Container Manager reads .env; `rig.py rtg` left PAL; UP-Term `exit` closes its window on 3.1.

## Gotchas
- NAS: 192.168.0.198, ssh port 1337, user spotUP, key ~/.ssh/id_ed25519. docker needs sudo with a password: the owner runs `make nas-deploy`; the tar copy step alone can run from the session.
- upterm-ports: `make host-opkg` does not rebuild on a C-source-only change; rm build/state/host-opkg.built first.
- ~/Code/UP-Term/upterm-ports on branch port/htop has another session's untracked files and worktrees; leave them. Saved patch: an earlier common.mk seq guard (not needed; 232ca13 fixed it).
- Two untracked lucide preview files were identical to 52e0a326 and moved to the s40 scratchpad to let the rebase run.
