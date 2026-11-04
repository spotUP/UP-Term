#!/usr/bin/env python3
"""RL20 SELECTA-AA-RICHUI rig STUB (not run yet): amarchy-gui draws the selected row and the welcome buttons with
antialiased rounded corners (amarchy/richaa_core.c, gui.c fill_round). Plan for the pixel proof, on a rig started with
`UPTERM_RIG=<n> rig.py start --max` (never rig 1 or another agent's rig):
  1. open the menu (or the welcome window) on a 24-bit screen and on an 8-bit screen, grab the window;
  2. the top-left 4 x 4 pixels of the selected row: pixel (0,0) must equal the row background, at least 2 of the others must
     be neither the accent colour nor the background (intermediate shades), the four corners must mirror each other;
  3. the AMARCHY_AA_GLYPHS counter proves the text path ran; rich corners are proven by the host test (rich_aa_calls);
  4. a 4-colour screen: the corners stay square (stock).
Prints [PASS]/[FAIL]. Modelled on amarchy_iclabel_rig.py; fill in with ami/condev_rig helpers when rig time is granted."""
import os, sys
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))

if __name__ == '__main__':
    print('[SKIP] amarchy_richaa_rig.py is a stub: pixel proof not written yet (RL20 open item)')
    sys.exit(2)
