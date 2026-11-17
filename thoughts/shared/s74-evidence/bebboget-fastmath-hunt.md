---
date: 2026-11-12
topic: bebboget hunt 4 - bebbossh 1.46.1, upstream head, fastmath suspect
status: FAIL, asm cleared, two new leads (flaky RSA result on the JIT rig; -22 despite h == h2)
---

# bebboget hunt 4 (fastmath suspect)

S = /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad

## Step 1: what is new upstream
- Aminet `bebbossh_1.46.1.lha` (in `$S/bebbossh-1.46.1/`, extracted to `x/`): a THIRD-PARTY V4SA build of
  bebbossh ("fixes by Matthias Muench"; 1.45 sources + patches 0001-0019 in `src/`, 1.46 key-exchange merge).
  No bebboget binary, no bebboget source. Only mention: readme.orig "Also get bebboget". None of its patches
  touch pkcs6/biginteger/fastmath/ssl3 (only 0008-random.patch matches the grep, unrelated). Not useful.
- Upstream bebbossh `https://franke.ms/git/bebbo/bebbossh` head 4c0b772 (2026-06-01, "add normalizeSharedSecret"):
  no bebboget in it. Shares fastmath*.asm / fastmath.cpp with bebboget (fastmath.cpp differs only in comments
  and a dropped `#include <math.h>`).
- Upstream bebboget `https://franke.ms/git/bebbo/bebboget` (clone in `$S/upstream-head/bg`): head
  9b223f1 (2025-12-05 "syn with bebbossh"), one commit after b18e500 "Release 1.11". 18 files, +173/-107,
  mostly comments, x25519.cpp, `#ifdef __AMIGA__` in sha256.cpp. pkcs6.cpp, biginteger.cpp, ssl3client.cpp,
  bebboget.cpp unchanged: NONE of the three fixes is upstream; nothing on RSA-PSS or fastmath.
  => No newer upstream binary to test. Used upstream commit 9b223f1 as the base for step 2.

## Step 2: builds (all from 9b223f1 + bebboget-all-fixes.diff, which applies cleanly)
Tree: `$S/bf4/pristine` (9b223f1), `$S/bf4/bebboget` (+fixes), `$S/bf4/bgC` (+C fastmath), `$S/bf4/bgD` (+C fastmath + H4 dumps).
Build: `mkdir -p Release` first (git tree has no Release/), then
    PATH=$HOME/opt/amiga/bin:$PATH make Release=1 TESTS= CC="m68k-amigaos-gcc -include $S/bebbocompat/compat.h" Release/bebboget
- `$S/bebboget.h4asm`  head + fixes, asm fastmath (upstream build)
- `$S/bebboget.h4c`    head + fixes, C fastmath: Makefile:32 without fastmath-mul32.asm/fastmath-all.asm, fastmath.cpp:271 `#if 1`
- `$S/bebboget.h4dbg`  h4c + dumps in ssl3client.cpp verifySignature (message, signa, n, e, signedContent) and md.cpp emsaPssVerify (j, data[j], mHash, h, h2)
- `$S/bebboget.h4O0`   h4dbg with fastmath.o rebuilt at -O0 (Makefile compiles fastmath.cpp at -O3)
- `$S/bebboget.h4dbg2` h4dbg + dumps in fastmath.cpp oddModPow around the first xmod (z, mod, buf0 before/after)
Note: the asm only replaces add/sub/mul/square; oddModPow, xmod, montgomery are C++ on both builds.

## Rig 94 (--max --os 32, JIT on: `jit_compiler = 1` in the rig's fs-uae config), certs from cacert.pem
installcerts via `ami.py exec 'RAM:installcerts RAM:cacert.pem'` (note: `;` in an exec line is an AmigaDOS
comment, run one command per exec). ISRG_Root_X1 present in ENVARC:certs, date 12-Nov-26.
Command for every run: `RAM:<bin> <NIL: -v4 -O -D RAM: -o t.txt https://github.com/spotUP/vtcon/releases/latest/download/upkg-index.txt`
Checker: `python3 -I $S/h4tools/check.py <log>` recomputes pow(signa,e,n) and the full PSS-SHA256 check in Python.

