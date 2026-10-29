---
date: 2026-10-28
session: vtcon-05 session 13 (UP-Term side)
status: in progress (rig 10 control-runs agent running when this was written)
ledger: thoughts/spot/todos.md (grep rows by name, never read whole)
---

# vtcon-05 session 13 handoff: rig 9/10 crash after bsdsocket start, crt0 ctors fix parked

## Slot rules (keep)
- Rigs 9 and 10 are ours. Never touch rigs 1, 2, 3, 4, 7, 8 (3/4/7 vtcon-a1, 2 Selecta, 8 musicplayer-7f). Never stop rig 1; tell the owner.
- Stop a rig only after the last command plus 5 s quiet.
- upterm-ports main checkout is on another session's branch: use worktree upterm-ports/.claude/worktrees/kit-main (branch port/curl-amissl).
- ixemul-vtcon main checkout: leave dirty include/sys/fcntl.h, include/sched.h, compat/test/fstatat_probe and the ixemul-mkdir, np-cli, wb-cd worktrees alone.
- Never write images/nyhd2.hdf without the owner's approval. Fix only clones.
- Keep VTC:Songs in every rig (owner uses it to test a music player).

## Current blocker: RIG10-RESET-LOOP
Findings: thoughts/spot/term-findings/rig10-reset-loop.md (sections Correction 02:12, bsdsocket hunt, Lead, Control runs).
- Rig 10: an ixemul program under Stack 262144 logs uae_start_tread "bsdsocket", then Illegal instruction 0009 at 00F9F366 (mid-instruction in Kickstart 40068), then a reset loop.
- Rig 9 died in the Installer the same way (Exception 3 after bsdsocket thread start); this is the owner's "loops back to TCP/IP and freezes" report.
- Ruled out: new crt0 (earlier verdict was wrong, corrected), kit ixemul build (master-image ixemul also crashes), ixnet, JIT, plain bsdsocket OpenLibrary, boot.
- Confound found: t_hello_old was linked with a docker-built crt0 ($SP/old, md5 735df185), not the installed toolchain crt0 (790224b8). A control-runs agent tests A existing ixemul program, B stock-crt0 hello, C docker crt0.
- Next if A crashes: bisect clone contents against a peer rig that does not crash (rig 7 logs bsdsocket starts without crashing; compare LIBS:, S:User-Startup, DEVS:).
- term-findings/curl-amissl.md line 114: OpenSSL_version alone froze rig 10 earlier. Recheck curl https results once the rig crash is understood; they may have been this bug.

## Parked: IXEMUL-CRT0-CTORS
- Commit 8c1eebb on branch fix/crt0-ctors in scratch worktree $SP/ixw (ixemul-vtcon, parent 8dad95d), not pushed. Test tests/host/test_crt0_ctors.py uncommitted (passes with CRT0_DIR=$SP/new).
- $SP is session-scoped: before the session ends, push the branch so the work survives.
- Remaining: rig proof (t_hello, t_ctor, cv5), PR (no footer), install into ~/opt/amiga/m68k-amigaos/ixemul/lib/*crt0.o (owner approved after rig proof), relink curl, test https, tell vtcon-a1 (UHC-HTTPS waits on curl).
- Open: GCC-6 .list___CTOR_LIST__ section not covered.

## Done this session (origin main)
2e9c21d0, 6aa65523, 561da46c, 423b9619, a9654527, 9883fbe7, 3096ae26, 752ccc45 (installer_rig prints answered pages when the link drops).

## Next after the crash and curl
upkg two-directory install rig proof (8c76c96d), RIG-KIT-DERIVE rig proof, RIG-RECHECK-STALE, INSTALLER-CLICK-SPEED step 3, TLS-SYSROOT (lynx, mutt, git), AmiNetXDuo case B (no-stack rig). NVIM-HUNT paused.

## Owner to-dos
- Rig 1 was booted by mistake by an agent (pid 86047, build/rig, 01:44); we may not stop it.

## Update: cause found

The rig 10 reset loop came from the docker-built crt0, not from ixemul, ixnet or the clone. Its .text started with the static `_va` wrapper functions that the build-inlines `inline/intuition.h` defines, so programs began executing inside `__EasyRequest_va`. Branch `fix/crt0-ctors` commit b37b93e moves the entry asm above the includes and adds a host test that checks offset 0 is a `bra.w` to `_ENTRY`. Rebuilt objects: `$SP/new2`. Next: rig proof (t_hello, t_ctor), then the PR, then the toolchain install and the curl https retest. The rig 9 Installer `Exception 3` is a separate open bug.

## End of session 13 (2026-10-28 late)

Merged to origin main this session:
- upterm-ports #19 (libgsasl 2.2.4 + mutt SASL), #20 (tig_RUNTIME_DEPENDS), #21 (git https over AmiSSL via libcurl, 62dbb86).
- UP-Term #5 (upkg Depends from `<id>_RUNTIME_DEPENDS`), #6 (git feed rows regenerated; pkg_rows uncovered_installs skips `$` paths, f17061d).

Closed analysis: IXCOMPAT-POLL-BSDSOCKET is an fd-space mismatch, not a poll() bug; keep the port-side approach (thoughts/spot/term-findings/ixcompat-poll-bsdsocket.md).

Open, in order:
1. RIG9-INSTALLER-CRASH hunt 7: name the reset caller. Hunt 6 ruled out Files/Reboot as trigger; likely the garbage bsdsocket base call from hunt 4. Plan and traps in the ledger row "hunt 6" and thoughts/spot/term-findings/rig9-installer-loop.md "## hunt 6 reset caller".
2. Small poll follow-ups: comment in ixemul-vtcon compat/poll.c, check that fails when a `-lamisslstubs` port has HAVE_POLL.
3. git README note: needs a long-filename volume (new tools/pkg_rows_prose/git.json).
4. ports main git recipe lacks git_SUMMARY (tests.test_dist_ipk errors). The Amarchy session (vtcon-32) may hit it; it said it will ask before anyone edits ports.
5. Other candidates: curl rig check cases, upkg two-directory install rig proof, RIG-KIT-DERIVE, RIG-RECHECK-STALE, INSTALLER-CLICK-SPEED step 3, AmiNetXDuo case B.

Coordination: vtcon-32 is preparing the UHC demo video from main on rig 7 and will ping when main is pushed. Our rigs: 9, 10, 11. All stopped.
