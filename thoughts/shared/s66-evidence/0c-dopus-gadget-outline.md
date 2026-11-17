---
date: 2026-11-09
topic: 0c DOpus 5.104 Environment gadgets have no flat outline
status: root cause found by disassembly; fix designed, NOT implemented (budget)
---

# 0c: why the DrawBox / DrawFieldBox LVO patches never fire

## Root cause (disassembly of dopus5.library 73.8 [OS3], m68k-amigaos-objdump, .text file offset 0x28)

- Library function table in .text at 0x26c4 (393 relocated longs); entry i is LVO -(i+1)*6.
  - LVO -696 DrawBox -> body 0x2450c.
  - LVO -702 DrawFieldBox -> stub 0x245c4: `2f0a 2f39 <GfxBase abs> 2f0a 2f09 2f08 4eba 000a` pushes
    rp, rect, info, GfxBase and bsr's the stack-convention body 0x245de.
- dopus5's own gadget classes call the BODIES with absolute `jsr`, never through the jump table:
  - `jsr 0x2450c` (DrawBox body) at 0x1d49e in fn 0x1d018 = dopusbuttongclass render
    (referenced by `lea 0x1d018(pc),a6` at 0x1d9e2 and `jsr` at 0x1e378).
  - `jsr 0x2450c` at 0x1429c in fn 0x1422c (listview; called from 0x12bfe, 0x13b42).
  - `jsr 0x245de` (FieldBox body) at 0x18d68 in fn 0x18b78 (string gadget; lea at 0x194ac).
- All 17 `jsr -696(a6)` / `jsr -702(a6)` in the binary are exec CreatePool/DeletePool, not self-calls.
- So the FLAT-D4 swap of the jump-table JMP (entry+2) only catches EXTERNAL callers (dopus5 program,
  modules). The internal gadget renders never reach it: matches rig 67 (dopus counter +0 on Environment).

## Button render frame logic (fn 0x1d018)
- flags in d0; if (flags & 0x300):
  - bit 10 set -> DrawBox body directly (rect 68(sp), a2 = DrawInfo from 120(a3)->28, d0 = (flags>>9)&1 recessed).
  - else -> GadTools DrawBevelBoxA (gadtools base bss 0x20, LVO -120) with GTBB_Recessed / GTBB_FrameType
    = 46(a4) / GT_VisualInfo. This path would go through our c8 frameiclass hook (frame= counter).
- Plus inline SetAPen/Move/Draw/RectFill (8/8/12/8) for the rest of the look.
- OPEN: which branch the Environment buttons take is NOT measured. Measure first on a rig: STATUS
  `frame=` and `dopus=` counters before/after opening Environment. If frame= moves but pens stay 000000,
  the defect is in the c8 path (GadTools frame type 46(a4), maybe BBFT_RIDGE / ICONDROPBOX), not D4.

## Designed fix (body inline hook, root-cause layer)
1. Find bodies: DrawBox = jmp target of LVO -696 (already flat_jmp_target); FieldBox body = stub + 14 + 2 + disp16
   of the `4eba` at stub+14 (verify the 14-byte stub pattern, GfxBase long wildcard).
