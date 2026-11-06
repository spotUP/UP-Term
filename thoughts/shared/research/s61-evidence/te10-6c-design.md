---
date: 2026-11-06
topic: TE-10.6c per-screen theming, amarchy-flat role pens on themed foreign screens
status: design, not implemented
ledger: thoughts/shared/todos.md TE-10.6C (row near :4345)
source: snapshot of origin/main (amarchy/), file:line below refer to that snapshot
---

# TE-10.6c design: flat role pens per themed foreign screen

## 0. Problem in one paragraph

amarchy-flat draws its flat look from one live record `r->st` (flat_state, flat_core.h:18-54). Its role pens
(pen_outline, pen_button, pen_ptext, pen_dimtext, pen_text, pen_line, pen_back) are Workbench pen numbers written by
desk_flat_state (desk_core.c:118). TE-10.6a/b added a per-screen draw table g_draw (flat.c:319); its slots copy the look
and overlay only the four pens amarchy-flat obtains itself (flat_state_for, flat_core.c:972). The role pens in a foreign
screen's slot are therefore still Workbench pen numbers: on a themed foreign screen they index that screen's palette at
the wrong place. The fix gives each themed slot its own role pens, taken from the pens the Amarchy pen record already
holds on that screen, resolved on the main task only.

## 1. Gap found in the brief's premise

The brief says slots hold plan pens "keyed by element GTE_*". The code does not:

- ip_holder stores `pcount; ppen[GTE_COUNT]; prgb[GTE_COUNT]` (iconpen_core.h:59-79): pen and colour pairs, no element.
- Only HELD exclusive entries are stored; ip_plan_sync compares by pen and rgb (iconpen_core.h:115, iconpen_core.c:105).
- The plan holds only colours no role pen carries, and exclusive pens only at or above `held`
  (gthemes_core.h:426-450; held per depth at gthemes_core.c:1924: 4/8/16/19/27).
- Slots >= 1 hold only plan pens (iconpen_core.h:152).

So "which pen draws GTE_BUTTON_FACE on screen N" is not stored anywhere. It must be computed: the theme colour of the
element (gt_elem_rgb, gthemes_core.h:294) matched against the slot's held pens plus the screen's own ColorMap. The theme
per slot is already in the pen record: `ipd_rec.st[IP_SCREENS]` (iconpen_dos.h:40), each with `t` (theme with model),
`flat`, `mask`, `have` (iconpen_dos.c:288-306, pens_select). Whether a slot is themed is computed there too:
`hold[i] = owner && gt_screen_theme_of(&r->rules, name, ...)` (iconpen_dos.c:349-353).

## 2. Mapping table (answer to point 1)

One table, one pure module: new `amarchy/flat_role_core.c/.h` (host-built, no AmigaOS headers). Each row: flat_state
field, the GTE element whose colour it paints, the GR role used when the theme leaves the element unset (gt_elem_rgb
already falls back to the role, gthemes_core.h:294), and the rule when no pen carries the colour.

| flat_state field         | element (gthemes_core.h:204-210)  | fallback role   | no-pen rule                         |
|--------------------------|-----------------------------------|-----------------|-------------------------------------|
| pen_outline (:29)        | GTE_BORDER                        | GR_ACCENT       | nearest pen in the screen ColorMap  |
| pen_button (:30)         | GTE_BUTTON_FACE                   | GR_SELECTION    | nearest pen                         |
| pen_ptext (:31)          | GTE_BUTTON_TEXT                   | GR_SURFACE      | nearest pen                         |
| pen_dimtext (:33)        | GTE_GADGET_GLYPH_INACTIVE         | GR_MUTED        | nearest pen                         |
| pen_text (:26)           | GTE_GADGET_GLYPH                  | GR_TEXT         | DrawInfo TEXTPEN                    |
| pen_line (:27)           | GTE_BORDER                        | GR_ACCENT       | DrawInfo SHADOWPEN                  |
| pen_back (:28)           | GTE_REQUESTER_BACKGROUND          | GR_SURFACE      | DrawInfo BACKGROUNDPEN              |
| pen_dim, pen_knob,       | not mapped: amarchy-flat's own    | -               | stay 0 on a foreign screen (stock   |
| pen_inbar, pen_sel       | pens, g_pens Workbench only       |                 | fallback paths already handle 0)    |

