---
date: 2026-11-13
topic: LHa for UNIX emits a match into the pre-file space preset; 7-Zip rejects it
status: root cause found, encoder patch built and proved
---

# LHa for UNIX encoder: match into the pre-file space preset

## Answer

Valid LHA by de facto convention, but a real encoder defect for portability.
The encoder deliberately presets its window to spaces (`memset(text, ' ', TXTSIZ)`),
and one search path can start a match up to `off` bytes *before* the first
input byte, into that preset. LHA-family decoders (LHa for UNIX, xadmaster LhA
client) preset their window to spaces too, so they decode it correctly. Decoders
that start from an empty window reject it (7-Zip: "Data Error") or silently
produce wrong bytes (XADMaster / The Unarchiver: zeros, then CRC error).
It is not stale or uninitialised memory: the bytes come from the defined space preset.

Fix: a 9-line patch to `src/slide.c` (below). Built, proved on ixkill at
-lh5-/-lh6-/-lh7-. Replacing the packer is not needed.

## Evidence

Input: `build/dist/up-term/Files/ixkill`, 8052 bytes, md5 493a04fff54808c6e3b4252c7c80b671.
File starts `00 00 03 F3`; bytes 6639..6642 are `20 00 00 03`.

Logging decoder (`scratchpad/lharoot/tools/lhdec.py`) on the Homebrew archive, all three methods:

```
pos=6639 dist=6640 len=4 src=-1 pre-start-bytes=1 produced=20000003
-lh5- lvl 2 orig 8052 out 8052 pre-start matches 1
```

One match, distance 6640 at output position 6639: source starts 1 byte before
the file. That byte is the preset `0x20`, followed by file bytes 0..2 (`00 00 03`).
Output with a space preset matches ixkill exactly.

Unpatched upstream HEAD (1.14i-ac20260723) produces a byte-identical archive to
Homebrew 1.14i-ac20220213, so the bug is still upstream.

### Encoder cause (upstream `src/slide.c`)

- `encode()`: `memset(text, ' ', TXTSIZ);` then input is read to `&text[dicsiz]`, `pos = dicsiz`.
  Only positions >= dicsiz are ever inserted into the hash chains.
- `search_dict()`: when a token's chain is too long (`hash[tok].too_flag`, chain >= LIMIT 0x100),
  it searches the chain of the token `off` bytes further on (`tok = NEXT_HASH(tok, pos+off)`).
- `search_dict_1()`: `scan_beg = scan_pos - off;` and the only lower bound is
  `scan_end = pos - dicsiz` (window size). With `scan_pos == dicsiz` (file start)
  and `off >= 1`, `scan_beg` = dicsiz-1, i.e. the preset space.
  Here the long chain is the `20 00 00` / `00 00 03` tokens of the Amiga hunk
  relocation table (`00 00 03 xx` repeated), which sets `too_flag`.
- There is no check against the start of input. The window bound is correct
  after the first slide (`update_dict`), because then text[0..] holds real data.
  The bug exists only within the first `2*dicsiz` bytes of a file, needs a
  too-long chain, and needs input that matches `' '` + the file's first bytes.
  That is why it is rare.

### Who is right per the format

There is no formal LHA spec; the reference is the LHarc/LHA (Yoshizaki) code,
whose LZSS lineage presets the dictionary with spaces. Decoders:

| Decoder | Window preset | ixkill (Homebrew archive) |
|---|---|---|
| LHa for UNIX `slide.c` decode | `memset(dtext, ' ', dicsiz)` (0 only with `--extract-broken-archive`) | OK |
| xadmaster LhA client 1.16 (Aminet `util/arc/xad_lha`, `LhA_external.c` ~l.1962) | `memset(text, ' ', dicsiz)`, applied to all methods | OK (by source; no rig run) |
| 7-Zip `CPP/7zip/Compress/LzOutWindow.h` `CopyBlock` | none: `if (distance >= _pos) { if (!_overDict ...) return false; }` | Data Error |
| XADMaster/The Unarchiver `XADLZHStaticHandle : XADLZSSHandle` | `memset(windowbuffer, 0, ...)` in `resetByteStream` | wrong byte (00 for 20) -> CRC error (by source) |
| LhA 2.15 (Amiga, Boberg) | closed source, no docs found | unverified; very likely space preset (LHA lineage) |

