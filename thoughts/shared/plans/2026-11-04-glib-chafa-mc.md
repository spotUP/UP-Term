---
date: 2026-11-04
topic: Port GLib 2.58.3, then chafa, then GNU mc to UP-Term (AmigaOS 3.x, ixemul-vtcon)
status: planned, awaiting owner answers to section 7
inputs: thoughts/shared/research/s56-evidence/glib-port-research.md, mc-port-research.md
repo: /Users/spot/Code/UP-Term/upterm-ports (recipes), ixemul-vtcon (libc gaps)
---

# GLib, chafa, mc

Plan only. Nothing here is built or measured yet; every claim about the upstream sources is
UNVERIFIED until the item that touches it records the measurement in its evidence file
(`thoughts/shared/research/s57-evidence/<ID>.md`, name the session at start).

## 0. Owner decisions already taken (not re-litigated)
- s56 owner answers (2026-11-04): Q1 skip libgio unless forced; Q2 --disable-nls, no libintl; Q3 g_thread_new fails loudly; Q5 KEEP the mc subshell (M12); Q6 kitty graphics work in vtcon today. Port starts only AFTER the current UP-Term release. Other Qs: recommended answers stand.
- GLib 2.58.3, autotools (last autotools series). Stock GLib plus our own OS3 backend.
- gmain: poll + wakeup pipe. gthread: single-threaded. gspawn: vfork-style replacement.
- libffi + libintl first (or --disable-nls). Order: GLib, chafa, mc. irssi later, not here.

## 1. What the kit gives us today (measured by reading, 2026-11-04)
- ixemul-vtcon `compat/` already has: `poll.c` (poll over select; POLLNVAL/IN/OUT/PRI), `spawn.c`
  (posix_spawn/posix_spawnp over vfork, error returned via a static, environ swap),
  `pthread.c` (single-thread subset: mutex EDEADLK on relock, recursive counts, keys), `pselect.c`,
  `clock_gettime.c`, `langinfo.c`, `getline.c`, `gai.c`, locale + wchar layers. `include/` has
  poll.h, pthread.h, langinfo.h, sys/mman.h, `pipe()` and `socketpair()` declarations. No dlfcn.h.
  UNVERIFIED: whether ixemul `pipe()` yields descriptors `select()` can wait on (this decides G9).
- upterm-ports: recipe = `pkgs/<pkg>/recipe.mk` + `patches/*.patch` (git format-patch, `make <pkg>-patches`)
  + `check/cases` (one "name<TAB>command" per line, run on the Mac by `make host-<pkg>` -> `build/expected/`,
  and on the rig through vsh by vtcon `tools/rig/userland_rig.py`) + optional `check/mask` (rows allowed to
  differ), `check/tty` (full-screen cases: nnn `list-quit`), `check/keys`, `check/data` (fixture tree).
  Deps: `<pkg>_DEPS := ixcompat ncurses` (stamps; libraries land in `build/sysroot`), `<pkg>_HOST_DEPS`
  for the Mac build, `<pkg>_LIBS`, `<pkg>_ENV` (CONFIG_SITE with `mk/config.site` + `pkgs/<pkg>/config.site`),
  `<pkg>_STACK` + `<pkg>_STACK_VAR` (program started by vfork+exec ignores the cookie; nnn needs `__stack`),
  `<pkg>_NO_KIT` (build and check only, not staged), `<pkg>_OWN` (own source in `pkgs/<pkg>/src`).
- Closest cases to copy: `pkgs/file` (configure, static libs, own `config.site`, `_MAGDIR`, `_DATA`/`_RIG_DATA`
  shipped data), `pkgs/libarchive` (long configure with many `--without-*`, `_LIBS`), `pkgs/nnn` (vfork patch,
  `_STACK_VAR`, ncursesw, tty check), `pkgs/ncurses` (`check/*.c` probe programs plus `.sh` wrappers: the
  pattern for a sentinel probe), `pkgs/iconv` (libiconv, needed by GLib).
- Rules that bind every item: no `fork` in any linked binary (`check_bin` fails it); libc gaps are fixed in
  ixemul-vtcon, never in a recipe; every `config.site` line names the build that needed it; JOBS <= 4;
  kit builds go through the kit-main worktree
  `UPTERM_PORTS=upterm-ports/.claude/worktrees/kit-main make dist`, never `make kit-stage` in `../upterm-ports`;
  agents commit locally, never move main (memory: agents-no-branch-reset); no `git stash`.
