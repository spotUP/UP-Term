---
date: 2026-10-13
topic: Making our improved ixemul available outside UP-Term (upstream, licence, commit classes, naming)
tags: [ixemul, upstream, licence, lgpl, aminet, neovim, release, libixcompat]
status: final
---

# ixemul outside UP-Term

Repo studied: `/Users/spot/Code/ixemul-vtcon` (github.com/spotUP/ixemul-vtcon, public). Research only; nothing was
changed or built there. Each claim says where it comes from; "unverified" marks what I could not confirm.

## 1. Upstream

- **Maintainer of 80.x: Jorgen Johansson** (the name is spelled "Jorgen" in the repo's ChangeLog entries
  2026-09-23..25, "Jurgen" in a fetched copy of the Aminet readme; the spelling is unverified).
  Evidence: `ChangeLog` in the repo; Aminet page `https://aminet.net/package/dev/lib/ixemul-80.1-m68k` and
  `https://aminet.net/dev/lib/ixemul-80.1-m68k.readme` (fetched; the page text is 176 KB and I read only the
  first 100 KB of the package page, unverified for the rest).
  Readme header: Author "Markus Wild, Hans Verkuil et al.; v80.1 by <Johansson>", Uploader the same person,
  Version 80.1, "Replaces: ixemul-80.0 m68k", Requires AmigaOS 3.x, 68000+.
  Uploader address is shown obfuscated as "jj201 telia com". That it is `jj201@telia.com` is **my inference,
  unverified**.
- **Dates:** 80.1 released 2026-09-25 (readme; one fetch said 2025-09-25, the other 2026-09-25 - treat the year as
  2026, matching `version.in` "80.1,5.10.26" and the ChangeLog). 80.0 shortly before. All 80.x activity I can see
  is inside the last month; there is no earlier 80.x history.
- **Public repo accepting PRs: none found.** `gh search repos ixemul` returned only `threader/ixemul` (a GitHub
  mirror of the old 48.x line: branches v48.x, v50.x-morphos, v63.x, v64.x; last commit on its default branch
  2018 by bebbo; 0 pull requests; GPL-2.0; owner is a user, not the maintainer), `threader/ixemul_v63`, ours,
  and unrelated repos. No 80.x repository exists there. Aminet text searches found nothing about 80.x either.
  Not checked (could not reach): SourceForge, Aminet comment pages, Amiga forums (Amiga.org, EAB, a1k.org),
  the maintainer's own site. **Unverified: a private/other-hosted repo may exist.**
- **Contribution rules:** none published. The only channel stated is the readme's support section: problem
  reports should include AmigaOS version, CPU, ixemul version, program and version, the error, and whether the
  previous library shows it too. The readme also says it "targets the traditional ixemul API and avoids broad
  semantic changes" - a signal that large behaviour changes (job control, ptys, AF_UNIX rewrite) are not what
  the maintainer wants to take blindly.
- **Prior line:** README history: Markus Wild -> Luebbert/Norrgard -> Hans Verkuil (42.0..48.x) -> interim
  David Zaroski (Product-Info). 80.x is a continuation after a gap of about 25 years; the maintainer is a single
  person, and the repo's 80.1 import is `21a78ae` (Aminet archive, sha256 in
  `thoughts/shared/plans/2026-10-05-ixemul-80-migration.md`).

## 2. Licence

Evidence in repo: `COPYRIGHT`, `LICENSES.README`, `COPYING.LIB`, `COPYING`, file headers (e.g. `library/ix_open.c`).

- ixemul.library is **GNU Library General Public License v2 or later** (LGPL-2). `LICENSES.README` (80.1):
  "distributed under the GNU Library General Public License, version 2 or, at your option, any later version";
  `library/*.c` headers say the same (120 files grep-match a GNU licence header; counted with `grep -l`).
  A full `COPYING` (GPL v2) is also in the tree, and Product-Info says "GNU Public License"; that is why GitHub
  labels our repo GPL-2.0. The authoritative statement is the per-file LGPL header plus `LICENSES.README`.
- Many files are BSD (Regents of the University of California) or other authors; their notices stay per file
  (`COPYRIGHT`: the restriction applies to each single file). The ixemul glue files from `gen_glue.c` are
  declared Public Domain. A program that merely uses the library via OpenLibrary is not a derivative
  (`COPYRIGHT`, LGPL section 5). Linking `libc.a`/`crt0.o` into commercial programs is allowed provided the
  customer is told ixemul.library and ixnet.library are free software and where to get the source.