| time  | binary  | RSA result vs Python | PSS in bebboget | outcome |
|-------|---------|----------------------|-----------------|---------|
| 07:28 | h4c     | (no dump)            | -               | [ERROR] certificate not verified, error -22 |
| 07:30 | h4dbg   | WRONG (255/256 bytes differ, last 0x46) | data[j]=0x53, fails | -22 |
| 07:31 | h4O0    | WRONG (256 differ, last 0x5a)           | data[j]=0xDE, fails | -22 |
| 07:31 | h4dbg2  | correct; xmod result correct            | data[j]=01, h == h2 (dumped, identical) | STILL -22 |
| 07:32 | h4dbg2 x2, h4dbg x2 | correct             | data[j]=01, h == h2 | STILL -22 |
| 07:32 | h4asm   | (no dump)            | -               | -22 |
In every wrong case Python's PSS check on the SAME dumped message/signature passes (mHash matches too).
Host: `$S/h4tools/rsa f1 f2 p1` (host Pkcs6::doRSA on the rig-captured inputs, objects from $S/hosttest/obj):
    f1: host last=bc amiga last=46 same=0
    f2: host last=bc amiga last=5a same=0
    p1: host last=bc amiga last=bc same=1
So the host computes the correct value for the very inputs the Amiga got wrong.

## Conclusions
1. The m68k assembler fastmath is NOT the (only) cause: the pure C build fails the same way.
2. Lead A (flaky arithmetic): the same h4dbg binary gave a wrong RSA result at 07:30 and a correct one at 07:32
   (different signatures each handshake). Host code is correct on the failing inputs. Run-dependent wrong
   big-number results on an FS-UAE JIT machine point to the JIT (64-bit mulu.l/divs.l in xmod/montgomery)
   or uninitialised memory. Unproven. Hunt 3's "always fails" runs were all on JIT rigs.
3. Lead B (decisive, still blocking): even with a correct RSA result, data[j]==1 and dumped h == h2 byte for
   byte, verifySignature still returns -22 (ssl3client.cpp:680, the only -22 source; message at :559).
   `int r = memcmp(h, h2, l) == 0` in md.cpp emsaPssVerify must then be returning nonzero, or the return value
   is lost between emsaPssVerify and `ok`. Suspects: libnix memcmp called with a variable length (VLA `l`)
   under `-mregparm=3` (calling-convention mismatch with the library), or the virtual call return path.
   The ECDSA path for github.com does not hit this code.