- Not in the kit, to build: libffi, gettext/libintl, pcre (GLib 2.58 bundles its own, use that), slang (use ncurses).

## 2. Ledger and tick rule
Items G1..G17, C1..C8, M1..M12 = 37 items. Report as "N of 37, K open". An item is ticked only when
its finish check ran and the number/log line is in its evidence file. A phase is not done on a unit test:
G17, C8 and M11 are the reachability finish lines.

## 3. Phase G: libffi, libintl, GLib
Backend decision up front (G0 below is a measurement, not code): GLib 2.58 on a platform with neither
pthreads nor fork nor real poll wakeups. We keep upstream files and patch minimally; our additions live in
ONE new file pair `pkgs/glib/src-amiga/gamiga.c/.h` copied into `glib/` by the patch (so `make glib-patches`
diffs stay small), not scattered `#ifdef`s.

- **G1 measure ixemul facilities** (sonnet, ixemul-vtcon). What: a probe program in
  `ixemul-vtcon/compat/test` (copy `spawnprobe`) answering: (a) does `pipe()` fd work with `select()`
  (readable after write, wakes a blocked select in the same task)? (b) `fcntl(O_NONBLOCK)` on it, (c) `socketpair`
  over ixnet, (d) `select` timeout granularity, (e) `posix_spawn` with a pipe dup2'd as child stdout/stderr,
  (f) `waitpid` after posix_spawn gives exit status and `WNOHANG` works, (g) `sigaction SIGCHLD` delivery (likely none).
  Finish: rig output of all seven lines recorded; each answer feeds G9/G10/G11. Copies:
  `make -C ixemul-vtcon/compat spawnprobe` command in upterm-ports RULES.md.
- **G2 libffi** `pkgs/libffi/recipe.mk` (3.4.x, autotools, `--disable-shared --enable-static`,
  `--disable-docs`, `--disable-exec-static-tramp`). Copies `pkgs/libarchive/recipe.mk`. UNVERIFIED: libffi has an
  m68k backend (it does, `src/m68k/`), closures need an executable trampoline page; AmigaOS has no W^X so
  `ffi_closure_alloc` malloc path should work, but test it. Finish: host `make host-libffi`; rig probe
  `check/ffi_call.c` calls `strlen` via `ffi_call` and a closure that returns 42; both exit 0 on rig.
- **G3 libintl** decision in section 7 (Q2). Recommended: build GLib with `--disable-nls` and ship no
  libintl now; add `pkgs/gettext-runtime-stub` only if mc needs `libintl.h` (mc uses `--disable-nls`
  too, so likely never). If owner wants NLS: `pkgs/gettext` (0.22, `--disable-shared`, intl only,
  `--without-emacs --disable-java --disable-curses`) copying `pkgs/iconv/recipe.mk`.
  Finish (disable-nls route): G5's configure log shows `NLS: no`, no `-lintl` in glib-2.0.pc.
- **G4 sysroot libs check**: zlib, iconv present as `<pkg>_DEPS`; confirm `libiconv.a` + `iconv.h` in
  `build/sysroot`. Finish: `ls` lines in evidence. (Merge into G5 if trivial.)
- **G5 glib recipe skeleton** `pkgs/glib/recipe.mk`: version 2.58.3 (URL download.gnome.org .../2.58/glib-2.58.3.tar.xz;
  sha256 to be computed by the agent and verified twice), `_DEPS := ixcompat zlib iconv libffi`,
  `_CONFIGURE := --disable-shared --enable-static --disable-nls --with-pcre=internal --disable-libmount
  --disable-selinux --disable-fam --disable-xattr --disable-dtrace --disable-systemtap --disable-gtk-doc
  --disable-man --disable-installed-tests --disable-always-build-tests --disable-libelf --with-libiconv=gnu`,
  `_NO_KIT := 1` (a library; apps link it statically, nothing to ship), `_ENV` CONFIG_SITE with
  `pkgs/glib/config.site`. UNVERIFIED: 2.58 configure flags exact spellings, check with `configure --help`.
  Needs `glib-genmarshal`/`glib-compile-resources`/`glib-mkenums` run at build time: they must be built for the host
  first (see G6). Finish: `./configure` completes, first-pass `config.site` list written from the measured failures.
  Copies `pkgs/file/recipe.mk` (host tool built first, target uses it).
