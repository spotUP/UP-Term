# Draft: bug report for bebboget (not sent)

Status 2026-11-12: draft. Owner chose "Post public issue as spotUP". Upstream has no GitHub repository
(`bebbo/bebboget`, `bebbo/bebbossh` return 404 there); the tracker is `https://franke.ms/git/bebbo/bebboget/issues`
and needs an account on that host, which this session does not have. Post only after the end-to-end rig test
passes (`bootstrap-end-to-end.md`). Attach `bebboget-all-fixes-v2.diff` (against commit 9b223f1; `src/md.cpp` has
CRLF line ends). Check every statement below against section "Hunt 5" of `bebboget-fastmath-hunt.md` and the
earlier hunt files before sending.

---

Title: bebboget 1.11 / 9b223f1: certificate check fails against github.com release downloads on m68k (fixes attached)

bebboget 1.11 (Aminet) and git head 9b223f1 cannot download a GitHub release asset with the certificate check on.
Tested on AmigaOS 3.2, 68020+ build, FS-UAE, root certificates installed with installcerts from curl.se
cacert.pem. URL shape: `https://github.com/<owner>/<repo>/releases/latest/download/<file>` (redirects to
release-assets.githubusercontent.com, TLS 1.3). With the attached patch the 34 MB download completes in 18 s
with the check on and `lha t` passes.

Causes found, each with its fix in the patch:

1. `src/pkcs6.cpp`: the notAfter date of a certificate is read wrongly, so valid certificates are rejected.
2. `src/biginteger.cpp`: one term (S5) of the P-384 fast reduction is wrong; an intermediate certificate signed
   with P-384 fails.
3. `src/md.cpp`, `emsaPssVerify`: `uint8_t h2[l]` is a variable-length array at the stack pointer. m68k gcc
   6.5.0b inlines the following memcmp and reads the array with `move.b (sp)+,d1` (opcode 121f). On m68k a byte
   pop from the stack pointer steps by 2, so every second byte is compared and the pointer runs off. The
   RSA-PSS signature of the TLS 1.3 handshake then fails. Fix in the patch: `uint8_t h2[64]`. This is a compiler
   bug as well; it may hit other variable-length byte arrays.
4. `src/fastmath.cpp`, `oddModPow` and the fill path: buffers from malloc are used without being cleared. Fix:
   memset after allocation.
5. `src/bebboget.cpp`: the HTTP status line is never parsed, so a 404 page is saved as the file, and the program
   returns 0 on every failure. With the patch a status of 400 or above prints `error: HTTP status N`, and failed
   certificate checks, a wrong host and HTTP errors return 10, so scripts can test the result.

Not tested: the 68000 build (bebboget00), JIT off, the host test suite.
