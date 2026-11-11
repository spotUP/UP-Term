---
date: 2026-11-11
topic: Time preferences year + month row full width -- pure core (step 1 of 2)
status: core + host tests done; Amiga glue (gadtools CreateGadgetA hook) is step 2
branch: s73-flat-time-row-full-width
---

# Time preferences top row full width: the pure core

Background and measurements: `thoughts/shared/s73-evidence/flat-time-row-full-width.md`. This step adds only pure,
host-tested logic to `amarchy/flat_core.c` / `amarchy/flat_core.h` (section S74-TIME-ROW). `amarchy/flat.c` is unchanged.

## What was searched and what is reused

- Gap: searched flat_core.h/.c for `spacing`, `gap`, `inset`, `FLAT-SPACING`. The patcher has no gap constant. Its
  "normal box gap" is the FLAT-SPACING rule, `flat_inset` (flat_core.h:759-763, flat_core.c:1395): every filled flat box
  is drawn shrunk by `side` columns left and right and 1 row top and bottom, so two program boxes that TOUCH show
  `2 * side` columns of window ground between them (the calendar cells and the button row show this on rig 68). Reused
  as is: the month is placed so its program box touches the year's frame box, which gives exactly the gap the rest of
  the screen shows. Placing the month at "year right + 2 * side" would double the gap (inset ground + program space).
  When a box is too small for `flat_inset` (it then leaves the box as it is), its `side` share of the gap is added as
  program space instead, so the visible ground stays `2 * side` in every case.
- Aspect: searched for `aspect`, `side_px`. The one per-screen source is `flat_side_px` (flat_core.c:1337) stored per
  screen in the `flat_aspect_slot` table and read with `flat_aspect_find` (flat_core.c:1346). The core takes that
  `side` as an argument; no second aspect computation. Vertical distances are not touched (only left edges and widths
  change), so only the horizontal `side` matters.
- Name compare: `ci_prefix` (flat_core.c:481, the FLAT-C4 kill-switch name compare) is reused for the case-blind match.

## Functions (flat_core.h, section S74-TIME-ROW)

    void flat_trow_init(flat_trow_tab *t);
    int  flat_trow_name(const char *prog);
    int  flat_trow_year(int row_left, int frame_out, int yl, int yw, int *new_left);
    int  flat_trow_month(int side, int frame_out, int ynl, int yw, int yt, int yh, int yr,
                         int ml, int mt, int mw, int mh, int *new_left, int *new_width);
    int  flat_trow_feed(flat_trow_tab *t, const void *task, const char *prog, int side, int frame_out,
                        const flat_trow_ev *e, int *new_left, int *new_width);
    void flat_trow_drop(flat_trow_tab *t, const void *task);

`flat_trow_ev` = { kind, left, top, width, height, id } of one CreateGadgetA call (GadTools kind, NewGadget
ng_LeftEdge/TopEdge/Width/Height/GadgetID). `flat_trow_feed` returns 1 = "create it with *new_left / *new_width", 0 =
"unchanged" (outputs untouched). Kinds: FLAT_GT_BUTTON 1, FLAT_GT_INTEGER 3, FLAT_GT_CYCLE 7 (gadtools.h).

## Identity rule and why

A task's sequence matches only when ALL hold, checked as each call arrives:
1. FilePart of the program name (text after the last `:` or `/`) equals `Time`, case ignored (AmigaDOS names are
   case-blind). Other programs never take a state slot.
2. The task created at least one BUTTON first, and only BUTTONs before the INTEGER. Row left = the leftmost of those
   BUTTONs' NewGadget left. On 39.1 the buttons Save (12), Use (122), Cancel (233) come first and Save is both first and
   leftmost, so "first" and "leftmost" agree; leftmost was chosen because it is the row's left edge by definition and
   does not depend on creation order inside the button row. The calendar (the owner's original reference, also at 12)
   is created after the GadTools gadgets, so it cannot be the source.
3. The INTEGER has gadget id 14, is not on the buttons' row (vertical spans do not overlap: no earlier gadget can sit in
   the space the year moves into), and row left + frame_out < its left (it really moves left).
4. The very next call is a CYCLE with id 15 whose vertical span overlaps the INTEGER's and whose left lies right of the
   INTEGER's stock frame right edge.

Gadget ids 14 and 15 ARE required. Reason: the INTEGER must be decided when it arrives (the CYCLE comes after, the
sliders much later), so the year move cannot be undone if the CYCLE then mismatches. The ids make a different program
named "Time" match only if it also uses ids 14/15 in this order after a button row, so a stranger stays stock rather
than getting a half-moved row. The 39.1 absolute geometry (54, 142 ...) is NOT required: it would break with any other
font, and the relations above (order, rows, left-of) already carry the shape.

On every mismatch the slot is freed (state reset), the gadget is created stock, and the reason goes to `t->why`:
NOBUTTON (INTEGER before any BUTTON), ORDER (another kind before the INTEGER, or a button on its row), ID, GEOM (row left
not left of it, negative or empty boxes), NOCYCLE (INTEGER moved, next gadget is no CYCLE id 15), ROW (CYCLE not on the
row, not right of the INTEGER, or would not grow). NOCYCLE and ROW are the only cases that leave the year moved and the
month stock: the year then sits at the row left with ground between it and the month; nothing overlaps because the
year only moved left into space no earlier gadget holds (rule 3). With the id rule this needs a program named Time
that creates INTEGER 14 then something other than CYCLE 15.