- **G6 host GLib** `glib_HOST_*`: macOS build of the same 2.58.3 into `build/host/glib/inst`, providing the
  code generators and the expected outputs of the checks. Use the same `--disable-nls --with-pcre=internal`.
  UNVERIFIED: 2.58.3 builds on current macOS clang without patches (known `-Werror=format-nonliteral` and
  libintl issues possible); budget a host patch. Finish: `make host-glib` ok; `glib-genmarshal --version`.
  Cross build points `GLIB_GENMARSHAL=`, `GLIB_COMPILE_RESOURCES=`, `GLIB_MKENUMS=` at the host copies.
- **G7 build core only**: first target libglib, libgthread (empty in 2.58, folded in), libgmodule (static,
  no dlopen: configure `--disable-modules`? UNVERIFIED flag; else stub `gmodule` dl backend returns "unsupported"),
  libgobject. Skip libgio at first (G14). Finish: `libglib-2.0.a` and `libgobject-2.0.a` exist in
  `build/sysroot`, `m68k_scan.py` clean (no 68030+/FPU insns).
- **G8 gthread (single-threaded)**: patch `glib/gthread-posix.c` -> new `glib/gthread-amiga.c` selected in
  `glib/Makefile.am` (`gthread-posix.c` is under `if THREADS_POSIX`). What: `g_mutex_*`, `g_rec_mutex_*`,
  `g_cond_*`, `g_rw_lock_*`, `g_private_*`, `g_once` bodies as plain counters/flags; `g_thread_new` returns
  NULL with a g_error "threads unavailable" (never a hang); `g_thread_self` returns one static object;
  `g_cond_wait` with no other thread would block forever: make it abort with a diagnostic unless timed,
  timed returns FALSE after sleeping. Do NOT go through ixemul's pthread stub: GLib 2.58 also uses futex
  (`gthread-posix.c` Linux path) and `pthread_attr_*`, `pthread_create`, which the stub lacks; a direct
  no-op file is smaller and has no hidden dependence. Pattern copied: ixemul-vtcon `compat/pthread.c`
  (EDEADLK on relock). Finish: host-independent probe `check/gthread_probe.c`: lock/unlock/recursive lock,
  `g_once`, `g_private_set/get`, `g_cond_wait_until` timeout returns FALSE; rig exit 0, relock reports error not hang.
- **G9 gwakeup + gmain poll**: `glib/gwakeup.c` (eventfd path off; pipe path on). `GWakeup` = `pipe()` pair
  non-blocking; `g_wakeup_get_pollfd` returns the read end; `g_wakeup_signal` writes one byte; `acknowledge`
  drains it. If G1(a) says pipe fds do not work in select: fall back to a counter-flag implementation
  (single-threaded: nothing can signal from another thread, only from the same task or a signal handler, so a
  flag checked before `poll` is sufficient). `g_poll` (gpoll.c) uses the system `poll()` when `HAVE_POLL`:
  libixcompat's poll maps to select, cap check FD_SETSIZE. gmain.c itself stays stock. ixemul facility:
  `select()` via `compat/poll.c`, `pipe()`. UNVERIFIED: ixemul's select waits on both dos filehandles and
  sockets in one call (WaitSelect underneath); a pipe fd plus a console fd in one poll is the case mc needs (G17 proves it).
  Finish: `check/gmain_probe.c`: timeout source fires 3 times at 50 ms (measured within 50..250 ms each),
  idle source, `g_main_context_wakeup` from a `SIGALRM`-less same-task path ends a blocked iteration;
  sentinel counter `gamiga_poll_calls` incremented in `g_poll` replacement and printed by the probe (> 0 proves our path ran).
- **G10 gspawn**: `glib/gspawn.c` is fork-heavy (`fork_exec_with_pipes`, child setup, reports errors over a
  pipe, `waitpid` loop, `g_child_watch` via SIGCHLD + gmain). Replace `fork_exec_with_pipes` core with `posix_spawn`
  from libixcompat (file actions for stdin/out/err dup2, `chdir` via a wrapper executable absent:
  UNVERIFIED, ixemul has no `posix_spawn_file_actions_addchdir_np`: then do `chdir` in the parent around the spawn,
  single-threaded so safe, restore after). The `child_setup` user function cannot run in a vfork child safely: reject
  with `G_SPAWN_ERROR_FAILED` unless NULL (mc passes none; irssi later: revisit). `G_SPAWN_DO_NOT_REAP_CHILD` and
  child watch: SIGCHLD may not arrive on ixemul; implement `g_child_watch` as a polling source that calls
  `waitpid(pid, WNOHANG)` each main-loop iteration with a 100 ms timeout floor (UNVERIFIED need, G1(g) decides).
  `g_spawn_sync` captures stdout/stderr via pipes and `read`s until EOF. Sentinel: `gamiga_spawn_calls` and a line
  `[gspawn] posix_spawn <argv0> rc=<n>` on stderr when env `GAMIGA_TRACE=1`. Finish: `check/spawn_probe.c`
  runs `g_spawn_sync("echo", "hi")` through the kit's `echo`, output `hi\n`, exit status 0, then a nonexistent
  program gives `G_SPAWN_ERROR_NOENT`-class error (not a hang, not a Guru), counter > 0.
