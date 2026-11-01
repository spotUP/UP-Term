---
date: 2026-10-26
topic: upkg always-current CI rolling feed (release tag `feed`)
status: draft
---

# upkg rolling feed

Finish line: a push to main of spotUP/vtcon or spotUP/upterm-ports ends, without the owner, with the new .ipk files and
Packages/Packages.gz/upkg-index.txt on release `feed`, and `upkg update` on a rig sees the new version.

## 1. Existing CI (measured 2026-10-26)

- vtcon: origin/main has NO .github directory (`git ls-tree -r origin/main | grep ^.github` is empty). No CI, no runner, no image.
- upterm-ports: origin/main has `.github/workflows/pr.yml` + `pull_request_template.md`. Trigger `pull_request` on pkgs/mk/tools/Makefile,
  `ubuntu-latest`, read-only token, no secrets, host checks only ("no emulator, no m68k toolchain"). It builds nothing.
  Its header states the owner builds and releases locally. Keep that file's no-secrets rule: the feed workflow must be a separate
  file that never runs on `pull_request` (a PR recipe is contributor code).
- Self-hosted runners: `gh api repos/spotUP/{vtcon,upterm-ports}/actions/runners` -> total_count 0 for both.
- Both repos and all siblings except amiga-gcc / amiga-gcc15 are PUBLIC (vtcon, ixemul-vtcon, upterm-ports, tmux-amiga, screen-amiga,
  neovim-amiga, cpython-amiga, amiga-pi). amiga-gcc and amiga-gcc15 are not under spotUP on GitHub (404): the toolchain in
  `~/opt/amiga` (381 MB) comes from bebbo's upstream and is built locally. Public repo => standard GitHub-hosted minutes are free.

## Build dependencies (Makefile `dist`, packages.txt column 9)

`make dist` = dist-base dist-claude dist-pi dist-nvim dist-python dist-git dist-perl dist-vim dist-bitchx dist-vbcc dist-src dist-index dist-ipk.
- vtcon itself: amiga C programs (UPConsole, amarchy-*, Claude, uptelnet, upupdate ...), terminfo, fonts (UNIFONT/SYMBOLS/EMOJI stamps).
- `m68k-amigaos-gcc` (~/opt/amiga/bin, AGCC) and `m68k-amigaos-strip`.
- Docker image `ixemul-gcc295` (IXGCC295, linux/amd64, gcc 2.95.3 for -mstackextend; built from ixemul-vtcon/docker/Dockerfile via docker/build.sh).
- ixemul-vtcon: `build295/library/68020/68881/amigaos/ixemul.library` and the soft-float/ixnet variants (IXEMUL_LIB, `sh docker/build.sh`).
- upterm-ports: `make sysroot` then `make kit-stage` -> `build/kit/userland` (51 recipes in pkgs/; local build dir is 3.2 GB). dist targets refuse a
  kit older than the `.checked` state (PORTS_KIT_FRESH), so ports must be built in the same job, not downloaded stale.
- Package source repos: tmux-amiga, screen-amiga (base, via ports kit), neovim-amiga (nvim), cpython-amiga (python), amiga-pi (pi), plus
  the ports recipes for git/perl/vim/bitchx/vbcc.
- wasabi pin `tools/nas/wasabi.rev` (dist-base stamp `build/wasabi/.stamp-$(WASABI_REV)`).
- Not needed for the feed: any emulator or rig (`pkgs.py ipk/feed` and `dist-check` are host-side; verify dist-check does not call a rig, F3).

Unknowns to measure first (F1): wall time of a cold `make sysroot kit-stage dist-ipk` and its peak disk. Not measured here (no builds allowed).

## 2. Runner choice

