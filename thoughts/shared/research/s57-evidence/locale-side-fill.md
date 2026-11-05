---
date: 2026-11-05
topic: S57 Locale side border fill in Amarchy flat mode
status: measured, no code fix needed on this evidence
branch: s57-locale-flat (2e28bdd5 + temporary dfill diagnostic, reverted)
rig: 127 (OS 3.2, 060 max)
---

# Locale active-window side fill, rig 127

## Setup

- Build of s57-locale-flat head 2e28bdd5 plus a temporary RectFill diagnostic ring
  (`dring`, 16 entries: rect, caller flags in-ROM/layer/window, window size and borders).
  The diagnostic was printed through `amarchy-flat STATUS` and is reverted; patch kept in the
  session scratchpad only.
- amarchy-flat copied to UP-Term:bin and VTCX:Apps/bin (the resident copy loads from UP-Term:bin),
  flat prefs on, `SYS:Prefs/Palette FROM palette-flat-pen8.prefs USE`, then `SYS:Prefs/Locale`.
- The Amarchy theme pick (d.pick hotkey menu) did not open on this rig ("no stable state within
  60s"), so the run uses the palette prefs only. The owner's path through the theme pick is not
  reproduced here.

## Measurements (ami.window + PIL on the grab)

- Locale window box: x 88..639, y 54..245 (552x192), borders L4 T11 R4 B2, active.
- STATUS: `rectfill installed=yes calls=683 hits=10 shits=4`: the side-fill branch ran 4 times.
- Left border columns x 88..91 at y 70..240: `6688bb 6688bb 13a907 13a907`
  (2 px outer ring in pen_outline, then pen_back).
- Right border columns x 636..639: `13a907 13a907 6688bb 6688bb` (pen_back, then 2 px ring).
- Bottom border at x 90/200/400/630: y 244 pen_back, y 245 ring 6688bb.
- Accent runs inside the window (Country list area) present, longest 342 px at y 137, 134..156
  px runs at y 81, 120, 202, 212.

## Verdict

The side and bottom fill already resolves to pen_back on 2e28bdd5; only the FLAT-OUTER-EDGE
ring (side_px = 2) is accent, which is the owner rule. The earlier register row
"side borders still accent x0..2" measured screen columns 0..2, which lie outside the Locale
window (it starts at x 88), so that verdict was a measuring error, not a code fault.

The diagnostic ring showed the remaining pen_outline vertical RectFills in the window come from a
non-ROM caller (flags layer|window, not in Intuition ROM range) at window x 12/149/150/167/196/
333/334/351/519/520/537: GadTools frame lines, not border strips.

## Open

- Pen 0 renders 13a907 with palette-flat-pen8.prefs on this rig, not the flat dark surface; the
  palette prefs file does not set the flat surface colour. "Dark" here means pen_back by index.
- Repeat with the real theme pick once the hotkey menu opens on a rig, to confirm the owner's view.
- S57-LOCALE-GADGET-BLACK repro not run (budget).
