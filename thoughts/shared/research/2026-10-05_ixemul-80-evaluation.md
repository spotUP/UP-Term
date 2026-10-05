---
date: 2026-10-05
topic: ixemul 80.1 (Aminet) against ixemul-vtcon 48.2+UP-Term at fa0156c -- re-evaluation with measurements
tags: [ixemul, abi, af_unix, scm_rights, pty, termios, sigwinch, libixcompat, rebase]
status: final
---

# ixemul 80.1: re-evaluation against ixemul-vtcon fa0156c

**Verdict: stay on the 48.2 fork (option a). Backport three small 80.x fixes in the
SIGWINCH/input.device area. A rebase onto 80.x stays a scheduled item, not a drop-in.**

The ABI is not a blocker. The vector table is append-only: measured on the binaries and on
libc.a, nothing below. What rules out an as-is switch is behaviour. 80.1 returns
`EPFNOSUPPORT` for `socketpair()` and `EOPNOTSUPP` for AF_UNIX `sendmsg`/`recvmsg`, which breaks
tmux and GNU screen. It also has no termios, PTY: or job-control path for vtcon.

Of our 27 commits since 48.2, 80.1 covers **3**:

- the `unp_accept` NULL name;
- the path a failed connect leaves bound;
- `poll()`/`realpath()`.

A few header items are partly covered too. Everything else has to be carried. About 12 of
those commits sit in files 80.1 rewrote.

**This updates `2026-09-30_ixemul-80.1-vs-our-48.2-patches.md`.** That document already found
80.1 (the premise that the 09-29 research "missed" it is only true of the 09-29 document) and
recorded the decision at `plans/2026-09-28-vtcon.md:1011`. Its verdict stands. What this
document adds:

- the 13 commits made since that comparison (`5c7bd59..fa0156c`);
- **measured** ABI checks (vector counts read from the binaries, libc.a stub offsets);
- the ixnet version lock;
- the SIGWINCH analysis against vtcon's injected resize events;
- the compat-layer audit of the ports;
- a cost estimate and the rig plan.

Trees used (session scratchpad, not kept):

- `ix80/x/` = `ixemul-80.1-m68k.lha`, sha256 `9bf3d573…308ae`.
- `ix80/cmp` = a git repo whose HEAD~1 is 48.2 pristine (`ixemul-vtcon 1ee186a`) and whose HEAD
  is 80.1's `ixemul/`.

80.1 file:line references below are into that tree. "ours" means `~/Code/ixemul-vtcon` at
`fa0156c` (branch `feature/wide-chars`).

## 1. Archive, source, licence, upstream

- **Download:** `https://aminet.net/dev/lib/ixemul-80.1-m68k.lha`, 5,536,049 bytes. Unpacked
  with `/opt/homebrew/bin/lha xq` (lha, 7z and unar are all on the host). **80.0 is gone from
  Aminet**: `ixemul-80.0-m68k.lha` and its readme both return 404, because the 80.1 readme says
  "Replaces: dev/lib/ixemul-80.0-m68k.lha".
- **Full source is included**, in `ixemul/`: 2372 files in the archive, all source directories
  plus `NEWS`, `ChangeLog`, `BUGS`, `BUGS_SIGWINCH`, `TODO` and `NEW_FUNCTIONS`. The binaries
  are in `Sys/libs`:
  - ixemul 000, 000-notrap, 000.trace, 020, 020-fpu, 040, 040-fpu, 060-fpu;
  - ixnet 000/020/040/060.
  - `gg/` holds `libc.a`, the crt0 variants, headers and tools.
- **Licence:** GNU LGPL v2 or later (`LICENSES.README`, `COPYING.LIB`), with BSD notices kept per
  file. 80.1 restored notices that 80.0's automated edits had dropped (ChangeLog 2026-09-24).
- **Upstream:** there is **no public repository**. Neither the readme, NEWS, ChangeLog nor
  README names one. Detailed history lives only in per-file revision comments (ChangeLog
  2026-09-13: "Detailed changes … are recorded in the revision comments").
  - The maintainer is Jürgen Johansson (uploader address in the readme).
  - The readme states that the 80.x work was done with ChatGPT, Copilot and DeepSeek.
