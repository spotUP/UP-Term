---
topic: bebboget 1.11 "certificate is not valid today" (notAfter == notBefore) root cause and fix
date: 2026-11-12
session: up-term-ba s74
status: notAfter bug fixed and rig-proven; a second, separate blocker remains (intermediate signature not verified)
---

# bebboget 1.11 notAfter hunt

## Cause (proven on rig 90, OS 3.2)
`readTimes()` decides "first time vs second time" from the struct, not from the parse position:

    src/pkcs6.cpp:315    if (!ci.validFrom) { ci.validFrom = tval; } else { ci.validTo = tval; return; }

but `checkCertificates()` runs `Pkcs6::verifyCertificates()` twice on the SAME `cis` vector:

    src/certstuff.cpp:193  Pkcs6::verifyCertificates(cis, false);   // pass 1: validFrom=notBefore, validTo=notAfter (correct)
    src/certstuff.cpp:222  Pkcs6::verifyCertificates(cis, true);    // pass 2: calls readTimes again (pkcs6.cpp:373)

On pass 2 `validFrom` is already non-zero, so the FIRST time in the cert (notBefore) lands in `validTo` and the
function returns. Result: validTo = notBefore = validFrom, and the check at certstuff.cpp:244
(`now > ci->validTo`) rejects every certificate issued before today. Not a libc/time_t/struct issue, and the
shipped binary matches the source (same behaviour reproduced).

The +05:00:00 display offset (notBefore 00:00:00Z shows 05:00:00, notAfter 23:59:59Z shows 04:59:59) is a
libnix mktime/ctime timezone quirk; it does not affect the outcome (unverified beyond display; not chased).

## Fix (diff in the copy, not under dist/)
Full diff: scratchpad bebbofix.diff (see paths below). Summary:

    src/pkcs6.cpp readTimes():
    +    int n = 0; // verifyCertificates runs more than once on the same CertificateInfo
    -        if (!ci.validFrom) {
    +        if (n++ == 0) {

    src/bebboget.cpp (rc on failure; RETURN_ERROR 10):
    +static int exitCode = 0;
     exitCode = 10 at: unsupported protocol, `error %ld` (connect), `error: certificate not accepted`,
     `unexpected connection close`, `can't open '%s' for writing`
    -    return 0;          (end of main)
    +    return exitCode;
    (HTTP status codes such as 404 are still not checked: an error page is saved with rc 0.)

## Build
Toolchain: ~/opt/amiga/bin (bebbo gcc 6.5.0b, the one vtcon's Makefile uses as AGCC). This libnix needs two
build-only shims the 1.11 source assumes (2-arg gets(), hardware/custom.h via the protos), put in a
force-included header, not in the source:

    compat.h: #include <stdio.h> / #include <hardware/custom.h> / #define gets(a,b) fgets(a, b, stdin)
    cd <copy>/bebboget-1.11 && mkdir -p Release && PATH=$HOME/opt/amiga/bin:$PATH \
      make Release=1 TESTS= CC="m68k-amigaos-gcc -include <path>/compat.h" Release/bebboget
    m68k-amigaos-strip -o bebboget.fixed Release/bebboget     (155724 bytes; shipped 138256)

## Rig result (rig 90, --os 32 --fresh, purpose "up-term-ba s74: bebboget certificate fix test", clock 12-Nov-26)
installcerts RAM:bg/cacert.pem: rc 0, certs installed incl. Sectigo_Public_Server_Authentication_Root_E46.

Shipped bebboget (baseline, reproduced):
    the certificate is not valid today:
    Owner: github.com / valid from: Fri Oct 30 05:00:00 2026 / valid to: Fri Oct 30 05:00:00 2026
    error: certificate not accepted          rc 0

Fixed bebboget, same URL (upkg-index.txt):
    leaf github.com now passes the date check (no "not valid today" any more)
    could not verify certificate for:
    Owner : Sectigo Public Server Authentication CA DV E36, Sectigo Limited, GB
    Issuer: Sectigo Public Server Authentication Root E46, Sectigo Limited, GB
    valid from: Mon Mar 22 05:00:00 2021 / valid to: Sat Mar 22 04:59:59 2036    <- dates now correct
    error: certificate not accepted          rc 10                               <- rc now non-zero
Fixed bebboget against https://expired.badssl.com/: `error -990`, rc 10.

Verdict: notAfter fix PASS, rc fix PASS, download still FAIL on a SECOND defect: the intermediate's signature
(ECDSA by Sectigo Root E46, a P-384 key, ecdsa-with-SHA384) is not verified although the root .cer is installed
in ENVARC:certs. Not hunted (budget). Leads: checkCertificates() root-load path certstuff.cpp:202-219 (with -v no
"attempt to load certificate" line was printed: check which flag sets L_INFO), and P-384/SHA-384 ECDSA support
in pkcs6.cpp verify + secpXXXr1.cpp. The shipped binary never reached this check because the leaf failed first.

## Files
- fixed binary (stripped): /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebboget.fixed
- unstripped: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebboget.fixed.unstripped
- patched source copy: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebbofix/bebboget-1.11 (1.4 MB)
- diff: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebbofix.diff
- compat header: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/bebbocompat/compat.h
- patch script: /private/tmp/claude-501/-Users-spot-Code-UP-Term/e384575c-7007-454b-9b66-cd8997e246e4/scratchpad/patch.py
- rig 90 stopped and removed; harness auto-saved its writes to build/rig-saved/90/20261112-024331 (1.4 MB, left)
