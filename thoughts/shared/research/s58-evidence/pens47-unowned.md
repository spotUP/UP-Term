# S57-PENS47-UNOWNED: pens 4-7 written without being obtained

Branch s58-pens47. Host tests only; no rig.

## Violated invariant
A pen Amarchy writes (SetRGB32) must be a pen Amarchy owns (ObtainPen exclusive, released with the rest). Pens 32-47 (icon pens)
obey it through the ipd holder (iconpen_dos.c pen_obtain/pen_release via ip_holder_obtain/ip_holder_release). The live role pens
4-7 (desk_live_role_pens: the pens no palette.prefs entry reaches on a screen deeper than 3 planes) did not:

- amarchy/iconpen_core.c ip_sync (OBTAIN branch, was :235): `o->set_rgb(lp[k], lc[k])` for desk_live_follow's pens, never obtained.
- amarchy/iconpen_dos.c:72 pen_set_rgb (the set_rgb op) is the SetRGB32 of that write.
- amarchy/desk_dos.c:626 dk_live_pens: SetRGB32 on the same pens for theme apply / restore, never obtained.
Line numbers in the ledger row matched (+-2).
Consequence: ObtainBestPen from another client may be handed 4-7; the next sync repainted them.

## Fix (one mechanism, the existing holder)
- ip_holder gains rcount/rpen/rhave/rrgb. ip_sync obtains each role pen through the same ip_ops obtain (PENF_EXCLUSIVE in
  pen_obtain) before writing, and writes only pens it got. ip_holder_release releases them with the icon pens (screen close,
  quit, Workbench reopen, last owner gone all go through it). ip_holder_verify checks them against the ColorMap like 32-47.
- ip_recolour (theme switch, ipd_recolour) recolours held role pens too.
- ip_role_claim / ipd_role_write: dk_live_pens writes 4-7 through the holder (only pens it owns; -1 = no holder runs = old direct write).
- Depth: desk_live_role_pens already returns 0 pens at 3 planes or fewer, so those screens obtain and write nothing (unchanged).
  At 4-5 planes pens 4-7 are role pens too and are now held (icon pens there are 8-15).

## Test
tests/test_iconpen.c test_pens47_owned (make test ONLY=iconpen).

## Runs (nice 19, host, ASan/UBSan)
- Before (ip_sync obtain reverted to bare set_rgb, rhave=0; same new test + updated recolour expectation 20): `[FAIL] iconpen: 2189 checks, 17 failed` (build/s58-before.log).
- After: `[OK] iconpen: 2189 checks, 0 failed` (build/s58-after2.log).
- Existing recolour check (test_iconpen.c ~601) changed 16 -> 20: ip_recolour now also rewrites the 4 held role pens.

## Open: needs a rig pixel check (not run here, host only)
- Risk: if stock 8-plane Workbench already allocates pens 4-7 (DrawInfo/system pens), the exclusive obtain fails, rhave=0, and
  the held-only rule stops 4-7 being recoloured (visible regression vs before). Check on a rig with an 8-plane screen: ipd status
  / is_free probe on pens 4-7, then pixel-check the role colours after theme apply. If the obtain fails there, decide fallback
  (write anyway) - design choice, not made here.
- Held role pens keep old colours if the flat/palette gate turns off; ip_status_line does not list role pens.