## Exact next step (one small agent)
1. Lead B first: in bgD/src/md.cpp replace `memcmp(h, h2, l) == 0` with an explicit byte loop, and log
   `r` plus the return value at ssl3client.cpp `ok`. Rebuild, run once on a rig. If it then passes the handshake,
   check `m68k-amigaos-objdump -d Release/md.o` for the memcmp call (regparm vs stack args) and fix there
   (or use the project's own equals.cpp helper). Also check every other memcmp/strcmp call with runtime length.
2. Lead A: start the rig WITHOUT JIT (`rig.py start --a1200 --os 32`, or the --max machine with jit off) and run
   h4dbg 5 times; count wrong RSA results with check.py. Zero wrong without JIT and some with JIT = FS-UAE JIT,
   not bebboget (real hardware unaffected; test with the JIT off).
3. Then the full up-term.lha download + `lha t` (34 MB).

## Source changes
None worth keeping yet (only debug dumps and the C-fastmath switch, which proved nothing). No
bebboget-all-fixes-v2.diff written: the fix set is still bebboget-all-fixes.diff, which applies cleanly to 9b223f1.
No `$S/bebboget.pass`.

## Cleanup
Rig 94 stopped and removed; its leftover wasabi debug process killed; build/rig-saved/94 (1.8 MB) deleted.
No up-term.lha downloaded. Left (all small): $S/bf4 5.7 MB, $S/upstream-head 3.0 MB, $S/bebbossh-1.46.1 1.2 MB,
$S/h4tools 0.2 MB, $S/bebboget.h4* (about 160 KB each), logs $S/h4*.log, $S/rep-*.log.

## Hunt 5 (2026-11-12): PASS

Verdict: PASS. bebboget downloads `https://github.com/spotUP/vtcon/releases/latest/download/up-term.lha`
on rig 95 (`--max --os 32`, JIT on) with certificate checking on (no `--sloppy`), exit code 0,
and `lha t` returns 0. Wrong-URL and failed-check cases return 10.

Hunt 4's lead texts were wrong on both leads: the memcmp failure is not a libnix memcmp or
regparm call problem (memcmp is inlined, there is no call), and the intermittent failure is
neither the FS-UAE JIT nor the stack size.

Files:
- Combined diff of ALL fixes against pristine upstream 9b223f1 (applies with `patch -p1`, dry run clean):
  `thoughts/shared/s74-evidence/bebboget-all-fixes-v2.diff` (5 files: bebboget.cpp, biginteger.cpp,
  fastmath.cpp, md.cpp, pkcs6.cpp; md.cpp is a CRLF file, keep the line endings).
- Passing binary (unstripped, 183080 bytes,
  sha256 2d50c924f5493ed2a68c29655de24f3c228fba9eb4c75741dd0d2a5312b6d57a):
  `/private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebboget.pass`
- Build tree: `<scratchpad>/bf5/bebboget`. Build command:
  `PATH=$HOME/opt/amiga/bin:$PATH make Release=1 TESTS= CC="m68k-amigaos-gcc -include <scratchpad>/bebbocompat/compat.h" Release/bebboget`
  (compiler `m68k-amigaos-gcc (GCC) 6.5.0b 20260819091705`, flags `-mregparm=3 -Os -fomit-frame-pointer`).

### Cause 1: m68k gcc 6.5.0b miscompiles the inlined memcmp in `MessageDigest::emsaPssVerify` (src/md.cpp)

`uint8_t h2[l]` is the newest variable-length array, so it sits at sp. gcc inlines
`memcmp(h, h2, l)` and uses sp itself as the pointer. Disassembly of the old `Release/md.o`
(`m68k-amigaos-objdump -d`, function `__ZN13MessageDigest13emsaPssVerifyEPKhiiiPhi`):

```
 650: moveal a5@(-8),a0
 662: moveb a0@+,d0
 666: moveb sp@+,d1      (opcode 121f)
 668: cmpb d0,d1
```

On m68k a byte post-increment on a7 steps by 2 (the stack pointer stays even), so h[i] is
compared with h2[2*i]. The compare fails although h == h2: `[ERROR] certificate not verified`,
`error -22`. The same instruction is in hunt 4's debug build md.o at 0x704. A scan of all
objects for a byte `sp@+` found only this one in code.

Fix: `uint8_t h2[64];` (largest digest is SHA-512) instead of the variable-length array.
New code: `lea a5@(-64),a0 ... moveb a1@+,d0 ; moveb a0@+,d1 ; cmpb d0,d1`.
The faulty layer is the compiler; the source change avoids the bad code pattern.

### Cause 2: uninitialised memory in `FastMath32::oddModPow` / `fill` (src/fastmath.cpp)

`mstl::vector` allocates with `malloc` (src/vector.cpp:43) and does not clear. `Pkcs6::doRSA`
uses `mlen = (mod.size() >> 2) + 1`, so a modulus with a leading 0 byte has a top limb of 0
and `muLen = modLen - 1`. `square()` / `mul()` write 2*muLen words, but
`montgomery(t, mod, modLen, n0)` reads 2*modLen words. The words t[2*muLen] and t[2*muLen+1]
are garbage in `uinta buf1(maxLen)` (oddModPow) and in `uinta x(modLen + modLen + 1)` (fill).
The host malloc returns zeroed fresh pages, so the host build is correct.

Rig proof, binary with fix 1 only, command for each run:
`RAM:bg5 <NIL: -v4 -O -D RAM: -o t.txt https://github.com/spotUP/vtcon/releases/latest/download/upkg-index.txt`
- JIT on, 6 runs: strict alternation. Runs 1, 3, 5: `[ERROR] certificate not verified`, `error -22`
  (used stack 3080/3028). Runs 2, 4, 6: handshake passes, then
  `could not verify certificate for: Owner: Root YR, ISRG, US / Issuer: ISRG Root X1 ...`,
  `(a)bort, (i)nstall or (c)continue (a/i/c): error: certificate not accepted` (used stack 4572).
  All `returncode 10`.
- JIT on with `Stack 400000` (default is 16384 bytes): same alternation, 6/6 fail. Stack ruled out.
- JIT off (`rig.py reboot --no-jit`, `jit_compiler = 0`): same alternation, 6/6 fail. JIT ruled out.

Fix: `memset(x.begin(), 0, x.size() * sizeof(uint32_t));` after the allocation of x in `fill()`,
and `memset(buf1.begin(), 0, buf1.size() * sizeof(uint32_t));` after the allocation of buf1 in
`oddModPow`. montgomery() leaves the upper half 0 afterwards, and buf0 is memset before xmod,
so both buffers stay clean.

After fix 1+2 (JIT off): 4/4 runs pass, `RAM:t.txt` 846 bytes, used stack 4548,
0 "not verified", 0 "could not verify".

### Cause 3: HTTP error status ignored (src/bebboget.cpp)

`https://github.com/spotUP/vtcon/releases/latest/download/no-such-file.lha` gave `rc 0` and saved
the 404 page of GitHub as `RAM:n2.txt` (225854 bytes). The status line was never parsed.

Fix: in the header loop, a line that starts with `HTTP/` and has a status >= 400 prints
`error: HTTP status <n>`, sets `exitCode = 10` and returns. After the fix:
`error: HTTP status 404`, `RAM:bebboget failed returncode 10`, no n2.txt created.
Note: this changes behaviour for every 4xx/5xx answer.

### Final run (fresh rig 95 start, JIT on, binary with fix 1+2+3 as `RAM:bebboget`)

| Case | Command / condition | Result |
|---|---|---|
| No certificates installed | small-file URL | `(a)bort, (i)nstall or (c)continue (a/i/c): error: certificate not accepted`, returncode 10 |
| Wrong host | `https://no-such-host.invalid/x.lha` (after `RAM:installcerts RAM:cacert.pem`) | `error -996`, returncode 10 |
| Wrong URL | `.../no-such-file.lha` | `error: HTTP status 404`, returncode 10 |
| Small file | `.../upkg-index.txt` | rc 0 |
| Full download | `RAM:bebboget <NIL: -O -D RAM: -o up-term.lha https://github.com/spotUP/vtcon/releases/latest/download/up-term.lha` | rc 0, 18 s host wall time, 34083462 bytes (server content-length 34083462), used stack 4548 |
| Archive test | `RAM:lha t RAM:up-term.lha` (LhA 2.15, the rig has no lha) | rc 0, `1552 files tested, all files OK.`, `Operation successful.` |

Certificates came from `vtcon/dist/net/bebboget-1.11/` (`installcerts`, `cacert.pem`), read only.

### Not verified

- The full 34 MB download ran once, with JIT on only. It was not repeated and not run with JIT off.
- `bebboget.o 3f6: orb d2,sp@+` in the object scan was not examined (likely data, not code).
- Host test suites were not run (another session runs tests).
- Nothing is committed and no tracked file is changed; the fixes exist only in the diff and the scratchpad tree.

### Next step

1. Apply `bebboget-all-fixes-v2.diff` to the bebboget source used for `vtcon/dist/net/`, rebuild, strip if a smaller binary is wanted.
2. Optional: repeat the full download a few times, and once with JIT off.
3. Report the three bugs upstream to Bebbo, including the gcc 6.5.0b `move.b (sp)+` code generation bug.