- **G11 file/stdio/unix bits** (`gfileutils.c`, `gstdio.c`, `glib-unix.c`, `gutils.c`, `giounix.c`, `gunicollate.c`,
  `glib-init.c`, `gtimer.c`, `gdatetime`/`gtimezone`): what is stubbed vs implemented, in section 5. Done as one
  small patch per file (one fix per commit), each driven by a compile or link error from G7. Finish: `libglib-2.0.a`
  links the whole probe set G8..G10 plus `check/fileutils_probe.c` (g_mkstemp, g_file_get_contents, g_dir_open on
  SYS:, g_get_home_dir, g_get_user_name, g_build_filename) on rig, outputs match the host build's expected text
  except masked path/user rows.
- **G12 locale/charset**: `g_get_charset` via `nl_langinfo(CODESET)` (compat/langinfo.c exists); C locale gives
  ISO-8859-1 per the kit's layer. Set `ac_cv_*` via config.site only from measured failures. Finish: probe prints
  charset and `g_utf8_validate`/`g_convert("UTF-8"->"ISO-8859-1")` roundtrip of a 2-byte sample equals expected.
- **G13 stack and size**: every GLib-using program defines `__stack` (memory: stack always) and carries
  `$STACK:`; GLib's `g_malloc`-heavy init needs it. Finish: `check_bin` stack line passes for all probes; `make sizes`
  row for libglib consumers recorded (guess 1-1.5 MB, UNVERIFIED).
- **G14 libgio** (needed by chafa? see C1; mc 4.8 does not need gio): defer until C1 says. If needed: build with
  `--disable-gio`-style trimming not available in 2.58, so build gio but stub `gunixmounts.c` (no mtab), `gdbus`
  off (`--disable-dbus`? UNVERIFIED), no inotify (`--disable-inotify`), no `GSocket` beyond TCP (bsdsocket via select).
  Finish: `g_file_new_for_path` + `g_file_load_contents` probe on rig.
- **G15 pkg-config files**: `glib-2.0.pc`, `gobject-2.0.pc` into sysroot `lib/pkgconfig`; kit builds set `PKG_CONFIG=false`
  today, so consumers (chafa, mc) get explicit `GLIB_CFLAGS`/`GLIB_LIBS` in their recipes OR we add a
  `PKG_CONFIG_LIBDIR` run of a host pkg-config. Recommended: host `pkg-config` with `PKG_CONFIG_LIBDIR=$(SYSROOT)...`,
  `--define-prefix`; add to `mk/common.mk` as an opt-in `<pkg>_PKGCONFIG := 1` (one commit, ports repo).
  Finish: `pkg-config --cflags --libs glib-2.0` printed in evidence prints sysroot paths.
- **G16 test suite subset**: run upstream `glib/tests` that can run: `strfuncs`, `hash`, `list`, `slist`, `queue`,
  `string`, `utf8-validate`, `regex`, `keyfile`, `markup`, `gdatetime`, `fileutils`, `mainloop` (single-thread cases).
  Rig runs only (never a sweep, 6a): fixed list in `pkgs/glib/check/cases`. Finish: per-test pass/fail in the evidence
  file, failures triaged into backend bug / stack / libc gap rows; no Guru.
- **G17 REACHABILITY GLib**: one rig test `pkgs/glib/check/cases` row `gamiga-all` that runs `gamiga_probe` (one program,
  linked from libglib, calls g_main_loop with a timeout source + a spawn-sync child + file read) and asserts:
  - stdout line `timeouts=3 spawn_out=hi file_bytes=<N>` with the numbers from the host run,
  - sentinels `gamiga_poll_calls>0`, `gamiga_spawn_calls=1`, `gthread_lock_calls>0` printed in a `[gamiga]` line,
  - exit 0, no Guru, no "stack" warning, run inside a vtcon window from the kit (`rig.py start --max --purpose
    "<session>: glib probe check"`), kit drawer produced via `make dist` with kit-main.
  Phase G done only when G1..G16 are ticked and G17 passes.

