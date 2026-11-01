---
date: 2026-11-01
topic: SELECTA-AA-ICONTEXT design probe - who draws Workbench icon labels, and how to antialias them
status: research complete; design proposed, not implemented
ledger: thoughts/spot/todos.md:3627 (SELECTA-AA-ICONTEXT)
---

# SELECTA-AA-ICONTEXT: Workbench icon label probe

## Answers in short

1. **Who draws the label.** On OS 3.2 (icon.library 47.4, workbench.library 47.33) the label is drawn by
   graphics.library `Text()` (LVO -60) called from INSIDE icon.library `DrawIconStateA()` (LVO -162),
   on the Workbench task. One `Text()` per label in the default label mode. Measured.
   On OS 3.1 (icon.library 40.1 has no `DrawIconStateA`) the Workbench draw path was NOT measured: the
   3.1 rig desktop is Directory Opus 5, not Workbench. Expected: workbench.library 40 calls `Text()`
   itself (unverified).
2. **Directory Opus.** The 3.1 rig runs Directory Opus 5 as desktop (and RULES.md rule 5 says the
   owner's A1200 does too). Its desktop labels are drawn by task `dopus` with one `Text()` per label,
   JAM1, on its own backdrop window with its own layer backfill hook. A patch at `DrawIconStateA` never
   sees these labels. A patch at `Text()` filtered by task and backdrop window sees both.
3. **Cost of reading the background back** (UAE JIT "060 max", emulated time, not real hardware):
   cgx `ReadPixelArray` RGB 57 us per 96x12 label; `ReadPixelArray8` 115-262 us; `ReadPixel` loop
   about 2 ms per label (unusable).
4. **Patch point:** graphics `Text()` (already patched by Amarchy for FLAT-BUTTON-FILL), with a
   label filter; `DrawIconStateA` patch on V44+ only as a context marker (optional, see Design).

## Probe method

Scratch program `iclprobe` (vbcc, 68020, NDK 3.2r4; source in the session scratchpad, not committed):
- `SetFunction` on graphics `Text` (-60) and, when icon.library >= 44, on `DrawIconStateA` (-162).
- Logs per call: task name, inside-DrawIconState flag, DrawMode, FgPen/BgPen, BitMap depth, font,
  cp_x/cp_y, layer, window and window flags, Layer->BackFill hook and h_Entry, the string; for
  `DrawIconStateA` also state, position and tags (Frameless, EraseBackground, Borderless,
  LabelShadow = ICONA_Dummy+93, LabelOutline = ICONA_Dummy+94, DrawInfo).
- Opens and closes a full-screen window to force the desktop to redraw, then restores both vectors
  (restored=1 on both runs).
- Times readback of a 96x12 box x10 with EClock (709379 Hz): `ReadPixel`, `ReadPixelArray8`, and
  cgx `ReadPixelArray` when the screen is a CyberGraphX bitmap.
- wasabi snoop traces DOS only, so it cannot answer this; hence the SetFunction probe.

## Evidence: OS 3.1 rig 26 (graphics 40.24, icon 40.1, workbench 40.5, cybergraphics 42.7)

- No `Workbench` task; desktop tasks `dopus`, `dopus_clock`. Quitting DOpus by ARexx
  (`address 'DOPUS.1' 'quit force'`) plus LoadWB did not replace it. Workbench 3.1 path: unverified.
- DOpus label lines (one per icon):
  `T dopus in=0 dm=0 (JAM1) fg=1 bg=0 font=topaz.font/8 ... win=10aac2a4 wfl=9203940 (backdrop) bf=10026cc4 bfe=1074b9de "Ram Disk"`
  (same for "System", "BOOTX", "Rushhours", "VTCX").
- Screen: RTG, 800x600 depth 24, `iscgx=TRUE pixfmt=3200`. `rp->BitMap->Depth` reads 4 here, which is
  meaningless on RTG: use `GetCyberMapAttr(CYBRMATTR_ISCYBERGFX/DEPTH)`.
- Readback per label: `ReadPixel` 1911 us, `ReadPixelArray8` 262 us, cgx `ReadPixelArray` RGB 57 us.
- Other `Text()` callers on the same screen: `Background` (Amarchy bar, helvetica 11, JAM2),
  `dopus_clock`. The label filter must reject these.

## Evidence: OS 3.2 rig 26 (graphics 47.10, icon 47.4, workbench 47.33)

- No ENV:Sys/wbpattern.prefs and no ENV:Sys/font.prefs on this rig (default look).
- 39 `D Workbench` (`DrawIconStateA`) calls, each followed by exactly one `T Workbench in=1`:
  `D ... cp=28,15 "RAM Disk"` then `T ... in=1 dm=1 (JAM2) fg=1 bg=0 topaz.font/8 depth 8 rp=100544b8
  layer=10054418 win=10053f74 wfl=2220307f (backdrop) bf=10053f50 bfe=1004601a cp=11,49 "RAM Disk"`;
  `"Empty-12MB-FFS"` D cp=24,55 / T cp=11,84.
- `state=0`, tags=0: no Frameless, Shadow, Outline or DrawInfo tags seen in the default mode.
- A drawer window opened by ARexx (`WINDOW SYS: OPEN`) did not produce draws on a second window in
  the log; all 39 were on the backdrop window. Drawer-window labels: unverified.
- Workbench 3.2 installs a backfill hook on the backdrop layer even without wbpattern prefs.
- Screen: AGA PAL, 640x256 depth 8, not cgx. Readback: `ReadPixel` 2009 us, `ReadPixelArray8` 115 us.
- Label shadow and outline modes (3.2 Workbench prefs) not exercised: unverified. Expected: more than
  one `Text()` per label at pixel offsets, all inside one `DrawIconStateA`.

## Design

### Patch point per OS

| OS / desktop | Who calls `Text()` | Filter |
|---|---|---|
| 3.2 Workbench | icon.library `DrawIconStateA`, task Workbench | task == Workbench AND layer belongs to a Workbench window (backdrop or drawer); optional: inside-DrawIconStateA flag |
| 3.1 Workbench | workbench.library (unverified) | task == Workbench AND Workbench window layer |
| Directory Opus 5 | task `dopus` | task == `dopus` AND window is backdrop (WFLG_BACKDROP) |

One mechanism: extend the existing graphics `Text()` patch in amarchy/flat.c (flat_tx_c / flat_tx.s,
tx_install / tx_remove) with an icon-label branch. Do not add a second `Text()` patch: two
SetFunctions on one vector double the chain-removal risk that strut_patch already handles. The
`DrawIconStateA` patch is optional on V44+: it gives a reliable "this Text is a label" flag and the
label state (selected) without a task-name test, but does not exist on 3.1 or for DOpus, so the task
filter is needed anyway. Recommendation: task + window filter only; add the DrawIconStateA marker only
if the filter shows false positives on 3.2 (for example Workbench window titles drawn by the same task
- those go through intuition, not `Text()` on the backdrop layer, so expected none; unverified).

Drawing: when the filter matches and the string fits the glyph source, replace the bitmap `Text()` by
an antialiased draw of the same string at the same baseline (cp_y - font baseline), same pen, and
advance cp_x as `Text()` would (callers may rely on it). On no match, or on any failure, call the
saved `Text()` with a6 = GfxBase (s28 hang rule in flat.c).

### Background: solid-colour detection first, readback second

1. **Solid**: no ENV:Sys/wbpattern.prefs, or the ROOT chunk (prefs/wbpattern.h, WBP_ROOT) has
   WBPF_PATTERN set and every tile pixel is one pen. Amarchy's own flat backdrop writes exactly this
   (desk_core.h:139 desk_wbpattern "one solid darker tone"; desk_dos.c:548-560, 700-710). Then
   bg = that pen, RGB from GetRGB32 on the screen ViewPort. No readback. DOpus: its own background
   prefs, not wbpattern: treat as non-solid unless detected otherwise (unverified).
2. **JAM2 labels** (3.2 default, bg pen 0): the label box is filled with bg pen first, so the
   background under the text IS solid (pen 0) whatever the wallpaper. Blend against bg pen, no
   readback.
3. **Non-solid JAM1** (picture backdrop, DOpus JAM1): read back the label box once per label:
   - RTG (cgx bitmap): `ReadPixelArray` RGB (57 us/label measured, emulated).
   - Pen screen (AGA, depth <= 8): `ReadPixelArray8` (115-262 us/label measured, emulated), then pen
     to RGB through the palette.
   - Never `ReadPixel`.
   Per-pixel blend needs an aa_ink variant that takes a per-pixel background buffer (see Reuse).

Cost at 30 icons: about 2 ms (RTG) to 8 ms (AGA) per full desktop redraw, emulated. Real 060 numbers
unverified; measure on the A1200 last (rigs first rule).

### Pen screens

On depth <= 8 the antialiased edge needs shade pens between fg and bg. aa_ink already allocates shade
inks (N=4 for depth <= 8, N=8 deeper pen screens, per the s29-aa-ramp branch). For per-pixel bg the
shade is chosen per pixel from the nearest pen to blend(fg, bg_pixel): aa_ink_ops.nearest_pen.

### Owner decision needed (tension)

Plan 2026-10-22-handler-rasterizer.md R6 (commit 708846de): owner chose "no RTG readback blend" for
the terminal. This task's case 3 is a readback blend for desktop labels. Not the same surface, but
the owner should confirm readback is wanted here, or limit SELECTA-AA-ICONTEXT to cases 1 and 2
(solid backdrop and JAM2), which need no readback.

## Reuse list

- amarchy/flat.c:281-376: graphics `Text()` patch (flat_tx_c, flat_tx.s flat_tx_entry, tx_install,
  tx_remove, flat_swap_install/remove, libpatch_done). Constants amarchy/flat_core.h:109-112.
- amarchy/strut_patch.c + amarchy/ipatch.h/.c: install pattern with semaphore, busy counter,
  passthru-then-unarm, leak if busy or chained, am_sentinel. ipatch_alloc takes IntuitionBase only;
  for an optional icon.library -162 patch, generalise ipatch_alloc to take a `struct Library *`, do
  not clone it.
- amarchy/libpatch.h libpatch_done.
- render/aa_ink.c/.h: aa_ink_draw (single bg) for cases 1 and 2 as-is; add one per-pixel-bg entry
  for case 3 in the same module (not a second module). Shade ramp work on branch s29-aa-ramp.
- amarchy/menu_text.c / menu_text_core.c (mt_open, mt_draw) and render/outline/ttf as glyph source
  (coverage per string).
- amarchy/desk_core.c:626-718 font.prefs WBFONT (label pen, JAM1) and desk_core.h:139 desk_wbpattern
  for the solid test; prefs/wbpattern.h flags.

## Test plan

- **Host tests** (where the logic lives):
  - label filter pure function: (task name, window flags, is-workbench-window, in-DrawIconState) to
    label yes/no; cases Workbench backdrop, Workbench drawer, dopus backdrop, Background bar, dopus_clock.
  - solid detector: wbpattern prefs bytes to solid pen or "not solid" (no file, ROOT solid tile,
    ROOT two-pen tile, picture entry).
  - per-pixel blend core: coverage x fg x bg buffer to RGB / nearest pen; exact numbers for 0, 50, 100%
    coverage.
- **One reachability test** (rig, OS 3.2 then 3.1, top-level entry = Amarchy start with the option on):
  - sentinel `AMARCHY_ICONTEXT_INSTALLED` from install, and a counter of labels drawn by the AA path
    (exposed like strut_counts) that must be > 0 after a forced desktop redraw and equal the icon count.
  - pixels: screenshot of one label ("RAM Disk"), measure that edge pixels contain colours strictly
    between fg and bg (not only fg and bg). Measured pixels, never pen reads.
  - option off: counter stays 0 and label pixels are only fg/bg.
- DOpus desktop: same rig test on 3.1 rig (DOpus desktop), counter > 0 for task dopus.
- A1200 last: one visual check plus the real-060 readback cost.

## Unverified

- Workbench 3.1 label path (rig runs DOpus).
- Drawer-window labels on 3.2; shadow/outline label modes; selected-state labels.
- DOpus background solid detection.
- Real 060 costs (all numbers above are UAE JIT emulated time).
