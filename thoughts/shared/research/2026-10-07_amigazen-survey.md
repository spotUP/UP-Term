---
date: 2026-10-07
topic: Survey of github.com/amigazen repositories for relevance to UP-Term/vtcon, amiga-pi, vsh-bash, ixemul/kit
tags: [amigazen, survey, amiga-pi, vsh, ixemul, kit, tls, http, toolchain]
status: draft
---

# amigazen survey

Source: `gh api users/amigazen/repos` (87 repos, none archived; 3 are forks: hubbub.library, Nami, quickjs.library). Read in full or in part via `gh api repos/amigazen/<n>/readme`: unsui, CShell, AmiTLS, AmiHTTP, AmiATP, Seiten, AmigaPython, Gen, make-amiga, curses.lib, clib2-amiga, libnix-amiga, asyncio.library, pcre.library, regexp.library, Codex, ToolChain (file list + README head). No README (404, judged by description only, UNCONFIRMED): unix.lib_sasc, cclib.library, Clipboard, Open, ToolKit, quickjs.library.

Common facts for all repos: part of the "ToolKit" standard, which targets **SAS/C 6.58 + NDK 3.2** (some also GCC/VBCC). Primary build is SAS/C smakefile. Nothing is built for ixemul/gcc 2.95 or amiga-gcc 16. Most are "refactored classic code, relicensed where the original allowed". Per-component licences vary; the GitHub licence field is NOASSERTION for most, so licence is "read LICENSE.md per repo" and is unconfirmed unless stated.

Overlap check against our own work: amigazen has NO terminal emulator, NO console handler (XCON/PTY), NO ixemul-based kit, NO tmux/screen/neovim, NO LLM client. Overlap exists only in: shell (CShell, shami/AXsh), C library/POSIX layer (UniLib3, clib2, libnix) vs ixemul, and the HTTP/TLS stack vs our planned amiga-pi networking (AmiSSL).

## Relevant repos

### Track (b) amiga-pi

**AmiTLS** (amitls.library) — BearSSL-based TLS client library over caller-owned bsdsocket sockets. Licence: MIT or BSD-2 for their code, BearSSL MIT (README). Push 2026-07-14. Helps: possible alternative to AmiSSL (small, no OpenSSL). Use: model to borrow from / optionally link instead of AmiSSL. Effort M. Risks: TLS 1.2 only (no 1.3, so an endpoint that demands 1.3 fails; the major LLM APIs still accept 1.2 as far as known, UNCONFIRMED), needs a PEM CA bundle on disk (no default), 68k SAS/C-first build, young library, shared-library dependency adds an install item. AmiSSL remains the plan-of-record; AmiTLS is a fallback if AmiSSL footprint or version pain bites.

**AmiHTTP** (amihttp.library) — HTTP/1.1 client shared library: sessions, transactions, redirects, pooling, chunked decode, gzip via z.library, HTTPS via AmiSSL or AmiTLS builds. Licence: own LICENSE.md (BSD-2 per GitHub), push 2026-08-30. Helps: closest thing to what amiga-pi Phase B needs. Streaming works through `HttpTransactionReadBody` with caller buffers (SSE can be parsed on top); BODY_CHUNK hook partial. Use: model to borrow from; reuse only as source reading, since we plan a native C harness with few shared-library dependencies. Effort M if used as a dependency, S for reading the API for design. Risks: pulls in amihttp + z.library + (amitls or amisslmaster) as runtime dependencies; `HttpConnection` tier 3 streaming calls are stubs; SAS/C build; low maturity (unconfirmed how it behaves on slow links / long-lived SSE).

**AmiATP** (amiatp.library) — AT Protocol client on AmiHTTP, JSON parsed inside the library. BSD-2. Push 2026-07-18. Helps: shows a JSON-over-HTTPS API client built on AmiHTTP; model only. Effort S (read). Risk: domain-specific, nothing to link.

**Seiten** — Bluesky client for Amiga on AmiATP. BSD-2. Push 2026-07-26. Model for a real consumer of the stack; low value to us. Effort S (read).

**z.library** (zlib shared library, no README, UNCONFIRMED licence) — needed by AmiHTTP for gzip. Only relevant if AmiHTTP is adopted.

**asyncio.library** (39.3) — double-buffered async file I/O. BSD per README. Push 2026-07-06. Helps: not needed for amiga-pi; could speed large file read/write in tools, low priority. Effort S. Risk: another shared-library dependency.