- **Toolchain:** "built in an ADE or Geek Gadgets development environment using GCC 2.95.3"
  (readme, "Source code"). That means a native Amiga build.

## 2. Our patches against 80.1

The **ours** column lists the commit in ixemul-vtcon. The **80.1** column gives the evidence in
`ix80/cmp`.

| # | Patch | Ours | 80.1 | Verdict |
|---|---|---|---|---|
| 1 | gcc 6 macOS build: catenation, own stdint.h | e2a51ca, 32efe2b | Native ADE build only. Makefile now in-tree with `true_srcdir=../../..` (`library/Makefile.in`). Host `gcc` runs `create_header` (needs native m68k or our cross fix). New `intops/`. Own `include/stdint.h` (+193). | **rework** build patches; drop our stdint.h |
| 2 | Docker gcc 2.95.3 build | bc7bbee, f354e6f | none | **keep**, adapt to in-tree layout and `intops` |
| 3 | termios and winsize by packet to vtcon | bf02921 | `__tioctl.c` refactored (+466/-92). TIOCGETA still synthesised from `f_ttyflags` (`__tioctl.c:376`). TIOCSWINSZ ignored (`:662`). | **rebase**: 4 early returns re-inserted by hand |
| 4 | ^\ and ^Z as SIGQUIT/SIGTSTP | 905dc76 | `machdep.c` unchanged | **rebase**, clean (`user.h` field re-appended) |
| 5 | Job control: stop, fg, bg | cfe5322 | `kern_sig.c` +23 (SIGWINCH only); stop code still `#if notyet` | **rebase**, clean |
| 6 | BSD ptys on PTY:, SIGWINCH to the slave's group | 64ae67f | still FIFO: ptys (`open.c:266-290`). Same string-literal bug fixed differently (stack buffer). `__close.c` select kick reworked onto per-process state (`__close.c:125-155`). | **rebase**, `__close.c` redesigned |
| 7 | Version id "[UP-Term …]" | e085db4 | `version.in` 80.1 | **rebase**, trivial |
| 8 | SIGTTIN/SIGTTOU | 6d36ffc | `__read.c` +183, `__write.c` +257 rewritten. Anchor in `ix_exec_entry.c` removed. | **rebase**, by hand |
| 9 | `_psignal` only for ixemul processes | 98cefba | `_psignal` still dereferences `getuser(t)` unguarded (`kern_sig.c:605-618`) | **rebase**, clean |
| 10 | `unp_accept` NULL name | ac7b11c | `copyout_unix_address` is NULL-safe (`unp.c:526-534`), called at `:1006` | **drop** (80.1 covers) |
| 11 | sendmsg/recvmsg with SCM_RIGHTS | bc5a7a1 | `EOPNOTSUPP` for DTYPE_USOCKET (`socket.c:327-331`, `:399-403`). `unp.c` rewritten (+1521/-466) with refcounted streams (`unp.c:172-250`). | **rewrite** onto the new stream model |
| 12 | libixcompat poll(), realpath(), `<poll.h>` | e137b24 | Vectors `realpath` 643 and `poll` 656 (`syscall.def`). Header values differ (`POLLWRNORM` 0x100 vs our 4). | **drop** the compat copies; keep one `<poll.h>` |
| 13 | stat of /dev/tty; failed connect leaves the socket unbound | 825ece7 | /dev/tty still maps to `"*"` (`__plock.c:213`). connect writes `server_path` only on success (`unp.c:1079`). | **split**: /dev/tty rebases, the connect part is dropped |
| 14 | execve native: environment as local variables | 5f2d856 | `execve.c` +165, none of this | **rebase** |
| 15 | execve: unmounted IXPIPE: fails quietly | 8836946 | still `Open("IXPIPE:%x")` unguarded (`execve.c:797`) | **rebase** |
| 16 | socketpair(AF_UNIX); socklen_t, sa_family_t; `*_r`; if_nametoindex | 5c7bd59 | `socketpair` returns `EPFNOSUPPORT` (`socket.c:260-266`). `socklen_t` typedef'd in `arpa/inet.h:55` and `netdb.h:103` but not `sys/socket.h`. No gmtime_r, no if_nametoindex (grep). | **rebase**. `unp_socketpair` rewritten. Unify the `socklen_t` guards (two different typedef guards = C89 redefinition) |
| 17 | SDK for gcc 6: va_start, C99 snprintf, headers | bb620d4 | `vsnprintf` n==0 count-only (`stdio/vsnprintf.c:72-90`). `_BSD_VA_LIST_` is `__builtin_va_list` for gcc>=3 (`machine/ansi.h`). `va_start` unchanged (`machine/stdarg.h:48`). basename/dirname vectors 644/645. | **partly drop** (snprintf, libgen). **Keep** va_start/va_copy, the signal.h gnu_inline fix, termios O* flags, socket.h names, langinfo |
| 18 | AF_UNIX: descriptors ride with their message; one waiter per direction; POSIX connect errors | 78d2934 | Still one `ss->task` per stream (`unp.c:1840`). Connect to a missing name gives `EADDRNOTAVAIL` (`unp.c:1062`). | **rewrite** with #11 |
| 19 | select: write-only wakes, EINTR from ixnet, console read/write | 3073a40 | `NET_waitselect` -1 returns with errno unset (`select.c:1084-1086`). `_cli_parse.c` still opens console stdin read-only. 80.1 polls first (`select.c:167`), which may cover the write-only case: **not verified**. | **rebase** all three (write part harmless if redundant) |
| 20 | FIONREAD on vtcon; printf `z t j hh` | 9c75798 | `__fioctl.c` and `vfprintf.c` byte-identical to 48.2 | **rebase**, clean |
| 21 | malloc: per-process small-block cache (10x) | 5fbb894 | `malloc.c` hardening only (rev 1.6/1.7, `malloc.c:20-45`), buddy allocator reworked. No cache. | **rebase** on changed `malloc.c`/`vfork.c`; re-measure |
| 22 | size_t is the compiler's under gcc 3+ | ddd6e22 | `machine/ansi.h` changes va_list only | **rebase** |
| 23 | __plock: a device-part name is absolute at cwd / | 0a1c765 | `info->is_root = u.u_is_root;` unchanged (`__plock.c:158`) | **rebase** |
| 24 | Non-seekable stream: fstat S_IFIFO, lseek ESPIPE | 8dad95d | ESPIPE only for `HANDLER_NIL` and non-files (`lseek.c:142,295`). No `ERROR_ACTION_NOT_KNOWN` handling (grep). | **rebase** onto rewritten `lseek.c` |
| 25 | libixcompat posix_spawn | 0f62d3e | none (grep) | **keep** (static library) |
| 26 | `<sys/dirent.h>`, `<sys/resource.h>` stand alone | 672d4f7 | unchanged | **keep** |
| 27 | libixcompat `__eprintf` | aea8ab7 | only declared in `assert.h` | **keep** |
| 28 | UTF-8 locale and wide chars | 2e152be | TODO "Add proper locale support" | **keep** (static library) |
| 29 | `<alloca.h>`, `<sys/select.h>`, ctype macros | a8d7bdb | own `sys/select.h` (no `<unistd.h>`). `ctype.h` unchanged. | **keep** ours; merge `sys/select.h` |

