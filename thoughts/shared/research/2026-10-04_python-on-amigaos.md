---
date: 2026-10-04
topic: How feasible is a current Python (CPython 3.13/3.14, MicroPython) on AmigaOS 3.x / 68k for UP-Term
tags: [vtcon, python, cpython, micropython, ixemul, port, research]
status: draft
---

# Python on AmigaOS 3.x (68k)

Owner 2026-10-04: "research how feasible it is to port a current version of python to amigaos".
Read-only research: no emulator was run, nothing was built for the Amiga beyond a compile-only
probe of the installed gcc. Every number marked **(est.)** is an estimate with its method stated;
**(unverified)** means I could not confirm it and say where I looked.

## Short answer

- **Feasible, but only as a reduced, single-threaded CPython on accelerated machines.** CPython
  3.13/3.14 needs C11, `_Thread_local`, atomics, IEEE 754 doubles, 4-byte-aligned objects and a
  "thread" API. Every one of these has a known single-thread answer on m68k-amigaos: CPython's
  own WASI configuration (pthread stubs, no fork, no dlopen, static modules) is the template,
  and the Neovim port (`~/Code/neovim-amiga`) already solved the atomics, clocks and missing-POSIX
  parts for ixemul. Upstream will not take it (PEP 11: unsupported platform; `configure.ac`
  refuses unknown cross hosts), so it is a permanent out-of-tree fork.
- **It does not fit a stock A1200** (68EC020, 2 MB chip, no FPU): the interpreter binary alone
  is 3-5 MB (est.) and AmigaOS loads it whole into RAM. Realistic floor: 68030 + FPU + 16 MB
  fast RAM; comfortable: 68060 or PiStorm/Emu68.
- **MicroPython already runs on AmigaOS 3.x today**, including a 68000 soft-float build for a
  stock A1200 (sidick v1.29.0-amiga, bebbo gcc; per its release page, not run here). It is the right on-Amiga
  scripting language for UP-Term; it is not CPython (no pip, no C extensions, no venv/sqlite3,
  subset `re`), which is why the Neovim port already ruled it out for coq_nvim.
- **Recommended:** baseline = Python on the Mac over UP-Term's ssh/telnet (A1, zero cost);
  on-Amiga scripting = MicroPython (adopt the existing port, then embed/plug into UP-Term);
  CPython 3.14 "WASI-shaped" ixemul port only if the owner wants unmodified pure-Python tools
  on the Amiga itself -- roughly 8-15 agent sessions to a usable `python3 -c "import os, re,
  json"` + subprocess + sockets (est.), see the milestones.

- **amigazen's AmigaPython (section 6) is not a starting point.** It is an unreleased Python
  2.7.18 (vbcc + PosixLib, no threads, Unicode off). Its web page's "2025-01-10 release" and
  its Aminet link do not exist. Two parts are worth reusing: its LoadSeg plugin scheme and the
  API of its Amiga modules.

## 0. Local facts (measured on this Mac, 2026-10-04)