| | GitHub-hosted + prebuilt toolchain image | Self-hosted on this Mac |
|---|---|---|
| Cost | free (public repos); ubuntu 4 vCPU, 16 GB RAM, ~14 GB disk free (a larger runner is paid) | free, but needs the Mac on and awake |
| Disk | cold ports build 3.2 GB + toolchain + dist fits in 14 GB; remove Android/dotnet dirs for +25 GB if needed | this Mac has 2 GB free: a ports build (3.2 GB) cannot run. Blocker |
| Reliability | clean machine each run, reproducible, linux/amd64 native so `ixemul-gcc295` needs no emulation (faster than Docker on the Apple-silicon Mac) | depends on lid, sleep, VPN, other sessions sharing cores (rule 6a-1), and runs untrusted-looking pushes on the owner's machine |
| Time | cold ~hours unless the toolchain is prebuilt; with image + ccache/`actions/cache` of `build/sysroot` incremental builds drop to minutes (to be measured) | warm tree, fastest |
| Security | no secrets except GITHUB_TOKEN | runner holds owner's keys and home dir |

Recommendation: GitHub-hosted `ubuntu-24.04`, with a prebuilt build-environment image on GHCR
(`ghcr.io/spotup/vtcon-buildenv`): contains `~/opt/amiga` toolchain, the `ixemul-gcc295` toolchain, the ixemul.library build, and the
ports sysroot. The image is rebuilt by its own workflow only when ixemul-vtcon, the toolchain pin or `mk/` toolchain files change
(weekly + manual dispatch). The feed job runs inside that image, restores `actions/cache` for `upterm-ports/build/state` keyed by recipe hashes,
so only changed recipes rebuild. Fallback if the 6 h job limit or 14 GB disk is hit: a paid larger runner, never the Mac.

## 3. Trigger wiring

- vtcon: `.github/workflows/feed.yml`, `on: push: branches: [main]` (paths: not `thoughts/**`, `*.md`), plus `workflow_dispatch` and
  `repository_dispatch: types: [ports-main]`.
- upterm-ports: new `.github/workflows/notify-feed.yml`, `on: push: branches: [main]`, one step: `gh api repos/spotUP/vtcon/dispatches -f event_type=ports-main
  -f client_payload[sha]=$GITHUB_SHA` with secret `VTCON_DISPATCH_TOKEN` (fine-grained PAT, repo vtcon, contents:write; or a GitHub App token).
  This is a push-only workflow, so the pr.yml no-secrets rule holds (secrets are not exposed to fork PRs).
- Sibling repos (ixemul-vtcon, tmux-amiga, screen-amiga, neovim-amiga, cpython-amiga, amiga-pi): the same notify file would be needed for
  true always-current. Start with vtcon + ports (owner scope) and add a 6-hourly `schedule` cron in feed.yml that rebuilds only if the head SHAs of
  the sibling repos differ from the `feed-sources.txt` asset on the release. That cron covers siblings without secrets in each repo.
- Concurrency: `concurrency: {group: upkg-feed, cancel-in-progress: true}` on the build job. A newer push cancels a running build (the
  feed is state, only the newest matters). The publish job has its own group `upkg-feed-publish` with `cancel-in-progress: false` so a
  cancel cannot land mid-upload. Publish only after the build job succeeded and only if the SHA is still the newest main.

## 4. Publish step

- Job `publish` needs `contents: write`; one fixed release tag `feed`, created once (F6), marked prerelease=false, `--latest`.
- Order that minimises the client-visible gap: upload every .ipk first with `gh release upload feed build/ipk/*.ipk --clobber`, upload
  `upkg-index.txt`, then `Packages` and `Packages.gz` last. Clients only learn of a version through Packages, so a client never sees an
  index naming a missing .ipk. A client that fetched the OLD Packages mid-run and then requests an old .ipk filename gets a 404 (the name carries
  the version, e.g. `up-nvim_1.0_m68k-amigaos.ipk`, so a same-version rebuild silently overwrites; a changed version removes the old file).
  Mitigation: do not delete old .ipk assets during the run; after the uploads, delete assets not in the new index (`gh release delete-asset`), and
  upkg retries `update` on 404 (document in the client, F9).
- Same-version rebuilds: content changes with the same Version make clients skip the update. Rule: the feed build stamps the version as
  `<major>.<minor>.<ctr>` or appends `+g<sha7>` ONLY if tools/pkgs.py `ipk` supports it (F4 decides; table stays authoritative).