**Count (27 non-research commits, #13 split):**

- **Dropped because 80.1 covers them: 3.** #10, #12, and the connect half of #13. #17 and #1
  also shrink.
- **Clean rebase onto untouched code: 10.** #4, #5, #9, #14, #15, #20, #22, #23, plus the
  static-library and header items #25-#29.
- **Hand rebase into rewritten files: 10.** #3, #6, #7, #8, #13a, #16, #19, #21, #24, and #17's
  header parts.
- **Rewrite: #11 + #18 (+ #16's `unp_socketpair`).** About 400 lines on 80.1's refcounted
  `sock_stream`.
- **Build system: #1/#2 rework.**

## 3. ABI

All counts below were **measured** from the binaries and from libc.a, not read from headers.

- **Vector table (binaries).** A hunk loader walks RomTag -> RT_INIT -> funcTable
  (`scratchpad/ix80/tools/functable.py`):
  - `ixemul020.library` 80.1: **660 vectors** (4 standard + 656), last LVO -3960.
  - `ixemul000.library`: the same.
  - Our `build295/.../ixemul.library`: 617 (4 + 613), last LVO -3702.
  - ixnet 80.1 and ours: both 112.
- **Vector table (source).** `include/sys/syscall.def` 48.2 -> 80.1 adds lines only: vectors
  614-656 are appended, and 1-613 are untouched (`git diff` shows a single hunk at line 629).
  The start.S `___must_recompile` list is unchanged.
- **libc.a stubs.** `objdump` jmp offsets in each stub, compared with 48.2's `syscall.def`
  (`scratchpad/ix80/tools/stubcheck.py`):
  - 80.1 `gg/lib/libc.a`: 581 stubs. **540 are at the same vector as 48.2, 0 moved**, and 41 are
    new (614-656).
  - The SDK's `~/opt/amiga/m68k-amigaos/ixemul/lib/libc.a`: 534 stubs, all matching.
  - So GG/48.x binaries keep their vectors on 80.1.
  - The reverse does not hold: anything linked against 80.1's libc.a that calls 614+ would jump
    past the end of a 48.2 library.
- **lseekw.** It is in no table we have:
  - not in 48.2's `syscall.def` (`git grep lseekw` on 48.2 finds nothing; `unistd.h:122` has it
    only as a comment);
  - not in 80.1's.
  - Only 80.0 added and exported it, and 80.1 removed it again (ChangeLog 2026-09-23).
  - 80.0 is not downloadable, so its slot cannot be checked. If 80.0 had put lseekw inside
    614-656, 80.0-built programs would call the wrong function on 80.1. That does not affect us.
  - Shipped code using lseekw: see section 4 (port audit).
