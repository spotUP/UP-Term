---
date: 2026-11-12
topic: bebboget 1.11, redirect host release-assets.githubusercontent.com
status: cause narrowed, not fixed
---

# bebboget redirect host hunt (RSA cross-cert / RSA-PSS)

## Verdict
The premise was wrong: the "Root YR x ISRG Root X1" cross-cert does NOT fail bebboget's RSA chain check.
The real failure on the Amiga is earlier, in the TLS 1.3 handshake: the server's CertificateVerify
(rsa_pss_rsae_sha256) is rejected, `src/ssl3client.cpp:674-680` returns -22, and `ssl3client.cpp:557-559`
prints "certificate not verified". The chain check (`certstuff.cpp:checkCertificates`) is never reached
for that host. No source change made in this hunt; no `bebboget.fixed3` built (nothing to fix yet).

## Evidence
Host (clang arm64, harness scratchpad/hosttest, sources = scratchpad/bebbofix):
- Live chain fetched again today with openssl s_client: 3 certs, byte-identical to hosttest/certs/r0..r2.der.
  All three sha256WithRSAEncryption (r2 = Root YR, issuer ISRG Root X1, notBefore 2026-05-13). No PSS, no SHA-384/512.
- ISRG Root X1 in dist/net/bebboget-1.11/cacert.pem: SHA1 CA:BD:2A:79... = hosttest/certs/x1.der.
- `./t r0 r1 r2 x1` (and with the cacert.pem X1): 0..3 all sig=1. Same with the pre-P-384-fix biginteger.
- Negative control: one signature bit of r2 flipped -> 2 sig=0. So the harness does test the signature.
- The earlier agent's "2 FAIL" is not reproducible. Note: hosttest/build.sh cannot compile src/sha256.cpp
  on macOS (mach-o `section` attribute, line 42); obj/sha256.o must come from hosttest/sha256h.cpp. A stale or
  mismatched obj/ is the likely source of the old FAIL (unproven).
- Server choice (openssl s_client): TLSv1.3, TLS_AES_128_GCM_SHA256, peer signature rsa_pss_rsae_sha256.
- PSS primitive on host: hosttest/t3.cpp = Pkcs6::doRSA + MessageDigest::emsaPssVerify (copy of the emBits logic
  of ssl3client.cpp:647-660) on openssl-made PSS signatures (2048-bit key, salt = digest length):
  sha256 ok=1, sha384 ok=1, sha512 ok=1.

Rig 92 (--os 32 --fresh, purpose "up-term-ba s74: bebboget full download test"), bebboget.fixed2, certs installed
from cacert.pem (ISRG_Root_X1 and USERTrust_ECC present in SYS:Prefs/Env-Archive/certs):
    bebboget <NIL: -v4 -O -D RAM: -o t.txt https://github.com/spotUP/vtcon/releases/latest/download/upkg-index.txt
    connecting to github.com on port 443 / using TSL1.3 TLS_CHACHA20_POLY1305_SHA256
    redirecting to: https://github.com/spotUP/vtcon/releases/download/v1.1/upkg-index.txt
    connecting to github.com on port 443 / using TSL1.3 TLS_CHACHA20_POLY1305_SHA256
    redirecting to: https://release-assets.githubusercontent.com/github-production-release-asset/...
    connecting to release-assets.githubusercontent.com on port 443
    [ERROR] certificate not verified
    error -22                     rc 10, RAM:t.txt not created, no certificate prompt shown
So the P-384 and notAfter fixes hold on the Amiga (github.com passes twice); the next failure is this one.

## Where the cause must be (narrowed, unproven)
The PSS code is correct on the host, so the fault is in what differs on m68k. The m68k Release build links the
assembler big-number code (Makefile:32 `fastmath-mul32.asm ... fastmath-all.asm`) where the host uses
src/fastmath.cpp. Before this run the Amiga never did an RSA operation in our tests (the github.com chain is all
ECDSA), so `Pkcs6::doRSA` -> `FastMath32::oddModPow` on m68k is the prime suspect; it would also break every RSA
chain check (pkcs6.cpp:455-497) once the handshake got that far. Other candidates: big-endian handling in
`FastMath32::byte2Int/int2Byte`, `-mregparm=3` against the asm calling convention.

Attempted proof: m68k builds of the harnesses (hosttest/t.m68k, hosttest/t3.m68k, linked against
bebbofix/bebboget-1.11/Release/libbebboget.a, flags -mregparm=3 -Os -fomit-frame-pointer -m68020 -noixemul).
Running t3.m68k on rig 92 hung the Amiga (no wasabid reply for 25 s). Not separated: harness startup problem
(libbebboget may need bebboget's own startup / c++support init or a bigger stack) versus a hang in m68k modpow.

## Next step (one agent, small)
1. Make t3.m68k run: copy bebboget.cpp's startup bits (stack cookie, any init it calls) or add `STACK 200000`
   first; then run `t3.m68k c.der m.bin s_sha256.bin 4` and `t.m68k r0 r1 r2 x1`. Files are in hosttest/pss and
   hosttest/certs.
2. If ok=0 there: build the same harness for m68k with src/fastmath.cpp instead of the asm objects. ok=1 then
   proves the asm modpow; fix there (or link fastmath.cpp in the Amiga build as the smallest fix, at a speed cost).
Estimated size if it is the asm: small to medium (one routine); if it is fastmath.cpp endianness: small.

## Build (unchanged, for reference)
    cd scratchpad/bebbofix/bebboget-1.11 && PATH=$HOME/opt/amiga/bin:$PATH make Release=1 TESTS= \
      CC="m68k-amigaos-gcc -include scratchpad/bebbocompat/compat.h" Release/bebboget
    m68k-amigaos-strip -o scratchpad/bebboget.fixed2 Release/bebboget

## Combined diff
thoughts/shared/s74-evidence/bebboget-all-fixes.diff: pristine 1.11 tarball vs scratchpad/bebbofix, 3 files:
bebboget.cpp (notAfter/rc), pkcs6.cpp (notAfter), biginteger.cpp (P-384 reduction). No RSA change in it.

## Cleanup
Rig 92 stopped; build/rig92 removed by rig.py, build/rig-saved/92 (1.8 MB) deleted. Scratch added by this hunt
is under 2 MB (hosttest/pss, t3*, *.m68k, live/); unpacked pristine tree deleted.
