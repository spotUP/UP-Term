# git kit: transport helpers (2026-10-17)

Problem: plain `git clone file://...` on the A1200 failed with `bad line length character: vsh:`.
git is built with SKIP_DASHED_BUILT_INS; the shell transport runs `git-upload-pack` / `git-receive-pack`, which were not in the kit.

Upstream (Makefile, ALL_COMMANDS_TO_INSTALL else-branch): with SKIP_DASHED_BUILT_INS the three
git-receive-pack, git-upload-archive, git-upload-pack are still installed in bin/ "because remotes expect them".
The port's INSTALL_CMD copied only bin/git.

Fix (upterm-ports pkgs/git/recipe.mk): install the three as copies of git (git.c dispatches on argv[0]; NO_INSTALL_HARDLINKS,
no hard links on the Amiga volume), add them to git_BINS (so check_bin, rig staging and kit-stage ship them), install
share/git-core/templates (git_DATA; before: "warning: templates not found"), git_CHECK_DEPS := coreutils (init/merge/clone cases use rm, cat).
Cost: git is 3,760,540 bytes, so 3 copies = about 11.3 MB more in the kit. libexec/git-core is NOT shipped: git's own dashed
commands are skipped and no remote helpers are built (NO_CURL); the three live in bin/ which is on PATH.

Cases (pkgs/git/check/cases): `--upload-pack="git upload-pack"` and `--receive-pack=...` removed.

Evidence (rig 1, userland_rig.py --only git):
- before (old kit, plain clone/fetch/push): fetch and push FAIL (empty output, helper not found). 4 of 9.
- helpers only: fetch, push PASS; init/merge/clone failed only because rm/cat were not staged (rig, not git).
- after: 9 of 9 PASS. Logs in the session scratchpad: before.log, after.log.

bd732f6 and FK1: yes. bd732f6 is in main history (merge 5f834d0) and changes patches/0004; 0004 is the FK1 start_async via
ix_vfork + ix_vfork_resume change and the unpack step applies pkgs/git/patches/*.patch to the source the kit builds.