The Workbench rows are the ones desk_flat_state writes today (desk_core.c:118: pen_outline = GR_ACCENT,
pen_button = GR_SELECTION, pen_ptext = GR_SURFACE, pen_dimtext = GR_MUTED, text/line/back from desk_flat_glyph_pens).
The element column is chosen so that a theme with no per-element overrides gives exactly the role colour, so the
foreign screen matches the Workbench look. UNCONFIRMED: whether pen_line should follow GTE_BORDER or the glyph pens;
desk_flat_glyph_pens decides it on the Workbench and the brief author (sonnet 1) reads it and copies its rule.

CORRECTED 2026-11-06 (brief 1, on main a43acea5): the first draft of this table had pen_line = GR_TEXT and
pen_back = GR_BACKGROUND. desk_flat_glyph_pens (desk_core.c:104-109) gives text = GR_TEXT, line = GR_ACCENT,
back = GR_SURFACE; the table above and `g_rows` in amarchy/flat_role_core.c now carry that. The UNCONFIRMED point is
closed: pen_line follows GTE_BORDER with fallback GR_ACCENT. Caveat: a theme that gives GTE_BORDER its own colour
moves outline and line together.

API as landed (amarchy/flat_role_core.h): `flat_role_row_of(i)` (table row), `flat_role_rgb_of(t, rgb)` (the seven
theme colours of a gtheme), `flat_role_resolve(...)`, `flat_role_apply(st, p)`. The distance is `gt_rgb_dist2`
(gthemes_core.c, shared with `gt_nearest_role_pen`). Not yet linked into amarchy-flat: brief 3 links it and checks
`make flat-budget`.

API (pure, host-tested):

```c
typedef struct { short pen[FLAT_ROLE_COUNT]; uint8_t ok; } flat_role_pens;
/* rgb[] = theme colour per row (from gt_elem_rgb), held[]/hrgb[] = the slot's plan pens, cmap[] = screen colours
 * (2^depth entries), dri[] = the screen's DrawInfo pens for the DrawInfo rows. Exact rgb match on a held pen first,
 * then exact match in cmap, then nearest in cmap (same distance as gthemes' fold). Pure: no allocation, no OS call. */
void flat_role_resolve(const uc_u32 *rgb, const short *held, const uc_u32 *hrgb, int nheld,
                       const uc_u32 *cmap, int ncolours, const short *dri, flat_role_pens *out);
void flat_role_apply(flat_state *st, const flat_role_pens *p);   /* writes the seven fields, nothing else */
```

Reuse: the nearest-colour distance must be the one gthemes already uses for "fold to the nearest of N pens"
(gthemes_core.c:1879-1943). Brief 1 extracts or calls that function; it does not write a second distance.

## 3. Per-screen pen source at sync time and epoch (point 2)

> CORRECTED 2026-11-06 (section 11): item 1's "fills the per-row theme colour from `st[slot].t`" and item 3's epoch rule
> are wrong. Section 11 holds the rule that briefs 2 to 4 follow.

All work runs in draw_sync (flat.c:1812) on the amarchy-flat main task, the place that already lists the public screens
with ipd_epoch_of_screen and fills g_draw under Forbid (flat.c:1812-1836).