## 4. Phase C: chafa
UNVERIFIED all of: chafa version, deps. Pin the newest release whose configure is autotools and whose README
lists GLib >= 2.35.8 (recorded by C1).
- **C1 inventory** (sonnet, read-only): chafa tarball configure.ac: GLib parts used (gobject? gio? `g_*` only),
  optional loaders (FreeType, librsvg, libjpeg, libtiff, libwebp, ImageMagick), bundled loaders (stb-style PNG/GIF/JPEG
  in recent chafa), thread use (`GThreadPool`, `g_thread_new` in the pixel pipeline: chafa parallelises; must be forced
  to one thread), SIMD (SSE/AVX/NEON `--disable-...`). Finish: list in evidence with file:line, and a verdict whether
  G14 (gio) is required.
- **C2 recipe** `pkgs/chafa/recipe.mk`: `_DEPS := ixcompat glib iconv`, `_CONFIGURE := --disable-shared --enable-static
  --without-jpeg --without-tiff --without-svg --without-webp --without-imagemagick --disable-man`
  (spellings from `configure --help`), `_BINS := bin/chafa`, `_STACK := 262144`, `_STACK_VAR := 1`. Copies `pkgs/file`.
  Finish: `make chafa` passes check_bin (no fork, no 030+ opcodes).
- **C3 threads off**: chafa's `chafa_num_processors` / worker threads: patch to 1, or rely on G8's `g_thread_new`
  failing => chafa must detect. Recommended: patch `tools/chafa/chafa.c` and `internal/chafa-*` to run workers inline
  (one patch). Finish: `chafa --threads 1 file.png` ok; no call to `g_thread_new` seen via the G8 sentinel counter `gthread_new_calls=0`.
- **C4 byte-order and alignment**: m68k big-endian, 2-byte-aligned ints (68000 targets): chafa's pixel packing
  (`ChafaPixelType` RGBA8 etc., `GUINT32_FROM_LE`) must give identical bytes. Finish: `check/cases` compares
  `chafa -f symbols -s 20x10 data/a.png` text output with the host build, byte-identical (masked none).
- **C5 symbols and kitty/sixel output**: `check/cases` rows `symbols-8x4` (ANSI text, host == rig),
  `kitty` (`chafa -f kitty -s 16x8 data/a.png | head -c 200` begins `ESC_G...a=T,f=32`), `sixel`. Host==rig except masked
  terminal-size query rows (`--size` always given so no ioctl).
- **C6 slow-path guard**: 68020 vs 060 speed; set `chafa --work 1` default via a small wrapper? No: measure time of a
  64x64 PNG on the rig and record; only then decide. Finish: seconds recorded (no gate unless > 60 s).
- **C7 data fixtures**: `pkgs/chafa/check/data/a.png` (16x16 four-colour quadrants, known RGB), `b.gif`.
- **C8 REACHABILITY chafa**: rig, kit `chafa`, in a vtcon (XCON:) window with kitty graphics enabled:
  `chafa -f kitty -s 32x16 data/a.png` run through `tools/run-tty.sh` style tty case; take a screenshot (Desktop copy,
  haiku reads it), and MEASURE pixels: the four quadrant colours present in the correct quadrants within a tolerance of 8/255,
  region sampled from the screenshot file (visual proof = measured pixels, memory). Sentinel: `chafa --version` prints
  glib version AND stderr line `[gamiga] poll=<n> spawn=0 thread_new=0` under `GAMIGA_TRACE=1` proving our backend was
  linked and ran. Depends on vtcon accepting kitty graphics: if not yet, the fallback finish line is `-f symbols` pixel-checked.
  Q6 answered: kitty graphics work in vtcon today (see section 7).

