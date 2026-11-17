---
date: 2026-11-10
topic: DOpus 5 lister title gadgets (lock id 11, iconify id 10, hijacked zoom id 13) visible and drawn with flat glyphs
status: plan (not implemented, not committed); base main 101686d5
related:
  - thoughts/shared/s70-evidence/dopus-blank-gadgets.md (root cause V)
  - thoughts/shared/s69-evidence/dopus-dividers.md (rig 68 pixel scan)
  - thoughts/shared/plans/2026-11-09-dopus-scroller-look.md
---
# DOpus title gadgets under amarchy-flat

Paths: A = vtcon/amarchy/, T = vtcon/tests/, D = ~/Code/Reference/dopus5/source (vendor 5.104).
V = read in code this session, U = inferred / not measured. Owner decision s70: the three gadgets are drawn with flat's own
glyphs (look of the flat close/depth glyphs, no SHINE/SHADOW box art, no divider lines) through the existing glyph/clone
path; every glyph sizes from side_px(r, screen) of the drawing screen (one per-screen aspect source). No parallel painter.

Executor rules: one commit per step (1, 2a, 2b), each with its tests; run only `make test ONLY=flat_core` plus the vbcc
flat budget check (`python3 tests/test_flat_budget.py $(FLAT_BUDGET_ASM)`, Makefile:1214-1222) under nice 19; no full sweep.

## Checklist (IDs; tick in the open, report "n of 17")
- [ ] S1.1 flat_title_gadget() in flat_core (filter) + header doc
- [ ] S1.2 tx_span / tx_gad use it (A/flat.c:1227-1249), walk cap 64
- [ ] S1.3 host tests: filter unit + DOpus lister span x1 == 348
- [ ] S1.0 rig measurement: Activation and Flags of the hijacked zoom (decides whether the geometry branch is live; code ships either way)
- [ ] S2a.1 flat_img_record accepts ZOOMIMAGE (0x01) in addition to 0x0A..0x0D
- [ ] S2a.2 FLAT_IMG_RECS 128 -> 256; c2_which_31 walks gadgets before the table
- [ ] S2a.3 host tests for the record rule
- [ ] S2b.1 FLAT_W_LOCK / FLAT_W_LOCKED glyph ids + g_lock shape in flat_glyph.c, lock-state rule in flat_glyph_draw
- [ ] S2b.2 flat_glyph_known / window_gadget / on_band include lock; flat_clone_route excludes it
- [ ] S2b.3 flat_dopus_image_which(type) pure map in flat_core (IM_ICONIFY 6 -> 0x16, IM_LOCK 8 -> FLAT_W_LOCK, else -1)
- [ ] S2b.4 class_slot generalised to (class name, tags, entry-or-subentry) and the dopusiclass slot found with it
- [ ] S2b.5 d5_disp_reg dispatcher (reg convention), install in dopus_scan after the d4 gate, remove in dopus_unpatch
- [ ] S2b.6 counter r->d5draws in flatshared + STATUS word (the rig sentinel)
- [ ] S2b.7 _d5_disp_reg added to ENTRIES in T/test_flat_budget.py:23; headroom >= 64
- [ ] S2b.8 host tests: g_lock shapes side 1 and 2, open/closed, routing, type map
- [ ] R.1 rig 68 proof (pixels + STATUS sentinel), square-pixel DOpus screen
- [ ] R.2 rig proof on a hires (side 2) screen + owner eye check

## Step 1: the band never covers a title-bar gadget (the bug)

Facts:
- tx_span (A/flat.c:1237-1249) folds only `g->GadgetType & GTYP_SYSGADGET` (:1245). V
- tx_gad drag test `(GadgetType & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING` (A/flat.c:1234) with no SYSGADGET check. V
- flat_title_span_add (A/flat_core.c:863-878) already handles any gadget: skips drag, y >= bar_h, zero width; relright. V
- Lock / iconify: dopusbuttongclass with GA_RelRight + GA_TopBorder TRUE (D/Library/boopsi.c:509-528) -> GACT_TOPBORDER
  set, GadgetType GTYP_CUSTOMGADGET, no SYSGADGET. V (code). Images via NewObject("dopusiclass") on 68k (boopsi.c:497-506;
  sysiclass only under MORPHOS/OS4/AROS :476-494). V
