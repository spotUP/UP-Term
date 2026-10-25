---
date: 2026-10-25
topic: irssi port to Amiga/ixemul (UP-Term) feasibility
status: research only (sources fetched to scratchpad irssi-src/, nothing built)
verdict: GO, medium job (about 4 new pkgs, 6-8 patches)
---
# irssi on ixemul: feasibility

Sources inspected (data only): irssi 1.4.5 tarball, glib 2.58.3, glib 2.56.4, in
/private/tmp/claude-501/-Users-spot-Code-UP-Term/5983afe9-3b58-421c-8142-35583cf17ab1/scratchpad/irssi-src/.

## What ixemul already gives (grepped in /Users/spot/Code/UP-Term/ixemul-vtcon)
- pthread: compat/pthread.c + include/pthread.h = single-thread subset. Mutex/once/key work; pthread_create fails EAGAIN, cond wait EDEADLK. glib will link; threads simply never start.
- poll(): compat/poll.c, on top of select(). select() is in the library (sockets via ixnet). Console select not verified here (needs a rig probe).
- getaddrinfo/getnameinfo: compat/gai.c, IPv4 only, over gethostbyname.
- langinfo, wcwidth, mbrtowc: compat/. setlocale exists.
- pipe, vfork, posix_spawn (compat/spawn.c): present. socketpair: only in man pages, no code found (irssi only uses it in capsicum.c, off by default).
- dlopen: absent (only man page link.5). iconv: no libc iconv; upterm-ports has pkgs/iconv but it installs only the program, not the library/header.
- fork: gone by policy (check_bin).

## 1. glib2
- Newest autotools: 2.58.x ships both, but the 2.58.3 tarball has NO generated configure (needs autoreconf with autotools on the build host). 2.56.4 tarball ships configure. Meson-only from 2.60. Recommend 2.56.4.
- Needs (configure.ac): libffi >= 3.0 (PKG_CHECK_MODULES, hard), system PCRE >= 8.31 with UTF8+unicode props (internal pcre dir is empty in both versions; default with_pcre=system), a threads impl (--with-threads=posix, satisfied by the stub pthread), iconv (--with-libiconv=native/gnu; libc has none), gettext (--disable-nls), libmount (--disable-libmount), zlib (gio only; exists).
- Trim: irssi needs only libglib-2.0 and optionally gmodule. Build only `glib/` (+ `gmodule/`) via `make -C glib`; skip gobject/gio/gthread so libffi is never built: pre-set LIBFFI_CFLAGS/LIBFFI_LIBS to dummy values to pass the configure check. Threads cannot be "off" in glib (no switch since 2.32) but the stub pthread is enough because irssi never creates a thread.
- Already in upterm-ports/pkgs: zlib, iconv (program only), ncurses, ixcompat. Not present: pcre, libffi, glib, gettext, openssl/libtls.
- glib is the real risk: autotools cross-compile (m68k-amigaos) needs many cache vars (endianness, printf sizes, atomic ops on 68k, GLIB_SIZEOF_*), plus gmodule dl check. Expect 4-6 patches/cache overrides and one rig run of glib's own tests (glib/tests subset).

## 2. irssi
- Version: 1.4.5 (latest 1.4 release). Min glib >= 2.32 (meson.build:184, configure.ac AM_PATH_GLIB_2_0(2.32.0)), so 2.56 is fine.
- Build system: the release tarball ships a generated `configure` AND meson.build. Use configure (no meson/autoreconf needed). Cross-compile with --host=m68k-amigaos.
- Deps: glib (required), ncurses (textui; ncursesw exists), OpenSSL is MANDATORY in 1.4.5 (configure.ac:313 AC_MSG_ERROR if libssl missing; meson also dependency('openssl') with no option; src/core/network-openssl.c, 944 lines, ~63 SSL/X509 symbols across core). There is no openssl/libtls in the sysroot (nc ships a tls_stub.c only, upterm-ports/pkgs/nc/compat/). Options: (a) patch irssi to compile without TLS (configure check + network-openssl.c replaced by a stub returning "no TLS", the tls-less IRC on 6667 still works); (b) port LibreSSL/AmiSSL later. (a) recommended: 1 larger patch, mechanical.
- perl: --without-perl (default off if absent). --without-proxy --without-bot optional. --disable-gregex only avoids irssi's own use; glib still needs PCRE. --without-modules (configure.ac:79) removes gmodule/dlopen need. utf8proc: --disable-utf8proc (else wcwidth-based fallback; check display width of CJK).
- Alternative to dodge OpenSSL: irssi 1.2.x/0.8.x (SSL optional via --enable-ssl in 0.8, in 1.2 also required? unverified). Not recommended: older and no real saving.