2. Gate exact prologue bytes (position independent, safe to relocate into a trampoline):
   - DrawBox 0x2450c: `48e7 3012 2408` (6 bytes: movem.l d2-d3/a3/a6,-(sp); move.l a0,d2).
   - FieldBox 0x245de: `48e7 2032 41ef 0014` (8 bytes: movem.l ...; lea 20(sp),a0). Note lea 20(sp) runs
     after the movem, so relocated copy is still correct as long as the trampoline is jumped to (not jsr'd).
   Keep the existing sum gate (flat_dopus_gate) as well; unknown prologue -> fall back to the LVO swap (5.82).
3. Write `4ef9 <abs32>` (6 bytes) at the body entry pointing at our routine; trampoline in flatshared =
   copied prologue + `4ef9 <body+len>`; CacheClearU inside Forbid.
4. In body mode do NOT also swap the LVO (external callers pass through the body: double count/draw).
5. FieldBox replacement must be stack convention (rp, rect, info, gfx at 4/8/12/16(sp)); fallback calls the
   trampoline with the same stack args. DrawBox body ignores incoming a6 (uses abs GfxBase), reg args a0/a1/a2/d0.
6. Remove: restore the original bytes only if our JMP is still there; else go inert (as FLAT_STILL_CHAINED).
   Keep dsw[].installed/saved semantics so STATUS and the Ctrl-C "still chained" check keep working.
7. Pure helpers in flat_core (body-from-stub, prologue match, trampoline build, jmp build) host-tested in
   tests/test_flat_core.c with the real 5.104 bytes above; revert check = test fails without the helpers.

## DT-11 (lister_toolbar.c:202 RenderImage(toolbar_arrow_image))
RenderImage is an image draw (IM_DRAW on dopusiclass), not DrawBox/FieldBox. The body hook does not reach it
unless dopusiclass's IM_DRAW calls the DrawBox body (not checked). Likely needs its own IM_DRAW hook. Unverified.

## Measurement (s66, rig 68, OS 3.2, DOpus 5.104 / dopus5.library 73.8, 2026-11-09)

Verdict: the defect is in the c8 frameiclass path, not FLAT-D4. Per the brief, the body JMP hook was NOT built.

- Setup: amarchy-flat from build/amiga (HEAD, no 0c change), prefs flat-system-look/flat-window-gadgets/desktop-theme on,
  theme picked with keep=True. DOpus runs on its own 8-plane screen "Directory Opus".
- Environment opened with a click on the lister, then Amiga-4. Window "Environment" at 26,33 588x189.
- STATUS before -> after:
  - `dopus=73.8 patched=3 calls=136 flat=112` -> `calls=136 flat=112` (D4 +0: the Environment gadgets never reach it)
  - `frame=hooked calls=26 flat=5` -> `calls=40 flat=10` (c8 +14 dispatches, only +5 drew flat)
- Pens: `text=2 line=3 back=0 outline=3 button=4`. DOpus screen palette (`dopus query palette`): pen 0 = 000000,
  pen 2 = BEBEBE, pen 3 = 8D8D8D.
- Edge pixels (screen grab, gadget boxes from the UI tree): every edge of gadgets 214/21/22 (Use/Save/Cancel), 19 (list),
  218/221/453 (checkboxes), 222/454 (strings), 354 (button) is 000000 on all four sides. The window background is also
  000000 (pen 0). The window border next to them is 8D8D8D (pen 3), so the theme pens are live on that screen.
- So no outline is drawn at all (edges = background pen). The frames that are drawn go through frameiclass
  (GadTools DrawBevelBoxA branch of fn 0x1d018), and 9 of the 14 dispatches did not draw flat.
- Next (not done): find why c8_skip / the IM_DRAW(FRAME) path skips or blanks these frames. Candidates: the GTBB_FrameType
  at 46(a4) (BBFT_RIDGE / ICONDROPBOX not handled flat), or the c8 flat draw using pen 0 on this screen.
  Log per-dispatch frame type and decision before any fix.
- The body-hook helpers written before the measurement (flat_fieldbox_body, flat_body_prologue, flat_jmp_build,
  flat_body_tramp_build, plus a host test with the real 5.104 bytes, 2804 checks passing) are saved, not applied, in
  thoughts/shared/s66-evidence/0c-body-hook-helpers.patch. They remain the fix for the internal DrawBox/FieldBox body
  calls (listview fn 0x1422c, string fn 0x18b78) if those still matter after the c8 fix.

## c8 hunt (s66, rig 68, OS 3.2, DOpus 5.104, 2026-11-09)

Verdict: the c8 frameiclass path is NOT defective. The earlier reading "14 dispatches, only 5 drew flat" counted
non-draw methods: `fcalls` increments on EVERY frameiclass dispatch (OM_NEW/GET/DISPOSE/ADDTAIL included, and c8_skip's
own GetAttr(IA_EdgesOnly) re-enters the dispatcher as OM_GET). The fix belongs in the D4 body hook, not in flat.c c8.

- Temporary per-dispatch ring (method, decision, dri shine/shadow/back pen + rgb, state, x, y, w<<16|h), printed by
  STATUS, removed again (flat.c / flat_shared.h restored byte for byte, amarchy-flat rebuilt clean).
- Environment open moved `frame=hooked calls=26 flat=5` -> `calls=40 flat=10` again. The 14 new ring entries, oldest first:
  OM_NEW(257) OM_ADDTAIL(261) OM_NEW(257) IM_DRAW(514) [flat, edges-only] OM_GET(260) OM_DISPOSE(258), then 4x
  { IM_DRAWFRAME(518) [flat, filled] + OM_GET(260) }. So 5 draw dispatches, 5 drawn flat (decision 10/11): 100%.
  No dispatch hit c8_skip's `return 0` after the decide (no raa_frame_draw failure), no BBFT_RIDGE/size reject.
- Those 5 frames are all at window x 142..156, 8 px wide, h 114/132 (IM_DRAWFRAME dims 0x80072 / 0x80084): the
  listview's scroller column (list19 spans window x 6..154). None is a button, checkbox or string gadget.
- DrawInfo on the DOpus screen: SHINEPEN = SHADOWPEN = BACKGROUNDPEN = 0 (rgb 000000 each). So any stock bevel there
  draws in pen 0 = background: invisible. Flat outlines must come from our own draw with the theme pens.
- Conclusion: Use/Save/Cancel, the checkboxes, string fields never dispatch frameiclass. They take fn 0x1d018's bit-10
  branch (jsr 0x2450c DrawBox body) and the string fn's jsr 0x245de (FieldBox body), which the LVO swap cannot see
  (D4 calls +0). The body inline hook (0c-body-hook-helpers.patch + the designed steps above) IS the root-cause fix.
- Not done: the body hook itself (outside this brief's c8 scope and budget). No commit from this hunt.

## Checkbox note (coordinator add-on; corrected to GadTools MX radio gadgets, Installer 43.3 on KS 3.1)

- NOT measured. The c8 ring above logs frameiclass only; the DOpus Environment page measured has no MX gadget, and the
  checkboxes there (218/221/453) produced no frameiclass dispatch at all (DOpus draws them itself, not GadTools).
- Code read only: GadTools MX_KIND renders sysiclass MXIMAGE (no frameiclass around it), so it goes through the sysiclass
  hook c2_draw (amarchy/flat.c ~560-585), not c8. c2_draw takes EVERY SYSIA_Which (no MXIMAGE special case: grep for
  MXIMAGE in flat.c / flat_core.c / flat_glyph.c finds nothing) and either clones the system image (flat_clone_route +
  c2_clone) or draws the block glyph (c2_glyph) with c2_get_pens. Under flat pens (SHINE == BACKGROUND, and on the DOpus
  screen even SHADOW == BACKGROUND) a clone of the stock MX ring would draw its ring in background-coloured pens, which
  matches "unselected = nothing, selected = only the dot". Unverified: needs a rig log of SYSIA_Which / imp_State /
  route (clone vs glyph) and the pens for MXIMAGE in Installer 43.3 on KS 3.1. Separate ledger task; not implemented.

## D4 body hook build (s66)

Files: amarchy/flat.c, amarchy/flat_core.c, amarchy/flat_core.h, amarchy/flat_shared.h, tests/test_flat_core.c. Commit: e2932d76.
Hook: JMP abs.l over DrawBox body 0x2450c (prologue 48e7 3012 2408) and FieldBox body 0x245de (48e7 2032 41ef 0014); trampoline = relocated prologue + JMP body+len; prologue mismatch leaves dopus5 on the LVO swap.
Tests: test_dopus_body_hook (real 5.104 bytes) and test_dopus_body_swap in tests/test_flat_core.c; flat_core run 2817 checks, 0 failed. Before the helpers existed the test did not build (fails); after, passes.
Rig 68 (OS3, DOpus 73.8 own screen 1344x1080, vantablack picked, flat prefs on): STATUS dopus= calls 76 -> 89 and flat 76 -> 89 after opening Settings > Environment (key Amiga-4). STATUS does not split body hits from LVO hits.
Pixels (window grab, pen 3 = 8d8d8d): string field gadget 222 (hit box 666,562): 1px frame of 8d8d8d at 662,559 around it, was 000000 only at the hit-box edges. Use/Save/Cancel (384,616 / 608,616 / 828,616) and checkbox 218 (636,463): edges still 000000, NO frame. OPEN: those gadgets are not drawn through the two hooked bodies, or are overdrawn; not proven.

## Button/checkbox path (s66)

Why e2932d76 missed them: the Use/Save/Cancel buttons and the checkboxes do not draw through
dopus5.library's DrawBox (body 0x2450c) or FieldBox (0x245de). They draw through an internal,
LVO-less static bevel at code offset 0x166d8 (dopus5.library 73.8, DOpus 5.104).

Draw path per gadget (code-hunk offsets):
- Buttons (dopusbuttongclass, dispatcher 0x19950, GM_RENDER 0x18b7c): default button path calls the
  bevel at 0x19370 (other in-class caller 0x19012).
- Checkboxes (dopuscheckgclass, same dispatcher 0x19950): the render tail draws GadgetRender /
  SelectRender with DrawImageState (-618) at 0x198ac / 0x1947c; the image is dopusiclass
  (dispatcher 0x1a86c, draw 0x1a946), whose types 0 and 1 call the bevel at 0x1a9da.
- Palette gadget (dopuspalettegclass, 0x154b4) calls it at 0x15d7a.
- Bevel args on the stack: (unused, rp, dri_Pens UWORD*, WORD rect[x,y,w,h], recessed (==1), thin).
  Under DOpus's own pens (all 0) the stock code drew pen 0 = 000000 edges.

Fix: a third D4 element FLAT_EL_BEVEL in the existing body-hook mechanism (amarchy/flat.c d4_bevelbody,
sharing d4_flat_pens with DrawBox/FieldBox). Location = DrawBox body - 0xde34 (flat_bevel_body, per-build
table row), gated by version 73.8, flat_sum_reloc 0xbdaa116a and the exact 8-byte prologue
48e7 3832 41ef 0020. No LVO fallback; an unknown build leaves the bevel stock. STATUS prints
"dopus bevel calls=N".

Test: tests/test_flat_core.c test_dopus_bevel_hook (real 64 bytes of 0x166d8). Fails before the fix
(flat_bevel_body 0, prologue 0, gate 3 not 7); passes after (flat_core 2838 checks, 0 failed).

Rig 68 (OS 3.2, 640x256 DOpus screen, theme outline pen 8d8d8d), Environment window at 26,33:
- STATUS: "dopus=73.8 patched=7 ... sums=780fd46b,ba2cf172,bdaa116a"; "dopus bevel calls=32" after open,
  34 after Cancel, 66 after reopening (Amiga-4).
- Edges (all four sides, every pixel): Use 32,204 132x14, Save 256,204, Cancel 476,204,
  checkboxes 284,51 / 284,147 / 284,179 26x14, button 556,147: all 8d8d8d, no 000000.
