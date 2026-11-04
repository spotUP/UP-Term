---
title: Flat draw path stack headroom (S56-FLAT-HEADROOM)
branch: s56-flat-headroom
---
# Result
make flat-budget: deepest chain _c2_disp_reg 384 -> 308 of 384 (headroom 0 -> 76), _c2_disp_stk 368 -> 292 (headroom 16 -> 92).
Entry frames unchanged (20, 4). tests/test_flat_budget.py now fails below 64 bytes of headroom (HEADROOM_MIN) and prints the headroom per entry; verified: HEADROOM_MIN=80 fails with 76.

# Deepest chain before (bytes)
c2_disp_reg 20 -> c2_draw 72 -> c2_clone 72 -> flat_clone_draw_in 80 -> flat_clone_map 76 -> clone_ground 44 (+4 per call) = 384.
After: 20 -> c2_draw 44 -> c2_clone 56 -> flat_clone_draw_in 48 -> flat_clone_map 76 -> clone_ground 44 = 308.

# Changes (behaviour identical)
- flat_glyph.c: flat_clone_draw_in's pen choice moved to clone_pens (role ints off its frame), and the band fill plus glyph run loop to clone_paint. Frame 80 -> 48.
- flat.c: c2_draw's Kickstart 3.1 gadget search moved to c2_which_31 (72 -> 44). c2_clone's empty-render diagnostics moved to c2_clone_note_empty and its fail counters to c2_clone_note_fail (72 -> 56).
- Tried and dropped: splitting flat_clone_map into peel and mark helpers made the chain worse (400), because the helper that peels still calls edge_line; reverted. flat_clone_map (76) remains the biggest frame on the chain.
