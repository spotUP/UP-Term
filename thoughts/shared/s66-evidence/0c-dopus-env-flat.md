---
date: 2026-11-09
topic: 0c DOpus Environment under Amarchy flat pens (s66)
status: (a) FAIL measured, not fixed; (b) PASS
---

# 0c: DOpus 5.104 Environment window under flat pens

## Setup
- Rig 67, KS 3.2 `--os 32 --max`, 640x256x8, topaz 8 (no ENV/ENVARC Sys/font.prefs), HEAD f7d1250c
  (includes 06fe144c flat DrawBox/DrawFieldBox outline). `rig.py dopus` run through a scratchpad wrapper that adds
  `--allow-stale-base` to its inner start (local main is 3 behind origin/main; RIG-STALE-BASE refused the install).
- Prefs flat-system-look/flat-window-gadgets/desktop-theme on; VTC:Amarchy + RAM:amarchy-flat (build/amiga, md5 = rig);
  theme pick tokyo-night -> vantablack via amarchy_desk_rig.pick(keep=True). STATUS after pick:
  `flat=on`, `pens text=2 line=3 back=0 outline=3 button=4`, `dopus=73.8 patched=3 calls=184 flat=164`
  (the DOpus lister/button bank DID go through the patched LVOs and drew 164 flat outlines).
- Environment opened with Amiga-4 on the DOpus window: window 26,33 588x189 (rows 33-221), active.

## (b) bottom border: PASS
- Rows 44-220: 2 px grey (8d8d8d) sides at x26-27 and x612-613, interior black.
- Row 221: full-width 8d8d8d from x26 to x613 (the bottom outline). Row 222 onward: screen behind.
- Same shape as the theme requester (bug0-bottom-border.md). Window ends at 221 < 256: not off screen.

## (a) gadget outlines: FAIL (text only)
- Opening Environment moved the DOpus counter by ZERO: `calls=184 flat=164` before and after. The window's frames
  never reach DrawBox (-696) / DrawFieldBox (-702) through the jump table, so 06fe144c cannot reach them.
- Edge pens per gadget (uitree box, all four edges): every pixel 000000 (= back) for Use/Save/Cancel (32/256/476,204
  132x14), listview 19 (32,46 148x156), checkboxes 218/221/453 (284,y 26x14), string fields 222/454, button 354.
  Only the label/glyph text (bebebe) is inside. Rows 205-206 and 215-216 are black across x28-611: no button box.
- Root cause (layer): dopus5.library's own BOOPSI classes (button_class / dopusiclass / listview / string field)
  draw their bevels by internal calls (direct BSR to the routine, not through the LVO), with SHINE/SHADOW pens that the
  flat theme maps to the back colour; the outline replacement lives only in the LVO swap (flat.c:823-852) and the
  frameiclass hook (flat.c c8_skip). Neither is on this window's path. This is the "not reachable by an LVO" list in
  flat.c:17-20, which in practice covers the whole Environment window, not just the path field.
- No source of dopus5 5.104 on this Mac (no button_class.c found under ~/Code), so the internal call target is
  inferred from the zero LVO count, not read.

## Fix options (design decision, not done)
1. Inline-hook the routine BODY (flat_jmp_target of -696/-702, checksum-pinned 780fd46b/ba2cf172): JMP at its entry
   to d4_*, the saved path a trampoline with the relocated prologue + JMP back. Catches internal callers too IF they
   call that same routine (needs a 5.104 disassembly to confirm; position-independence of the copied prologue).
2. Hook the dopus classes' dispatchers (button_class etc.) and draw the outline after their GM_RENDER, like c8_skip.
Either is a new mechanism; size: one agent with disassembly of dopus5.library 5.104.

## Side findings
- rig.py start failed once copying rig 1's VTC: (distkit/Files entries vanished mid-copy: someone was rewriting
  build/rig/vtc/distkit); the retry passed.
- `rig.py dopus` has no way to pass --allow-stale-base to its inner start.
