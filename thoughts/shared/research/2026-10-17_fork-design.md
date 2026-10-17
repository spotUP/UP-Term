# fork() on AmigaOS 3.x / ixemul for git (network) and rsync - design options
Date: 2026-10-17. Scope: research only, no code, no rig. Question: how can UP-Term give git
(fetch/clone/push over ssh and http) and rsync the process model they need, when ixemul has
vfork but no fork? Candidates ranked at the end; phased plan with IDs at the bottom.

Prior notes this builds on (read, not repeated):
- `/Users/spot/Code/UP-Term/vtcon/thoughts/shared/research/2026-10-17_fp44-git-fork.md` (git 2.56.0 fork-site audit)
- `/Users/spot/Code/UP-Term/upterm-ports/thoughts/shared/fp44-git-notes.md` (git local port: vfork start_command, async stubbed)
- `/Users/spot/Code/UP-Term/upterm-ports/thoughts/shared/fp44-rsync-notes.md` (rsync 3.4.1 fork sites)
- `/Users/spot/Code/UP-Term/upterm-ports/thoughts/shared/fp44-perl-notes.md` (perl built with `-Ud_fork`)

Paths: ixemul = `/Users/spot/Code/UP-Term/ixemul-vtcon`; git host tree =
`/Users/spot/Code/UP-Term/upterm-ports/build/host/git` (2.56.0 with the port patches applied).
No rsync 3.x tree is on disk; rsync line numbers come from fp44-rsync-notes.md (so marked).

## What exactly needs fork, and what kind of fork

| Tool | Site | What the child does after fork | Fork flavour needed |
|---|---|---|---|
| git | `run-command.c:787` start_command | dup2/chdir/signal/execve only | vfork (DONE in port: `-DUSE_VFORK`) |
| git | `run-command.c:1209` start_async (NO_PTHREADS) | runs `async->proc(in, out, data)` in-process, then `exit()` | a concurrent "thread" with the parent: reads one fd, writes another |
| git | `setup.c:2218` daemonize (gc --auto) | detaches | not needed; port returns -1/ENOSYS |
| git | `transport-helper.c:1624/1633` | fork loop for remote helpers | same as start_command (vfork+exec) |
| rsync | `main.c:1032` do_recv | child = receiver, parent = generator; both keep using the parent heap (file list, options, io buffers, error_pipe) | real fork OR a redesign |
| rsync | `pipe.c:124` local_child | in-process `child_main` acting as server for local copies | real fork OR vfork+exec of `rsync --server` |
| rsync | `pipe.c:62` piped_child | fork+exec of the remote shell (ssh) | vfork+exec (IXPIPE: for a native child) |
| rsync | `socket.c:599/823` daemon | fork per connection | out of scope (no daemon mode) |

Async callers in git that matter for UP-Term (non-server): `fetch-pack.c:985` (sideband_demux,
`fetch-pack.c:897`: `recv_sideband("fetch-pack", xd[0], out); close(out);`),
`send-pack.c:703` (push sideband demux, `send-pack.c:251`), `convert.c:725` (clean/smudge
filter; then `strbuf_read(&nbuf, async.out, 0)`). `finish_async` (`run-command.c:1318`) under
NO_PTHREADS does `wait_or_whine(async->pid, ...)` then `invalidate_lstat_cache()`.

Key property: every git async proc is a pure pump (read fd A, write fd B / stderr) that
touches only its own stack, a few sideband statics (`sideband.c:23` keyword table, `:131-133`
colour cache set once via config) and `xmalloc`. It does not depend on the parent's heap
changing underneath it, and the parent does not read the child's writes to globals.
rsync's receiver/generator pair is the opposite: both halves mutate shared globals
(`am_generator`, `am_receiver`, io buffers, stats) and expect isolation.

## Candidate 1: ix_vfork + ix_vfork_resume as a "thread with its own fd table" (ixemul, exists)

