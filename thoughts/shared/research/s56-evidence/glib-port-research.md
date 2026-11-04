# GLib for AmigaOS 3 (ixemul): can the OS4 port seed it, and what does it unlock

Research only. 9 tool calls. Items marked UNVERIFIED come from general knowledge, not a fetched source.

## A. The AmigaOS4 GLib 2.30.1 port

Source: https://aminet.net/dev/lib/glib_os4.readme (uploader Chris Young; author field "Various").
- Licence: readme states none. GLib itself is LGPL-2.1+, so the port is LGPL by derivation.
- Source availability: readme does not say. The Aminet archive (dev/lib/glib_os4.lha) is a binary/dev package; whether it contains a diff is UNVERIFIED (I did not download it; no GitHub or SourceForge repo found by search).
- Changes: "quick port", "some functions are stubbed out", one replacement pipe() added to one file, entirely untested, author says to use it as a basis for a full port. Needs libffi_os4 and the OS4.1 SDK, ppc-amigaos 4.0.0+.
- Which backend files it touched (gmain/gspawn/gthread/...): not stated anywhere I could reach. UNVERIFIED. The readme's pipe() note plus "stubs" suggests gmain wakeup pipe (gwakeup.c / gmain.c) and spawn/child-watch were stubbed.
- OS4-only dependence: it targets newlib + OS4 SDK, ppc. Whether it uses pthreads.library or WaitSelect is UNVERIFIED. Expect gthread-posix (pthread) to be the stubbed or newlib-pthread part.

Estimate for OS3: the portable part (gstring, ghash, glist, gutf8, gregex via bundled PCRE, gdate, gmarkup, gkeyfile, gvariant, gobject) is plain C and carries over with any port; that is the bulk of the code and no port is needed for it. The hard 5-10 percent is backend: gmain poll (needs poll() or select shim; ixemul has select), gwakeup (pipe or socketpair; ixemul pipe is the issue), gthread (pthread stub: use a single-threaded, mutex-as-no-op build; GLib 2.32+ mandates thread-safe primitives but mutex/cond can be no-op stubs with only one thread), gspawn (no fork: replace with vfork+exec or SystemTags), gfileutils/gunix*. The OS4 port gives little here beyond a list of what to stub, and its source is not confirmed available. Writing the backend stubs ourselves, starting from stock GLib, is the realistic route. Other Amiga ports (MorphOS 2.6.6, AROS 2.6.6 from gtk-mui CVS, m68k 2.4.4) are too old and AROS lacks timers and fork-based functions (https://morphos.aminet.net/dev/misc/glib.i386-aros.readme, https://morphos.aminet.net/dev/c/glib-morphos.readme).

Version target:
- Current mc minimum is GLib 2.32 (master m4.include/mc-glib.m4: `glib-2.0 >= 2.32`, also gmodule 2.32): https://raw.githubusercontent.com/MidnightCommander/mc/master/m4.include/mc-glib.m4. Debian's 4.8.33 lib/glibcompat.c carries shims for newer functions, so mc builds against 2.32 (https://sources.debian.org/src/mc/3%3A4.8.33-1/lib/glibcompat.c). My earlier note of 2.30 was for 4.8.25; correct to 2.32.
- Last autotools series: 2.58.x (2.60 is Meson-only; autotools dropped on master Jan 2019): https://blog.gtk.org/author/ebassi/page/2/ and https://lists.gnome.org/archives/gtk-devel-list/2018-June/msg00012.html. Recommendation: 2.58.3 (last 2.58), autotools, cross-builds with plain configure (needs libffi, zlib, libiconv, gettext/intl or --disable-nls; pcre is bundled in 2.58, system pcre optional). Versus 2.32-2.40: smaller and fewer new deps, but 2.58 gets bug and security fixes, and chafa (>= 2.35.8 in Debian packaging) and irssi/profanity need newer than 2.32. 2.58 is also still smaller than Meson-era (2.62+ needs Python/Meson cross files, and 2.7x need a newer pcre2 and more libc). Size cost: libglib alone roughly 1-1.5 MB on m68k (UNVERIFIED); gobject and gio add more, so build with --disable-gio-tests and link only glib+gobject where the app allows.

## B. What GLib unlocks (GLib-only hard dependency, terminal use)

Kit libs today: ncurses, zlib, iconv, lua, perl, libarchive, xz, bzip2, file. Missing: glib, libffi (needed by gobject 2.58), gettext/libintl, pcre2/slang, openssl-class TLS (AmiSSL exists on Amiga but is not a kit lib).

1. mc 4.8.x: dual-pane file manager. GLib >= 2.32, ncurses ok (slang optional). Needs: gettext stubs, PTY subshell dropped. Beyond GLib: medium-large (no fork: subshell, VFS helpers, extfs). Highest value: the classic Unix file manager.
2. irssi: IRC client. GLib >= 2.28 (Meson build; UNVERIFIED exact), ncurses, optional OpenSSL and Perl. DNS lookup helper and /exec use fork (vfork replacement needed); GIOChannel sockets over bsdsocket via select. Beyond GLib: medium. Networked, very good fit for a terminal on a 68060.
3. chafa: images to terminal (sixel/kitty/ANSI); library plus CLI. GLib only for the core (libchafa depends on libglib2.0 >= 2.35.8 per Debian: https://packages-pkgmirror-csail.debian.org/bullseye/armel/libchafa0). CLI needs image loaders (stb-style built-in loaders in recent releases; older needed ImageMagick: https://github.com/oakes/chafa). Beyond GLib: small-medium. Directly exercises UP-Term's own graphics output.
4. profanity: XMPP client. Needs GLib, ncurses, libcurl, readline, sqlite3, libstrophe (expat/OpenSSL) and more: https://profanity-im.github.io/guide/0180/build.html. Most of those are NOT in the kit. Large.
5. bitlbee: IRC gateway daemon (GLib + libgcrypt/gnutls/openssl; forks per connection or runs inetd). Runs a server; fork-heavy; low value on a single Amiga terminal. UNVERIFIED details.
Others (from memory, UNVERIFIED): finch/libpurple (needs libxml2, many deps, huge), atop (no GLib: libz/ncurses, Linux-only), ncmpc (GLib plus libmpdclient; useful only with a remote MPD), tmux/weechat/vifm do not need GLib, newsboat is Rust/C++ (no), gnunet no.

Ranking for an Amiga terminal user, value over port cost:
1. mc (GLib + ncurses only; owner-wanted)
2. irssi (IRC, very commonly wanted; deps in reach)
3. chafa (shows off the terminal; small beyond GLib)
4. ncmpc (small, GLib + libmpdclient, remote music control; needs libmpdclient port which is tiny) - lower confidence
5. profanity (valuable but deps explode) - bitlbee is not a user-facing app.
Verdict on cost: GLib itself is the one big prerequisite that serves 3-4 apps; if only mc is wanted, vifm (ncurses only, see mc-port-research.md) is far cheaper.

Sources: aminet glib_os4.readme, morphos/aros readmes above, mc-glib.m4, Debian glibcompat.c, GTK blog 2.58 notes, gtk-devel-list meson thread, profanity build guide, chafa Debian page.