## 5. GLib backend section (per file; what is stubbed vs implemented, ixemul facility)
All rows UNVERIFIED until G7/G11 compile; file names are GLib 2.58 layout.
| File | Decision | ixemul facility | Notes |
|---|---|---|---|
| `glib/gmain.c` | stock, unchanged | `poll` (libixcompat on `select`) | uses `g_wakeup`, `g_poll`; `g_main_context_check` fine single-threaded; `g_child_watch` source rewritten in G10 |
| `glib/gpoll.c` | stock path with `HAVE_POLL=1`; add sentinel counter | `poll()` | cap on FD_SETSIZE; ixemul select max fds unverified (G1) |
| `glib/gwakeup.c` | pipe path, else flag path | `pipe`, `fcntl`, `read/write` | eventfd off; `ac_cv_func_eventfd=no` |
| `glib/gthread-posix.c` | replaced by `gthread-amiga.c` | none | no-op locks, timed cond sleeps; `g_thread_new` fails loudly |
| `glib/gthreadprivate.h`, `gthread.c` | stock | none | `g_thread_self` static |
| `glib/gspawn.c` | patched core | `posix_spawn(p)`, `waitpid`, `pipe`, `dup2` | child_setup unsupported; no `fork` linked (check_bin gate) |
| `glib/giounix.c` | stock | `read/write/lseek/fcntl` | `GIOChannel` on pipes and console; poll via `g_poll` |
| `glib/gfileutils.c` | stock + small patches | `open/mkstemp/rename/link` | `link()` and symlinks maybe missing: fall back copy/rename (measured) |
| `glib/gstdio.c` | stock | `stat/lstat/chmod/utime` | `g_stat` struct sizes checked |
| `glib/glib-unix.c`, `gunix*` | stub missing (`g_unix_signal_*` signalfd path off; plain `sigaction` source) | `sigaction` | UNVERIFIED SIGCHLD/SIGINT delivery |
| `glib/gutils.c` | stock; `g_get_user_database_entry` needs `getpwuid`: ixemul has? else stub to env `USER`/`HOME` | `getpwuid` | measured by G11 |
| `glib/gtimer.c` | stock | `clock_gettime` (compat) / `gettimeofday` | monotonic = gettimeofday-based; 1 s clock jumps tolerated |
| `glib/gdatetime`/`gtimezone.c` | stock, TZ files absent: UTC fallback | none | `/usr/share/zoneinfo` absent |
| `glib/gslice.c` | `G_SLICE=always-malloc` default via config | none | avoids aligned-alloc/posix_memalign needs |
| `glib/gmem.c` | stock | `malloc` | alignment 2 on 68000, GLib assumes 8: check `G_MEM_ALIGN` |
| `gmodule/` | `--disable-modules`-equivalent: dl stub returns error | none (no dlopen) | mc needs gmodule at configure only |
| `gobject/` | stock | libffi | closures via G2 |
| `gio/` | deferred (G14) | `select` | only if chafa/mc need |
| `glib/gregex.c` + `glib/pcre/` | bundled PCRE | none | slow but fine; UTF-8 on |

## 6. Phase M: GNU mc
Version: newest 4.8.x tarball (UNVERIFIED which; needs GLib >= 2.32, we give 2.58).
What mc drops (OWNER s56: the subshell is KEPT, see M12): VFS: `--disable-vfs-sftp --disable-vfs-fish`
(ssh) and extfs (needs perl scripts and shell helpers: configure `--disable-vfs-extfs`? UNVERIFIED spelling) while keeping
`cpio`/`tar` VFS only if cheap (tar VFS runs `gzip`, `bzip2` through spawn: drop first), `mcserv` (not in 4.8; check),
`--without-x`, `--disable-background` (uses fork; check_bin gate), `--without-diff-viewer` initially (needs diff built-in; keep if it compiles),
`--disable-mclib`, `--disable-nls` (G3), `--with-screen=ncurses` (ncursesw from kit), `--without-gpm-mouse`,
`--without-internal-edit` NOT dropped (mcedit is a main feature; keep), `--disable-doxygen-doc`, `--with-search-engine=glib`
(GLib regex, no pcre2). Spawns left: `ptyrun`-like `execute_external` for F3/F4 viewer/editor and `!` shell commands:
patch `lib/utilunix.c` `my_system`/`my_fork`-based paths to `posix_spawn` or G10's `g_spawn_sync` (single patch, nnn pattern).
- **M1 inventory** (sonnet read-only): list every `fork`, `pipe`, `waitpid`, `tcgetattr`, `openpty`/`grantpt`, `setsid`,
  `signal(SIGCHLD)` use in mc 4.8 `src/`, `lib/`, with file:line and verdict (compile out / replace / keep). Also the
  `m4.include` configure checks that will fail cross-compiling. Finish: evidence table; total fork-site count counted
  from `nm`-less grep of `fork(` AND from a later `check_bin` run (two ways, per the double-count rule).