- **May we release a modified binary + source on Aminet: yes.** LGPL-2 allows modified copies under the same
  terms. Obligations:
  1. Ship the complete corresponding source (or a written offer) with the binary; the Aminet 80.1 archive
     itself does this (src in the same lha); do the same.
  2. Keep every existing copyright and licence notice; each modified file carries a prominent notice that it
     was changed and the date (LGPL-2 section 2a). Our commits modify files but the in-file change notices are
     not guaranteed: **check before release** (unverified; I did not audit headers of the ~45 touched files).
  3. New files (`compat/*.c`, `library/__vtcon.c`, `cli_args.c`, `tests/`, `docker/`) need a header; choose
     LGPL-2-or-later for anything that links into the library, so it merges upstream.
  4. Ship `COPYING.LIB`, the `COPYRIGHT` and `LICENSES.README` texts unchanged; state who changed what.
  5. Do not remove the original-author credits (Wild, Luebbert, Shepherd, Verkuil, ... ) from README/Product-Info.
  6. LGPL-2 section 5 information to users as above (customers of programs linked with `libc.a`).
- **Aminet-specific:** Aminet requires a readme with `Short/Author/Uploader/Type/Version/Replaces/Architecture`
  fields and allows source in the archive; "Replaces: ixemul-80.1-m68k" would remove the maintainer's archive
  from the listing, so a differently named archive (section 5) must not claim to replace it. Aminet's own
  upload rules were not fetched: unverified.
