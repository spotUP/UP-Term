# B10 (a) RGB compare: rig check of 1a0465c8

Rig 96, `rig.py start --os 32 --max --purpose "up-term-49: B10 RGB compare rig check"`, DOpus 5.104 (dopus5.library 73.8, sums 780fd46b,ba2cf172).
df before start: 26 GiB free. build/amiga/amarchy-flat (1a0465c8) refreshed onto the rig by the start gate ("refreshed amarchy-flat 210220 bytes") and put to RAM: by the script.

## Step 3 (stock look) -- PASS, ran first because the rig booted stock
Workbench pens (dripens Workbench): SHINEPEN = pen 2 (FFFFFF), BACKGROUNDPEN = pen 0 (AAAAAA); different numbers, different RGB. No Amarchy theme was active at boot.
STATUS before lister: `flat=on calls=10 ... dopus=73.8 patched=3 calls=0 flat=0`
STATUS after DeviceList NEW: `flat=on calls=126 ... dopus=73.8 patched=3 calls=8 flat=0`
dopus calls grew 0 -> 8, flat stayed 0: stock bevels kept. PASS. (Opens the pen-number-differs path, so d4_flat reached icl_rgb and RGB differed.)

## Step 2 (flat look) -- PASS with caveat
Theme: not tokyo-night. Used the repo's flat-pen fixture tests/data/desk/palette-flat-pen8.prefs via Palette USE (the amarchy_flat_rig.py d4 route); DOpus quit, palette set, amarchy-flat + DOpus restarted.
Pens after: SHINEPEN = 0, BACKGROUNDPEN = 0, RGB 13A907 / 13A907 (equal numbers).
STATUS before lister: `dopus calls=0 flat=0`; after DeviceList NEW: `dopus calls=8 flat=8`. calls and flat grew equally. PASS.

## Not reached
The case the commit adds (different pen numbers, equal RGB, expected flat) was not set up: no fixture gives it, and it would need a custom palette. So the RGB-equal branch of flat_dopus_decide is not shown on a rig by this run; only the pen-equal (flat) and different-colour (stock) outcomes are.
No second lister opened; no hang seen (rig answered Version rc 0 after each step).

Rig stopped; no scratch over 10 MB made (the scratch scripts are tiny).

# Part 2: equal-RGB branch (different pen numbers, same colour) -- PASS

Rig 96 (OS 3.2, DOpus 5.104 reinstalled by `rig.py dopus --force --os 32`, purpose "up-term-49: B10 equal-RGB rig proof"). Scratch script only (tiny); route = amarchy_flat_rig.py --d4 (DOpus quit, `Palette FROM T:flatpal.prefs USE`, amarchy-flat, DOpus, `amarchy-flat DOPUS`, DeviceList NEW).
Fixture tests/data/desk/palette-equalrgb-pen8.prefs = palette-p1a-pen8.prefs (stock pen map) with colour 2 set to colour 0's RGB (13A907); 6 bytes differ from the stock fixture.

Equal-RGB: dripens Workbench `pens: 0 1 2 2 253 3 0 0 2 2 0 253` -> SHINEPEN (slot 3) = pen 2, BACKGROUNDPEN (slot 7) = pen 0; `rgb:` slots 3 and 7 both 13A907.
STATUS before DeviceList NEW: `dopus patched=3 calls=0 flat=0`; after: `calls=8 flat=8`. calls and flat grew by the same amount -> PASS (SKIP from the equal-RGB branch of flat_dopus_decide, pen numbers differ).

Negative control, same boot, stock palette back via Palette USE: pens identical numbers, rgb slot 3/7 = FFFFFF / 13A907 (different).
STATUS before: `calls=0 flat=0`; after: `calls=8 flat=0` -> flat stops growing, stock bevels kept. PASS.
Text checks only, no screenshots. Rig stopped afterwards.
