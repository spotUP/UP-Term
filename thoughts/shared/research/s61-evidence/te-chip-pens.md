# TE-COLOUR-LIST-ALL-SWATCHES

Cause: amarchy/gui.c obtained a pen for each colour chip and for the theme preview (fill_rgb), drew, and released it at once. On a pen screen the next obtain reuses the number and recolours every earlier chip.
Repro (rig 172, 3.2, 256-colour pen screen, Settings > Colors > Text colour): 1 distinct chip colour of 7, following the cursor.
Fix: reuse amarchy/pen_cache.h (chip_pens, prev_pens); release in draw_full, pens_release.
After: 7 distinct of 7 in 4 shots, matching the list rgb. Shots: ~/Desktop/upterm-shots/chip-pens-a0..a3.png.
Tests: test_gthemes_chip_pens_held_per_frame (host), tests/test_gui_chip_pens.py (source guard, fails on HEAD).
Open: Theme Editor swatch row count (17) not checked on the rig.