- **M2 recipe** `pkgs/mc/recipe.mk`: `_DEPS := ixcompat ncurses glib iconv zlib`, configure flags above, `_BINS := bin/mc`,
  `_STACK := 262144`, `_STACK_VAR := 1`, `_ENV` with `pkgs/mc/config.site`, `_DATA` for `share/mc` (skins, syntax files,
  `mc.ext`, `mc.menu`, help `mc.hlp` text) shipped to the kit, `MC_DATADIR` set at configure. Copies `pkgs/nnn` and `pkgs/file`.
  Finish: configure completes, `make mc` links.
- **M3 fork removal patch**: `fork` unlinked, `check_bin` passes. Pattern `pkgs/nnn/patches/0001-...patch` (vfork) / G10.
  Finish: `check_bin` output line OK, and `grep -c 'fork' ` of patched tree lists only comments.
- **M4 terminal**: ncursesw, `TERM` = what vtcon sets; mouse off first; function keys F1..F10 sequences measured
  from vtcon (`TERM=xterm`-like). Finish: `mc -V` prints version and `Screen library: ncurses` on rig.
- **M5 filesystem**: `panel` listing of `SYS:` and `RAM:`: AmigaDOS paths, volume names with ':' in `vfs_path`
  parsing (mc treats `:` plain? UNVERIFIED), `/` root semantics under ixemul, `stat` fields (`st_ino`, `st_dev` absent),
  `readdir`, `getcwd` returning `SYS:...`. Patch `lib/vfs/path.c`/`src/filemanager/layout.c` only where measured.
  Finish: panel shows rows for `SYS:C`, `SYS:S`, `SYS:Libs` (names from the host listing of the kit drawer).
- **M6 viewer/editor**: F3 view of a text file, F4 mcedit open+quit. Finish: tty check cases keyed on screen text.
- **M7 copy/move/delete**: F5/F6/F8 within `RAM:` (uses `rename`, `link`, `chmod`, `utime`, `open/read/write`). Finish: tty check
  verifies files exist (listing in a following `ls`) and counts match.
- **M8 stack and heap**: `__stack` + cookie; run mc with a 5000-entry directory (`RAM:t` fixture), measure free memory
  (`Avail`) delta < 2 MB on 3 MB reserve? record, no guess gate.
- **M9 help/config drop**: ship `share/mc` minimal tree; strip `mc.ext` entries that shell out to missing tools.
- **M10 kit staging**: `pkgs/mc` staged in `build/kit/userland` via kit-main; `SOURCES.txt` has GPL-3 source offer line for mc and
  LGPL for GLib (glib is `NO_KIT`, but statically linked into mc: licence text must be in the mc entry).