## Geometry

- Year: new left = row left + frame_out, width kept (`flat_trow_year`). frame_out = columns the drawn INTEGER frame
  stands outside the NewGadget box (0 when NewGadget carries the frame box).
- Month: year frame box = new left - frame_out .. new left + width - 1 + frame_out. Month new left = year frame right +
  1, plus `side` for each of the two boxes that `flat_inset` cannot shrink (vertical extent from the NewGadget top and
  height). Month new width = old right edge - new left + 1, so the right edge stays. Refused (0) when it would not
  grow (new left >= old left), when not on the row, not right of the INTEGER, or any value is negative/empty. Since
  the year only moves left and the month must lie right of the stock year, a valid month always grows, so a "too
  narrow" month cannot happen; the guard covers it anyway.
- Measured 39.1, square pixels (side 1), NewGadget = frame box 54 w53: year -> 12 w53 (frame 12..64), month 142 w178 ->
  65 w255 (right edge 319 unchanged), ground between 64..65 = 2 columns. PAL hires (side 2): same program boxes, ground
  4 columns. No font-size constants: every size comes from the NewGadget values.

## NewGadget inner/outer assumption (rig step must verify)

ASSUMED: for INTEGER_KIND the NewGadget box is the drawn frame box (documented GadTools behaviour: ng_Width/Height
cover the whole gadget including its bevel; GadTools makes the string gadget inside it). Then the glue passes
frame_out = 0. uitree reports the inner gadget 60,35 41x8 and the frame at 54..106, i.e. 6 columns outside.
VERIFY on the rig: log the INTEGER's NewGadget left/width in the hook. If it reads 54/53, frame_out = 0. If it reads
60/41, pass frame_out = 6 (the test `test_trow_inner_box` covers this: the frame then still lands at 12). PASS on screen
after the hook: year frame left at screen x 12 (Save's left), month frame right still at 319, 2 columns of ground
(4 on a non-square screen) between them.

## Call sequence for the glue (step 2)

- One static `flat_trow_tab` in the flat shared block, `flat_trow_init` at start. FLAT_TROW_SLOTS = 4 states at most;
  only tasks whose program is Time take one; a slot is freed when its sequence ends (row changed or any mismatch); when
  all four are busy the oldest (by event clock) is evicted (a task that died mid-sequence).
- Key: the creating task pointer, `FindTask(NULL)` inside the CreateGadgetA patch (the task calling gadtools).
- Program name: for a CLI process `pr_CLI` -> `cli_CommandName` (BSTR, convert), else the task's `tc_Node.ln_Name`
  (Workbench start). Passed as is; the core takes the FilePart.
- side: `flat_aspect_find` for the screen the gadgets are for. OPEN: NewGadget only carries ng_VisualInfo (private);
  the glue needs the screen without reading private fields. Time opens on the default public screen, so the default
  public screen (or the Workbench screen) is the proposed source; the rig step decides.
- For each CreateGadgetA(kind, prev, ng, tags): if amarchy-flat is enabled and not held, call
  `flat_trow_feed(&tab, task, name, side, frame_out, &ev, &nl, &nw)`; on 1 copy *ng into a private NewGadget on the
  hook's stack (budget entry), set ng_LeftEdge = nl, ng_Width = nw, call the original with the copy; on 0 call the
  original with the program's ng. Never write the program's NewGadget.
- Optional `flat_trow_drop(&tab, task)` when the glue sees the task's window close; not required for correctness.
- If CreateGadgetA returns NULL for a changed call, nothing else is needed (the slot already ended or ends on the
  next call).

## STATUS fields to expose

- `trow_hook` installed 0/1 (the gadtools LVO swap is in place),
- `trow_rows` = `tab.rows` (rows changed so far; the reachability sentinel),
- `trow_why` = `tab.why` (last mismatch reason, FLAT_TROW_* name; OK after a changed row).

## Tests (tests/test_flat_core.c, suite flat_core)

test_trow_name, test_trow_time39 (measured layout numbers above), test_trow_inner_box (frame_out 6),
test_trow_aspect (side 2, and boxes too small for flat_inset), test_trow_big_font (year 70 w69, month 186 w240 ->
12 / 81 w345, right 425), test_trow_mismatch (other program, every FLAT_TROW_* reason, INTEGER then non-CYCLE,
degenerate widths), test_trow_tasks (two Time tasks interleaved, drop).

Results: see the "Test results" section below.

## Test results

- `make test ONLY=flat_core` (host, c89 -pedantic -Werror, ASan/UBSan): before this change 4389 checks; after
  4474 checks, 0 failed (85 new checks in the seven test_trow_* functions).
- Bite proof: month placement broken on purpose (`nl = yfr + 2` instead of `yfr + 1` in flat_trow_month): 4474 checks,
  16 failed (time39, inner box, aspect, big font, tasks: month left 66 want 65, width 254 want 255, ground 3 want 2 /
  5 want 4). Restored: 4474 checks, 0 failed.
- Build note: `make test` regenerates build/amiga/menu_icons.inc and needs python3 with fontTools + PIL; the shell
  python3 and the main checkout's .venv lack fontTools; /opt/homebrew/bin/python3.11 has both (put first on PATH).