- Hijacked zoom: D/Program/lister_window.c:309-332 clears GTYP_SYSTYPEMASK|GTYP_SYSGADGET, sets GadgetID 13, leaves
  Activation and Flags alone. Whether Intuition (KS 3.1) sets GACT_TOPBORDER on its own zoom gadget: U (no Intuition
  source in ~/Code/Reference; expected yes since V36 border sizing uses the border flags). When the lister is position
  locked (LISTERF_LOCK_POS) the zoom is NOT hijacked, only its image cleared (:316-321) -> still SYSGADGET. V
- Zoom cover gadget (graphics.c:154-166): GTYP_BOOLGADGET, Activation 0, same box as the zoom; added to the window only
  as size_cover when position-locked (lister_window.c:339-348). Folding it is harmless (same box). V

Change (S1.1), A/flat_core.c next to flat_title_span_add, declared in A/flat_core.h:364-373 block:
```c
/* FLAT-TITLE-GADGETS: 1 when a gadget belongs to the title bar for flat_title_span: a system gadget, a GACT_TOPBORDER
 * gadget (app iconify / lock / snapshot: DOpus, MUI, ReAction), or (no border flag at all, e.g. DOpus's hijacked zoom if
 * Intuition left GACT_TOPBORDER clear) a gadget wholly inside rows 0..bar_h-1 of a window that is not GIMMEZEROZERO
 * (in a GZZ window an inner gadget's TopEdge is inner-relative and would look like a title gadget). */
int flat_title_gadget(unsigned gadget_type, unsigned activation, int gzz, const flat_gadrect *g, int bar_h);
```
Rule: `(gadget_type & 0x8000) || (activation & 0x0010 /*GACT_TOPBORDER*/) ||
(!gzz && !(activation & 0x00E0 /*LEFT|RIGHT|BOTTOMBORDER*/) && g->y >= 0 && g->y + g->h <= bar_h)`.
Constants as literals with the names in comments, like flat_gadget_which (flat_core.c:352) - flat_core is host-built.
Reuse checked: flat_gadget_which / flat_border_button / flat_button_gadget (flat_core.c:350, 733, 738) answer other
questions (image lookup, GADGHIMAGE buttons); none is a title-membership test, so this is a new 4-line predicate.

S1.2, A/flat.c:
- tx_gad: `gr->drag = (g->GadgetType & GTYP_SYSGADGET) && (g->GadgetType & GTYP_SYSTYPEMASK) == GTYP_WDRAGGING;`
- tx_span: `for (g = w->FirstGadget, n = 0; g && n < 64 && ng < 8; g = g->NextGadget, n++) { tx_gad(w, g, &gr);
  if (flat_title_gadget(g->GadgetType, g->Activation, (w->Flags & WFLG_GIMMEZEROZERO) != 0, &gr, w->BorderTop))
  { flat_title_span_add(...); ng++; } }`. tx_gad moves before the test (the filter needs the resolved y/h). ng cap 8 now
  counts title candidates: DOpus lister = close, depth, zoom, drag, size?(no: size fails the filter unless SYSGADGET ->
  it is SYSGADGET, counted, then skipped by span_add), iconify, lock = 7 <= 8. V (lister gadgets: s69 uitree table).
  Walk cap 64 matches c2_which_31 (A/flat.c:604).
- Frame: tx_span gains one int (n); flat_title_gadget is a leaf. Check `_flat_tx_c` headroom in the budget output (>= 64).

Result: span on the DOpus lister ends at lock.LeftEdge - 1 = 348 (LeftEdges 349/373/398/421 of win_w 444, s70 evidence
test plan). Pixel aspect: the span is pixels, no glyph; unaffected. After step 1 alone the gadgets show DOpus art
(dopusiclass square / padlock, ROM zoom) in flat pens: visible, not yet flat-styled.

S1.0 (rig, before or with S1.2): dump the hijacked zoom's Activation/Flags on rig 68 (uitree prints type; if it does not
print Activation, read the Gadget struct with the rig's memory peek at the window's FirstGadget chain). Record in
s70-evidence. If GACT_TOPBORDER is set the geometry branch is dead for DOpus but stays for apps without border flags.

## Step 2a: hijacked zoom reaches the flat glyph/clone path

- KS 3.2+: c2_draw's `GetAttr(SYSIA_Which)` answers (OM_GET), which = ZOOMIMAGE, flat_clone_route -> c2_clone, else
  c2_glyph (A/flat.c:628-634). The hijacked zoom is already flat there; step 1 alone makes it visible. V (code)