- **Other ABI surfaces.**
  - **Unchanged:** `crt0.c`, `stack/` and the `ix_get_vars` protocol (`libsrc/` has no diff);
    `FILE`, `struct stat`, `termios` and `errno.h`.
  - **`struct user` is private** (programs reach it only through the library) but gets fields
    inserted mid-struct:
    - `u_open_name_buf`/`u_path_buf` after `u_strtok_last`;
    - `u_vfork_msg_buf` after `u_mini_stack`;
    - `A4_POINTERS` 100 -> 1000 (`include/user.h`).
    - This breaks only tools that read `struct user` by offset: an old ixtrace, gdb. It also
      adds about 5 KB RAM per process (estimated from the declarations).
  - **`struct file`** (private, `_INTERNAL_FILE`) gains `f_name_buf[256]` at the front
    (`sys/file.h`), +264 bytes per open file.
- **ixemul and ixnet are version-locked.** `ixnet_open.c:82-91` compares version **and
  revision** and calls `ix_panic` on a mismatch.
  - The rig installs only `ixemul.library` into `VTC:ixp6` (`tools/rig/ixpty_rig.py:26-27`;
    `rig.py:72` RIG_LIBS). ixnet comes from `DH0:Libs`, which is 48.2.
  - **An 80.x ixemul on the rig needs its own ixnet.library in VTC:ixp6, or every program that
    opens the network gets a panic requester.**
- **Header and source-level changes** that would break port sources, not binaries:
  - `struct timespec` `ts_sec/ts_nsec` -> `tv_sec/tv_nsec`;
  - `sys_siglist` declaration removed from `unistd.h`;
  - `sethostname(…, size_t)`;
  - a second `socklen_t` typedef in `arpa/inet.h`/`netdb.h`.

