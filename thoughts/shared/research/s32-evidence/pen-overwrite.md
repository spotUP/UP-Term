# Who writes Workbench pens 249-251 (and 36-38)? S32-INACTIVE-BAR follow-up, 2026-11-03

Code reading only (HEAD c019788b). No rig probe run: a Ctrl-D to amarchy-flat runs dim_release, which would ReleasePen 249-251 on
the current ColorMap where (per the conclusion below) another client owns them. That corrupts rig 35's pen refcounts and wipes the evidence.

## Verdict

Nobody overwrites 249-251 by number. amarchy-flat's dim/knob/inbar pen numbers are STALE: they were obtained on an earlier Workbench
screen (ColorMap A). The Workbench was then closed and reopened (ColorMap B), and amarchy-flat never re-obtains these three pens on B.
On B, pens 249-251 are free from amarchy-flat's point of view and another client allocated them for the icon colours. The hook still draws
with r->st.pen_dim/knob/inbar = 249/250/251, so it paints whatever colours B's owner put there.

- amarchy/flat.c:1433-1440: the 1 s Workbench watch (pen_tm) calls only pens_sync (icon pens 32-47 via ipd_owner/ip_sync). dim_refresh
  runs only at start (1429) and on Ctrl-D (1446-1451). A reopen between Ctrl-Ds leaves g_dim_pen/g_knob_pen/g_inbar_pen pointing at A.
- amarchy/flat.c:1235-1293 dim_refresh / 1295-1313 dim_release: no screen identity recorded; on the next Ctrl-D they ReleasePen the
  stale numbers on B (pens never obtained there: the same free-list corruption ip_holder_verify warns about, iconpen_core.h:83-90).
- The icon holder solves exactly this for pens 32-47: ip_holder_verify (iconpen_core.c:147) asks the ColorMap via o->is_free whether a
  held pen is still held, forgets without ReleasePen on a reopen, and ip_sync re-obtains (iconpen_core.c:199). That is why 36-38 are
  correct on B and are re-held exclusively.

## Why the colours equal pens 36-38, in reverse order

- dim_refresh obtains in the order inbar, knob, dim (flat.c:1265, 1276, 1286); fresh pens come from the top of the free list, giving
  inbar=251, knob=250, dim=249 on A.
- On B, 251/250/249 hold 323F5F/747A94/858BAA = icon slots 4/5/6 (pens 36/37/38: ACCENT33, FG66, SELFG66). A client that requests the
  icon colours by value with ObtainBestPen cannot share 32-47 (held PENF_EXCLUSIVE, iconpen_dos.c:42), so it allocates fresh pens top-down:
  slot 4 -> 251, 5 -> 250, 6 -> 249 (slots 0-3 presumably matched existing shared pens). Most likely icon.library remapping the
  amarchy-icons colour icons (their palette carries the 16 icon colours). Owner of 249-251 on B: UNVERIFIED (no PalExtra read tool).
- On one ColorMap both cannot own the same fresh pen, so two ColorMaps (a reopen) are required. None of the SetRGB32 writers can reach
  249-251: gthemes_dos.c:51 (pens 0-3), gthemes_dos.c:146 (desk preview, palette.prefs pens), desk_dos.c:612 (role pens 4-7 / 252-255),
  iconpen_dos.c:67 (held 32-47 only, h->first + i), render/amiga_render.c:169 (its own exclusive scratch pens, 588-589).

## Where the reopen comes from (not measured on rig 35)

desk_dos_apply (desk_dos.c:1047): dk_flat_pens / palette.prefs write and dk_wbborder (wbborder USE, desk_dos.c:354 "reopens the
screen") reopen the Workbench through IPrefs, asynchronously; dk_flat_sync's Ctrl-D (desk_dos.c:522) can land before the reopen, so
dim_refresh obtains on the closing screen. Boot is the other path: amarchy-flat BOOT runs before LoadWB / the IPrefs RTG reopen.
Either way the 1 s watch sees the new screen and fixes only the icon pens.

## Fix (owner layer: amarchy-flat's pen code, flat.c + flat_core / iconpen_core; not the palette writers)

Palette writers are correct; the bug is a shared-pen holder with no screen identity. Fix in the layer that holds the pens:
1. Give the three shared pens a holder with the same identity rule as ip_holder: record the screen/ColorMap they were obtained on;
   before release, verify (the screen is still the one, the ColorMap answer); on a reopen forget them WITHOUT ReleasePen.
   Reuse the iconpen pattern (ip_ops callbacks: obtain/release/is_free-style check), do not write a second ad hoc one. Simplest reuse:
   make pens_sync report the ip_sync action (IP_SYNC_OBTAIN = new screen or reopen detected by ip_holder_verify, which also catches the
   same-address reopen) and, on OBTAIN, call a dim_forget() (drop numbers, no ReleasePen) then dim_refresh().
2. Call that from the pen_tm branch (flat.c:1433-1440), so a reopen re-obtains dim/knob/inbar on the new screen with colours read from
   the new palette. Keep the Ctrl-D path, but dim_release must not release on a screen it did not obtain on.
3. Status: add the screen-identity / stale flag (and inbar_rgb) to STATUS so the rig can see it.

## Host test that fails on revert

Pure core in flat_core (driven by tests/test_flat_core.c, fake ColorMap like test_iconpen.c's ip_ops fakes): obtain the three pens on
screen A (fake palette pA) -> 251/250/249; simulate a reopen (screen B, same address allowed, every pen free on B; icon holder reports
OBTAIN); run the 1 s sync step. Assert: zero ReleasePen calls on B for 249-251, three ObtainBestPen calls on B, inbar colour =
flat_inbar_colour(B pen0, B pen2) (5A5E75 for tokyo-night 1a1b26/a9b1d6), sentinel/call count of the re-obtain = 1. With the fix
reverted (watch calls only pens_sync) the obtain count on B is 0 and the held numbers still name A: the test fails.

Rig confirmation after the fix (rig 35 or any 3.2 rig): apply a theme that rewrites palette.prefs (reopen), wait 2 s, dripens:
pen_inbar's palette entry = 5A5E75.
