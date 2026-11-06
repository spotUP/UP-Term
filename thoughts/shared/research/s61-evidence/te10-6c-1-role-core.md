# TE-10.6c brief 1: flat_role_core (pure role-pen resolver)

Branch te10-6c-1 in /Users/spot/Code/UP-Term/vtcon-te-role. Files: amarchy/flat_role_core.c/.h, tests/test_flat_role_core.c,
gt_rgb_dist2 extracted in amarchy/gthemes_core.c/.h, suite registered in Makefile (TESTS, VTTEST_HOST_SRC, ONLY comment) and tests/test_main.c.
Not touched: flat.c, flat_core.c, iconpen_dos.c.

## API as built (deviations from the design)
- Types as designed. Added: enum FLAT_ROLE_OUTLINE, BUTTON, PTEXT, DIMTEXT, TEXT, LINE, BACK, COUNT (the pen[] order); FLAT_ROLE_DRI_TEXT=2,
  SHADOW=4, BACKGROUND=7, FLAT_ROLE_DRI_PENS=8 (Intuition DrawInfo indices, repeated so no OS header is needed).
- Accessor is `flat_role_row_of(i)` (the design's natural name `flat_role_row` clashes with the struct tag; C has one namespace for the function and typedef).
- `flat_role_rgb_of(const gtheme *t, uc_u32 *rgb)` fills rgb[] from the table via gt_roles_of + gt_elem_rgb (brief 2 uses it directly).
- `ok` = 1 when a ColorMap was given (ncolours > 0). dri may be NULL (DrawInfo rows then 0). flat_role_apply turns a pen outside 0..255 into 0.

## Table (one const table g_rows, flat_role_core.c)
| field | element | fallback role | no-pen rule |
| OUTLINE | GTE_BORDER | GR_ACCENT | nearest |
| BUTTON | GTE_BUTTON_FACE | GR_SELECTION | nearest |
| PTEXT | GTE_BUTTON_TEXT | GR_SURFACE | nearest |
| DIMTEXT | GTE_GADGET_GLYPH_INACTIVE | GR_MUTED | nearest |
| TEXT | GTE_GADGET_GLYPH | GR_TEXT | DrawInfo TEXTPEN (2) |
| LINE | GTE_BORDER | GR_ACCENT | DrawInfo SHADOWPEN (4) |
| BACK | GTE_REQUESTER_BACKGROUND | GR_SURFACE | DrawInfo BACKGROUNDPEN (7) |

## Design corrections found
- The design table had pen_line fallback GR_TEXT and pen_back fallback GR_BACKGROUND. The Workbench rule (amarchy/desk_core.c:104-109
  desk_flat_glyph_pens, used by desk_flat_state at :118ff) is text = GR_TEXT, LINE = GR_ACCENT, BACK = GR_SURFACE. Copied the Workbench rule:
  pen_line follows the accent role (element GTE_BORDER, as pen_outline), pen_back the surface role (element GTE_REQUESTER_BACKGROUND, the surface-group
  element whose stock meaning is DrawInfo BACKGROUNDPEN). R6 proves it (a GR_TEXT line would fail).
- GTE_BORDER's own group is GTG_MUTED_TEXT; with a model that gives it a ref, outline/line follow that ref, not the accent. That is the design's choice for
  outline and I kept it for line; a v1 theme (no model) gives the accent exactly.

## Distance reused
New non-static `gt_rgb_dist2(a, b)` at amarchy/gthemes_core.c (above gt_nearest_role_pen, ~line 163), extracted from the loop body of
gt_nearest_role_pen, which now calls it. flat_role_resolve calls the same function; strict < so a tie keeps the lower pen. gthemes suite unchanged (6947 checks).
R3: target (0,0,0), pens (30,30,30) squared 2700 vs (0,0,60) squared 3600; a Manhattan distance (90 vs 60) would pick the other pen.

## Test lines
- Red: `tests/test_flat_role_core.c:5:10: fatal error: '../amarchy/flat_role_core.h' file not found` (make Error 1).
- Green: `[OK] flat_role_core: 48 checks, 0 failed`; `[OK] gthemes: 6947 checks, 0 failed`; `[OK] flat_core: 2705 checks, 0 failed`.

## m68k
Not linked into amarchy-flat (brief 3 does that). Compile-check with the amarchy-flat vc flags (-cpu=68020 -O2 -warnings-as-errors ...): flat_role_core.c rc 0
(1640-byte object), gthemes_core.c rc 0 after the extraction. flat-budget not run (flat.s/flat_core.s unchanged, nothing linked).

## Open points
- Brief 2: caller must build cmap[] from the screen ColorMap (GetRGB32 to 0xRRGGBB, 8-bit per gun) and held[]/hrgb[] from ip_holder ppen/prgb; dri from DrawInfo->dri_Pens.
- Brief 3: add amarchy/flat_role_core.c to amarchy-flat's source list and link; check flat-budget then (numbers 308/76, 292/92).
- Equal-colour pens in a ColorMap: lowest pen wins (R2). Held pens beat ColorMap pens even if a lower ColorMap pen has the same colour.
- A held-pen match is by exact rgb; hrgb is compared masked to 24 bits.