How it works. ixemul's vfork child is a separate Amiga Process made with `CreateNewProc`
(`library/vfork.c:682` `_vfork(int own_malloc, struct reg_parms rp)`, tags NP_Entry launcher,
NP_Name "vfork()'d process", NP_StackSize, NP_Cli). The launcher (`vfork.c:153-412`) gives the
child its own `struct user`: a copy of the fd table with `f_count++` on every open file,
sockets released by the parent and re-obtained by the child (ix_release_socket /
ix_obtain_socket), own uid/gid/pgrp/session, own signal state, own atexit list
(`library/exit.c:39` `#define __atexit (u.u_atexit)`). Code, data, a4 (u_segs/u_a4) are shared.
With plain `vfork()` the parent sleeps on `u_sync_mp` until the child execs or `_exit`s.

ixemul also ships two documented extensions in `include/ix.h:43-88`:
- `int ix_vfork(void)`: like vfork but the child gets a private malloc pool
  (`vfork.c:519` `vfork_own_malloc`), own errno/h_errno/environ.
- `void ix_vfork_resume(void)` ("DO NOT use with vfork(), only with ix_vfork()"):
  `_vfork_resume(u_int *copy_from_sp)` (`vfork.c:622`) copies the live stack frame into the
  child's own stack, sets SPLower/SPUpper, and `ReplyMsg`s the parent so BOTH run.
  Comment at `vfork.c:607`: "Since I can't provide a real fork() on the Amiga, this function is
  a third possibility to make the parent resume. You have then two concurrent processes sharing
  the same frame and global data." ksh used this historically.