## 4. What 80.x has that we lack, and what it would retire in the ports

**Method.** Each port's shim archives and compat headers were compared at symbol level against
48.2's SDK `libc.a` and 80.1's `gg/lib/libc.a` (`nm -A`). The symbol lists are in the session
scratchpad, `agentA/`.

**Not in 80.1 at all.** fork (still ENOSYS); mkfifo; openpty, forkpty, ptsname, grantpt and
posix_openpt; locale and wide characters (TODO); posix_spawn; `<inttypes.h>`; strtok_r;
lldiv/llabs; the `*_r` functions; fseeko/ftello; `sockaddr_storage`, `sa_family_t` and `SHUT_*`.

**In 80.1's sources but not usable by programs.**
- `getline`/`getdelim` (`stdio/getline.c`) have no vector, no stdio.h prototype and no libc.a
  symbol.
- The same holds for `getaddrinfo`/`getnameinfo` (`ixnet/getaddrinfo.c`), although
  `include/netdb.h:107,200` declares them.

**What 80.1 would retire.** The 41 vectors 614-656 would replace these shims:

| Port | Shims 80.1 retires | Still needed |
|---|---|---|
| libixcompat (ours) | poll, realpath, basename/dirname, strtoll/strtoull/strsignal | `c99.c` bundles the last three with ctime_r/fseeko/ftello/fmod/round, so it stays. gmtime_r/localtime_r, if_nametoindex, selfpath, nl_langinfo, `__eprintf`, posix_spawn, all of locale/wide, newlib `wcs*` also stay. |
| cpython-amiga | poll, realpath, strsignal; `amiga/compat/inet.c` (inet_ntop/pton, identical prototype `arpa/inet.h:71`) | getentropy, wcstol, the newlib errno hook, C99 math, `config.site` answers (fork/forkpty/openpty=no), patches 0005-0013 |
| neovim-amiga (libamigacompat, shared with CPython) | clock_gettime/getres, nanosleep, strnlen, mkdtemp, atoll; the time.h, string.h, stdlib.h and endian.h declarations; the `struct addrinfo` header part | getaddrinfo family (`netdb.c`), pread/pwrite, the `*_r` functions, iconv, pthread stubs, strtok_r, math, the gcc 6 `va_start` fix, libuv's vfork spawn and PTY spawn |
| tmux-amiga | about 11 compat files: asprintf, clock_gettime, explicit_bzero, memmem, reallocarray, strcasestr, strlcat/strlcpy, strndup, strnlen, strtonum, once `amiga/tmux/config.h` sets HAVE_* (vis/unvis likely, unconfirmed) | getline, imsg, closefrom, getpeereid, setproctitle, and so on; libevent needs our `sa_family_t`/`sockaddr_storage` |
| upterm-ports (grep 3.12, ncurses 6.6) | gnulib's strnlen/memrchr/mempcpy/stpcpy/reallocarray (configure drops them itself) | everything the locale layer provides (setlocale, nl_langinfo, mbrtowc/wcrtomb, isw*, wcs*) |
| screen-amiga | poll, realpath | ix_self_path, the vfork-only paths |

None of the big gaps closes under 80.1: fork, ptys, locale/wide, posix_spawn, getaddrinfo,
SCM_RIGHTS. **libixcompat stays either way.**

**Conflicts if the ports compiled against 80.1's SDK headers.**
- **libgen.h:** 80.1 declares `char *basename(char *)` (`include/libgen.h:7-8`); ours declares
  `const char *`. So `compat/libgen.c` no longer compiles. 80.1's versions also **write into their
  argument** (`library/basename.c:106`, `dirname.c:128,160,164`). vtcon's `tests/amiga/ixc99.c`
  passes them string literals.
- **poll.h:** `nfds_t` is `unsigned long` in 80.1 (`include/sys/poll.h:60`) and `unsigned int` in
  ours (`include/poll.h:14`). `POLLWRNORM`/`POLLWRBAND` also differ: 0x100/0x200 in 80.1 against
  our 4/0x100.