1. New pen-record reader in iconpen_dos (main task only, takes `r->ss` like ipd_epoch_of_screen, iconpen_dos.c:432):
   `int ipd_screen_roles(const void *scr, const uc_u32 *row_rgb_out, short *held, uc_u32 *hrgb, int cap, int *themed)`.
   It finds the slot by screen pointer, reports `themed` (the same gt_screen_theme_of result as hold[i],
   iconpen_dos.c:351), copies the slot holder's ppen/prgb, and fills the per-row theme colour from `st[slot].t`
   with gt_elem_rgb. No obtain, no ColorMap write. Returns the slot epoch.
2. draw_sync, per listed non-Workbench screen: call the reader, read the screen's ColorMap (GetRGB32, read only) and
   DrawInfo pens (GetScreenDrawInfo / FreeScreenDrawInfo, main task), then flat_role_resolve, then pass the result into
   flat_draw_sync so flat_state_for applies it to the slot copy (flat_core.c:972, after the four-pen overlay).
3. Epoch: the slot already stores `epoch` (flat_draw_slot, flat_core.h:490-494) from ipd_epoch_of_screen. The pen record
   bumps a screen's epoch when its theme or plan pens change (ip_screen_slot.epoch, iconpen_core.h:199). Rule: a slot
   is refilled when the epoch moved or the screen pointer is new; equal epoch keeps the resolved pens. Hooks keep using
   flat_draw_epoch (flat_core.h, pinned by tests/test_flat_pens_per_screen.py) to drop stale cached pens.
4. The Workbench is not touched: live `r->st` stays the source for the Workbench and the no-screen case
   (flat_draw_state rule, flat_core.c:1034). desk_flat_state keeps writing it as today. No lag: the Workbench path gains
   no call and no wait.

## 4. Screen filter, pointer lookup only (point 3)

- Add `uint8_t themed` to flat_draw_slot (flat_core.h:490). draw_sync sets it from the reader's `themed` flag.
- flat_state_for writes `st.enabled = 0` into an unthemed slot copy, and `plain.enabled = 0` (flat_draw_sync,
  flat_core.c:1001). Hooks then gate on the per-screen state pointer's `enabled`, not on `r->st.enabled`.
- Result: an excluded screen (rule Exclude) and an unlisted screen (default Workbench only, D10-1) draw fully stock;
  a themed screen draws flat with its own role pens; the Workbench draws as on main.
- Hooks do `ps = st_scr(r, scr)` / `st_rp(r, rp)` (flat.c:321, :326): a pointer compare loop over at most
  FLAT_PENS_SLOTS (= IP_SCREENS 40, flat_core.h:470) entries. No ipd_ call, no copy. `r->st.calls++` stays on the
  live record (flat.c:476).
- Open: whether the default rule `all` (GT_SCR_ALL, gthemes_core.h:465) should make every listed screen themed; it
  should, since gt_screen_theme_of already answers that, so the filter has one source.

## 5. Hook paths reading r->st, and how each finds its screen (point 4)

- c2_ring_refill :335: gate `r->st.pen_outline` :338; edge pen via st_scr. Change: gate on ps.
- c2_get_pens :352: `p->text = r->st.pen_ptext` :370; ps = st_rp already. Change: ps->pen_ptext.
- c2_draw :469: calls/enabled/held :476-477, flat_clone_route(&r->st) :489; rp in hand. Change: ps = st_rp, enabled
  from ps; the route keeps &r->st (look fields only).
- c8_skip :556: flat_frame_decide(&r->st) :565, 48-byte copy `st = r->st` :583; window in hand. Change: decide on
  ps, remove the copy (below).
- d4_flat :636 (DOpus): flat_dopus_decide(&r->st) :640; DrawInfo only, no rp. Change: see below.
- flat_tx_c :911 (Text): gate r->st :917, icl_label :970; st_rp(r, rp). Change: gate on ps.
- flat_rf_c :1157 (RectFill): gates :1162 and :1227, pen_back :1217, flat_gh_match(&r->st) :1168; st_rp, menu has
  none. Change: ps for every read; menu below.
- ghost_menu :1119: flat_gm_pen(&r->st) :1128; Layer NULL (off-screen menu bitmap). Change: see below.
- scroller_pen :1057: st_scr already. No change.