- **M12 mc subshell (owner: keep it)**: build mc with the subshell on. mc opens a pty master/slave pair, forks the shell on the slave and talks to it through the pty (Ctrl-O toggles, prompt hook reports the cwd). Port: openpty/grantpt on the UP-Term pty handler (handler/pty_handler.c; find its open API and the precedent in any existing pty user, e.g. vsh or the kit's script/expect ports), child start via posix_spawn (ixemul compat/spawn.c) instead of fork, vsh as the subshell with a precmd/prompt hook that prints the cwd escape mc parses (mc supports bash/zsh/tcsh/fish/ash: pick the closest hook form, add it to vsh if missing; Unix and Amiga path forms both). Finish: on rig, mc Ctrl-O shows a vsh prompt, `cd SYS:C` there, Ctrl-O back, active panel lists SYS:C; sentinel = `[gspawn]` trace line for the subshell spawn plus a vsh hook counter. UNVERIFIED: whether the pty handler exposes a POSIX openpty path to ixemul programs; first step of M12 measures it. Size: medium-large.
- **M11 REACHABILITY mc**: rig, kit `mc`, vtcon window (`rig.py start --max --purpose "<session>: mc check"`),
  `tools/run-tty.sh` case `mc-start-quit` (copy `pkgs/nnn/check/tty` `list-quit`): start `mc`, wait for both panels, assert screen
  text shows two panel frames each listing `SYS:` entries (fixture-independent rows: `C`, `Libs`, `S` present on both panels),
  press F10 (key sequence from the vtcon table), exit code 0, no Guru, no requester, program gone from `ps`.
  Sentinels: `GAMIGA_TRACE=1` stderr shows `[gamiga] poll>0` (main loop ran through our g_poll), and a `!` command in the panel
  (`echo ok`) shows `[gspawn] posix_spawn ... rc=0` plus `ok` on screen. Screenshot pixel check: panel frame colour rows
  measured from the Desktop copy.

## 7. Risks and open questions (owner may veto; recommended answer first)
1. Q1 libgio: build only if chafa/mc force it. Recommended: skip gio until C1 says (saves ~half the library size and the biggest port risk).
2. Q2 NLS: `--disable-nls` everywhere now, add gettext later with irssi. Recommended: disable. Risk: translated UIs never; none today.
3. Q3 threads: GLib's `g_thread_new` fails loudly. Recommended: yes. Risk: any app that spawns a worker thread dies at that call (chafa C3 patches it out).
4. Q4 SIGCHLD: assume none; poll for child exit. Recommended: yes pending G1(g). Risk: child-watch latency up to 100 ms.
5. Q5 mc subshell: OWNER ANSWER 2026-11-04: do NOT drop it. Keep Ctrl-O and cd-tracking; new item M12 ports it onto the UP-Term pty handler + vsh.
6. Q6 chafa finish line needs kitty graphics in vtcon: confirm vtcon renders kitty graphics today, else use `-f symbols` first and a vtcon graphics ticket. Recommended: confirm before C8. ANSWERED s56: vtcon renders kitty graphics today (K1-K14 shipped; UD3 kitty placement PASS rig 104, 2026-11-04). C8 uses kitty output directly.
7. Q7 licence: GLib LGPL-2.1+ static link into GPL-3 mc is fine; mc GPL-3 must ship source offer in the kit. Recommended: add to `SOURCES.txt` rows (M10).
8. Q8 size: libglib+gobject statically in mc (~1.5-2.5 MB, UNVERIFIED) on a 2 MB-chip class Amiga. Recommended: accept (owner: no size budget).
9. Q9 host GLib on current macOS may not build 2.58.3 unpatched. Recommended: allow a host-only patch in `pkgs/glib/patches/host-*.patch` (separate from target patches).
10. Q10 pkg-config: add opt-in `<pkg>_PKGCONFIG` to `mk/common.mk` (G15). Recommended: yes; the alternative (hand `GLIB_LIBS` strings) duplicates per consumer.
11. Q11 tarball hosting: download.gnome.org may not be stable; sha256-pin and mirror under `build/dl`. Recommended: pin, no mirror yet.
12. Q12 ixemul gaps found by G1/G11 (`getpwuid`, `link`, `posix_spawn_file_actions_addchdir`) are fixed in ixemul-vtcon with their own test, one commit each (cross-repo rule: same subject, plus repos.lock). Recommended: yes.

## 8. Agent split (small briefs, one item per brief; at most 2 running, sonnet unless noted; each brief: grep first, read ranges,
logs to scratchpad, stop after ~30 tool calls, write findings to the evidence file, reply at most 20 lines, no sub-agents, clean scratch,
commit locally only, never touch main)
- B1 (sonnet, rig steps fixed): G1 probe, results file. Opus only if a result is a crash with unknown cause (crash-hunt memory).
- B2: G2 libffi recipe + rig `ffi_call`. B3: G3+G4 (libintl decision, sysroot check).
- B4: G5+G6 recipe skeleton + host build (host GLib patches). B5: G7 first cross build, iterating `config.site` rows (one agent, ends when libs link).
- B6: G8 gthread file + probe. B7: G9 gwakeup/gpoll + probe (needs B1). B8: G10 gspawn + probe (needs B1, B6 for locks).
- B9: G11 per-file patches (split into 2 briefs if more than 6 files break). B10: G12+G13. B11: G15 pkg-config opt-in. B12: G14 only if C1 demands.
- B13: G16 rig test list (haiku may run fixed cases; sonnet triages). B14: G17 reachability test and the kit build via kit-main (owner-visible rig purpose text named in the brief).
- B15: C1 inventory (sonnet, read-only; can run while phase G is being built). B16: C2+C3. B17: C4+C5+C7. B18: C6+C8.
- B19: M1 inventory (sonnet, read-only; can start with C1). B20: M2+M3. B21: M4+M5. B22: M6+M7. B23: M8+M9+M10. B24: M11. B25: M12 inventory (pty open path + vsh prompt hook, read-only, sonnet). B26: M12 implement (sonnet, opus if a hang/crash shows up).
- Order constraint: B1 before B7/B8; G17 before any C/M build against the kit lib; C complete before M2 (shared GLib proof), M1 and C1 may run during G.
- Never a rig per row: C/M verify items ride one shared rig run per brief. Rigs started with `UPTERM_RIG=<n> rig.py start --max` and a purpose title; stopped at the end.