- **neovim `compat/include/netdb.h`:** it redefines `struct addrinfo`. 80.1 defines that struct
  unguarded at `netdb.h:107`, so the build fails.
- **Headers 80.1 lacks that the ports depend on.** Our SDK additions would have to be carried:
  - `sys/socket.h:131-156`;
  - `errno.h` EOVERFLOW/EBADMSG/EILSEQ;
  - the gcc 6 `va_start`/`va_copy`;
  - `size_t` as `__SIZE_TYPE__`;
  - `MB_LEN_MAX 4` (80.1: 1);
  - alloca.h, langinfo.h, spawn.h, wchar.h, wctype.h.

**Link level: no conflict.** Every libc.a stub is its own archive member, and the ports put their
shim archives before `-lc`. A shim that shares a name with an 80.1 vector silently wins.

**lseekw: nothing we ship calls it.** `grep -rl --binary-files=text` finds no hit in any source,
header, Amiga executable or `.a` under these trees:
- ixemul-vtcon, cpython-amiga, neovim-amiga, upterm-ports, vtcon, tmux-amiga, screen-amiga;
- `~/opt/amiga`.

The only hits are the 09-30 research doc and its copies in agent worktrees.

**Found on the way: the SDK's libixcompat.a is stale.** It does not depend on the 80.1
question.
- `~/opt/amiga/m68k-amigaos/ixemul/lib/libixcompat.a` is 4,656 bytes, dated 30 Sep, and has 8
  members: poll, realpath, time_r, if_nametoindex, libgen, c99, selfpath, langinfo.
- The fork's current build (`compat/libixcompat.a`) is 46,180 bytes.
- CPython, neovim, tmux, screen and vtcon's ixc99 link the stale copy through `-lixcompat`. Only
  upterm-ports links the fresh one, by path.
- This is why CPython carries its own `libnewlibwcs` and neovim its own `__eprintf` and wctype.
- `make -C ~/Code/ixemul-vtcon/compat install` would update it. The owner decides when; it
  changes what those ports link.

## 5. Risks

- **SIGWINCH / input.device teardown freeze** (`BUGS`, `BUGS_SIGWINCH`). **It is not new in
  80.x.**
  - 48.2 has the same handler (`library/ix_sigwinch.c`), and **our fork is more exposed than
    80.1**. Ours installs it at every program start (48.2 `ix_exec_entry.c:93`), so every ixemul
    process adds and removes an input.device handler.
  - 80.1 installs it only when a program catches SIGWINCH (`kern_sig.c` rev 1.7), makes the
    install idempotent (`ix_sigwinch.c:83`), and zero-fills the whole
    IORequest (`createextio.c:71`).
  - Our path interacts with it in one place. vtcon's console injects an `IECLASS_SIZEWINDOW`
    event at priority 20 after a resize (`vtcon/handler/vtcon_handler.c:5265-5320`), which
    ixemul's priority-10 handler then reads (`user->u_window`). It already has a
    `winch_closing` guard for its own teardown.
  - The PTY: SIGWINCH path (`__vtcon_winch`, `_psignalgrp`) does not use input.device at all.
  - 80.1's lazy install still covers the programs that matter: tmux, vim, less and nano all
    catch SIGWINCH.
  - **Not reproduced on our rig. No freeze entry in vtcon's `thoughts/` matches it** (grep for
    "freez").
- **Per-process WAIT_CHAR against our one-waiter handlers.**
  - 80.1 keeps the select packet in `u.u_fselect_states`, one per process per file
    (`__fselect.c` rev 1.6).
  - `pty_handler.c:532-536` and `vtcon_handler.c:5584-5585` end an older WAIT_CHAR when a newer one
    arrives.
  - So two processes selecting one side would cancel each other. This is inferred, not run; the
    09-30 document calls it R1.
  - It needs a waiter list in both handlers before any switch.