Paths without a rastport screen:

- Screen-bar layer (layer with no Window, flat.c:1162): container-of `rp->Layer->LayerInfo` minus
  offsetof(struct Screen, LayerInfo), used ONLY as a key compared against the table; never dereferenced unless it
  matches a listed screen. Unmatched gives plain (stock), so a wrong guess never draws flat on a foreign screen.
- Menu bitmap, Layer NULL (ghost_menu): no screen in the call. Use IntuitionBase->ActiveScreen as the key (menus draw
  on the active screen while the menu is up). Read is one pointer, not locked; a race only picks plain or the
  Workbench, both safe. UNCONFIRMED: menus of a non-active screen; test on rig.
- d4_flat (DOpus): gets a DrawInfo. draw_sync stores each slot's DrawInfo pointer (from GetScreenDrawInfo, kept while
  the slot lives, freed on reap) so the hook maps dri pointer to slot by compare. No match: live record when dri is
  the Workbench's, else plain.
- c8_skip copy: draw_sync fills `slot.st.side` (side_px, flat.c:1090) and the four own pens are already in the slot
  copy, so the hook reads `ps` directly. The Workbench keeps `r->st.side` set by the main task as today. This removes
  the only 48-byte flat_state copy on a hook stack.

## 6. Budget estimate (point 5)

Limits unchanged (tests/test_flat_budget.py:17-20): ENTRY_MAX 64, HELPER_MAX 80, CHAIN_MAX 384, HEADROOM_MIN 64.
Measured today: `_c2_disp_reg` chain 308 (headroom 76), `_c2_disp_stk` chain 292 (headroom 92); only these two
entries are tested (:21).

- c2_get_pens: `r->st.pen_ptext` to `ps->pen_ptext`, ps already a local: 0 bytes.
- c2_draw: one more pointer local (ps) plus the st_rp call already inlined or called once: estimate +4 to +8 bytes,
  plus at most one return-address frame if st_rp is not inlined (+4 to +12). Worst case chain 308 + 20 = 328, headroom
  56: BELOW HEADROOM_MIN 64. So c2_draw must reuse the ps that c2_get_pens computes (pass it down, or compute once in
  c2_draw and hand it to c2_get_pens), giving at most +8: chain near 316, headroom near 68. Brief 4 measures before and
  after; if headroom drops under 64 the brief stops and reports, it never moves a limit.
- c8, d4, tx, rf are not under the budget test; removing the c8 48-byte copy lowers their stack.
- The resolver, the reader, GetRGB32 and GetScreenDrawInfo run on the main task: no hook cost.

## 7. Tests written first (point 6)