So lha and xad are "right" by the de facto format; 7-Zip and XADMaster are stricter
or wrong. For our kits the point is moot: the archive must open everywhere, and
the encoder gains nothing (1 byte at most) from these matches.

## Fix: encoder patch (built and proved)

`scratchpad/lharoot/lh-prestart.patch`, against upstream jca02266/lha HEAD `16619b0`:

```diff
--- slide.c.orig
+++ slide.c
@@ -96,6 +96,8 @@
 static unsigned int txtsiz;
 static unsigned long dicsiz;
 static unsigned int remainder;
+/* text[] index of the first input byte; below it lie only the ' ' preset bytes */
+static int data_start;
 
 struct matchdata {
     int len;
@@ -170,6 +172,7 @@
     remainder += n;
 
     *pos -= dicsiz;
+    data_start -= dicsiz;
     for (i = 0; i < HSHSIZ; i++) {
         j = hash[i].pos;
         hash[i].pos = (j > dicsiz) ? j - dicsiz : NIL;
@@ -200,6 +203,13 @@
     int scan_end = pos - dicsiz;
     unsigned int len;
 
+    /* The hashed search key may lie `off' bytes into the match, so
+       scan_beg can fall before the first input byte, into the ' ' preset.
+       Decoders that start from an empty window (7-Zip, XADMaster) reject
+       or misdecode such matches, so never let a match start there. */
+    if (scan_end < data_start - 1)
+        scan_end = data_start - 1;
+
     while (scan_beg > scan_end) {
         chain++;
 
@@ -318,6 +328,7 @@
     if (match.len > remainder) match.len = remainder;
 
     pos = dicsiz;
+    data_start = dicsiz;
     token = INIT_HASH(pos);
     insert_hash(token, pos);     /* associate token and pos */
 
```

The chain is in descending position order and the loop already stops at the
first out-of-range entry, so raising `scan_end` drops exactly the illegal candidates.
After the first slide `data_start` <= 0 and `pos - dicsiz` dominates, so behaviour
there is unchanged. Nit: `data_start -= (int)dicsiz;` would make the cast explicit.

Proof (patched binary, ixkill):

```
lh5: size brew=4100 patched=4099; 7zz: Everything is Ok; lha t: ixkill - Tested
lh6: size brew=4100 patched=4100; 7zz: Everything is Ok; lha t: ixkill - Tested
lh7: size brew=4100 patched=4100; 7zz: Everything is Ok; lha t: ixkill - Tested
pre-start matches 0 (all three); lha-extract and 7z-extract bytes match ixkill (cmp).
```

Header code is untouched, so protection bits / level-2 headers are written exactly as
before; only the compressed stream changes.

Regression (first 400 files under `build/dist/up-term` < 2 MB, each packed alone at -lh5-
with unpatched and patched upstream, archives in scratch): `files=400 orig_7z_fail=0
patched_fail=0` (patched checked with both `7zz t` and `lha t`). The sample did not happen
to contain another trigger file; ixkill is the only known one. Upstream `make check`
was not run.

## Candidate fixes compared

1. **Patch the encoder (recommended).** 9 lines, root cause, keeps the existing
   lha pipeline, headers and protection-bit handling. Cost: we must build lha from
   source (or a Homebrew formula with the patch) instead of `brew install lha`.
   Optionally offer upstream later (owner decision; not done).
2. **Replace the packer.** No other host packer writes Amiga -lh5- with Amiga
   protection bits as directly as lha; writing one is far more code than the patch.
   Not worth it.
3. **Workaround without a patched binary:** after packing, verify with `7zz t` and,
   on failure, repack that file with another method (e.g. -lh6- vs -lh5- does not help
   here: all three fail) — so no clean workaround exists. A guard `7zz t` in the
   kit build is still worth keeping as a check after the patch.

## Recommendation

Build lha from upstream with this patch for the kit pipeline, and keep a
`7zz t` step on every produced archive as a guard. LhA 2.15 behaviour on the
patched archive is unverified (no rig run); the patched stream uses no
pre-start matches, so any LZH decoder handles it.