That is exactly the shape git's start_async wants under NO_PTHREADS: two schedulable
processes, shared globals, separate fd tables (so `close(unused ends)` in the child does not
close the parent's), separate malloc so the child's `xmalloc`/`free` never corrupt the parent
pool, and `waitpid(async->pid)` works because the child is a real ixemul child.

Patch (git, `run-command.c` under `#if defined(NO_PTHREADS) && defined(USE_VFORK)`, replacing
the port's current ENOSYS stub), about 40-60 lines:
1. `fflush(NULL); async->pid = ix_vfork();`
2. child: `ix_vfork_resume();` (parent continues), close the parent's pipe ends it does not use,
   `process_is_async = 1`, `int r = async->proc(proc_in, proc_out, async->data);`
   then `_exit(!!r)` (not `exit`: do not run the parent's atexit list; the child's own list is
   empty anyway, and `git_atexit_clear()` becomes unnecessary).
3. parent: unchanged (close child ends, return 0). `finish_async` unchanged.
4. Rules for the child: never `free()` memory allocated by the parent (it lives in the parent's
   pool); never write to parent globals it did not own before the call. The three async procs
   already obey this (they allocate only through their own `xmalloc` and write to fds).
5. `async_with_fork()` must stay true so `send-pack.c:251` closes the write end in the child.
6. Stack: `ix_vfork_resume` copies only the current frame onto a new stack; the child's stack
   size comes from the process (`git_STACK 262144` in recipe.mk). `recv_sideband` uses a
   fixed `LARGE_PACKET_MAX` buffer (about 64 KiB) on the stack: within 256 KiB.
7. `tools/check_bin.sh:34` rejects `_fork`/`_fork1`/`_forkpty` symbols; `ix_vfork` is not in that
   list, so no tooling change.

Effort: S (one patch file in `pkgs/git`, plus one reachability test: `git fetch` from a `file://`
remote that forces sideband, i.e. the local protocol with `uploadpack` sideband; sentinel =
the async pid differs from getpid and the demux writes bytes).
Risks: (a) `ix_vfork_resume` is old and little used; the frame copy assumes the caller's frame is
self-contained (the start_async code must call it from the function that then calls `proc`, not
through a helper that returns); (b) signal delivery to the child (SIGPIPE on a closed sideband
output) goes to the child's own signal state, which is a copy of the parent's: git's async
procs expect SIGPIPE ignored or EPIPE: keep `sigchain_push(SIGPIPE, SIG_IGN)` behaviour as in
the pthread branch; (c) both processes share `errno`? No: `ix_vfork` gives the child its own
errno (ix.h); (d) `invalidate_lstat_cache()` after wait is harmless.
Unlocks: git network over any exec'd transport (ssh/local/file: fetch, clone, pull, push),
clean/smudge filters (git-lfs style). Does NOT unlock rsync: rsync's pair shares and mutates
globals, and the receiver `free()`s parent-pool memory; a same-address-space "thread" breaks it.
Verdict: BEST for git. Not applicable to rsync.

## Candidate 2: per-tool rewrite into vfork+exec helpers or single-process loops

git. Same patch as candidate 1; no other git site needs a continuing child
(`fp44-git-fork.md` "Smallest source change"). transport-helper's fork loop is exec-shaped and
already goes through start_command.

rsync (3.4.1, sites from fp44-rsync-notes.md):
- `piped_child()` (`pipe.c:62`, fork+exec of the remote shell): vfork+exec. On UP-Term the shell
  is native BebboSSH (the kit ships ssh and curl as libnix builds; plan row 5.7 "ixemul rsync
  spawning native bebbossh through IXPIPE: (exists). Probe the pipe throughput first"). ixemul
  routes a native child's stdin/stdout through IXPIPE: unless they are plain files
  (`stdlib/execve.c:649-666`: "Any other file type is routed thru an IXPIPE though"). Effort S;
  risk = IXPIPE: throughput and whether BebboSSH's stdin reading tolerates a pipe handler.
- `local_child()` (`pipe.c:124`, in-process server for `rsync src dst` on one machine):
  replace with vfork+exec of `rsync --server <server_options()>` connected by a socketpair or
  two pipes. rsync already has `server_options()` (builds the argv it sends to the remote side)
  and the `--server` entry point, so the exec'd copy is a normal remote server. Effort S-M
  (about 60-100 lines). Cost: a second rsync image in memory (binary is shared code, data copied
  by the loader; fine on a 060 with fast RAM).
- `do_recv()` (`main.c:1032`, receiver/generator pair): the hard one. Two options:
  (2a) exec a receiver helper: after `recv_file_list`, the client vforks+execs
  `rsync --server --receiver-helper <options>` and hands it the file list over a pipe using the
  existing `send_file_list`/`recv_file_list` wire format, plus the data fd and error_pipe.
  The generator stays in the parent. Needs a new entry point that: parses options, reads the
  file list from fd, calls `recv_files()`, exits with the receiver status. Effort L (roughly
  300-600 lines across main.c/options.c/flist.c/receiver.c, plus keeping `--server` protocol
  state consistent: compat flags, checksum seed, protocol version must be passed down).
  Risk: rsync's receiver reads from the same socket as the generator writes to (full-duplex
  single fd); passing the fd to an exec'd child is fine on ixemul (f_count++ on exec), but the
  generator must not read from it: that is already the design (generator writes only).
  (2b) single-process rewrite, the DOS/Amiga 2.5.5 way: Chris Simmonds' MSDOS port "replaced
  several core functions with new implementations that do all of the work in a single process"
  (README.Amiga in the Aminet rsync-2.5.5_bin archive; the Amiga port by Chris Jones is a
  recompile of it for ixemul 48.2). Documented limits of that approach: no daemon, no
  ownership/permissions, no rsh/ssh, no `rsync://`, no backup files, no compression, no
  bandwidth limit (same list in the FreeDOS rsync 2.2.5 package text). Effort M against 2.5.5
  (already done, unmaintained protocol 26, no modern server will talk to it) or XL against 3.4.x
  (the generator/receiver split is deeper now: incremental recursion, `--delete-during`).
Effort total for rsync usable over ssh with a modern server: L (piped_child S + local_child S +
do_recv helper L). Risks: protocol-state duplication in the helper; IXPIPE: throughput for ssh.
Unlocks: git network (via the candidate-1 patch), rsync client over ssh and local copies.
Verdict: the only realistic route for rsync; pick (2a) over (2b).

## Candidate 3: real fork() by copying data/bss/heap/stack into a new Process (no MMU)

How it would work. The child Process gets fresh copies of the parent's writable segments,
heap and stack, and runs the same code. Without address translation the copies live at
DIFFERENT addresses, so every pointer inside them (heap-to-heap, stack-to-heap, globals-to-heap,
return addresses, saved frame pointers, a4-relative data under -fbaserel only for the data
segment) is wrong in the child. Prior art checked:
- ixemul itself: `library/misc.c:164` `fork()` sets errno=ENOSYS and returns -1; the author's
  comment at `vfork.c:607` says a real fork is not possible and offers ix_vfork_resume instead.
- MiNT (Atari, same 68k, same flat memory): the 1991 manual says Pfork "copies the address
  space" but "the current implementation suspends the parent until the child exits or execs"
  (discmaster copy of the MiNT 1991 manual, pfork.html); the modern tos.hyp entry still
  describes Pfork as copy-and-suspend with Pvfork sharing. MiNT's real trick was to copy the
  child's data back and forth on every task switch (both run at the SAME address, only one is
  resident): correct, but O(size of data+heap+stack) per context switch.
- no-MMU Linux/uClinux and OpenBSD-nommu discussions (LKML 2006, "copy-on-task-switch fork")
  reach the same conclusion: either vfork or swap-on-switch.
- libnix, clib2 (OS3), AROS, MorphOS, OS4 newlib/clib2: none has fork; OS4's simplegit port
  (billyfish/simplegit build.md) is a libgit2 front end, not a forked git.
- Geek Gadgets ixemul (the ancestor of ixemul-vtcon): same vfork + ix_vfork_resume.
Copy-on-switch cost for the two targets: git's async pair exchanges one packet at a time
(sideband frames up to 64 KiB) across a heap of tens of MB during a fetch: unusable.
rsync's receiver/generator exchange per-file messages over a heap holding the whole file list:
unusable.
Effort: XL to build, and the result is slower than the rewrite. Risks: correctness holes
everywhere (pointers in copied memory, DOS file handles and Exec message ports in the copied
`struct user`, timers, bsdsocket descriptors). Unlocks nothing the cheaper candidates do not.
Verdict: rejected.

## Candidate 4: real fork() through mmu.library (Thomas Richter's MuLib)

How it would work. MuLib contexts are per-Task address spaces: "Each Exec Task belongs to a
MMUContext ... Contexts are scheduled as the Tasks belonging to them are scheduled ... a
Context should be seen as the AmigaOs equivalent of a Unix process" (MuLib manual, pdftotext
in the session scratchpad, mmudoc.txt lines 203-260). A private context is made with
CreateMMUContext, a task is moved into it with `EnterMMUContext(ctx, task)`; pages can be
mapped with MAPP_WRITEPROTECTED (fault on write, so copy-on-write is possible via the
exception hook), MAPP_GLOBAL (shared across contexts), MAPP_REMAPPED (alias to another
physical range). With that, fork could: allocate the child's copies of heap/stack/data, map
them at the parent's virtual addresses in a child context, create the child Task in that
context. Pointers then stay valid.
Why it still fails for a libc:
- Supervisor code and all OTHER tasks run in the public context. Every filesystem handler
  (ram-handler, FFS, PFS3, the UP-Term NAS handler), bsdsocket.library's own task, console
  handlers and DMA devices read and write the caller's buffers at the addresses the caller
  passes. A remapped child buffer is invisible or wrong from those tasks. ixemul would need
  copyin/copyout bounce buffers in every syscall that passes user memory to DOS/bsdsocket
  (read/write/stat/getdents/recv/send/select/ioctl...), and every DOS packet it sends.
  The manual's DMA section (chapter 5) exists precisely because public-context aliasing is
  not automatic.