- KS 3.1 (rig 68 is 3.1 per test-register rows s56, U for today's config): no OM_GET -> c2_which_31 (A/flat.c:599-610):
  img_lookup (arrows only) then flat_gadget_which over the gadgets, which needs GTYP_SYSGADGET (flat_core.c:352) ->
  NOWHICH -> stock ROM draw. V
- Intuition makes the window's zoom image through sysiclass NewObject at OpenWindow, so c2_disp_reg/stk sees OM_NEW and
  img_note_new (A/flat.c:509-528) reads SYSIA_Which = 1, but flat_img_record rejects it (flat_core.c:360: 0x0A..0x0D only). U
  (that Intuition 3.1 goes through the class dispatcher for its own window images; the arrows path proves the hook sees
  app NewObject calls, not Intuition-internal ones -> confirm on rig: STATUS / a debug count of recorded ZOOM entries).

Change:
- S2a.1 flat_img_record accepts which == 0x01 (ZOOMIMAGE) as well as 0x0A..0x0D. Not depth/close/size: those stay
  SYSGADGET and are found by the gadget walk; recording them would only fill the table. Header comment flat_core.h:237-245
  updated (DOPUS-ZOOM-WHICH).
- S2a.2 FLAT_IMG_RECS 128 -> 256 (flat_core.h:242): every zoom window now takes a slot (~8 bytes each, +1 KB static).
  c2_which_31: walk the gadgets first, then img_lookup (an app arrow or hijacked zoom is never a SYSGADGET render, so the
  order does not change any answer; the common system-gadget draw skips the Forbid + table walk). Full table = image
  stays ROM-drawn (existing fail-safe).
- Then the zoom goes the existing route: flat_which_alias -> c2_get_pens (on band: flat_glyph_on_band includes ZOOM,
  A/flat_glyph.c:212) -> flat_clone_route (clone of the ROM zoom, dividers peeled) else c2_glyph with side_px. Same look as
  close/depth by construction.
Gap (state in commit message): on KS 3.1 a zoom image created before flat installed its hook (lister opened before
amarchy-flat started, or flat restarted under open listers) is not in the table -> stock ROM 3.1 zoom art in flat pens
until the lister reopens. Not fixable without the image's private Which (3.1 sysiclass instance layout); out of scope.

## Step 2b: dopusiclass iconify / lock drawn by flat

How flat gets control (chosen: swap the class dispatcher slot, the existing C1 model):
- dopusiclass is a PUBLIC class: init_class -> MakeClass(name, "imageclass") + AddClass (D/Library/boopsi.c:51-63),
  created in the library's init (D/Library/libinit.c:1364-1365), freed only at expunge (boopsi.c:83 class_free). V
- On 68k its dispatcher sits in cl_Dispatcher.h_Entry directly (boopsi.c:57-60; h_SubEntry only on MorphOS), declared
  `image_dispatch(REG(a0) Class*, REG(a2) Object*, REG(a1) Msg)` (image_class.c:29). So the slot to swap is h_Entry
  (not h_SubEntry as for sysiclass, A/flat.c:8, :2117) and the replacement is ALWAYS register convention on every
  Kickstart (CallHookPkt jumps h_Entry with a0/a2/a1). V
- Find it with the existing class_slot (A/flat.c:2103-2126): throw-away NewObjectA(NULL, "dopusiclass", DIA_Type 0,
  IA_Width 8, IA_Height 8), OCLASS(o), DisposeObject. Generalise class_slot(frame) to
  `class_slot(const char *name, const struct TagItem *t, int entry)` (entry: h_Entry vs h_SubEntry); sysiclass/frameiclass
  callers unchanged in behaviour. No FindClass (not in the 3.1 intuition LVOs). U on FindClass absence; the object route
  works regardless.
- Install: in dopus_scan (A/flat.c:2209-2283) after the d4 gate succeeded (r->dlib held). Reason: the class lives exactly
  as long as dopus5.library; flat already holds an OpenLibrary count on it (A/flat.c:2221 "keeps it from expunging"), so
  the class cannot be freed under the hook. One lifetime, one gate, fail closed on unknown builds. Swap under Forbid with
  flat_swap_install(&r->d5sw, slot, (uintptr_t)d5_disp_reg); r->d5slot = slot.
- Remove: dopus_unpatch (A/flat.c:2186-2207) removes d5sw first (flat_swap_remove; FLAT_STILL_CHAINED -> keep the library
  open and report, as the d4 entries do); d4_installed() must count d5sw.installed so Ctrl-C refuses to unload while chained.
- Class created after flat starts: dopusiclass exists iff dopus5.library is in LibList; flat finds it on start and on
  Ctrl-D (`amarchy-flat DOPUS`, desk apply; A/flat.c:2577-2583, :2680-2687). Same constraint as FLAT-D4 today: DOpus
  started after flat is patched only after a rescan. U whether anything in dist sends DOPUS after DOpus starts (grep of
  dist found no caller) - existing D4 gap, not new; note in the plan's Found.

d5_disp_reg (A/flat.c, next to c2_disp_reg :636-647):
```c
ULONG d5_disp_reg(__reg("a0") struct Hook *h, __reg("a2") Object *obj, __reg("a1") Msg msg)
{
    if (msg->MethodID == IM_DRAW && obj && d5_draw(h, obj, msg)) return TRUE;   /* d5_draw: own function (budget) */
    return ((oldfn_reg)g_r->d5sw.saved)(h, obj, msg);
}
```
d5_draw: enabled/held/imp_RPort/imp_DrInfo checks as c2_draw (:620-623); `type = *(short *)((UBYTE *)obj +
((struct IClass *)h)->cl_InstOffset)` (BoopsiImageData.type is the first field, short: D/Library/boopsi.h:139-146 V;
h == the Class since cl_Dispatcher is IClass's first member, image_class.c:29 V); which = flat_dopus_image_which(type);
which < 0 -> 0 (stock). Then c2_get_pens(r, d, im, which, &pens) and c2_glyph(obj, msg, which, &pens) (A/flat.c:368, :581).
No clone: c2_clone renders through the saved SYSICLASS routine with its hook (A/flat.c:290-295), and there is no ROM
lock image at all and no 3.1 iconify image. r->d5draws++ on a drawn glyph.
DOpus draws these images with DrawImageState -> IM_DRAW (D/Library/button_class.c:845-869: IDS_SELECTED / NORMAL /
INACTIVE*). IM_DRAWFRAME is not used for them: stock path. V

Type map (S2b.3, flat_core, host-tested): IM_ICONIFY 6 -> FLAT_W_ICONIFY 0x16 (g_iconify, A/flat_glyph.c:84), IM_LOCK 8
-> FLAT_W_LOCK; every other DIA_Type (arrows, BBOX, gauge...) -1. IM_* values: D/Library/dopusbase.h:239-247 V.
Iconify state: DOpus fills FILLPEN/BACKGROUNDPEN by active state and draws the square in SHADOWPEN (image_class.c:307-347);
flat replaces it entirely: band box + g_iconify bar in the title text pen (c2_get_pens on-band branch, A/flat.c:382-386).

Lock glyph (S2b.1), A/flat_glyph.h: `#define FLAT_W_LOCK 0x3E` and `FLAT_W_LOCKED 0x3F` (private: above every SYSIA_Which
0..0x2C, so no sysiclass image can alias it; cf_mask bit = which & 31 = 30 / 31, unused). DOpus semantics (V,
image_class.c:500-553, lister_idcmp.c:1160-1165, lister_window.c:165-167): the lock is a GACT_TOGGLESELECT gadget; the
padlock is ALWAYS drawn; GFLG_SELECTED (= LISTERF_LOCK_FORMAT, format locked) draws the shackle "all the way down"
(closed), unselected draws it "a little way down" (open). So:
- flat_glyph_draw: for FLAT_W_LOCK, `closed = ids == 1 /*IDS_SELECTED*/ || ids == 6 /*IDS_INACTIVESELECTED*/`;
  which = closed ? FLAT_W_LOCKED : FLAT_W_LOCK; ids = closed ? (ids == 1 ? 0 : 5) : ids. The toggle state is carried by
  the SHAPE; colours stay the normal/inactive ones (no permanently inverted box). Owner decision O2.
- g_lock(plan *g, int closed), solid blocks like g_depth / g_zoom, sized in visual units (rows; one visual unit = g->side
  columns), inside the inner box ix0..ix1 / iy0..iy1 (plan_run :410-412):
  ```
  ih = iy1-iy0+1; iw = ix1-ix0+1;
  bh = max(2, (ih+1)/2);                         body rows (lower half)
  bw = min(iw, side * max(3, ih*2/3)); bw -= (iw-bw)&1 ? 1 : 0;  body columns, centred: bx0 = ix0+(iw-bw)/2
  t  = side;                                     leg thickness: 1 visual unit = side columns
  sh = ih - bh;                                  shackle rows above the body (>= 1)
  body:   box(bx0, iy1-bh+1, bx0+bw-1, iy1, fg)
  top:    put(bx0+t, iy0, bx0+bw-1-t, iy0, fg)   1 row
  left:   put(bx0+t, iy0, bx0+2t-1, iy1-bh, fg)  down to the body
  right closed: put(bx0+bw-2t, iy0, bx0+bw-1-t, iy1-bh, fg)
  right open:   put(bx0+bw-2t, iy0, bx0+bw-1-t, iy0 + max(0, sh/2-1), fg)   stops short: a gap of >= 1 row above the body
  ```
  If bw < 4t+1 (tiny box) draw the body only (still reads as a lock state-less; logged as degenerate in the test).
  Example side 1, gadget 24x14 (inner 6..17 x 3..10, ih 8, iw 12): bh 4, bw 5, body x 9..13 rows 7..10, shackle legs
  cols 10 and 12, top row 3 cols 10..12, open right leg rows 3..4. Side 2 (hires, same 14-row bar): bw = 2*5 = 10,
  legs 2 columns. Executor fixes the exact goldens from the generator and pastes them in the test; the rule above is the spec.
- S2b.2: flat_glyph_known, flat_glyph_window_gadget, flat_glyph_on_band (A/flat_glyph.c:167-212) gain LOCK/LOCKED;
  flat_clone_route (:227-230) gets `&& which != FLAT_W_LOCK && which != FLAT_W_LOCKED` (defensive: d5 never calls the
  clone, but the predicate must say the truth). plan_run switch (:415-425) calls g_lock(g, which == FLAT_W_LOCKED).
- No divider lines / no box art: plan_run paints the whole box in the band role then the glyph (:413); nothing else.

Risks (2b):
- Library expunge: impossible while flat holds its open count (A/flat.c:2221); dopus_unpatch removes the swap before
  CloseLibrary. A DOpus quit with flat running: the library stays resident (flat's count) - same as D4 today. V
- Foreign patch on the same h_Entry (another theme tool): flat_swap_install refuses a non-original slot? U - read
  flat_swap_install's contract in flat_core.c before coding; on refusal leave dopusiclass stock and report in STATUS.
- Instance layout: `type` read at cl_InstOffset+0. Stable in all 5.x sources (boopsi.h). The d4 gate ties it to known
  builds, so an unknown layout never reaches d5.
- DOpus internal calls of the dispatcher that bypass h_Entry (e.g. image_dispatch called directly): grep showed only
  image_dispatch_entry wrapper (image_class.c:580-586) for the BOOPSI entry; DrawImageState goes through the hook. U for
  other direct calls; rig sentinel (d5draws) proves the hook path is live.
- Stack: d5_disp_reg runs on the caller's (DOpus/Intuition) stack: chain d5_disp_reg -> d5_draw -> c2_get_pens /
  c2_glyph -> flat_glyph_draw -> plan_run -> g_lock -> put -> fill. Same depth as the c2_draw glyph branch, minus c2_draw's
  frame, plus d5_draw's (keep c2_pens in d5_draw, not d5_disp_reg). Add `_d5_disp_reg` to ENTRIES
  (T/test_flat_budget.py:23); ENTRY_MAX 64, HEADROOM_MIN 64. _c2_disp_reg headroom is only 68 today (101686d5 message):
  g_lock must keep its locals to ints in registers (no address-taken locals; see plan_roles split, A/flat_glyph.c:397-405)
  or plan_run's frame grows and _c2_disp_reg drops under 64.
- flatshared layout: add d5sw/d5slot/d5draws at the END of flatshared (A/flat_shared.h); check its version/size rule
  (test_layout pins flat_state, not flatshared - U; read flat_shared.h header before editing).

## Test plan

Host (T/test_flat_core.c; flat.c is Amiga-only and not host-linked, so the host tests drive the extracted flat_core /
flat_glyph functions):
1. test_title_gadget_filter (S1.3): flat_title_gadget(0x0005, GACT_TOPBORDER 0x10, 0, iconify rect, 14) = 1;
   (0x0005, 0, 0, rect y 0 h 14, 14) = 1 (geometry branch); same with gzz = 1 -> 0; (0x0005, GACT_RIGHTBORDER 0x20, 0,
   y 0 h 14) = 0; content gadget y 20 -> 0; (0x8040 sys depth, 0) = 1. Drag: the tx_gad rule is in flat.c; add
   `flat_title_drag(gadget_type)` to flat_core alongside if the executor wants it host-tested (recommended: same commit).
2. test_title_span_dopus_lister (S1.3): win_w 444, bar_h 14, rects close {0,0,20,14}, depth {-23,0,24,14,relright},
   zoom {-46,0,24,14,relright} type 0x0005 act 0x10, iconify {-71,0,24,14,relright} type 0x0005 act 0x10, lock
   {-95,0,24,14,relright} same, drag {0,0,-n,14,relwidth,drag}, size relbottom. Fold through flat_title_gadget +
   flat_title_span_add exactly as tx_span does: x0 == 20, x1 == 348. Revert check: SYSGADGET-only filter gives 420.
   (Exact LeftEdges/widths: take from s69 uitree table; 349/373/398/421 of 444 per s70 evidence.)
3. test_img_record_zoom (S2a.3): flat_img_record(..., 0x01, ...) = 1 and flat_img_which returns 1; 0x00 / 0x03 still 0
   (keeps T/test_flat_core.c:1067); table of 256 fills and the 257th returns 0 (update :1075-1076 to FLAT_IMG_RECS).
4. test_dopus_image_which: 6 -> 0x16, 8 -> FLAT_W_LOCK, 0/5/7/9/-1 -> -1.
5. test_lock_glyph: flat_glyph_draw on the cv canvas (helpers T/test_flat_core.c:492-516) for FLAT_W_LOCK at 24x14:
   side 1 ids 0 (open: gap row between right leg and body), ids 1 (closed: right leg reaches the body, colours = ids 0
   colours, NOT inverted), ids 5 and 6 (inactive open/closed: inactive glyph pen); side 2: leg width 2 columns, body
   width 2x side-1 width, same rows. Golden rows from the generator. Also flat_glyph_known / window_gadget / on_band
   true, flat_clone_route false for both lock ids.
6. Existing: test_glyph_route (:801), test_glyph_states (:626), test_arrow_aspect (:1976) stay green.

Reachability (ONE, the feature as a whole): host cannot link flat.c, so the reachability proof is the rig run through the
real entry points (Intuition -> sysiclass hook c2_disp_reg for the zoom; dopusbuttongclass GM_RENDER -> DrawImageState
-> dopusiclass h_Entry -> d5_disp_reg for lock/iconify; Text -> flat_tx_c -> tx_span for the band), with sentinels:
- STATUS gains `d5=<d5draws>` (S2b.6); r->ttitle and r->cdraws already exist (A/flat.c:1256, :564).
- Rig 68 steps (purpose text `rig.py start --purpose "<session>: DOpus lister flat title gadgets"`), build from main:
  1. amarchy-flat running, `amarchy-flat STATUS` -> dopus=<v.r> patched, d5=0.
  2. Open a new DOpus lister (after flat), activate it. STATUS d5 >= 2 (iconify + lock drawn), ttitle grew.
  3. ami.grab rows 0..BorderTop-1, x = 349..444: inside each of the three gadget boxes there are glyph-pen pixels on the
     band pen; no SHINE (pen 2) / SHADOW (pen 1) columns (no dividers); band pixels end at x 348. Before the fix: x 335..420
     all accent (s69 scan).
  4. Click the lock: shackle closes (glyph pixels in the gap row), d5 grows; click again: opens.
  5. Deactivate the window: inactive pens, glyphs still present.
  6. Repeat 2-5 on a hires (side 2) screen: legs 2 columns wide, the lock not squashed.
  Delete the grabs after recording the verdict (keep a FAIL grab until fixed).
- Owner eye check (Blocked on owner, LOCAL rig 68 window): lister title bar shows flat lock / iconify / zoom glyphs
  matching close/depth, no dividers, on the square-pixel DOpus screen and on hires. Start the rig and open the lister
  before asking (memory: eye-check-rig-ready).

## Files touched
- A/flat_core.h, A/flat_core.c: flat_title_gadget (+ optional flat_title_drag), flat_img_record widening, FLAT_IMG_RECS,
  flat_dopus_image_which.
- A/flat_glyph.h, A/flat_glyph.c: FLAT_W_LOCK / FLAT_W_LOCKED, g_lock, lock-state rule in flat_glyph_draw, known /
  window_gadget / on_band / clone_route.
- A/flat.c: tx_gad, tx_span, c2_which_31 order, class_slot generalised, d5_draw / d5_disp_reg, dopus_scan install,
  dopus_unpatch + d4_installed, STATUS d5 word.
- A/flat_shared.h: d5sw, d5slot, d5draws (end of struct).
- T/test_flat_core.c (tests 1-5), T/test_flat_budget.py (ENTRIES).
- thoughts/shared/s70-evidence/dopus-blank-gadgets.md: S1.0 measurement and rig verdicts.

## Owner decisions (veto-able)
- O1 Scope: step 2b (dopusiclass hook, new lock glyph) is a new class hook in DOpus's library; step 1 alone makes the
  gadgets visible with DOpus art. Recommended: do all three steps (owner already chose flat glyphs in s70).
- O2 Lock toggle look: closed shackle = locked, colours unchanged (recommended, DOpus semantics) vs inverted box when
  locked (the generic flat SELECTED look; reads as a stuck-pressed button).
- O3 "Same look as close/depth": on every ROM close/depth/zoom/iconify take the CLONE route (ROM art peeled, flat pens)
  where a ROM image exists (A/flat_glyph.c:224-230). Lock and 3.1 iconify have no ROM image, so they are pure flat glyphs;
  they will be geometric solid shapes next to cloned ROM shapes. Recommended: accept; alternative is a separate decision to
  drop the clone route for title gadgets (out of this plan).
- O4 Pre-flat listers on KS 3.1 keep the ROM zoom art until reopened (gap in 2a). Recommended: accept.

## Step 3 (PRIORITY, owner s70 clarifications 1-3): no frames around DOpus lister buttons

What the owner means: the frames around the buttons in the strip over the lister toolbar (owner shot 01:18, 196x32 crop):
down-arrow button, "<" parent button, "SRCE/DEST/OFF" state box. These are not gadgets; they are GUI_Element areas of the
lister status row:
- name_area (path/name), command_area (down arrow = command popup), parent_area ("<"), status_area (SRCE/DEST/OFF text),
  laid out in D/Program/lister_display.c:105-131 with 3 px pitch (`rect.MaxX + 3`): each area 1 px frame + 1 px gap. V
- Content: lister_title.c:270-345 - lister_title_pens (:379-420: SOURCE / DEST colour pens, OFF = TEXTPEN on
  BACKGROUNDPEN) then lister_clear_title_area = RectFill of the area interior (:692-695), then BltTemplate arrow
  (command, parent) or the SRCE/DEST/OFF string (status, :337-345). No frame there. V
- FRAME: lister_display.c:276-280 `display_gui_border(&name_area / &command_area / &parent_area / &status_area)` ->
  D/Program/display.c:927-939 rect grown by 1 px each side -> do3dbox(rp, &border, 0) (graphics.c:27-30) ->
  DrawBox(rp, rect, GUI->draw_info, FALSE) through the dopus5.library JUMP TABLE (LVO -696). V
- Same route (Program do3dbox -> LVO -696) for: lister toolbar buttons (lister_toolbar.c:199, 378, 388, 472; :627 recessed
  = pressed), column header / title boxes (lister_title.c:784, 914, 1015, 1039 sel), dual-lister header and close buttons
  (lister_dual.c:1481 display_gui_border, :1497, :1549), lister_idcmp.c:923. V (grep of lister*.c). Button banks: not
  checked (U; Program/buttons*.c - executor greps do3dbox / DrawBox there before step 3 lands).
- Title lock / iconify: dopusbuttongclass with an image in the top border draws ONLY the image (button_class.c:839-874:
  DrawImageState, return; no border code) -> their "frame" is the dopusiclass art itself (IM_ICONIFY shadow border, IM_LOCK
  fill), removed by step 2b. V

What flat does with that frame today (why the owner sees a frame):
- dopus_scan patches LVO -696 (FLAT-D4); every call enters d4_drawbox (A/flat.c:914-919) -> d4_flat -> d4_flat_pens
  (:885-901). Under flat pens flat_dopus_decide (A/flat_core.c:280-293) says SKIP, and d4_flat_pens then paints
  flat_dopus_box_draw (A/flat_glyph.c:478-481) = flat_frame_draw FLAT_ROLE_PLAIN = a 1 px (side columns) RING in pen_line.
  That ring is the frame. It was added on purpose: DOPUS-ENV-UNSTYLED (A/flat.c:882-884, owner, DOpus Environment, rig 63):
  "drawing nothing left every DOpus button and field without a border". V
- Discriminator that exists already: flat_core.h:161-163 - dopus5's gadget CLASSES (Environment buttons, fields) call the
  DrawBox / DrawFieldBox BODIES by absolute jsr, so they arrive as FLAT_EL_BEVEL / FLAT_EL_FIELDBOX body hooks
  (d4_bevelbody / d4_fieldbody); Program do3dbox (lister status row, toolbar, headers) arrives as FLAT_EL_DRAWBOX via the
  jump table. The rig-63 decision was about the class-drawn buttons/fields. U: that no Environment button reaches LVO -696
  (verify with dcalls/dbevel counts on the Environment window before and after).

Change (reuse, no new painter):
- S3.1 flat_core: extend the decision with a third result, e.g. `FLAT_DRAW_FACE` ("flat button: no ring"), returned by a
  new pure `flat_dopus_ring(element, recessed)` (or an extra out-param of flat_dopus_decide): element FLAT_EL_DRAWBOX and
  recessed == 0 -> no ring; FIELDBOX / BEVEL / recessed DrawBox -> ring as today (fields, file-list border, pressed
  toolbar button stay outlined). Same rule as FLAT-BUTTON-NO-RING for flat's own buttons (flat_core.h:458-462: push
  buttons are a fill, no ring; fields and bevel boxes stay outlines).
- S3.2 A/flat.c d4_flat_pens: on FACE, paint the ring band (the 1 px border rectangle only, never the interior: DOpus
  fills and draws the content itself, and a DrawBox after content would erase the arrow / SRCE text) in the FACE pen of
  that area, not pen_line. Face pen = the pen of the interior: `ReadPixel(rp, x0 + side, y0 + 1)` (inside the border).
  Result: SRCE / DEST areas read as solid coloured blocks 1 px larger, OFF / BACKGROUNDPEN areas read as plain arrow /
  text with no box. Reuse flat_frame_draw with a solid-ring role/pen argument, or call c2_fill four times through the
  existing fill callback - no new drawing helper. Ring width from st.side (side_px of the drawing screen) as today.
  U: ordering. If DOpus draws the border BEFORE the content fill on some path (lister_display.c:276-280 runs at full
  refresh; the content refresh lister_title.c:270+ may run after), ReadPixel reads the old pixel. Then the content
  RectFill only covers the interior, so a stale ring colour stays until the next full refresh. Measure on rig 68 (S3.4);
  if wrong, fall back to: ring band in BACKGROUNDPEN (= "draw nothing", the area fill alone forms the button) - owner O5.
- S3.3 host tests (T/test_flat_core.c): decision table (DRAWBOX raised -> FACE; DRAWBOX recessed, FIELDBOX, BEVEL -> ring;
  stock pens -> STOCK); ring-band draw on the cv canvas: border pixels take the face pen, interior pixels untouched,
  side 1 and side 2 widths. Revert check: ring in pen_line.
- S3.4 rig 68 proof: lister with SRCE, DEST and OFF states; ami.grab the status row (rows of name_area.rect -1..+1):
  no pen_line pixels on the 1 px border of command / parent / status areas; interior arrow / text pixels unchanged;
  Environment window buttons still ringed (rig-63 decision kept). Sentinel: new counter r->dface in STATUS
  (`dopus=... face=N`), N > 0 after the lister refresh.
- Budget: d4_flat_pens runs inside the patched DrawBox on DOpus's stack; ReadPixel adds a graphics call frame. Add
  `_d4_drawbox` to T/test_flat_budget.py ENTRIES if not there (U; it lists 4 entries today) and keep headroom >= 64.

Checklist additions (total now 22): S3.1 decision, S3.2 ring-band draw, S3.3 host tests, S3.4 rig proof, S3.5 button-bank
grep (do3dbox users outside lister*.c) recorded in s70-evidence.

## Owner decisions added by the clarifications
- O5 Status-row look: (Recommended) frame band in the area's own fill pen - SRCE/DEST become solid blocks, OFF shows bare
  arrow / text; alternative: no ring and no band paint (window background shows in the 1 px gap; same look for OFF, a
  1 px background line around SRCE/DEST).
- O6 Scope of "no ring": (Recommended) every raised Program DrawBox (status row, toolbar buttons, header boxes, dual close
  buttons) - one rule, matches FLAT-BUTTON-NO-RING; alternative: status row only, which needs a geometry match against
  lister areas (fragile, no reuse).
- O7 Keep the rig-63 decision for class-drawn Environment buttons and fields (ring stays). Recommended: keep; revisit
  only if the owner wants no rings anywhere in DOpus.

## Owner decisions (s70, 2026-11-10)
- O1: all three steps (S1 band, S2 glyphs, S3 rings). Owner chose flat glyphs earlier in s70.
- O2: lock shows open vs closed shackle, same pens; no inverted box.
- O3: lock/iconify solid flat glyphs, close/depth/zoom keep cloned ROM shapes; owner judges at the rig eye check (follow-up if they clash).
- O4: pre-flat listers on KS 3.1 keep ROM zoom until reopened: accepted gap (default, not vetoed).
- O5: status-row 1 px border painted in the box's own fill pen (one flat block, no frame).
- O6: no ring for every raised Program DrawBox (status row, toolbar buttons, header boxes).
- O7: Environment buttons and fields keep their rings (rig 63 decision stands).
