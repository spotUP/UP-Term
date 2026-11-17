---
topic: bebboget 1.11 "could not verify certificate for Sectigo Public Server Authentication CA DV E36" root cause and fix
date: 2026-11-12
session: up-term-ba s74
status: cause proven on host and fixed in the copy; m68k binary built; rig run NOT done (budget); a second, separate RSA failure on the redirect host found and not hunted
---

# bebboget intermediate certificate hunt

## Cause
Not a missing algorithm and not root-file naming. P-384 and SHA-384 both exist; the P-384 **field reduction is wrong**:
`src/biginteger.cpp:332 fastSecp384Mod()` implements the NIST/Solinas P-384 reduction
(FIPS 186-4 D.2.4, Guide to ECC alg. 2.30: T + 2S1 + S2 + S3 + S4 + S5 + S6 - D1 - D2 - D3) but leaves out the
whole **S5 = (0,0,0,0,A23,A22,A21,A20,0,0,0,0)** term: words 4..7 lack +c[20], +c[21], +c[22], +c[23].
Every mod-P step on secp384r1 (`operator&` -> `BigInteger::modSecP`, biginteger.cpp:391, P.usedLen == 12 branch)
is wrong, so every ECDSA P-384 signature fails (`SecpR1::verify`, secpXXXr1.cpp:302). P-256 uses fastSecp256Mod and works,
which is why the leaf (P-256 key) passed and DV E36 (signed by Root E46's P-384 key) failed.

Why the root-load path is irrelevant: github.com sends [leaf, DV E36, Root E46 cross-signed by USERTrust ECC].
DV E36 is verified with the key of cert 2 that the SERVER sent (pkcs6.cpp:500-502 lastec = ec of the previous cert),
not with any installed file. The root lookup (certstuff.cpp:202) is for cert 2's issuer
(USERTrust ECC Certification Authority, ...), and installcerts names files with the same makeCertName(readOids(owner)),
so the names match. (No "attempt to load" line with -v: logme L_INFO level, not chased.)

## Evidence (host, clang, arm64; harness runs Pkcs6::verifyCertificates on the live chains)
- P-384 constants p/n/Gx/Gy in secpXXXr1.cpp:116-129 checked byte-for-byte against the standard: correct.
- Raw SecpR1::verify(sha384(tbs of DV E36), r, s, Root E46 pubkey) from openssl-extracted values: before fix 0, after fix 1.
- github.com chain [leaf, DV E36, Root E46 x USERTrust] + USERTrust ECC root from dist/net/bebboget-1.11/cacert.pem:
  before: 0 ok, 1 FAIL, 2 FAIL; after: all 4 sig=1.
- letsencrypt.org chain (YE2 -> Root YE -> ISRG Root X2 x X1 -> X1, mixes P-384 ECDSA and RSA-4096): all sig=1 after fix.

## Fix (copy only; diff: scratchpad/bebbofix-p384.diff)
src/biginteger.cpp fastSecp384Mod():
    t[4]: + c[20]     t[5]: + c[21]     t[6]: + c[22]     t[7]: + c[23]
(plus a comment naming the formula). Combined with the earlier notAfter/rc fix (bebbofix.diff).

## Build
Same as bebboget-notafter-hunt.md "Build":
    cd scratchpad/bebbofix/bebboget-1.11 && PATH=$HOME/opt/amiga/bin:$PATH make Release=1 TESTS= \
      CC="m68k-amigaos-gcc -include scratchpad/bebbocompat/compat.h" Release/bebboget
    m68k-amigaos-strip -o scratchpad/bebboget.fixed2 Release/bebboget      -> 155804 bytes, build rc 0
Binary: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebboget.fixed2

## Rig result
NOT RUN (tool-call budget reached). Next step: rig >= 91, --os 32 --fresh, purpose
"up-term-ba s74: bebboget intermediate certificate test"; installcerts cacert.pem; then
bebboget.fixed2 -O -D RAM: -o t.txt https://github.com/spotUP/vtcon/releases/latest/download/upkg-index.txt.

## Second defect, redirect host (found on host, not hunted)
release-assets.githubusercontent.com today serves *.github.io <- Let's Encrypt YR1 <- ISRG Root YR (cross-signed by
ISRG Root X1), all RSA (2048/2048/4096), sha256WithRSA. Host harness with ISRG Root X1 appended:
0 ok, 1 ok, **2 FAIL** (Root YR x X1), 3 ok. Same X1 key verifies "ISRG Root X2 x X1" in the letsencrypt.org chain and its
own self-signature, so it is something specific to that cross-cert (RSA path pkcs6.cpp:455-497: doRSA/decodeRSA/
SIGNATUREHASH_PATH), unproven. Expect the github download to get past github.com and then fail on the redirect
until this is fixed. Host harness to reproduce: scratchpad/hosttest (t.cpp, build.sh, certs/r*.der, x1.der).