- Record `feed-sources.txt` (SHAs of vtcon, ixemul-vtcon, ports and each sibling) as an asset: reproducibility and the cron's skip check.
- Keep versioned `v1.x` releases: yes, as frozen snapshots for the whole-kit lha (`UP-Term-1.x.lha`, 20+ MB) and installer downloads; the feed carries
  only .ipk + indexes (+ the current lha optionally). GitHub does not allow two releases to share a tag, hence the fixed `feed` tag. Also mind the
  per-asset limit of 2 GB and ~1000 assets: no issue here (28 packages).
- Tag drift: the `feed` tag should point at the built SHA; move it with `gh release edit feed --target <sha>` is not allowed for existing tags, so
  use `git tag -f feed && git push -f origin feed` in the publish job (a force push to a CI-owned tag, not a shared branch; the owner still
  confirms this in decision D4).

## 5. upkg.conf and migration

- Edit `dist/upkg.conf` line 6 to `src/gz upterm https://github.com/spotUP/vtcon/releases/download/feed`. `releases/latest/download` cannot serve the
  feed unless `feed` is the release marked Latest; GitHub picks "latest" among non-prerelease, non-draft releases by creation date, so a later `v1.7`
  would steal it unless created with `--latest=false`.
- Existing installs (A1200 etc.) have the OLD upkg.conf pointing at `latest/download`. Two migration paths:
  1. Keep `latest` valid: mark `feed` as Latest on every publish (`gh release edit feed --latest`), and cut versioned releases with `--latest=false`
     from now on. Old installs then follow the feed with NO conf change. Recommended.
  2. Ship the conf change as a package in the feed (a `up-term-conf`/base .ipk whose postinst rewrites `UP-Term:etc/upkg.conf`) for installs that should
     pin `feed` explicitly. Optional, later. The conf is part of the kit's Files/upkg.conf (part system), so also check whether `upkg` overwrites a user-edited conf (F8).
- Check that the old v1.6 `Packages` (6 .ipk only) does not clash: the first `upkg update` against `feed` replaces the list; installed packages not in the feed stay installed.

## 6. Reachability test (one)

`tests/feed-e2e` (a workflow_dispatch + scheduled-safe job, `.github/workflows/feed-e2e.yml`): 
1. In upterm-ports create a branch with a sentinel recipe change: a dummy package `up-feedcheck` at Version `0.<run_number>` (new tiny recipe in a
   test-only directory that the dist table lists with a flag, or an existing small package whose version is bumped), merge it to main of a
   throwaway fork `spotUP/upterm-ports-feedtest` wired to a throwaway release `feed-test` (same workflows, parameterised by `FEED_TAG` and the dispatch
   target), so the real feed is never touched by the test.
2. Wait (polling with timeout via `gh run watch`, not sleep loops) until the vtcon feed run for that dispatch finishes.
3. Download `https://github.com/spotUP/vtcon/releases/download/feed-test/Packages.gz`, assert it lists `up-feedcheck` with the sentinel version and that
   `up-feedcheck_<ver>_m68k-amigaos.ipk` exists on the release with the matching md5/size from Packages.
4. Sentinel proof the new code RAN: the .ipk contains a file with the run id and the build log prints `FEED-SENTINEL <sha>`; the test asserts both.
Rig-side proof (owner-run, listed under Blocked on me when done): `upkg update && upkg list | grep up-feedcheck` on rig 7 with conf pointing at `feed-test`.

## Checklist