**quickjs.library** (fork, no README, UNCONFIRMED) — QuickJS as shared library. Relevant only to amiga-pi Phase L (extensions) if extensions were ever JavaScript. Effort L. Risk: memory footprint on 68k, unconfirmed.

### Tracks (a)+(c) shell, vsh bash compatibility

**unsui** — "POSIX environment": UniLib3 (POSIX/C99 library for SAS/C, derived from unix.lib), shami (POSIX sh, source in `src/shami`; the files there (axsh.c, parsecommand.c, getline.c, cusershell.s) suggest it is a rework of AXsh, UNCONFIRMED), and ~100 "koans" (command ports: awk, bc, ed, grep, sed, sort, tar, vim, xvi, less, m4, bison, flex, zip, unzip, bzip2, lzip, make-related, etc., each a directory in `src/`). Per-component licences "some GPL, some BSD" (README, flagged). No GitHub licence. Push 2026-03-14. Helps: (1) test corpus and comparison: koan commands are Amiga-native with getopts and ReadArgs, native AmigaDOS, no fork; these are exactly what vsh (no fork) runs well, so they are a candidate source of kit tools that do NOT need ixemul; (2) shami as a design comparison for a native POSIX sh. Effort: M per tool to evaluate, L to build a kit lane from them. Risks: SAS/C-primary, GPL components mixed in, WIP (README marks many as "in development"), unknown test quality, UniLib3 overlaps ixemul scope but without fork. Recommended action: audit the `src/` list against our kit list and pick BSD-licensed koans we lack.

**CShell** — csh-like native shell (Matt Dillon lineage). Licence: per COPYING / LICENSE.md, UNCONFIRMED (original Dillon CShell was freely redistributable, unverified). Push 2025-08-30. Helps: comparison for vsh (history, aliases, scripting, builtins); not a bash model. Effort S (read). Risk: csh semantics, not POSIX/bash; SAS/C.

**Codex** — C linter and style checker for Amiga (SAS/C, VBCC, DICE compatibility checks). Licence NOASSERTION. Push 2026-04-06. Helps: optional dev tool for our C sources, low value. Effort S.

### Track (d) ixemul/POSIX layer, toolchains, build, packaging

**clib2-amiga** — Olaf Barthel's clib2 for SAS/C and GCC, with POSIX-like routines and net.lib sockets. BSD-3. Push 2025-09-10. Helps: reference implementation of POSIX functions on native AmigaDOS without fork; source to read for specific functions (not to replace ixemul, which our kit depends on for fork and signals). Effort S (read). Risks: not ixemul ABI-compatible; no fork.

**libnix-amiga** (3.1) — GCC libc alternative to ixemul; amigazen removed GPL/LGPL parts, permissive. Licence per README: public-domain style (NOASSERTION on GitHub). Push 2025-10-11. Helps: a `-noixemul` lane for tools that need no fork; reference. Effort M for a build lane. Risk: replaces ixemul semantics; our kit depends on ixemul behaviour.

**curses.lib** — Simon Raybould's Amiga curses, BSD-2 after relicense. Push 2025-09-11. Dual mode: custom screen or ANSI over terminal. Helps: only as a model/test client; ANSI mode could serve as a test corpus for the terminal emulator (not a replacement for ncurses/terminfo used by tmux/neovim). Effort S. Risk: SAS/C, 8 colours, old API subset.

**make-amiga** — GNU make 4.4.1 port, GPL-3.0 (flag). Push 2025-08-30. Helps: only if the kit lacks make; we presumably have our own via ixemul (unconfirmed). Effort S. Risk: GPL-3, SAS/C build; upstream dropped Amiga support after 4.4.1.

**Gen** — GenIn (create .info files), GenMaki (convert/check smakefile/GNU/lmk/dmake makefiles), GenDo (Autodoc/AmigaGuide generation), packaging manifest tools (GenGen "coming later"). Licence NOASSERTION. Push 2025-10-02. Helps: GenIn directly useful for the Installer/kit packaging (generate .info files in a build script); GenDo for docs. Effort S to M. Risks: native Amiga binaries built with SAS/C, cannot run on the macOS cross-build host (need an Amiga/emulator step), tools unfinished (several "coming soon").