| Item | Value | How |
|------|-------|-----|
| gcc | `m68k-amigaos-gcc (GCC) 6.5.0b 20260819091705` at `~/opt/amiga/bin` (not on PATH; `/opt/amiga` has no gcc) | `--version` |
| C library choices | `-mcrt=ixemul / nix / nix13 / clib2 / library` in the specs; ixemul SDK at `~/opt/amiga/m68k-amigaos/ixemul` (Aminet ixemul-sdk 48.x headers+libs), libnix at `.../libnix`; clib2 crt not installed (`cannot find ncrt0.o` when linking with `-mcrt=clib2`) | `-dumpspecs`, link probe |
| C11 | `__STDC_VERSION__ 201112L`; `_Static_assert`, `_Alignof`, `<stdatomic.h>`, `_Thread_local` all compile | probe `c11.c` in the session scratchpad |
| Atomics | `__GCC_ATOMIC_INT_LOCK_FREE 1` ("sometimes"): every RMW is a **library call** (`__atomic_compare_exchange_4`, `__atomic_fetch_add_8`), never inline CAS. `libatomic.a` exists in gcc's lib dir with these symbols; the Neovim port supplies its own Disable()/Enable() versions instead (`neovim-amiga/amiga/compat/amiga-os.c:8-15`: CAS is a read-modify-write bus cycle that the Amiga chip bus does not carry) | `-S` output, `nm` |
| Thread-locals | `_Thread_local` becomes **emutls**: each access calls `__emutls_get_address` (present in libgcc) | `-S` output |
| Float | `-m68020` alone = soft float (`jsr ___muldf3`); `-m68881` gives inline `fmul.x`/`fcmp.x` | `-S` output |
| ixemul libc (SDK 48.x) | has `_fork _vfork _pipe _select _socketpair _mmap _sigaction _setlocale _getrlimit`; **no** `dlopen`, `getaddrinfo`, `clock_gettime`, `nl_langinfo`, `mbrtowc`, `poll` symbol in libc.a (a `poll.h` header exists) | `nm .../ixemul/lib/libc.a`; the patched ixemul-vtcon may differ (unverified) |
| bebbo libpthread.a | 84 pthread functions over exec tasks (references `SysBase`); not ixemul-aware | `nm` |
| Host CPython 3.13, M1 Pro | pystone 1.1 (from 2.7's `Lib/test/pystone.py`, ported) **431,296 pystones/s**; `-I -S -c pass` ~25 ms wall incl. process spawn; 24 modules at bare start, 56 after `import os, re, json`; their `.pyc` total 438 KB | runs in scratchpad |
| Stdlib size | 10.1 MB of `.py` without tests/idlelib/tkinter/turtledemo/ensurepip; one file name > 30 chars (`_sysconfigdata__darwin_darwin.py`, platform-named) | `find` over 3.13's Lib |

The Neovim port is the closest precedent and is DONE on the rig (0.12.5, 9 of 9): libuv got an
AmigaOS backend with **no threads** (`UV_NO_THREADS`, work queue run on the loop), vfork spawn,
E-clock timers, a static dlopen table, and a compat layer for missing POSIX
(`neovim-amiga/thoughts/shared/plans/2026-10-03-neovim-port.md`, section R lists what ixemul
lacks: inttypes, C99 math parts, wctype, getaddrinfo, pread/pwrite, nanosleep, mkdtemp,
getpw*_r, `__eprintf`, a `va_start` gcc bug, `unsetenv` returning void). Gotchas recorded there
apply 1:1 to CPython: `size_t` = `unsigned int`, `off_t` 32 bits, `long` 32 bits, the compiler
defines `AMIGA`/`amiga` (guard with `__amigaos__`).

## 1. What current CPython needs from the platform

| Requirement | CPython 3.13 / 3.14 | On m68k-amigaos + ixemul | Answer |
|---|---|---|---|
| C11 compiler | Required since 3.11; optional C11 features not required ([configure docs](https://docs.python.org/3.14/using/configure.html)) | gcc 6.5 is C11 | OK |
| Atomics | 3.13+ needs C11 atomics or GCC `__atomic` builtins (`Include/cpython/pyatomic.h:532-543`: `#error "no available pyatomic implementation"`) | builtins compile to libcalls | Link the Neovim compat atomics (Disable/Enable) or libatomic; one thread, so a plain-load/store implementation is correct. Cost: a `jsr` per refcount-free atomic -- the default (GIL) build uses few atomics on hot paths (unverified count) |
| Threads | "Support for threads" required since 3.7 ([configure docs](https://docs.python.org/3.14/using/configure.html)); `--without-threads` is gone. **But** `Python/thread.c:56-70` takes `HAVE_PTHREAD_STUBS` (`Include/cpython/pthread_stubs.h`): `pthread_create` fails, mutex/cond are no-ops, TLS keys work -- that is how WASI (a PEP 11 Tier 2 platform) builds | ixemul has no pthreads; bebbo's libpthread makes raw exec tasks that must not call ixemul | Build as WASI does: `HAVE_PTHREAD_STUBS`, `Py_CAN_START_THREADS` undefined (`Include/pyport.h:473-479` in 3.13). `threading` imports; `Thread.start()` raises. Free-threading (`--disable-gil`) is opt-in and requires mimalloc -- irrelevant here |
| Thread-local | `_Py_thread_local PyThreadState *_Py_tss_tstate` (`Python/pystate.c:66-71`) when `HAVE_THREAD_LOCAL` | emutls: a function call on **every** current-thread-state access | Patch `_Py_thread_local` to nothing for this single-thread build (a plain global). Not doing it would cost interpreter speed on every call |
| Object alignment | GC flags in the low 2 bits of `_gc_prev`; `PyStackRef` tags in the low bits of `PyObject*`. m68k's traditional 2-byte alignment breaks this: Debian m68k segfaults in `_bootstrap_python` / asserts `!PyStackRef_IsTaggedInt` ([gh-127545](https://github.com/python/cpython/pull/127546), [Debian #1105110](https://www.mail-archive.com/debian-bugs-dist@lists.debian.org/msg2036721.html)) | **m68k-amigaos gcc uses 2-byte alignment** (the AmigaOS ABI; NDK structs depend on it) | Carry the explicit-alignment fix: merged to main June 2025 via [#135016](https://github.com/python/cpython/pull/135016)/#135209; the 3.13 and 3.14 backport labels were removed, so it is in 3.15 only (unverified for 3.15.0's release date). `-malign-int` globally is wrong: it changes ixemul/NDK struct layouts |
| IEEE 754 + NaN | Required since 3.11 | 68881/68882/040/060 FPU and gcc soft-float are both IEEE doubles | OK. CPython already has m68k FPU code: it sets the `%fpcr` precision for correct float repr (`HAVE_GCC_ASM_FOR_MC68881`); its operand constraint was fixed for gcc 15 ([#142343](https://github.com/python/cpython/pull/142343), backported to 3.13/3.14). A 68EC020/68LC040 build needs soft float: every float op becomes a libgcc call (est. 20-50x slower float, ints unaffected) |
| 64-bit integers | `long long` required; `Py_ssize_t` = 32-bit here | gcc has `long long` | OK (`sys.maxsize` = 2^31-1; 32-bit `off_t` = 2 GB file limit; 32-bit `time_t` = 2038) |
| Dynamic loading | Optional: all extension modules can be static (`Modules/Setup.local`), as on WASI/Emscripten ([Tools/wasm/README](https://raw.githubusercontent.com/python/cpython/3.13/Tools/wasm/README.md)) | no `dlopen` in ixemul | Static first. AmigaPython 2.7.18 shows the way out: `LoadSeg` plugins (`lib-dynload/_name.module`) calling back through a host function table (section 6); CPython 3's `_PyImport_FindSharedFuncptr` hook is the same seam. Still no `pip install` of upstream C-extension wheels |
| mmap | Only the `mmap` module and mimalloc use it; pymalloc arenas fall back to malloc | ixemul has `mmap` (emulation, semantics unverified) | Build `--without-mimalloc`; leave `mmap` module out first |
| select/poll | `select`, `selectors`, `asyncio`, `subprocess.communicate` | ixemul `select()` covers files, pipes, sockets and consoles (ix_select; UP-Term's P-phase relies on it) | `select` yes, `poll` no (est.: configure will find no `poll`, selectors uses `SelectSelector`) |
| Sockets | `socket` module, `getaddrinfo` mandatory in `socketmodule.c` | ixemul sockets via ixnet + bsdsocket.library (Roadshow/AmiTCP); `getaddrinfo` missing | Reuse the Neovim compat `netdb.c` (getaddrinfo over gethostbyname). IPv4 only |
| Signals | `signal` module, `SIGINT` -> `KeyboardInterrupt` | ixemul `sigaction`, Ctrl-C as SIGINT, job-control signals work (vtcon P6) | OK |
| fork/exec/subprocess | `os.fork`, `_posixsubprocess.fork_exec` | **vfork only** (vtcon ledger P7: tcsh `&` fails with "No more processes" because ixemul cannot fork); Neovim/libuv and tmux spawn with vfork | Patch `_posixsubprocess` to use `vfork()` (it already has a vfork path on Linux, gated; unverified whether the gate is `__linux__`-only in 3.13). `os.fork` removed/ENOSYS. `multiprocessing` out |
| Locale/encoding | PEP 538/540: C locale -> UTF-8 mode | ixemul locale is "C"; patched ixemul adds UTF-8 wchar (ledger N1, planned) | Default to UTF-8 mode (`PYTHONUTF8=1` baked in); `nl_langinfo`/`mbrtowc` missing in the SDK libc -- `locale` module trimmed |
| File system | posixpath; import checks file-name case only on `_CASE_INSENSITIVE_PLATFORMS` (`win`, `cygwin`, `darwin`, iOS...) (`Lib/importlib/_bootstrap_external.py:54-56`) | AmigaDOS is case-insensitive, case-preserving; ixemul maps `/Vol/x` <-> `Vol:x` | Add `amigaos` to the case-insensitive list. Keep `os.path` = posixpath over ixemul's Unix view; `Work:foo` style names work for `open()` but confuse `os.path.join/abspath` (`:` is not a separator) -- a small `amigapath` tweak or document "use /Work/foo". OFS 30-char names: only `_sysconfigdata_*` exceeds; ship the stdlib as a zip anyway |
| Memory | No hard minimum; host 3.13 traced heap ~2 MB at start (64-bit, `-X tracemalloc`) | AmigaOS has no demand paging: the **whole binary is loaded** | See section 3 |
| C stack | 3.12/3.13 count C recursion; **3.14 checks the stack pointer against `Py_C_STACK_SIZE`, default 4,000,000 bytes** unless the OS reports bounds (`Python/ceval.c:369-383`, `hardware_stack_limits`) | Amiga default stack 4-8 KB; vsh honours `$STACK:` cookies (vtcon P8) | Give `hardware_stack_limits` an AmigaOS branch (`FindTask(NULL)->tc_SPLower/tc_SPUpper`, ~10 lines) and ship a `$STACK: 1048576` cookie as Neovim does. Without the branch 3.14 crashes instead of raising RecursionError |
| Cross build | `--with-build-python` = same-version host Python (Homebrew has 3.13 and 3.14) | -- | `configure.ac:316-346` aborts with "cross build not supported for $host" for any host not in its list: add an `*-*-amigaos*` case (MACHDEP `amigaos`), then a hand-checked `config.site` |

Platform status: PEP 11 lists Tier 1-3 platforms; everything else is "unsupported", and
contributions are considered only with "minimal maintenance burden" ([PEP 11](https://peps.python.org/pep-0011/)).
Tier 3 needs a buildbot and a core developer. AmigaOS is not in the removed list either; it was
simply never there. Treat the port as a permanent fork.

## 2. ixemul / libnix / clib2 and how earlier Amiga ports solved it

- **ixemul (our fork, ixemul-vtcon 48.2 + patches)**: the only Amiga libc with a Unix process
  model close enough for CPython: signals, vfork+execve, BSD sockets through ixnet, select over
  everything, termios through UP-Term's console (P4-P6), BSD ptys on `PTY:`. Missing pieces are
  the list already compiled for Neovim (R1 "libixcompat"). CPython on ixemul = the same compat
  layer + `getaddrinfo` + a few more (realpath/`nl_langinfo` stubs; unverified exact list until
  configure runs). **Cost:** ixemul.library (~168 KB per the ledger G3 note) must be installed;
  fine for UP-Term users, who already have it.
- **libnix**: tiny, static, AmigaOS-native; no fork/vfork, no signals beyond Ctrl-C, no select
  across file types, no sockets (bsdsocket directly). MicroPython's ports use this style
  (native AmigaOS APIs); CPython would need much more glue. Not recommended for CPython.
- **clib2 (68k)**: more POSIX than libnix (sockets via bsdsocket, select on sockets) but no
  vfork/exec process model like ixemul; not installed here. AmigaOS 4's clib4 (v2.4, Sept 2026,
  [amiga-news](https://www.amiga-news.de/en/news/AN-2026-10-00001-EN.html)) is PPC-only.
- **Prior Python ports**:
  - Irmen de Jong's AmigaPython 1.5-2.0 (1999-2000): "68030 CPU, FPU required", 4+ MB RAM,
    AmiTCP for networking; its readme reports **355 pystones/s** on the author's machine
    (machine not stated) ([AmigaPython repo](https://github.com/amigazen/AmigaPython),
    [Python/ readme](https://github.com/amigazen/AmigaPython/tree/main/Python)).
  - amigazen's AmigaPython 2.7.18 (vbcc + PosixLib, no threads, Unicode off): unreleased work
    in progress, evaluated in section 6.
  - AmigaOS 4: ships Python 2.5 ([Hyperion forum](https://forum.hyperion-entertainment.com/viewtopic.php?t=4525)).
    **geekychris/python-amigaos4 is CPython 3.12.7 for OS4 PPC** (newlib, gcc in Docker, threads
    yes, `subprocess` deliberately not, ~15 MB stripped with 52 built-in extensions, "Phase 1
    bootstrap", MIT) ([repo](https://github.com/geekychris/python-amigaos4); found through
    AmigaPython's README; read from its GitHub page only, not cloned). Its `amiga_shim.c` and
    patch set are the closest existing Amiga-side CPython 3 patches (section 6).
  - Geek Gadgets: Aminet `dev/gg/python2.4-m68k-amigaos` is Python 2.4.6 on **ixemul 48.3**,
    "ALPHA BETA ... might not be the case" working
    ([Aminet](https://aminet.net/dev/gg/python2.4-m68k-amigaos.readme)). Shows CPython on
    ixemul was done once, two decades ago.
  - MorphOS (PPC, has its own ixemul): yomgui's Python 3.2 in 2011; **the MorphOS SDK of
    2026-05-30 ships Python 3.14.4 with pip and venv** and moved ixemul/libnix to 64-bit
    `time_t`/`off_t` ([MorphOS news](https://www.morphos-team.net/news)). Closest evidence that
    a current CPython runs on an ixemul-family libc. Its source/patches would be worth reading
    (not located; unverified whether public).
  - Debian m68k (Linux, 68020+ with FPU) builds CPython 3.13 and fought 3.14's alignment and
    gcc-15 issues ([Freexian 2025-10](https://www.freexian.com/blog/debian-contributions-10-2025/),
    [Debian #1121780](http://www.mail-archive.com/debian-bugs-dist@lists.debian.org/msg2071519.html)):
    the m68k CPU side of CPython works; what is new for us is the OS layer.
- **amiga-gcc 6.5.0b**: C11 yes; `<stdatomic.h>` yes (libcalls); `_Thread_local` yes (emutls);
  long long yes; 68881 inline FPU with `-m68881`. bebbo's newer gcc (the sidick MicroPython
  release says "gcc-v16.2") would make 3.14's C easier (unverified that it is installable here;
  ours is 6.5.0b).

## 3. Size and speed (estimates, method stated)

**Binary.** CPython's core + built-in modules: arm64 `Python` framework `__TEXT` 3.7 MB (host,
`size`), Debian armhf `libpython3.13` installed 5.2 MB ([packages.debian.org](https://packages.debian.org/sid/armhf/libpython3.13)).
m68k code density is near Thumb-2's. **CPython static m68k hunk binary: 3-5 MB (est.)**, plus
the frozen startup modules. For scale: Neovim 0.12.5 for m68k is 7.2 MB stripped.

**RAM for `python3 -c "import os, re, json"`:** binary 3-5 MB (loaded whole) + heap ~1.5-2.5 MB
(host 64-bit traced heap ~2 MB at start, +0.1-0.2 MB for os/re/json; 32-bit pointers shrink
objects by roughly a third) + stack 0.5-1 MB = **6-9 MB (est.)**. So: no on 2 MB chip; tight on
8 MB fast; workable on 16 MB+; comfortable on 64 MB+ (PiStorm, 060 boards).

**Speed (pystone 1.1).** Anchor: CPython 3.13 on the M1 Pro here = 431k pystones/s. Scaling by
integer throughput (assumed Dhrystone-class ratios: M1 Pro core ~ 300x a 68060/50, ~2,000-3,000x
a 68030/50, ~10,000x+ a 68EC020/14 from chip RAM), with no allowance for the interpreter's
working set thrashing 8 KB (060) / 256-byte (030) caches:

| Machine | CPython 3.13 pystones/s (est.) | Startup to prompt (est.) | `import os, re, json` |
|---|---|---|---|
| A1200 stock, 68EC020/14, 2 MB chip | does not fit | -- | -- |
| 68030/50 + 68882, fast RAM | 100-250 | 30-60 s | +5-10 s |
| 68060/50 | 700-1,500 | 5-15 s | +1-2 s |
| PiStorm + Emu68 (Pi 4) | several times a 060 (unverified, JIT-dependent) | 1-5 s | <1 s |

Cross-check: Irmen's Python 2.0 got 355 pystones on a late-90s Amiga (machine unstated) -- in
range. The Neovim rig run (headless start 37-47 s on FS-UAE's non-cycle-exact 020) shows the
same order of magnitude for a big interpreter start-up. Startup is dominated by unmarshalling
frozen modules and creating ~1,000 type/function objects; `-S -I` and a trimmed `site` help.
**These numbers are not measured on an Amiga; the first rig/real-hardware run replaces them.**

**MicroPython** (from the ports): heap default 128 KB (OoZe1911), growable with
`HEAP=`/`MAXHEAP=` tooltypes (sidick); 64 KB stack recommended. Binary size undocumented by
both (est. 300-700 KB). It starts in well under a second on a 68020 (est.) and has a 68000
soft-float variant for the stock A1200.

## 4. Options, ranked

1. **Remote Python over UP-Term (do nothing).** Python 3.14 on the Mac or a Linux box, reached
   by `uptelnet` (A1.1) or ssh (research 2026-10-03_ssh-for-up-term). Works on a stock A1200,
   zero port cost, full ecosystem. Loses: scripts that touch Amiga files/ARexx/screens. **Do it
   regardless; it is already on the plan.**
2. **MicroPython (existing ports).**
   [sidick/micropython v1.29.0-amiga](https://github.com/sidick/micropython/releases/tag/v1.29.0-amiga):
   three binaries (68000 soft-float, 68020/030+FPU, 68040), bebbo gcc, AmigaOS-native (no
   ixemul), bsdsocket `socket`, `select.poll/select`, a cooperative `asyncio` with TLS,
   AmigaDOS-aware `os.path` (`Volume:dir`), an `amiga` module (ARexx, ASL, icons, catalogs,
   volumes) ([port README](https://raw.githubusercontent.com/sidick/micropython/amiga-port/ports/amiga/README.md)).
   [OoZe1911/micropython-amiga-port](https://github.com/OoZe1911/micropython-amiga-port) (v1.28,
   020+FPU, AmiSSL, no threads, blocking sockets) and
   [jyoberle/micropython-amiga](https://github.com/jyoberle/micropython-amiga) (OS 3.1, 68000,
   preliminary; [Aminet readme](https://aminet.net/dev/misc/AmigaMicropython.readme)) also exist.
   Effort to adopt: ~0 (install); to make it a UP-Term citizen (run in vsh with termios raw
   mode for its REPL line editor, UTF-8 through the console, a `vtcon` module): 1-3 sessions
   (est.). Limits: a Python subset (no C extensions, no pip, subset `re`, no `subprocess`,
   no sqlite3/venv).
3. **CPython 3.14, reduced "WASI-shaped" build on ixemul** (the real port). Configuration:
   `HAVE_PTHREAD_STUBS`, `_Py_thread_local` = plain global, static modules only
   (`Modules/Setup.local`: posix, _io, _sre, _json, _struct, math, time, select, _socket, zlib,
   binascii, _random, _hashlib-free hashes, _posixsubprocess-on-vfork), `--without-mimalloc`,
   `--without-doc-strings`, `--disable-test-modules`, stdlib in `python314.zip` (`.pyc` only,
   m68k-independent bytecode, built on the host), frozen startup modules as upstream. Carry
   patches: alignment (#135209 backport), stack bounds, `configure.ac` host case, import
   case-insensitivity, `getaddrinfo`/compat (shared with Neovim via the planned libixcompat).
   Needs 68030+FPU and 16 MB fast RAM in practice; a soft-float 68020 build is possible but slow
   on floats. Effort: 8-15 agent sessions to milestone C3 below, then a long tail of stdlib
   modules (est., compared with the Neovim port which needed libuv's backend plus compat in
   about two days of sessions; CPython's OS surface is wider -- posixmodule alone is huge).
4. **"Full" CPython 3.13 with threads on ixemul.** Would need real threads: ixemul binds its
   per-process state (user struct, fds, signals) to one exec Task; bebbo's libpthread starts raw
   tasks that cannot safely call ixemul. Making ixemul thread-aware is a deep library change
   (unverified scope; nothing in the ixemul-vtcon notes suggests a start). Not recommended: the
   pure-Python stdlib works without threads, and asyncio does not need them.
5. **Cross-compiling to WASI and running a wasm runtime on the Amiga.** Two interpreters stacked
   (wasm3-style interpreter, then CPython inside); 10x+ slower than a native build and needs more
   RAM. Rejected. What *is* useful from WASI is its configuration (option 3 copies it).

Recommended: 1 now, 2 next as UP-Term's on-Amiga scripting language, 3 only on a clear
owner request for unmodified CPython tools on 030/060/PiStorm machines.

## 5. Recommended path and milestones

**Baseline (no new work):** finish A1 (uptelnet) / ssh; Python runs on the host.

**Track M -- MicroPython for UP-Term (start here):**
- M1 (owner, real hardware or rig run by the owner): run sidick's three binaries; record start
  time, `import json, re` time, a pystone-like loop on A1200 stock / 030 / 060. Owner test, no
  emulator from the agent.
- M2 Run it inside UP-Term from vsh: REPL line editing, cursor keys, UTF-8, Ctrl-C. Fix on
  whichever side owns the bug (a console/termios issue is ours; a REPL issue is the port's).
- M3 A `vtcon` module (window size, title, colours via the existing slash-command table from
  C1, so scripts drive the same settings the menus do) -- one table, no second copy.
- M4 Scripting hooks: UP-Term runs `S:UPTerm/init.py` (or a `/py` slash command) through a
  MicroPython process talking to the term over the same packet/ARexx path C:UPTerm uses. Embed
  the VM in the handler only if a measurement says the process hop is too slow.

**Track C -- CPython 3.14 (only if asked):**
- (Inputs from section 6: C1a read geekychris/python-amigaos4 patches first; C4a LoadSeg plugins + `amiga`/`arexx` modules with AmigaPython's API.)
- C0 Host proof (as Neovim's D-10): build 3.14 on the Mac with the same reduced config
  (pthread stubs, static modules, no mimalloc) and run the stdlib tests that must pass; this
  proves the config off the Amiga.
- C1 Cross configure: `*-*-amigaos*` host case, `config.site`, `_bootstrap_python` from the
  host, link with ixemul + compat; target: links, size measured (replaces the 3-5 MB estimate).
- C2 Patches: alignment backport, stack bounds (tc_SPLower/Upper), `_Py_thread_local` plain,
  atomics shim, case-insensitive import, `$STACK:` cookie.
- C3 Rig/hardware: `python3 -c "import os, re, json; print(json.dumps(os.listdir('.')))"` in
  vsh; RAM (AvailMem before/after) and time recorded. This is the reachability sentinel.
- C4 `subprocess` on vfork, `socket` with getaddrinfo shim, `select`; `pip`-free install of
  pure-Python packages by copying from the host.
- C5 Regression subset: run the relevant `Lib/test` modules on the host-built config and the
  feasible ones on the Amiga, recorded resumably.

**What UP-Term gains:**
- Scripting/plugins (Track M): a real language for user scripts (startup, key macros, themes,
  triggers on output), using the C1 settings table -- something ARexx can do today but few
  users write.
- Running Python tools in vsh (Track C): unmodified pure-Python CLIs (formatters, small
  utilities, `json.tool`, `http.server`-style tools, text processing) on 030/060/PiStorm. Not
  coq_nvim (needs venv, SQLite and threads), not anything with C extensions.
- A stress test for UP-Term itself: CPython's REPL (3.13+ `_pyrepl`) uses termios raw mode,
  cursor movement and colour -- another real program to hold to xterm parity, like Neovim.

## 6. AmigaPython (amigazen) as a starting point -- evaluated 2026-10-04

Owner: "maybe he doesnt need to start from scratch? maybe he can start from this?"
<https://github.com/amigazen/amigapython> and <https://www.amigazen.com/amigapython/>.
Cloned into the session scratchpad and read; nothing built or run.

**What it is.** CPython **2.7.18** (the last Python 2, EOL 2020-04) for classic 68k AmigaOS,
continuing Irmen de Jong's 1.4-2.0 ports. `main` holds only the 1999-2000 originals (9 commits,
2025-08-14). The work is on branch **`AmigaPython2.7.18`**: 24 commits, 2025-08-14 to
**2026-08-17** (last: SlimPython second interpreter, `_socket`/`_ssl` LoadSeg plugins, AmiTLS).
GitHub: 3 stars, 0 issues ever, one release (`AmigaPython2.0`, the 2000 archive).
**No 2.7.18 interpreter binary is published**: the branch's `Python/` release drawer holds only
`_socket.module` (37 KB) and `_ssl.module` (21 KB); the only hunk executable is Irmen's 1.5.2
`SlimPython` (266 KB). Active, single developer, pre-release.

**Toolchain and targets** (`BUILD.md`, `Source/vmakefile`): **vbcc 0.9h + PosixLib**
(`+aos68k_posix`), NDK 3.2 headers, `-cpu=68020 -fpu=soft` (soft float: runs without an FPU,
slow floats), `-lmieee`, built natively on an Amiga with `make -f vmakefile` (cross with vbcc
"possible"). SAS/C abandoned (could not compile 2.7's type objects); gcc not supported
("SAS/C [ ] VBCC [X] GCC [ ]"). No ixemul, no libnix, no clib2.

**What works per its README/FAQ** (not verified by running):
- 2.7 language; builtins `array math time struct binascii cStringIO cPickle _sre zlib zipimport
  select _collections itertools _functools _random _io`; standard build adds `datetime cmath
  _csv md5 sha _hashlib`(crc.library) `pyexpat pwd grp crypt syslog`; stdlib from
  `lib/python27.zip`.
- `os` is backed by a native **`amiga`** module (`posixmodule.c` not compiled):
  listdir/stat/open/dup2/pipe/`system`/`popen`/`waitpid`/`kill`... (2,018 lines).
- Sockets: `_socket` as a LoadSeg plugin over bsdsocket.library (Roadshow), own fd table
  ("psockets") so `select`/timeouts/TLS work; `_ssl` via AmiTLS (BearSSL); AmiSSL alternate
  broken (`SSL_connect` fails).
- Amiga APIs: ARexx (`_arexx`), ASL requesters, catalogs, icons, Workbench start-up, `amigagui`,
  `amigalibs`, OS4-compatible `site-python` shims.
- **Not there:** threads (`WITH_THREAD` undefined), Unicode (`Py_USING_UNICODE` off, so
  `"abc".encode("ascii")` fails and ElementTree breaks), `fork`, full signals, `mmap`, `bz2`,
  `ctypes`, `sqlite3`, `multiprocessing`; `subprocess` not described as working (unverified).

**Licence.** `LICENSE.md` is Python's own licence stack (CWI / CNRI / BeOpen / PSF; GPL-
compatible). GitHub reports `NOASSERTION`; no separate licence for the amigazen code was found
(unverified whether one exists). Reusing its Amiga code in a CPython 3 port is compatible as
far as the files show.

**The amigazen web page contradicts the repository.** It lists "Version 2.7.18.0 Released
2025-01-10", betas 2024-11-20 / 2024-12-15 ("Fixed threading issues"), "Better Unicode support",
"Built using SAS/C and smake", a "Download (Aminet)" link, 68020+, OS 3.1+, 4 MB RAM (8 MB+
recommended), PSF licence. Checked: the Aminet link (`dev/lang/amigapython2718`) is **404**
(control package 200), and Aminet search for "python" shows no 2.7.18; the repository was
created 2025-08-14, after the page's "release"; the repo says no threads, no Unicode, vbcc not
SAS/C. **Treat the page as placeholder copy, not a release record**; the repo is the source
of truth. (The page also links `amigazen/amigapython27`, which is 404 on GitHub.)

**Distance to CPython 3.13/3.14.** Twelve years of interpreter change sit between them:
Unicode-only `str` (PEP 393), importlib-based imports with frozen bootstrap, `getpath.py`
(3.11+), the 3.11+ specialising interpreter, C11/atomics/thread-local requirements, the
mandatory `posix` module (`os.py` in 3.x knows only `posix`/`nt`), Argument Clinic. Its core
changes (typeobject/import/exceptions edits for vbcc, `io_nounicode.c`, the no-Unicode
codec gaps) do not carry forward at all. vbcc as the compiler for 3.13+ is doubtful: it would
need C11 `_Thread_local`/atomics or CPython's fallbacks (unverified for vbcc 0.9h); our
ixemul + gcc path does not have that question.

**What does carry forward** (about 8,800 lines of Amiga code, read):
- The **LoadSeg extension-module scheme** (`Python/dynload_amiga.c` 167 lines,
  `Include/pyamiga_plugin.h` 302, `Amiga/pyamiga_host.c` 1,553): a plugin is a LoadSeg'd hunk
  file that gets the interpreter's API through a host table. That answers "no dlopen" for a
  3.x port too (CPython's `_PyImport_FindSharedFuncptr` is the hook); the table would have to
  be regenerated for the 3.x C API.
- The **Amiga modules as API design**: ARexx, ASL, catalog, icon, Dos helpers and the OS4
  `site-python` shims (ARexxmodule 1,245, Doslib 964, amiga_* ~1,400 lines). Rewritable as 3.x
  extension modules; Python-level API kept so scripts written for it run on both.
- **psockets** over bsdsocket (inside pyamiga_host.c) -- relevant only to a non-ixemul build;
  with ixemul, sockets are already fds.
- Not useful to us: the `amiga` posix replacement (ixemul gives the real `posix` module) and
  PosixLib specifics.

**Verdict.** Not the starting point for a current Python: it is Python 2 without Unicode or
threads, built with a different compiler and C library, and nearly all of its interpreter
changes are 2.7-specific. Harvest its LoadSeg plugin design and its Amiga module APIs into
Track C. **Not something UP-Term should ship or recommend today**: no 2.7.18 binary is
released (the web page's release and Aminet link do not exist), and Python 2 does not run
today's scripts. Revisit as a "recommend" item if amigazen publishes a 2.7.18 build -- it
would then be the richest Amiga-integrated Python for 68k (ARexx, requesters, TLS), useful for
Amiga-side automation, never for modern Python tools.

**Effect on the plan.** The recommendation is unchanged (remote baseline, MicroPython for
on-Amiga scripting, CPython 3.14 on ixemul only if asked). Track C gains two inputs:
- C1a (new): read geekychris/python-amigaos4's patch set (`amiga_shim.c`, `Modules-*/Python-*/
  Objects-*.patch`) against 3.12 before writing ours; it is the only living Amiga CPython 3
  port. Also read the MorphOS SDK's 3.14.4 patches if public.
- C4a (new): extension modules beyond the static set as LoadSeg plugins, AmigaPython-style;
  `amiga`/`arexx` modules with AmigaPython's Python-level API.
The Aminet MicroPython 1.28 (OoZe1911, `dev/lang/micropython`, "2 MB RAM (4 MB recommended for
networking/TLS)", 68020+, OS 3.0+) is now confirmed as an Aminet release
([readme](https://aminet.net/dev/lang/micropython.readme)), which supports M1.

## Open points (owner)

- Track C at all? It is weeks of fork maintenance for 030/060/PiStorm owners only.
- Which MicroPython port to standardise on (sidick v1.29 is the most complete per the READMEs;
  not run here).

## Sources

- CPython build requirements: <https://docs.python.org/3.14/using/configure.html>
- PEP 11 tiers: <https://peps.python.org/pep-0011/>
- WASI/Emscripten limits: <https://raw.githubusercontent.com/python/cpython/3.13/Tools/wasm/README.md>, <https://docs.python.org/3/library/intro.html>
- Source read (raw.githubusercontent.com, branches 3.12/3.13/3.14): `Include/pyport.h`, `Include/cpython/pyatomic.h`, `Include/cpython/pthread_stubs.h`, `Python/thread.c`, `Python/pystate.c`, `Python/ceval.c`, `configure.ac`, `Lib/importlib/_bootstrap_external.py`
- m68k alignment: <https://github.com/python/cpython/pull/127546>, <https://github.com/python/cpython/pull/135016>, <https://www.mail-archive.com/debian-bugs-dist@lists.debian.org/msg2036721.html>, <https://wiki.debian.org/M68k/Alignment>
- m68k fpcr: <https://github.com/python/cpython/pull/142343>
- Debian m68k 3.13: <http://www.mail-archive.com/debian-bugs-dist@lists.debian.org/msg2071519.html>, <https://www.freexian.com/blog/debian-contributions-10-2025/>
- AmigaPython: <https://github.com/amigazen/AmigaPython> (branch `AmigaPython2.7.18` @ b21b5ea, 2026-08-17, read in full), <https://www.amigazen.com/amigapython/>, <https://amigazen.com/amigapython.html>; Aminet `dev/lang/amigapython2718` = 404
- OS4 CPython 3.12.7: <https://github.com/geekychris/python-amigaos4>; Geek Gadgets Python 2.4.6: <https://aminet.net/dev/gg/python2.4-m68k-amigaos.readme>; Aminet MicroPython: <https://aminet.net/dev/lang/micropython.readme>
- AmigaOS 4: <https://forum.hyperion-entertainment.com/viewtopic.php?t=4525>, <https://www.amiga-news.de/en/news/AN-2026-10-00001-EN.html>
- MorphOS: <https://www.morphos-team.net/news>, <https://www.meta-morphos.org/article.php?sid=1178>
- MicroPython Amiga: <https://github.com/sidick/micropython/releases/tag/v1.29.0-amiga>, <https://raw.githubusercontent.com/sidick/micropython/amiga-port/ports/amiga/README.md>, <https://github.com/OoZe1911/micropython-amiga-port>, <https://github.com/jyoberle/micropython-amiga>, <https://aminet.net/dev/misc/AmigaMicropython.readme>
- Debian armhf libpython size: <https://packages.debian.org/sid/armhf/libpython3.13>
- Local: `~/Code/neovim-amiga/thoughts/shared/plans/2026-10-03-neovim-port.md`, `~/Code/neovim-amiga/amiga/compat/amiga-os.c`, `thoughts/shared/plans/2026-09-28-vtcon.md` (N1, Q1, P6-P8, A1)