- **Performance claims.** These are m68k code paths: bcopy/bcmp per CPU (`string/bcopy.c`
  +665), a BFFFO fd scan in select, buddy allocator changes.
  - **They cannot be measured on the host.**
  - The A/B candidate is vtcon's `tests/amiga/mallocbench.c`, which is untracked in vtcon:
    someone's work in progress. Run it under the 80.1 prebuilt `ixemul020-fpu.library` against
    ours on the rig.
  - Ours has the 10x small-block malloc cache (5fbb894). 80.1 has none, so 80.1 is likely slower
    on malloc-heavy ports such as CPython. **Unverified.**
- **Size.** `ixemul020-fpu.library` 80.1 is 191,800 bytes. Ours is 168,268 bytes
  (`build295`, 2 Oct). That is +23.5 KB before our patches.
- **Toolchain.** 80.1 assumes an in-tree, native ADE build:
  - host `gcc` compiles `create_header`, which prints `struct user` offsets;
  - our cross build replaced that in 32efe2b (`library/create_header.c` +31/-28).
  - Our Docker gcc 2.95.3 image is the right compiler. `docker/build.sh` needs an in-tree build
    (or symlinked `true_srcdir`), the `intops` targets, and the glue link.
  - bebbo's gcc 6 (with the btst and opt_strcpy cc1 fixes) is not needed for the library.
- **Provenance and churn.** AI-assisted edits, one maintainer, no public VCS, 10 days old, and
  80.1 had to repair licence headers its own tooling had dropped.

## 6. Options, cost, recommendation

Estimates are in focused agent-days, rig time included.

- **(a) Stay on the 48.2 fork, backport single 80.x fixes.** About 0.5 day now. The candidates
  have small diffs and close known 48.2 defects that we carry:
  1. `createextio.c`: zero the whole IORequest.
  2. `ix_sigwinch.c`: an idempotent install.
  3. The lazy SIGWINCH install from `kern_sig.c` setsigvec, with `ix_exec_entry.c` no longer
     installing it. This shrinks exposure to the teardown freeze. Check that vtcon's resize
     still reaches tmux, vim and less, which all catch SIGWINCH.
  4. Later, if hit: the vfork message lifetime (`execve.c` rev 1.3, clear `p_vfork_msg` before
     `ReplyMsg`) and the select CANCEL protocol.
- **(b) Rebase our patches onto 80.1.** About 5-7 days:
  - build system: 1 day;
  - clean rebases: 0.5 day;
  - hand rebases into rewritten TTY/close/select/lseek/malloc files: 1.5-2 days;
  - AF_UNIX rewrite (SCM_RIGHTS, socketpair, waiters, connect errors): 1-1.5 days;
  - a waiter list in `pty_handler.c` and `vtcon_handler.c` for per-process WAIT_CHAR: 0.5 day;
  - the full rig matrix below: 1 day;
  - plus re-measuring malloc.
  - The result is a fork of a 10-day-old fork that has no upstream VCS to pull from.
- **(c) Adopt 80.1 as-is plus small patches.** **Not viable.** Without #3-#9, #11, #16 and #18,
  tmux and screen lose socketpair/SCM_RIGHTS, and every vtcon terminal loses termios, job
  control and PTY:. Patching those back in *is* (b).

**Recommended: (a).** The decisive trade-off: 80.1 gives none of the things UP-Term's Unix layer
depends on (fork, termios, ptys, descriptor passing all stay ours), while it rewrites exactly the
files those patches live in. The robustness fixes we would gain are small and can be backported
one by one, starting with the SIGWINCH handler trio, which also cuts our exposure to the same
freeze 80.1 lists.

Revisit (b) when either of these happens:

- 80.x gets a public repository and a maintenance release or two;
- a 48.2 bug that 80.x documents as fixed bites us (select/close lifetime, the vfork message).

## 7. Rig test plan (before any switch, and after any backport)

**Install.** Build, then copy **both** libraries into `VTC:ixp6`, because of the version lock in
section 3:

```
shutil.copyfile(<build>/library/.../ixemul.library, VTC/"ixp6/ixemul.library")
shutil.copyfile(<build>/ixnet/.../ixnet.library,   VTC/"ixp6/ixnet.library")
```