## 3. Fork sites (irssi 1.4.5 src/, grep of fork|vfork|pipe|exec|socketpair|dlopen)
| site | what | patch |
|---|---|---|
| src/core/net-nonblock.c:40 net_gethostbyname_nonblock | forks resolver child writing RESOLVED_IP_REC into a pipe | irssi ALREADY has the fallback: if fork() < 0 it resolves blocking in-process, writes the record into the pipe and returns 0. Patch: replace `pid = fork();` with `pid = -1;`. Caller (servers.c:419-441) treats pid 0 fine; connect_tag reads the pipe next loop. Cost: UI freezes during DNS (acceptable; same as BitchX no-fork). Check ixemul pipe buffer holds sizeof(RESOLVED_IP_REC)+errstr (~ small) so the in-process write does not block: needs a rig probe. net_disconnect_nonblock kill(pid) is only called for pid>0. 1 patch, 1 line. |
| src/fe-common/core/fe-exec.c:295 (/exec) | pipe x2 + fork + execvp/shell | Either disable: make the fork-failure path (line ~506 "pipe() or fork() failed") the only path (`rec->pid = -1`), /exec then prints an error. Or vfork+exec with fd dup via posix_spawn file actions (compat/spawn.c) like nnn/mutt patches. Recommend disable first (1 patch), spawn later. |
| src/core/session.c:54 execv (/UPGRADE) | execv of itself, no fork | Not a fork; link-clean. Disable /UPGRADE or leave (needs a re-exec path under vsh; low value). |
| src/otr/key.c:250 | fork | OTR off by default (needs libotr). Nothing to do. |
| src/core/capsicum.c:342 socketpair | FreeBSD capsicum | Off by default. Nothing. |
| src/core/modules-load.c g_module_open | dlopen | --without-modules. |
| src/core/network.c:419 getaddrinfo | used by net_gethostbyname | present in compat/gai.c (IPv4 only), use --disable-ipv6 style: irssi falls back to AF_INET lookups. |
Also: sockets and the glib main loop use g_poll -> poll() -> select(); the main loop needs console input (stdin fd 0) and sockets selectable at once. Unverified on ixemul: one rig probe (select on con + socket).

## 4. Blockers and size
No hard blocker found. Soft blockers, in order of risk:
1. OpenSSL hard dependency: needs the TLS stub patch (or an AmiSSL/LibreSSL port, a separate large job; defer). Result: irssi without TLS (no SSL to libera.chat:6697; plain 6667 only, or a stunnel-style helper not available). Decide whether TLS is a requirement: if yes, this job becomes LARGE.
2. glib cross-build: cache vars, atomics on 68k, no iconv in libc -> may need a libiconv library pkg (current pkgs/iconv builds program only; glib's g_convert/charset needs iconv_open at link). Plan: change pkgs/iconv to also install libiconv.a + iconv.h under a different prefix, or glib --with-libiconv=gnu. Risk medium.
3. Console+socket select and pipe buffer (see above): unverified.
4. Stack: irssi + glib are stack-hungry; set nnn-style __stack (STACK_VAR), measure.

Size (new pkgs): pcre (8.45, trivial autotools), glib (2.56.4, hard), irssi (1.4.5), plus libiconv library pkg or pkgs/iconv rework. libffi avoided. That is 3 new pkgs + 1 rework.
Patches: glib 4-6 (config overrides, gthread/atomics, gmodule off), irssi 4-6 (fork x2, TLS stub, --help/usage like other ports, modules/perl off).
Effort estimate: roughly 3-5 rig-assisted work sessions; glib is about half of it. Risk: medium-high on glib, low on irssi once glib links. Reachability test: drive the irssi binary from vsh to connect to a local ircd on localhost, join a channel, assert a message round-trip (also proves the blocking resolve path ran).

## Verdict
GO, medium size, provided "no TLS" is acceptable for v1. If TLS is required, add a LibreSSL/AmiSSL port and call it large.