**ToolChain** (README missing; files: SDK, src, BUILD.md, AMIGAZEN.md) and **ToolKit** (no README, "Universal SDK for Amiga projects") — standard SDK layout, SAS/C-centred. Helps: model for a kit/SDK layout, and the "SDK:" assign convention. Effort S (read). Risk: SAS/C-only conventions, UNCONFIRMED contents.

**pcre.library** (PCRE shared library, Alfonso Ranieri wrapper) and **regexp.library** (Henry Spencer regexp) — regex libraries with permissive/BSD notes in README. Helps: nothing we need; our tools use libc regex (via ixemul). Effort S. Skip unless vsh `[[ =~ ]]` needs a regex engine without ixemul, then regexp/regcomp from our libc or ixemul is the answer.

**AmigaPython** — Python 2.x for Amiga (Irmen de Jong's port, being moved to 2.7.18), PSF-style licence (unconfirmed). Push 2026-08-17. Helps: our kit lists python; this is a Python 2 only, SAS/C-first, VBCC/GCC "probably". Not a replacement for a Python 3 ixemul port. Effort L. Risks: Python 2 EOL; build status unconfirmed.

**unix.lib_sasc** (no README, UNCONFIRMED) — "C standard library with C99 and POSIX functions for SAS/C"; probably the standalone source of UniLib3. Same relevance as unsui's UniLib3.

**cclib.library** (no README, UNCONFIRMED) — "CCLib is the Standard C Library as a shared library". Conceptually ixemul.library-like for SAS/C; worth a read when comparing library designs. Effort S.

**Scion**, **Insight**, **Clipboard**, **Open** — small ToolKit tools, READMEs missing or not read. Open (universal open tool) might be a nice small model; UNCONFIRMED. Skipped.

## Top picks (ranked)

1. **unsui** — koan command ports plus shami: candidate no-fork tools for the kit and a design comparison for vsh. Audit licences per component. (M to evaluate)
2. **AmiHTTP** — read its API as the model for amiga-pi Phase B (sessions, transactions, streaming reads); decide against depending on it. (S)
3. **AmiTLS** — BearSSL option if AmiSSL proves heavy; TLS 1.2 only and CA bundle on disk are the gates. (M)
4. **Gen (GenIn)** — .info generation for kit/installer packaging. (S)
5. **clib2-amiga** — BSD-3 reference for POSIX routines on native AmigaDOS. (S, read)
6. **CShell** — comparison for vsh features. (S, read)
7. **curses.lib** — ANSI-mode test client for the terminal emulator. (S)
8. **libnix-amiga** — possible `-noixemul` lane for fork-free tools. (M)

## Licence flags

- **make-amiga**: GPL-3.0. **ctags-amiga**: GPL-2.0. **texinfo-amiga**: GPL-2.0. **OpenTriton**: GPL-2.0.
- **unsui**: mixed per component (GPL and BSD named in README), no repo-level licence; check each koan before taking code.
- NOASSERTION or blank on most repos (AmiFTP, AmigaPython, AmiTLS repo field, CShell, clib2 is BSD-3, etc.): read LICENSE.md in each before reuse. Several blank repos (cclib, ToolKit, quickjs.library, z.library, Scion) have no licence file shown at repo level: treat as unknown.

## Skipped (not relevant)

AmiFTP, Amigami, aml.library, AmRSS, APP, ASAP, AWeb3, BGUI, Bonami, btree.library, ClassAction, crc.library, ctags-amiga, DataType, DiffView, DrawingDT, etags-amiga, expat.library, FastForward, FileTypes, ga.lib, GadToolsBox, Gengui, gtdrag.library, gtlayout.library, hotlinks.library, HTTPMount, hubbub.library, iconv.library, iffparse, ifftools, InAction, Introspection, LhASsA, LogBook, LZXa, ma.lib, MarkdownDT, mushin, MusicDT, Nami, OberonA, Objection, OpenTriton, OUI, popupmenu.library, Post, PowerTools, ProjectX, reaction.lib_sasc, rman-amiga, rtasl.lib, Seiso, tb.lib, texinfo-amiga, ToolManager, ttengine.library, TTX, Voyager, WBTrash, Workspace.
(Note: iconv.library, expat.library, LZXa could matter later for a kit with iconv/XML/lzx needs; ctags-amiga could ship in the kit as a GPL-2.0 tool; none judged useful now.)