- Granularity: pages are 4 KiB (or 8 KiB) and AmigaOS allocations are not page-aligned; the
  heap, data and stack segments must be allocated page-aligned and never share a page with
  anything public (ixemul's own `struct user`, message ports, the CLI structures). Doable for a
  fresh process layout, not for arbitrary ixemul binaries.
- Hardware: 68020+68851, 030, 040, 060 only; UP-Term rigs are 060 so that is fine, but the
  kit then depends on mmu.library being installed and configured (MuFastRom/MuProtectModules
  users already have it; others do not), and it conflicts with other MMU users (Enforcer,
  MuForce, CyberGuard, debuggers).
- The child's Exec Task still shares the public context for interrupts and supervisor code,
  so any ixemul path that touches user memory from an interrupt (the VERTB itimer server
  `u.u_itimerint`, signal delivery via `tc_Launch`/`tc_Switch` hooks at `execve.c:640-642`)
  must be made context-aware.
Effort: XL (new ixemul subsystem plus bounce buffers in the syscall layer), weeks, and it
produces a fork that is slower than native (bounce copies) and fragile. Risks as above.
Unlocks: in theory everything (rsync unchanged, git unchanged). In practice an endless tail
of "works on my rig" bugs.
Verdict: rejected. Keep in mind only as a far-future "real UNIX processes" project.

## Candidate 5: existing Amiga ports of git and rsync

- rsync: Aminet `util/misc/rsync-2.5.5_bin` (Chris Jones, ixemul 48.2, recompile of Chris
  Simmonds' MSDOS single-process port, http://www.2net.co.uk/rsync.html). Client only, protocol
  26, no ssh/rsh, no daemon, no compression. Confirms candidate 2b is possible and shows its
  ceiling. Nothing newer on Aminet; no OS4Depot rsync found; no AROS/MorphOS rsync with fork.
- git: no OS3 port found. OS4 has simplegit (libgit2-based, billyfish) and a libgit2 build: a
  different program, no fetch-pack/send-pack process model. OpenSSH on OS3 (Diego Casorran,
  2003, openssh-unix-dev list) was built with `-Dfork=vfork`: a hack that works because sshd's
  forks exec or exit quickly; same trick as git's start_command, not applicable to async.
- Perl: UP-Term's perl is built with `-Ud_fork` (fp44-perl-notes.md); unaffected by this choice.
Verdict: no reusable artefact; the rsync 2.5.5 port is evidence for candidate 2b only.

## Other blockers beyond fork (per transport / feature)

| Blocker | Needed by | ixemul status (evidence) | Verdict |
|---|---|---|---|
| ssh transport | git fetch/push over ssh, rsync over ssh | git/rsync vfork+exec the native BebboSSH binary; ixemul routes the child's pipes through IXPIPE: (`stdlib/execve.c:649-666`); plan row 5.7 says probe throughput first; comment in execve.c says ptys must go through ixpipe too | PROBE (FK2) |
| https transport | git clone https:// | git needs libcurl (recipe: `NO_CURL=1`); an ixemul libcurl needs TLS: AmiSSL from an ixemul program has different socket descriptors (plan row 5.6 "risk, probe first"); alternative: a `git-remote-https` helper written as a thin script around native curl, dumb-HTTP only (no smart protocol push) | LATER (FK4) |
| bsdsocket from ixemul | all network | ixnet sockets exist (`library/socket.c`); git local port already links; smart protocol over ssh does not use sockets directly | OK |
| select/poll on pipes | sideband demux, rsync io | `library/pipe.c:159-176` DTYPE_PIPE with `f_select = unp_select`; `library/poll.c` implemented over f_select | OK |
| socketpair | git credential helpers, rsync local_child replacement | `library/socket.c:261` -> `unp.c:1209 unp_socketpair` (AF_UNIX, SCM_RIGHTS) | OK |
| setsid / daemonize | gc --auto | `library/misc.c:76` setsid (EPERM when session count<=1); port's daemonize returns -1 | OK (feature off) |
| mmap | git packfile reading | `library/mmap.c:169` mmap, `:341` munmap (MAP_ANON/SHARED/PRIVATE); git also has NO_MMAP fallback | OK |
| pthreads | git pack/index threads, preload-index | none in ixemul (grep pthread_create: zero); `NO_PTHREADS=1` makes them sequential | OK (slower) |
| signals in the ix_vfork child | SIGPIPE in sideband demux | child has its own signal state copied from parent (`vfork.c:153-412`) | OK, keep SIG_IGN push |
| pid guards | tempfile.c:68-74 owner pid, async `process_is_async` | child is a real pid (CreateNewProc) | OK |
| stack | async child frame copy | `git_STACK 262144`; recv_sideband buffer about 64 KiB | OK |
| rsync daemon mode | rsync:// serving | fork per connection | OUT OF SCOPE |
| rsync compression | -z | ported zlib exists (recipe EXTLIBS libz.a) | OK |
| rsync ownership/permissions | -a | ixemul maps uid/gid/mode onto Amiga protection bits | PARTIAL (document) |

## Recommendation

1. git: candidate 1. Patch start_async to `ix_vfork()` + `ix_vfork_resume()` under
   `NO_PTHREADS && USE_VFORK`, child calls `async->proc` and `_exit`s. Small, uses a
   documented ixemul feature, no new ixemul code, no tooling change, and finish_async's
   waitpid already matches. This alone gives fetch/clone/pull/push over ssh and file.
2. rsync: candidate 2a. Replace `local_child` and `piped_child` with vfork+exec (`rsync
   --server`, native ssh through IXPIPE:), then split `do_recv` into an exec'd receiver helper
   fed the file list over a pipe in rsync's own wire format. Larger, but keeps protocol 31/32
   and modern servers. Do the git patch first; its ssh/IXPIPE: probe (FK2) is shared.
3. Reject 3 (no address translation: copies break pointers; swap-on-switch is O(heap) per
   message) and 4 (public-context handlers and bsdsocket cannot see remapped pages; bounce
   buffers in every syscall; mmu.library dependency).
4. Do not consider an ixemul `fork()` that returns a `ix_vfork_resume`-style twin as a general
   libc fork: it is only safe for code that never frees parent memory or mutates shared
   globals, i.e. git's async procs. Keep `fork()` returning ENOSYS so check_bin.sh keeps
   catching accidental users.

## Phased plan

- FK1 git async via ix_vfork (S). Patch `pkgs/git` 0004: start_async/finish_async as above.
  Reachability test: `git clone file://` of a repo on the rig with `uploadpack.allowSidebandAll`
  / protocol v2 forcing sideband, sentinel = child pid in stderr trace (`GIT_TRACE=1` shows
  "run_command: async") and a non-empty pack received. Also `git push` to a file remote.
  Exit: clone, fetch, push over file:// pass on rig 2.
- FK2 ssh transport probe (S, shared git/rsync). ixemul git execs native BebboSSH through
  IXPIPE:; measure throughput for a 10 MB fetch and check that BebboSSH reads stdin from a pipe
  handler without a pty. Exit: `git clone user@host:repo` passes; numbers in the ledger.
  Fallback if IXPIPE: is too slow or breaks: port BebboSSH's transport to ixemul (plan 5.6
  risk) or use `core.sshCommand` pointing at a small ixemul wrapper that opens the pipe itself.
- FK3 rsync vfork+exec sites (S-M). `piped_child` -> vfork+exec; `local_child` -> vfork+exec
  `rsync --server` with `server_options()` argv over socketpair. Exit: `rsync -av a/ b/`
  local copy passes; `rsync -av host:dir/ b/` reaches the do_recv blocker and fails cleanly.
- FK4 rsync receiver helper (L). New `--receiver-helper` entry point; parent vforks+execs it
  after `recv_file_list`, hands it the file list via `send_file_list` over a pipe, the data fd
  and the error pipe; parent continues as generator. Exit: pull from a Linux rsync 3.x over ssh
  with `-av --delete`, byte-identical tree, sentinel = helper pid logged with `-vv`.
- FK5 git https (M, optional). Either `git-remote-https` helper around native curl (dumb HTTP,
  fetch only) or ixemul libcurl + AmiSSL probe (socket descriptor mismatch risk). Decide after
  FK2 shows whether ssh covers the user's remotes.
- FK6 cleanup. Document in fp44-git-notes.md and fp44-rsync-notes.md what is supported;
  keep `fork()` ENOSYS and check_bin.sh unchanged; add `ix_vfork` note to RULES.md ports
  section ("async children: use ix_vfork + ix_vfork_resume only for pump-style procs").

Open questions for FK1 (answer by test, not research): does `ix_vfork_resume` survive being
called from a function compiled with -O2 that keeps live values in registers across the call
(the asm `_ix_vfork_resume` at `vfork.c` about line 600 saves the frame; confirm d2-d7/a2-a6
are restored in the child); does the child's own malloc pool get freed at `_exit` (vfork.c
`vfork_own_malloc` path) so a long fetch does not leak per async call.