Host C, new tests/test_flat_role_core.c (built with the existing host suite, ONLY= runnable):
- R1 exact held match: theme rgb equals a held plan pen, row gets that pen.
- R2 exact ColorMap match beats nearest; R3 nearest uses the gthemes fold distance (compare with gthemes' function).
- R4 DrawInfo rows: no colour match takes dri TEXTPEN/SHADOWPEN/BACKGROUNDPEN.
- R5 flat_role_apply writes the seven fields and leaves enabled, look fields and the four own pens untouched.
- R6 a theme with no element overrides gives the same pens as the role column (Workbench parity).
Host C, extend tests/test_flat_core.c:
- D1 flat_draw_sync with an unthemed slot: slot st.enabled = 0, plain.enabled = 0; themed slot enabled = 1 with the
  role pens applied; Workbench lookup returns the live record unchanged.
- D2 epoch equal keeps resolved pens; epoch moved refills.
- D3 slot st.side filled from the side value passed in.
Python guards, extend tests/test_flat_pens_per_screen.py:
- G1 no `r->st.enabled` / `r->st.pen_` read inside c2_draw, c2_get_pens, c8_skip, flat_tx_c, flat_rf_c, ghost_menu,
  d4_flat bodies (only `r->st.calls`).
- G2 no `flat_state st = r->st` and no `flat_state` local in any hook body.
- G3 no `ipd_` and no GetScreenDrawInfo/GetRGB32 in any hook body; ipd_screen_roles called only from draw_sync.
- G4 no ObtainBestPen/ObtainPen with a non-Workbench ColorMap in flat.c (amarchy-flat never obtains on a foreign
  screen); pens_run obtains on the Workbench only (flat.c:1837).
- G5 the mapping table exists once: the GTE_ names of section 2 appear only in flat_role_core.c.
Budget: tests/test_flat_budget.py unchanged, must pass.

Rig proof TE-9.6 (one shared rig run, 060 max, built from main with every brief merged):
- Setup: a foreign public screen A with a rule giving theme T (distinct button and border colours), a second foreign
  public screen B under Exclude, the Workbench on the main theme. Open a GadTools window with a button, string gadget
  and scroller on each.
- Pixel asserts (measured from the screenshot, never pen reads): on A the button face and border pixels equal T's
  GTE_BUTTON_FACE / GTE_BORDER rgb (or the nearest held pen, reported); on B every sampled pixel equals the stock
  look (bevel shine/shadow pens present); on the Workbench the pixels equal a capture from main before the change
  (byte-equal regions).
- Sentinels: flat calls counter moves on A, the c2 path ran for A's window (calls delta > 0), B's window gives no flat
  draw (counter for B-routed draws 0, or stock pixels).
- Record in thoughts/shared/research/test-register.md.

## 8. Sonnet briefs, in order (point 7)

> CORRECTED 2026-11-06 (section 11): item 2 cannot return a copy of the slot theme, and item 3 applies role pens on
> every sync, not on an epoch change.

1. flat_role_core: the pure module, its mapping table and tests R1-R6. Reuse the gthemes fold distance. Depends on
   nothing.
2. iconpen_dos reader ipd_screen_roles: slot by pointer, themed flag from the same gt_screen_theme_of result, plan pens
   copy, and a COPY OF THE SLOT THEME (st[slot].t) into the caller's buffer. Decision 2026-11-06: the reader does not
   compute row colours; draw_sync (brief 4) calls flat_role_rgb_of on the copy, so iconpen_dos stays free of
   flat_role_core and only amarchy-flat links it. The slot lookup and copy are a pure part in iconpen_core with host
   tests in the iconpen suite. Depends on nothing in 1.
3. flat_core: `themed` in flat_draw_slot, enabled = 0 in plain and unthemed slots, flat_state_for applies role pens and
   side; tests D1-D3. Depends on 1.
4. flat.c draw_sync and hooks: draw_sync calls the reader, GetRGB32, DrawInfo and resolve; every hook in section 5 moves
   to the ps pointer; c8 copy removed; screen-bar and menu keys; d4 dri map; guards G1-G5; budget measured before and
   after. Depends on 2 and 3.
5. Rig proof TE-9.6 on one shared rig from main, pixel asserts, register row. Depends on 4 merged.

## 9. Owner decisions (point 8, product behaviour only)

Q1 A themed foreign screen whose palette holds no exact pen for a role colour:
   (a) flat look with the nearest pen the screen holds (Recommended: the screen still looks themed, colours slightly off
       on low depths); (b) stock look on that screen (exact or nothing, but a half-themed screen looks broken).
Q2 Menus on a themed foreign screen:
   (a) flat menus with that screen's pens (Recommended: matches the window look; relies on the active-screen key);
   (b) stock menus on foreign screens (safer, but menu and window look differ).
Q3 DOpus on a themed foreign screen:
   (a) flat with the screen's pens (Recommended: same rule as every other window);
   (b) DOpus flat only on the Workbench.
OWNER ANSWERS 2026-11-06 (question tool): Q1 = (a) nearest pen. Q2 = (a) flat menus with that screen's pens.
Q3 = (a) DOpus flat with the screen's pens. All three recommended options; no brief changes shape.

Already decided, not asked again: default Workbench only (D10-1), Workbench tick row leaves colours alone (D10-10),
colours per screen and look global (D10-11).

## 10. Risks and unconfirmed (point 9)

- UNCONFIRMED: pen_line row (GTE_BORDER or glyph pens); brief 1 reads desk_flat_glyph_pens.
- UNCONFIRMED: menu screen key by ActiveScreen for menus of a screen that is not active.
- UNCONFIRMED: whether a themed foreign screen's DrawInfo passes the flat frame gate SHINEPEN == BACKGROUNDPEN
  (flat_core.c:193, :203). If the pen record does not rewrite the foreign DrawInfo, c8 and d4 decide "not flat" on that
  screen and the window stays stock whatever the slot holds. Brief 4 checks on the rig first; if the gate fails, the
  decide functions take the slot's themed flag instead of the DrawInfo test for listed screens (Workbench keeps the
  DrawInfo test).
