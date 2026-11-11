---
date: 2026-11-11
topic: Time Preferences year + month row full width (amarchy-flat)
status: STOPPED before the first edit -- design bigger and riskier than briefed
branch: s73-flat-time-row-full-width (off main 0f72dd7b), no source changed
---

# Time Preferences year/month row full width -- analysis, STOPPED

## Owner request and fixed decisions
"the 2026 input and the month selector should be full width". Year left (box left = calendar box left), keeps its
width; month cycle grows left to meet it with the patcher's normal box gap, right edge stays. Time Preferences only.
Real gadgets must move (click area, frame, text, refresh agree); no draw-only shim.

## Analysis (before any edit)

Violated invariant (owner's): the top row's left edge should equal the left edge of the widest element below it
(calendar box, x=12). Today the program lays the row out with the year frame at 54 and the cycle at 142.

Level of the change: program layout, i.e. the Gadget structures Time owns and GadTools builds. Every hook amarchy-flat
has today is a DRAW-time hook (sysiclass / strgclass / frameiclass dispatchers, graphics Text and RectFill, dopus5
DrawBox/FieldBox/bevel; `hook_install` flat.c:2887). There is no hook on window opening or gadget creation anywhere in
flat.c (grep: OpenWindow, CreateGadget, AddGList, RemoveGList, RefreshGList, gadtools -> 0 hits). So the change cannot
live in any existing mechanism: it needs a new kind of hook that changes another program's data.

## Measurement on rig 68 (uitree + the program binary)

`ami.py uitree`, W0 "Time Preferences" 0,14 332x203, gadget list in list order (screen coords, id, kind, text):

    G 2  12,198 87x14  button "Save"      G 8 122,198 button "Use"    G 9 233,198 button "Cancel"
    G 14 60,35  41x8   integer "2026"     (GadTools INTEGER_KIND; ridge frame drawn 54..106 = gadget +-6)
    G 15 142,32 178x14 button "November"  (GadTools CYCLE_KIND: a bool gadget; frame, glyph and label from GadTools)
    G 13 130,156 prop + 126,154 194x11 button "Hours"    (SLIDER_KIND + label)
    G 12 130,168 prop + 126,166 194x11 button "Minutes"
    G 11 12,53  311x100 custom ""          (the calendar)

`Version SYS:Prefs/Time` -> `time 39.1 (08/13/92)`. Binary strings (13028 bytes): opens gadtools.library,
locale.library + `Sys/prefs.catalog` (title is localised), and `gadgetclass` (the calendar is the program's own
private class made from gadgetclass, not a GadTools kind).

What this shows:
1. The row is two GadTools gadgets. GadTools draws the integer frame and the cycle frame, glyph, divider and centred
   label from its own per-gadget data built at CreateGadgetA time.
2. The calendar sits LAST in the list, after the sliders: it is linked after every GadTools gadget, so it is created
   after the year and month gadgets (list order read from uitree; I did not disassemble Time to confirm the call order).

## Why the briefed design cannot be built safely

Two ways exist to move the real gadgets. Neither meets the brief:

A. Change the live gadgets after the window opens (the only point where the calendar exists, so the only point where
   "row left = calendar left" can be derived). For the cycle this means changing LeftEdge/Width AND GadTools' private
   per-gadget data: the frame image width, the glyph/divider placement and the label centring that GT_SetGadgetAttrs
   recomputes on every month change. That data is undocumented and differs between gadtools V39/V40 ROMs. Writing it
   from outside is exactly the "real gadgets cannot be moved safely" case. Moving only LeftEdge/Width would give a
   frame, label and click area that disagree after the first month change (the draw-only failure the owner ruled out).

B. Hook gadtools.library CreateGadgetA (documented interface; GadTools then builds frame, label, click area and
   refresh consistently). This is safe in principle, but:
   - the calendar does not exist yet when the year/month gadgets are created, so the row left cannot be taken from
     the calendar. The nearest structural source at that moment is the Save button's NewGadget left (12, created
     first, bottom row), which is a different rule than the one fixed in the brief;
   - it is a new mechanism class for amarchy-flat: a new library LVO swap (gadtools), a per-task creation-sequence
     state machine, unload-in-flight gate like the class hooks, a stack budget entry in flat-budget, and STATUS
     fields. That is well beyond "one change" in ~40 tool calls.

Window identification rule I would use for B (not built): the creating task is the Time preferences program by
identity -- process `pr_CLI` command name or Workbench task name whose FilePart is "Time" AND the NewGadget sequence
matches (INTEGER_KIND immediately followed by CYCLE_KIND, followed later by two SLIDER_KIND). The localised title is
not used. A different program named "Time" with a different gadget sequence is left unchanged.

## Decision needed from the owner
- Option B with the row left taken from the Save button's NewGadget (12 on this layout, the same column as the
  calendar), accepting a new gadtools.library hook in amarchy-flat. Size: one dedicated implementation pass (new hook +
  gate + budget + tests + rig proof), not a small change.
- Or keep the row as the program lays it out.
Option A is not offered: it writes undocumented GadTools internals of another program.

## Tests, build, rig
None run: no source was changed, so there was nothing to test, build or prove. Rig 68 was only read (uitree,
`Version`, one file `get`); it is left as found: main-checkout binary, W0 "Time Preferences" 0,14 332x203 active.
