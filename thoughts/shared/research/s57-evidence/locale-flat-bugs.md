---
date: 2026-11-05
topic: S57 Locale flat bugs on OS 3.2 (rig 127)
status: partial
---
# S57 Locale flat bugs (rig 127, OS 3.2, --max)

Branch s57-locale-flat (from 88aa4274). Shots: ~/Desktop/upterm-shots/rig127-locale-flat-before-sidefix.png, rig127-locale-flat-after.png.

## S57-LOCALE-LIST-NO-SELECT: fixed (selected row), side borders open

- Root cause: desk_pens_flat (amarchy/desk_core.c) mapped DK_FILL (FILLPEN, slot 5) to the surface and FILLTEXT to text.
  GadTools listviews fill the selected row in FILLPEN, so the row fill equalled the list background: no visible selection.
- Fix: FILL and FILLTEXT keep the stock map (accent / surface). Goldens palette-flat-pen8/pen4.prefs updated (slot 5 = 3, slot 6 = 0, both tables).
- Test: test_desk fails 10 checks with the fix reverted, 0 after.
- Rig 127 pixels after: Country list rows y 196..202 show accent 6688bb runs of 88..125 px (x 0..149 of the window); before, no accent run in the list.
- Side effect, OPEN: the Intuition ROM also fills the active window's left/right/bottom border strips in FILLPEN.
  Measured after: x 0..2 and x 549..551 = 6688bb at y 100/150/200; owner rule FLAT-TITLE-ACCENT wants them dark.
  Added flat_side_fill_pen (flat_core.c, test_flat_core: 3 fail before, 2422/0 after) and a RectFill branch in flat.c,
  but on the rig the side pixels did not change: the ROM side fill does not reach that branch
  (candidates: the border is drawn on the window's border layer RastPort whose Layer->Window check fails, or not via RectFill
  in the irom range, or by the outer-edge path returning first). Next: count calls to the branch (rhits) per rect on rig 127.
  Do not merge this branch until the sides are dark.

## S57-LOCALE-GADGET-BLACK: not reproduced on this build

- With the FILL map change, the Locale iconify gadget (x 507..530, y 47..57) draws its glyph: surface lines on the accent band,
  no solid (26,27,37) block. No before measurement on main this run.
- Candidate root cause: a gadget path that falls back to a stock or partial draw and fills with FILLPEN; under the old map
  FILLPEN = surface (26,27,37), which is exactly the solid block colour. The rig 96 XCON-after-SetWindowTitles report fits the
  same path (stock re-render of title and gadgets after SetWindowTitles, filled in FILLPEN). Not reproduced on rig 127
  (budget spent); next: main build vs this branch, shell window with a SetWindowTitles loop, measure the gadget rect.

## FLAT-BUTTON-FILL: the read was wrong, the code is right

- Governing rule: S56-PRIMARY-ACCENT-FILL (todos.md:4088), owner answer "Accent fill (Recommended)":
  "flat primary buttons ... fill with the theme accent colour, label in the background colour" (merged 9b9edbfb, 4a37f93f).
- rig124-release-reads-2.md line 8 applied the superseded rule at todos.md:3355 (2026-10-31, solid raised fill in
  lighter_background = 24283b). Idle fill 7aa2f7 = accent is correct.