- **Commit trailers:** 56 of the 92 commits carry `Co-Authored-By: Claude ...` and `Claude-Session:` lines. They
  are not a licence problem (copyright in our additions is the repo owner's), but they will go into any history
  published upstream or in an Aminet changelog. For PRs, build fresh commits (new branch per topic from the
  80.1 import, squashed, no trailers), as the owner's rule requires.

## 3. Our 92 commits after 21a78ae, classified

Counts (checked: 45 + 18 + 3 + 26 = 92; `git log 21a78ae..HEAD | wc -l` = 92, 0 merges).

| Class | Count |
|---|---|
| (a) general, upstreamable | 45 |
| (b) UP-Term-specific | 18 |
| mixed (one commit = a part of (a) and a part of (b)) | 3 |
| (c) build system / compiler porting / plans | 26 (15 code or build, 11 plan or doc only) |

Method: file lists and bodies of every commit (`git show --name-only`, bodies of the doubtful ones read). The
plan commits are not in 80.1's tree to begin with.

### (a) General, grouped into PR-sized topics

| # | Topic | One line | Commits |
|---|---|---|---|
| A1 | AF_UNIX on the refcounted streams | socketpair, SCM_RIGHTS, a select waiter per direction, POSIX connect errors, sun_path with no NUL; the biggest item (about 400 lines, `library/unp.c`) | 423aff5, 2ae1c37; connect part of 1c29e36 |
| A2 | socklen_t / header guards | one `socklen_t` whichever of sys/socket.h, netdb.h, arpa/inet.h comes first | 627d254 |
| A3 | Streams that cannot seek | a stream with no Seek and no Examine is a FIFO: fstat S_IFIFO, lseek ESPIPE (fixes `wc -c` on PIPE:) | f320791 |
| A4 | select | a write-only select wakes at once; EINTR when ixnet's WaitSelect fails | select part of 953a309 |
| A5 | Command-line parsing | split the argument line as ReadItem does, `*` escapes in quotes, read RunCommand's copy from Input() | 74fd55b, 74c77ac, 5cc91cd |
| A6 | execve / system | quote a native program's arguments AmigaDOS way, pass the environment as its local variables, `system()` runs the shell by path | 511c224, 2ccf63e, fcc2c7b |
| A7 | Small POSIX fixes | getgroups(0), usleep returns int, fnmatch FNM_CASEFOLD, glob GLOB_NOMATCH, fstatfs of a directory, `__plock` device-named absolute path, `_psignal` only for ixemul processes | 16e01bd, 2c90ef1, 610cf88, b4630aa, 6279020, b8ad353, 87c9687 |
| A8 | printf length modifiers | z, t, j, hh (an unknown one shifted every later argument) | printf part of e513866 |
| A9 | malloc small-block cache | per-process cache of small freed blocks, "10x faster"; needs re-measuring on 80.1 and a look at the vfork interaction | d8e75c3 |
| A10 | Missing/wrong SDK headers | math.h C99 macros (a0cb0bd, 8ff7a77), inttypes.h (47b6874), errno ENOTSUP (ab91b75), sys/resource.h (cc6b8df, e5d22d5, ba2029d), stdarg va_start builtin (ae7db4b), sys/mman.h MAP_FAILED (b8f7643), utime.h (9e810a4), size_t under gcc 3+ (7e12252), alloca.h/sys/select.h/ctype (12cd42a) | 12 commits as listed |
| A11 | libixcompat.a (static, SDK side) | not a fix but a new component: poll, realpath, posix_spawn, getopt_long, pselect, C99 integer/division/strto*imax functions, wide chars and UTF-8 locale, tsearch, strtok_r, bswap, nanosleep, getpgid, lchown, atan2, `__eprintf`, stpcpy/mempcpy | 0d09e12, a105524, 2767cac, 3804757, c15c438, 54bf915, 8beebf9, 14a4b3b, 03d9a2f, 432f177, e5a1ac9, 9197f21, 23399b9, e733967, a7b3c5b |

Of the 45: A1 2, A2 1, A3 1, A5 3, A6 3, A7 7, A9 1, A10 12, A11 15 (A11 includes a7b3c5b) = 45. The mixed commits
supply A1 (part), A4 and A8.

Upstreamability notes: A3, A5-A8, A10 are small and low risk. A1 is large and rewrites `unp.c`; A9 changes the
allocator the maintainer has just hardened in 80.x (his `malloc.c` revisions 1.6/1.7), so expect resistance.
A11 does not go into the library: offer it as a separate SDK package (see section 6).

### (b) UP-Term-specific, and can one binary serve both?

18 commits: 2422a6d (termios and window size by packet), cee2c09 (^\ ^Z as SIGQUIT/SIGTSTP), fc8ed8f (job
control), 262acc4 (BSD ptys on PTY:, SIGWINCH to slave's group), 64e9b26 (SIGTTIN/SIGTTOU), a9d3e61 and eab76b0
(^C and EINTR in reads of a vtcon console), 3e6eff6 (do not re-ask a handler that refuses), 5e4b457 (FIONREAD
on a PTY: pipe), ebbdc56 (/dev/tty from TIOCSCTTY), aa9b132 (unmounted IXPIPE: fails quietly), e7e660b (ixnet
mismatch requester names the UP-Term Install), 962b536 (umask from local variable `UMASK`, which vsh sets),
06baad5 (/bin/sh is vsh), 2cda2f6 and 39f6760 (vsh's out-of-band argv), a029e7f (version string "UP-Term"),
69feef4 (posix_openpt/grantpt/ptsname over the BSD ptys).

Mixed: e513866 (FIONREAD via ACTION_VTCON_NREAD 0x765A = (b), printf modifiers = A8), 1c29e36 (/dev/tty stat for
GNU screen = (b)-ish, connect/bind fix = A1), 953a309 (console is read/write = (b), write-select and EINTR = A4).

**Can they be optional by runtime detection? Mostly they already are**, verified in source: `library/__vtcon.c:17,43`
- a handler that answers a vtcon packet with `ERROR_ACTION_NOT_KNOWN` is asked once and then the stock path is used
(`__vtcon_refused`); the e513866 body says "a handler that does not know it keeps the old answer". So a stock
console or a file handle never sees the packets. What remains to make one binary serve both:
1. Version string suffix (a029e7f): harmless to keep (section 5).
2. `UMASK` local variable: absent variable = old 022 default (962b536 body); harmless.
3. `/bin/sh` as vsh in execve (06baad5), vsh argv (2cda2f6, 39f6760): only act when the vsh variable/handle is
   present - **to confirm by reading the code before a release (not done here)**.
4. `IXPIPE:` quiet failure (aa9b132): only changes an error path.
5. ixnet's requester text (e7e660b): text only; reword to name no product.
6. PTY: / posix_openpt (262acc4, 69feef4): only reachable if a `PTY:` mount exists; without one `open` fails as before
   (**unverified**).
Behaviour change that remains even with detection: BSD pty names resolve to `PTY:` (48.x/80.x used FIFO: ptys), only
when PTY: is mounted. Verdict: one binary is plausible with the checks in 3 and 6 done and a rig run on a stock
Workbench with no UP-Term assigns (the rig exists, see `2026-10-13_standalone-addons.md` matrix).
The (b) code is a poor upstream PR as it stands: it depends on the vtcon handler (itself not public in a form the
maintainer can test); the neutral part is "packet-based termios/winsize on any handler that implements them",
which is a separate protocol proposal, not a patch.

### (c) Build only, 26 commits

Build: 0c5d510, c25e17e (bebbo gcc 6 on macOS), 120b1b0, b30c907, f60cf0c, e18e3ce, b54cf98 (Docker gcc 2.95.3),
d9a0984 (vector table checks), 0093ec5 (docker README), 45f8bd3, 40852da, e8bcc9d (gcc 16 / gcc 4.9+ source
changes), 3ebef54, 8fb9acb, ff795a5 (gcc 16 asm/stackextend fixes; ff795a5 and 8fb9acb correct real latent
mistakes - register clobbers, saved signal mask - so they could go upstream as a small "asm constraints" PR, but
only matter if the maintainer builds with a newer gcc: unverified what he uses).
Plans/docs: f557286, 5bdb27b, 21a33f3, e0f7026, 537fc55, b626bd6, d6b52a6, 45c9b17, 0632df8, c2ccd96, b5fd40a.

## 4. What Neovim needs

- **Confirmed:** Neovim does not run on stock ixemul 48.3 ("Failed to start the in-process UI!") and runs on the
  kit's 80.1 build (rig run in `2026-10-13_standalone-addons.md`, rows nvim, lines 112, 132, 191). The same
  note shows Python only needs any ixemul >= 47.
- **Which change fixes it: inferred, not bisected.** The port runs the TUI in the same process over a
  `socketpair` as a loopback msgpack channel (`~/Code/neovim-amiga/thoughts/shared/plans/2026-10-03-neovim-port.md`
  lines 245-257). Stock 48.3 `socketpair` returns `EPFNOSUPPORT`
  (`2026-10-05_ixemul-80-evaluation.md:15,92`). Therefore **A1 (423aff5, 2ae1c37 and the connect fix in 1c29e36;
  plus A2 627d254 for the header)** is the probable need. Unstated, the 80.1 line itself (non-48) matters for
  version >= 47 checks.
- **Probably also used by the port (inferred from the plan, not tested individually):** pty spawn for `:terminal`
  (69feef4 posix_openpt over BSD ptys, 262acc4 - Q16 still not run on the rig), vfork spawn (existing), libixcompat
  pieces (A11) and `select` fixes (A4) through libuv.
- **Not needed by Neovim:** the (b) job control/termios packets are used for TUI behaviour in UP-Term but nothing
  states Neovim requires them. Whether Neovim works with only A1 on an otherwise stock 80.1 was never measured.
  **The decisive experiment (not run - out of scope, needs rig):** stock 80.1 (`21a78ae` build) vs
  80.1 + A1 + A2 only, Neovim starts or not. That one result defines the minimal "ixemul for Neovim" release.

## 5. Version naming

Facts: `version.in` is `80.1,5.10.26`; library version is `major.minor`; ixemul and ixnet are version-locked
(`ixnet_open.c:82-91`); programs check `OpenLibrary("ixemul.library", N)` with N = 47 or 80 (Neovim's package
records need 80; `2026-10-13_standalone-addons.md:191`). Our build already appends " [UP-Term..." to the
version string (a029e7f) while keeping the number 80.1. Collision risk: if upstream later ships 80.2, ours
with the same number would carry different content.

Recommendation: **do not take 80.2 or any number upstream may use.** Keep 80.1 as the library number for the
first release (our code is 80.1 plus patches) and identify ours in the version string only:
`ixemul.library 80.1 [vtcon-1]`; archive name `ixemul-vtcon-80.1.1-m68k.lha`; readme "Replaces:" left empty and
"Based on ixemul-80.1 by <Johansson>". Version numbers are `major.minor` integers, so there is no safe
in-between number: if Neovim ever has to tell patched from stock by number, the number must be agreed with the
maintainer (ask which minors he reserves; unverified that he cares). Neovim's package record needs 80 today,
which 80.1 satisfies.
Both libraries (ixemul + ixnet) ship together with the same build number.

## 6. Recommendation

**Both, in this order: own Aminet archive first, upstream PRs second.**

1. **Contact the maintainer (0.5 day incl. waiting).** One message via the Aminet uploader address, with the
   branch and a list of topics A3, A5-A8, A10, A1. Ask: does he take PRs, where, and which version numbers to use.
   No public repo exists, so patches would be mailed or sent as a repo he pulls. Outcome decides everything else.
2. **Make one binary (1-1.5 days):** check (b) items 3 and 6 above; remove "UP-Term" wording from user-visible
   texts (e7e660b); run the rig matrix on a stock Workbench. Includes the decisive Neovim experiment (section 4).
3. **Own Aminet archive (1-1.5 days):** `ixemul-vtcon-<n>-m68k.lha` with ixemul + ixnet from one build, src,
   readme in Aminet format, the LGPL obligations in section 2, a differently named archive that does not replace
   the maintainer's. This unblocks Neovim outside UP-Term now, whatever upstream answers. Needs the owner's
   decision on publishing under the name `spotUP` and on the cost of owning a fork (every upstream change must be
   merged by us).
4. **Upstream PRs (3-4 days, as he accepts):** order by risk: A10 headers + A7 small fixes (1 day), A3 + A5 +
   A6 (1 day), A4 + A8 (0.5 day), A1 + A2 (1-1.5 days with its rig tests), A9 last or never. Each PR a fresh
   branch from `21a78ae` with a clean commit and no AI trailers. Keep libixcompat (A11) as a separate SDK
   package (headers and a static library are not his library); offer it, do not insist.
5. Never upstream: (b) as it stands. Offer later only the protocol idea (termios/winsize on a handler), if he
   wants a terminal-aware ixemul.

Open decisions for the owner (not mine): publish a fork under our name at all; version label; whether the
fork may carry UP-Term strings; whether A1 should be contributed given its size.
