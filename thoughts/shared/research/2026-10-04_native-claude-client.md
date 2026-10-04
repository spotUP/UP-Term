---
date: 2026-10-04
topic: Claude on the Amiga -- what can run where, for the native Claude client (ledger A2) and the LAN route (A1)
tags: [claude, a1, a2, nodeamiga, python, amissl, amiga]
status: draft
---

# Claude on the Amiga: research for the native client (A2)

Owner, 2026-10-04: "claude cli on the amiga is on the top of my wish list ... separate from the
python thing". Ledger: A1 (Claude Code on the Mac, UP-Term as its terminal over the LAN) and A2
(a native client) in thoughts/shared/plans/2026-09-28-vtcon.md. Target machines: 68020+, high
end 68030/68060 first.

## Claude Code itself cannot run natively

- Claude Code is a Node.js program: a multi-megabyte JavaScript bundle on Node 18+.
- Node.js's engine (V8) has no 68k port; no JIT or interpreter backend exists for m68k.
- Even a perfect JS engine would face: real `child_process.spawn` with streamed stdio, raw tty
  mode on process.stdin, WebStreams, AbortController, npm packages with native parts.
- So: either run Claude Code elsewhere and use UP-Term as its terminal (A1), or write a native
  client that speaks the Claude API itself (A2).

## NodeAmiga (Aminet dev/lang/NodeAmiga, 0.28.0, 2026-06-06)

Evaluated from the Aminet readme (not run):
- NOT Node.js: "A complete JavaScript engine and runtime environment for Classic Amiga, MorphOS
  and AmigaOS4", "Built from scratch in C", "Inspired by Node.js". Author Juen/R3D+Appendix+Nah-Kolor.
- Engine: own lexer, recursive-descent / Pratt parser, TREE-WALKING interpreter by default; an
  experimental bytecode VM bundled.
- Language: ES5.1 + much of ES6+ (arrow functions, classes, async/await, Promises, template
  literals, destructuring, spread, generators, BigInt, ?. and ??), ES modules incl. dynamic import().
- Node-like APIs: fs (sync, async, streams), http client/server, https via optional AmiSSL,
  fetch(), child_process.execSync (via SystemTagList -- no streaming spawn), readline, Buffer,
  typed arrays, streams, events, crypto (MD5, SHA-256), process.argv/env/cwd/exit, os.cpus().
- Amiga bindings: Intuition/GadTools GUI, clipboard (IFF FTXT), ARexx send(), FFI to libraries.
- Requirements: 68000+ (68000/020/040/060 builds), Kickstart 2.04+, 1 MB RAM minimum, no FPU;
  bsdsocket.library for networking, AmiSSL for HTTPS.
- Limitations stated: "No Proxy", "await is synchronous (spin-waits on event loop)".
- NO LICENCE STATED: UP-Term cannot bundle it in the kit.
- Verdict: cannot run Claude Code (bundle size on a tree walker; missing Node APIs). Could host a
  small Claude client written in JavaScript (fetch + https + readline + fs) -- kept as a fallback
  runtime for A2, not the plan.

## Python as the client runtime

From research/2026-10-04_python-on-amigaos.md:
- The official Anthropic Python SDK is not portable: it needs pydantic v2, whose core
  (pydantic-core) is Rust, and Rust has no AmigaOS 68k target.
- A client on CPython's stdlib (urllib + ssl) would need the CPython 3.14 port (PY1, 68030+ with
  FPU and ~16 MB fast RAM) plus an `ssl` module on AmiSSL.
- MicroPython 1.28 (Aminet dev/lang/micropython, 68020+, OS 3.0+, 2 MB; 4 MB for networking/TLS)
  could host a small client sooner -- TLS on the Amiga not verified.
- amigazen AmigaPython is Python 2.7.18, unreleased, no Unicode -- not a basis.

## The chosen design for A2: a native C client

Why C: shippable in the kit (no licence questions), fast on a 68020, no runtime under it, and it
reuses UP-Term's own pieces (markdown rendering from the highlight/markdown work, line editing
and history in the window, vsh for running commands).

Pieces (ledger A2.1-A2.4):
1. Transport: HTTPS to api.anthropic.com via AmiSSL (OS 3.x, 68020+) on bsdsocket.library
   (Roadshow, Miami, AmiTCP). TLS handshake cost on a 68030 to be measured (seconds expected).
2. API: the Messages API with streaming (server-sent events), JSON parsing in C (small, robust;
   the stream's deltas are small), API key from ENV: or a file with restricted access, never
   echoed or logged.
3. UI: streamed text in an UP-Term window, markdown rendered as it arrives, the line editor and
   history UP-Term already has.
4. Agentic tools as Claude Code has them: read / write / edit files, list / grep, run a command
   through vsh -- each tool call shown and confirmed (a permission prompt), AmigaOS paths
   (volumes, assigns) handled.
5. Rig check through FS-UAE's bsdsocket emulation (AmiSSL installed on the rig disk), then the
   owner's A1200 / 68030 / 68060.

Open questions for the A2 plan:
- AmiSSL version on OS 3.x and its API (AmiSSL 5 exposes OpenSSL 3.x through amisslmaster).
- Proxy-free direct HTTPS from the owner's network; certificate store on the Amiga.
- Model choice and token limits for a 68030 (long answers stream fine; the client holds the
  conversation in RAM).
- Tool-use loop details from the Messages API documentation (tool_use / tool_result blocks).

## Sources

- https://aminet.net/package/dev/lang/NodeAmiga (readme, read 2026-10-04)
- research/2026-10-04_python-on-amigaos.md (CPython, MicroPython, AmigaPython, OS4 CPython 3.12)
- ledger A1/A2 in thoughts/shared/plans/2026-09-28-vtcon.md