Then `Avail FLUSH` and `Assign LIBS: VTC:ixp6` first (`ixpty_rig.use_ixemul`). For an 80.x
build, extend `use_ixemul` / `RIG_LIBS` to carry ixnet too. The A/B baseline is
`ixpty_rig.py --orig`, or the stock 80.1 `ixemul020-fpu.library` + `ixnet020.library` from the
archive.

**Checks.** Each line: what to run, then what PASS means.

1. **Library loads, GG binaries.**
   - Run: `Version LIBS:ixemul.library` and `Version LIBS:ixnet.library` (same version and
     revision); then GG coreutils 5.2.1 (`ls -l`, `echo hello | wc -c`), less, nano, tcsh.
   - PASS: the versions match, there is no ix_panic requester, `wc -c` prints `6`, and less and
     nano draw and quit cleanly.
2. **PTY: and job control.**
   - Run: `tools/rig/ixpty_rig.py`, `ptytest_rig.py`, `ttyprobe_rig.py`, `getty_rig.py`.
   - PASS: ixpty 24/24 (the count recorded in 0a1c765).
3. **Console stdio, C99, select.**
   - Run: `tests/amiga/ixc99` and `ixbg`/`ixsig`/`ixsock`/`ixwait` through their rigs.
   - PASS: ixc99 17/17 (0a1c765); job-control stop/continue in tcsh (`sleep 5`, `^Z`, `bg`,
     `fg`).
4. **tmux and screen.**
   - Run: `tools/rig/tmux_rig.py` and `screen_rig.py` (all four children, reattach =
     SCM_RIGHTS).
   - PASS: tmux_rig 7/7 and screen_rig all pass.
5. **IXPIPE.**
   - Run: `tools/rig/ixpipe_rig.py`.
   - PASS: 4/4, and no "insert volume IXPIPE:" requester.
6. **vsh-launched ixemul programs.**
   - Run: `vshpath_rig.py`, `slash_rig.py`, `tests/amiga/dotdot`.
   - PASS: `cd /` then `SYS:C` and `RAM:` work.
7. **SIGWINCH.**
   - Run: resize an XCON: window running tmux, then vim, then less; a pty-master resize via
     tmux's split.
   - PASS: each redraws to the new size.
   - Then exit 20 ixemul programs while dragging another window's title bar (BUGS_SIGWINCH's
     recipe). PASS: no freeze.
8. **CPython.**
   - Run: `Python3:bin/python3 Python3:c3-sentinel.py` (`~/Code/cpython-amiga/amiga/c3-sentinel.py`,
     its ledger's "C3 owner steps").
   - PASS: every line `OK`, no `FAIL`. On the host, `tools/vector-audit.py` (pointed at the
     installed library's `syscall.def`) must report 0 mismatches.
9. **neovim.**
   - Run: `~/Code/neovim-amiga/tools/nvim_rig.py --tui`, then `--v012 --tui`.
   - PASS: both reach the TUI steps they assert.
10. **upterm-ports.**
    - Run: GNU grep 3.12 and the ncurses 6.6 cases (`tools/run-cases.sh` against
      `build/expected/<pkg>/`; `make -C ~/Code/ixemul-vtcon/compat spawnprobe` on the rig for
      posix_spawn).
    - PASS: every case matches its expected output.
11. **malloc A/B.**
    - Run: `tests/amiga/mallocbench` on ours against 80.1.
    - PASS: record both. Not a gate, it informs (b).

## Not confirmed / where I looked

- No library was built and no emulator was run.
- 80.1 building under our Docker gcc 2.95.3: inferred from `library/Makefile.in`, not built.
- Whether 80.1's POLL-first select makes our write-only `__fselect` wake redundant
  (`select.c:167` doc comment only).
- The per-process WAIT_CHAR conflict (inferred from `__fselect.c` and the handlers).
- The malloc speed difference (not measured).
- 80.0's lseekw slot (archive gone).