| ID | Item | Files | Done when |
|---|---|---|---|
| F1 | Measure cold and warm build time + peak disk of `make sysroot kit-stage dist-ipk` | none (run once on a runner or the dist worktree after disk is freed) | numbers in this plan; decides cache vs paid runner |
| F2 | Buildenv image: Dockerfile (toolchain pin, ixemul-gcc295, ixemul.library, ports sysroot) + `buildenv.yml` pushing to GHCR | `vtcon/tools/ci/Dockerfile.buildenv`, `.github/workflows/buildenv.yml` | `docker run ghcr.io/spotup/vtcon-buildenv m68k-amigaos-gcc --version` works; image under 10 GB |
| F3 | Confirm `make dist` is host-only (no rig, no Docker-in-Docker issue, no `dist-check` rig call); add a `make dist-feed` target = ports `kit-stage` + `dist-ipk` only (skip lha/src if not needed) | `vtcon/Makefile` | `make dist-feed` in the image yields build/ipk/Packages(.gz) and upkg-index.txt |
| F4 | Version policy for same-version rebuilds (sha suffix or counter) | `tools/pkgs.py`, `dist/packages.txt` rules | rebuilding a changed package yields a version `upkg` treats as newer; test in tests for pkgs.py |
| F5 | `feed.yml` in vtcon: triggers (push main, repository_dispatch ports-main, cron sibling check, dispatch), concurrency, build job in the image, cache of ports state | `vtcon/.github/workflows/feed.yml` | a manual dispatch produces artifacts |
| F6 | Create release `feed` once, mark latest; publish job (order: .ipk, upkg-index, Packages, Packages.gz; prune stale assets; move tag; feed-sources.txt) | `feed.yml`, `tools/ci/publish-feed.sh` | `curl -L .../releases/download/feed/Packages.gz` returns the new index |
| F7 | `notify-feed.yml` in upterm-ports (push main -> dispatch) + secret `VTCON_DISPATCH_TOKEN`; keep pr.yml untouched | `upterm-ports/.github/workflows/notify-feed.yml` | a ports main push starts a vtcon feed run (check `gh run list -R spotUP/vtcon`) |
| F8 | Change `dist/upkg.conf` line 6 to `releases/download/feed`; check Files/upkg.conf kit copy and whether upkg overwrites edited confs | `vtcon/dist/upkg.conf`, `Files/upkg.conf` | grep shows one source of the URL; kit build passes `dist-check` |
| F9 | Client behaviour mid-update: confirm upkg retry/404 behaviour; document | upkg docs under `docs/` | written, with measured result on a rig |
| F10 | Migration: `gh release edit feed --latest`, later releases `--latest=false`; document in release procedure | release notes/doc, `RULES.md` Commands | old A1200 conf (`latest/download`) resolves to the feed |
| F11 | Reachability test (section 6) with throwaway `feed-test` release and test fork | `.github/workflows/feed-e2e.yml`, `tools/ci/feed-e2e.sh` | run is green; sentinel version found in Packages.gz and .ipk exists |
| F12 | Owner rig check: `upkg update` + install of a package from the feed on rig 7 and the A1200 | none | owner confirms (human step) |
| F13 | Optional: notify-feed in sibling repos (ixemul-vtcon, tmux-amiga, ...) replacing the cron | each repo | sibling push starts a feed run |

## Owner decisions

1. Runner: GitHub-hosted + prebuilt GHCR image (recommended) / self-hosted Mac (not possible at 2 GB free) / paid larger runner (only if F1 shows >6 h or >14 GB).
2. Sibling repos: cron poll every 6 h (recommended, no secrets in 6 repos) / dispatch workflow in each repo.
3. Latest marker: keep `latest` = `feed` so old installs migrate silently (recommended) / switch conf only (old installs stay on v1.6).
4. Force-moving the CI-owned `feed` git tag per publish (recommended) / leave the tag on its first commit and record the SHA only in feed-sources.txt.
5. PAT secret for dispatch: fine-grained PAT stored in upterm-ports (recommended, simplest) / GitHub App.
6. Same-version rebuilds: sha-suffixed version for feed builds only (recommended) / require the owner to bump the table.

## Owner answers (2026-10-26)
1. Runner: GitHub-hosted ubuntu + prebuilt toolchain image on GHCR.
2. Sibling repos: cron every 6 h comparing source SHAs.
3. Latest marker: `feed` is marked Latest on every publish; v1.x snapshots use --latest=false.
4. Tag force-move: default taken (CI moves the `feed` tag per publish) - not asked separately.
5. PAT: default taken (fine-grained PAT stored in upterm-ports) - owner must create it (F-item).
6. Same-version rebuilds: SHA-suffixed versions on feed builds only.

- F1 placement (owner, 2026-10-26): measure build time and peak disk on the GitHub runner, not locally. F1 runs after F2 (GHCR toolchain image) as the first CI job.