- Budget: c2_draw headroom can drop under 64 if ps is computed twice; brief 4 must pass it down.
- Screen-bar container-of: key only, never dereferenced unless matched.
- GetScreenDrawInfo pointers kept per slot must be freed when the slot is reaped (pens_reap, flat.c:1795) and before
  the screen closes; a screen closing between sync ticks keeps a dangling pointer in the table that is only compared,
  never read.
- The theme per slot (`st[slot].t`) may hold no model when the theme loaded without one (iconpen_dos.c:268-270): the
  reader then reports themed = 0 for colours and the slot draws stock.

## 11. Corrections after reading iconpen_dos.c on origin/main a43acea5 (2026-11-06)

Two premises of sections 3 and 8 are wrong. Both were found by reading the code before brief 2 was written.

1. The slot theme `ipd_rec.st[i].t` is valid only during one sync. The sync start sets `have = 0` and `t.model = 0` for
   every slot, the theme is read only for `need[i]`, and the sync end calls `gt_free` on every slot theme ("the models
   are this process's heap: none stays in the shared record"). Only `st[i].name` stays. A reader that runs between
   syncs cannot copy the theme.
2. A theme or rule change on an open non-Workbench screen does NOT bump the slot epoch. It only sets a re-plan
   (`ip_screens_recheck_plans`). The slot epoch moves when the slot's screen pointer or depth changed
   (`ip_screens_apply`) or on a reopen at the same address (`ip_screens_sync`).

Rules that replace them:

- Role colour source: the pen record keeps the seven role colours per slot, durable. They are written at the one
  theme read site (the `need[i]` path, right after `pens_theme_named`), with `flat_role_rgb_of` on the slot theme,
  together with the screen pointer of the slot's holder, the theme name they came from and an ok flag. A theme change
  forces a re-plan, a re-plan reads the theme, so the stored colours follow the held pens. iconpen_dos therefore calls
  flat_role_core (one mapping table, no copy); every binary that links iconpen_dos.c links flat_role_core.c.
- Reader: `int ipd_screen_roles(const void *scr, ipd_roles *out)`, main task only, under `r->ss`. `themed` = the rules
  give the screen a theme (the reader calls `gt_screen_theme_of` itself: a non-owner process computes hold = 0) AND the
  stored colours belong to this screen pointer and this theme name. Out: themed, epoch, held plan pens (number and
  colour), the seven role colours.
- Record layout grows: `IPD_SEM_NAME` moves to `Amarchy.IconPens.6`. The single rig 96 reinstall at AMARCHY READY
  covers it.
- No epoch gating in amarchy-flat. `draw_sync` calls the reader and `flat_role_resolve` for every listed non-Workbench
  screen on EVERY sync (main task, 1 s watch and CTRL_D) and passes themed flag, role pens and side in the list entry.
  `flat_draw_sync` writes them into the slot on every sync. Test D2 ("equal epoch keeps the resolved pens") becomes
  "a second sync with other role pens or another themed flag replaces the slot's values, same slot address".
- Lag: a theme change reaches a non-Workbench screen's flat pens after the pen record's next sync plus the next
  draw_sync (at most about 1 s each). Between the two, hooks draw with the previous pens of that screen.


## 12. Notes after briefs 2 and 3 (2026-11-06)

1. Enum name clash. `flat_role_core.h` names `FLAT_ROLE_TEXT`, `FLAT_ROLE_LINE`, `FLAT_ROLE_BACK` also exist in
   `amarchy/flat_glyph.h:28` with other values. flat.c and flat_glyph.c include flat_glyph.h, so they cannot include
   flat_role_core.h today. Workarounds in the tree: `IPD_ROLE_COUNT` in iconpen_dos.h (brief 2), a forward declaration
   `struct flat_role_pens;` in flat_core.h and the draw tests in tests/test_flat_role_core.c (brief 3). Brief 4a renames the
   enum in flat_role_core.h (prefix `FLAT_RP_`), then `IPD_ROLE_COUNT` goes.
2. Brief 3 does not land alone. Callees already gate on the per-screen state: flat_glyph.c `flat_clone_route`,
   `flat_glyph_route`; flat_core.c `flat_tx_pen`, the scroller box, `flat_title_decide`. Brief 3 sets `enabled = 0` in every
   non-Workbench slot until `roles` is filled, so with brief 3 only, glyphs draw stock and frames draw flat on such a screen.
   Branch `te10-6c-3` is the integration branch for briefs 3, 4a, 4b, 4c; one push to main after 4c.
3. Consequence for section 5: the hook gates in flat.c (`r->st.enabled` at c2_draw, flat_tx_c, flat_rf_c) are the only
   `enabled` reads left to move; the callees need no change.
4. `flat_draw_scr.roles->ok == 0` means the slot is not themed (draws stock).

### 12.4 Lock order for brief 4b (measured 2026-11-06, iconpen_dos.c on te10-6c-3)

- The pen record sync takes `r->ss` FIRST and `LockPubScreenList()` SECOND (iconpen_dos.c:520-540, :551-559), and holds
  `r->ss` across its DOS calls.
- `ipd_screen_roles` takes `r->ss` (iconpen_dos.c:461). `ipd_epoch` / `ipd_epoch_of_screen` take no semaphore.
- Rule: amarchy-flat NEVER calls `ipd_screen_roles` while it holds the public screen list lock (reverse order = deadlock
  with the pen record sync; and a reader that waits on a DOS read would stall every OpenScreen in the system).
- Shape of `draw_sync` (main task), three passes:
  1. Under the list lock: collect the screen pointers and epochs (as today). Unlock.
  2. No lock: per collected pointer call `ipd_screen_roles(scr, &g_roles_in[i])` (the pointer is only a compare key
     there; a closed screen is harmless). Static array, never on the stack (`ipd_roles` holds two GTE_COUNT arrays).
  3. Under the list lock again: per collected pointer still in the list (`ipd_screen_in(pl, scr)`), and only when the
     reader said `themed`: GetRGB32 of the ColorMap (read only), GetScreenDrawInfo / FreeScreenDrawInfo,
     `flat_role_resolve` into a static `flat_role_pens g_roles_out[i]`; `side` from `side_px` (the aspect table, no
     Intuition call). A screen gone since pass 1, or not themed: `roles = 0` (draws stock). Unlock.
  4. `Forbid(); flat_draw_sync(...); Permit();` as today.
- A screen closed and reopened at the same address between pass 1 and pass 3 reads one sync of the old roles; the next
  sync (every sync refills, rule 6) corrects it.
- Guard G6 (python, tests/test_flat_pens_per_screen.py): in `draw_sync` no `ipd_screen_roles` call sits between a
  `LockPubScreenList` and its `UnlockPubScreenList`.
