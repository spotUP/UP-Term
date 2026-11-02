---
date: 2026-11-02
topic: Inverted Workbench icon colours with flat look on, vantablack, depth 8
status: open (cause partly found, no fix yet)
rig: 61 (FS-UAE, OS 3.2, --max)
---

# Inverted icon colours: where the hunt stands

Owner report (todos.md OWNER-ICON-COLOURS-INVERTED): flat look on, theme
vantablack, screen depth 8, Workbench icon colours look inverted.

No fix is committed. The tool-call budget ran out before the root cause was
proved. This file records the measurements so the next agent starts here.

## Repro on rig 61

1. `amarchy_gui_rig.py --setup`, then ENV:Amarchy/prefs with flat-system-look
   and flat-window-gadgets on, theme picked as vantablack in the theme picker.
2. Amarchy launches `UP-Term:bin/amarchy-icons APPLY`
   (amarchy/desk_dos.c:467-484, dk_icons_sync).
3. Read pens with RAM:iconpens, grab the screen, measure with PIL.

## Measurements

iconpens after picking vantablack (prefs on disk say vantablack, flat on):

    DEPTH 8
    HOLDER owners=7 held=16 refs=2 first=32 count=16
    PEN 32 555555, 33 bebebe, 34 bebebe, 35 8d8d8d, 36 454545, 37 898989,
        38 868686, 39 696969, 40 3f3f3f, 41 7a7a7a, 42 505050, 43 1a1a1a,
        44 222222, 45 5c5c5c, 46 222222, 47 222222

Expected for vantablack: foreground ffffff, background 000000, darker 070707.
Pen 33 bebebe is the matte-black foreground, and ground 222222 matches no
vantablack slot. The 16 held pens carry a stale or mixed theme, not the theme
in ENV:Amarchy/prefs.

Screenshot icon column, box (20,25,70,210) of iconcol-before-on.png
(copied to ~/Desktop/upterm-shots/s31-iconcol-before-on.png):

    222222 4224, 7a7a7a 1855, 000000 1833, bebebe 1338

The icons on screen are the stock 3.2 icons, remapped onto the held theme
pens. APPLY had not yet replaced any .info file when the shot was taken.

## amarchy-icons is slow, and two copies run at once

wasabi ps showed two amarchy-icons processes (CLI 6 and CLI 8), both alive
for many minutes (stack 16384, 6-14 KB free, so not a stack overflow). A
snoop trace (15:40:39 to 15:42:23) shows they are not hung: they walk every
volume and back up each .info file, and for each icon they rewrite
SYS:Storage/Icons/Amarchy/manifest via manifest.amt and manifest.amo:

    Open("SYS:Storage/Icons/Amarchy/manifest.amt", create) = ok
    Rename("SYS:Storage/Icons/Amarchy/manifest", "...manifest.amo") = ok
    Rename("SYS:Storage/Icons/Amarchy/manifest.amt", "...manifest") = ok
    DeleteFile("SYS:Storage/Icons/Amarchy/manifest.amo") = ok

453 .info opens in under two minutes, about one icon every 0.25 s plus a
manifest rewrite per icon, so the walk is quadratic in the number of icons.
Two copies run this same sequence on the same manifest at the same time, so
they race on manifest.amt and manifest.amo.

## Candidate causes (unproved)

1. Stale held pens: pens_theme (amarchy/iconpen_dos.c ~107) reads the theme
   when the holder syncs pens. Pen 33 still holds the matte-black foreground
   after vantablack was picked, so a later theme change did not recolour the
   held pens. Stock icons remapped onto those pens look inverted.
2. Overlap: until APPLY finishes (minutes), Workbench shows stock icons on
   flat-theme pens. The owner may see this window as "inverted".
3. Two concurrent APPLY runs (the desk_dos.c `last` guard is per Amarchy run,
   so a restart of Amarchy or a second sync path starts a second copy) race
   on the manifest and can leave mixed icon sets.

## Next steps

1. Prove cause 1: switch theme while the holder is held, then read iconpens.
   Pen 33 must follow the theme foreground. Fix in iconpen_dos.c so every
   ipd_owner/ipd_recolour call rereads the theme. Host test in the iconpen
   ONLY= suite.
2. Make APPLY single-instance (named semaphore or port) and write the
   manifest once at the end of the walk, not per icon.
3. Rig proof: pixels of the icon column after APPLY finishes, flat on versus
   flat off.
