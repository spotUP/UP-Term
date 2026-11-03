---
topic: UPDEMO-TYPE-KANIM evidence
rig: 93 (fresh-all install via installer_rig.py ONLY fresh-all, INSTALLER_KEEP=1; kit rebuilt from main 1cb23f27)
date: 2026-11-03
---

# Type UP-Term:share/updemo/kanim-stream.bin: does it play?

Verdict: YES, frames play. It is an endless animation loop, so there is no "end": the solid blue square seen
earlier is just the frame showing when the shot was taken.

## Measured (14 shots after Return, ~0.7 s apart; shots in build/rig93/shots/k, copies ~/Desktop/upterm-shots/rig93-kanim-*.png)
Window region x 4..796, y 22..420, background black. Each frame is one 64x64 (4096 px) solid square; bbox of
non-background pixels is the whole window every time (the Type output leaves prompt text and the blue/grey pens
around, 13060 px of 102,136,187 constant), so the square is identified by its colour count.
- Distinct region hashes over 14 shots: 8 (of 14). Square colours seen in order: red (s02,s03), green (s04..s06),
  blue (s07), yellow (s08), blue (s09), yellow (s10), blue (s11), green (s12), red (s13,s14).
- s05 and s09 show 2688 px of the square (a partial update mid-frame).
- A shot 8 s later: blue again. Never settles; it cycles red / green / blue / yellow.
- Distinct colour counts per shot: 6..8 (pre-run window 16).

## File structure (host: build/dist/up-term/Files/share/updemo/kanim-stream.bin, 65885 bytes)
Kitty graphics, APC `ESC _G ... ESC \`, 24-bit RGB 64x64, base64 chunks (m=1/m=0):
- starts `ESC[2J ESC[3;3H`, then `a=T,f=24,s=64,v=64,i=1,p=1,q=2,m=1` (frame 1, transmit + display)
- 3 x `a=f,i=1,c=1,f=24,s=64,v=64,z=100,q=2,m=1` (frames 2..4; z=100 = 100 ms gap each)
- `a=a,i=1,r=1,z=100` (set frame 1 gap to 100 ms) then `a=a,i=1,s=3` (animate: s=3 = loop, no loop count = forever)
- `ESC[12;1H`. 4 frames total (a=T count 1, a=f count 3 by grep and by the APC list), 18 APC sequences.
First 400 bytes printable: `\x1b[2J\x1b[3;3H\x1b_Ga=T,f=24,s=64,v=64,i=1,p=1,q=2,m=1;/wAA/wAA/wAA...` (base64 of 0xFF0000 red).

## Version C:UPTerm FULL
```
UPTerm 1.0 (12/09/62)
UP-Term
```
Expected a date from (03.11.26); got (12/09/62). Not what was expected; not investigated (task said do not fix).
