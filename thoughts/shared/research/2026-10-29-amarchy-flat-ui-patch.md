---
date: 2026-10-29
topic: Amarchy own flat-UI system patch (replace VisualPrefs + VP-QuickFix, FullPalette, Birdie 2000)
tags: [amarchy, intuition, boopsi, setfunction, visualprefs, birdie, theme, os3.2]
status: draft
---

# Amarchy flat-UI patch: feasibility spike (read-only, no rig run)

Sibling: `2026-10-29-amarchy-visualprefs-birdie.md`. Plan context: `thoughts/shared/plans/2026-10-29-amarchy-p1b-third-party-theme.md` (R1.25).
Evidence base: static disassembly of VP-QuickFix `VisualPrefs_060` and `Birdie` (archives in the earlier session's scratchpad `vp-dl/`, still present; hunk files split with a throw-away parser, disassembled with `~/opt/amiga16/bin/m68k-amigaos-objdump -b binary -m m68k:68040 -D`; NEVER run), plus NDK 3.2 headers/FDs in `~/opt/amiga16/m68k-amigaos/ndk-include` and `ndk/lib/fd` (intuition.h is 47.7). No rig was started: nothing below about 3.2 ROM internals is observed on a live system. Only mechanisms recorded, no code copied.

Tags: V = verified (evidence given), U = unverified.

## 0. What the two products really do (V)

- Birdie `Birdie` (88704 bytes, HUNK, 5 hunks): a code hunk calls `Forbid`, then SetFunction (`jsr -420(a6)`) eight times in a row (disasm bd.1 offsets 0x2c..0x12c), each new function stored with the old pointer in a data cell; ends with `CacheClearU` (-636 exec... see below) and `Permit`. Offsets patched, with replacement-code sizes: -90 (2002 B), -606 (1618), -636 (1328), -306 (754), -60 (1102), -246 (604), -276 (1068), -72 (2026). V.
  Name mapping from the NDK FDs: graphics -606 BltBitMapRastPort, -636 BltMaskBitMapRastPort, -306 RectFill, -60 Text, -246 Draw; intuition -276 SetWindowTitles, -72 CloseWindow; -90 is layers DeleteLayer or graphics SetSoftStyle. Which library base each patch hits is U (base-pointer cells filled in another hunk, not traced). Library strings in the binary: intuition, graphics, layers, cybergraphics, datatypes, commodities, screennotify (V, `strings`). Consequence: Birdie does NOT touch BOOPSI classes; it intercepts primitive drawing calls made by Intuition's border renderer and rewrites them. That is why it needs "pens" heuristics and flood masks, and why it only does fills.
- VisualPrefs `VisualPrefs_060` (99680 B, 3 hunks): strings (V) name the public classes `rootclass imageclass frameiclass sysiclass propgclass buttongclass`, its own `tbiclass` (title bar image) plus `muitbiclass`, `select.gadget`, `gadtools.library`, `intuition/graphics/layers.library`, `ENV:Sys/gui.prefs`, env tuning keys `VisualPrefs/{SafetyLevel,Special,BirdieMode,CopyGTText,DOpusDelay,MinStackSize,WBOpenDelay}`. Its SetFunction wrapper (disasm vp.0 0x16dc2..0x16e4c) is: Forbid; `old = SetFunction(lib, lvo, new)`; store old in the patch record; `CacheClearU`; Permit. Its REMOVE routine (0x16df6..0x16e50) is the textbook safe unpatch: SetFunction(old) back, compare the returned value with our own function; if it differs (someone patched on top of us), put our returned value back and instead write `old` into the other patcher's JMP slot (`movel old,(a0@(2))`), set a "removed-but-still-chained" flag bit. V. Every patch is therefore removable; the stub memory just has to stay resident when something sits on top.
  The exact LVO list VP patches is U (patch records are built inline; not extracted). Quick-fix notes in the readme (earlier doc) say it patches Intuition, GadTools, frameiclass, sysiclass, OpenWindow.
- Priorities: the 3.2 pain points reported by VP-QuickFix (ICONIFYIMAGE, frameiclass types, BARCONTOURPEN, IControl exceptions) are all in the list below, and are exactly the places where a 20-year-old patcher assumed fixed internals (V from readme in earlier doc; mechanism of failure U).

## 1. Element table

Default drawing facts marked V come from NDK 3.2 headers; "internal" means no public class and nothing in the NDK says otherwise (the claim "no public hook" is U until we dump a rig; headers cannot prove absence, see section 6).

| Element | Drawn in 3.2 by | Hook point for Amarchy | VP / Birdie | Risk |
|---|---|---|---|---|
| Window border frame (bevel, fill) | Intuition internal (rendered with the window's RastPort via graphics Draw/RectFill/Move; pens from DrawInfo: SHINEPEN, SHADOWPEN, FILLPEN/BACKGROUNDPEN). No public class. U (no header); Birdie behaviour (V) shows it goes through Draw/RectFill/BltBitMapRastPort | Option A: replace the pens, not the shape: our DrawInfo pens set so shine=shadow=fill gives a flat 1-colour border without any patch (U, but cheap and testable, see section 7). Option B: private border renderer = SetFunction on Intuition window refresh path (needs a rig disassembly of 3.2 intuition.library, which LVO). Option C (Birdie way): SetFunction graphics Draw/RectFill filtered by "RastPort belongs to a window border" | VP: own internal border code + tbiclass; Birdie: graphics primitives (V) | High for B/C (hot path, every pixel call, 68000-060 cache/flush, RTG); A is low |
| Title bar (text, background, bar height) | Intuition internal; title text drawn with RastPort Text(); height from font + 3.2 system mechanism | Pens + font (SA_SysFont) cover colour/height; shape change needs patch of SetWindowTitles/refresh | VP: tbiclass image installed as title bar image; Birdie patches SetWindowTitles (-276, V) | Medium |
| Close / depth / zoom / size / iconify gadgets | Real system gadgets with `sysiclass` images (SYSIA_Which = ZOOMIMAGE 1, SIZEIMAGE 2, CLOSEIMAGE 3, SDEPTH 5, ICONIFYIMAGE 0x16): V intuition/imageclass.h:173-193. Public class dispatchable | Replace the dispatcher of `sysiclass` (swap `cl_Dispatcher.h_Entry`, chain to the old one for unknown SYSIA_Which and for `OM_NEW` when the user switched us off). Stable, documented-by-header, no LVO patching | VP: patches sysiclass (strings V) and offers bitmap symbols; | Low-medium. Dispatcher swap is the intended extension point; risk = objects already created keep the old class code only if we unload (they hold `cl_Dispatcher` via class, so swapping at runtime changes ALL live instances' next method call: fine) |
| Scroller / prop gadgets (border scrollers) | `propgclass` (public, PROPNEWLOOK flag in Flags, V intuition.h:596-602) plus knob rendering internal unless SMARTKNOBIMAGE (V intuition.h:601 says custom BOOPSI knob supports IM_DRAWFRAME, V47) | Replace `propgclass` dispatcher for GM_RENDER; or hand windows a custom knob via SMARTKNOBIMAGE only for our own windows (not system wide) | VP patches propgclass (V strings) | Medium: classic Gadget structs (non-BOOPSI props created by pre-V36 apps) go through internal Intuition rendering, not propgclass: U that those still look 3D |
| Button frames (BOOPSI/ReAction/MUI-less) | `frameiclass` with IA_FrameType FRAME_DEFAULT 0 / BUTTON 1 / RIDGE 2 / ICONDROPBOX 3 (V imageclass.h:104,220-223) via IM_DRAW/IM_DRAWFRAME/IM_FRAMEBOX (V imageclass.h:237-239) | Replace `frameiclass` dispatcher (draw flat rectangle with our pens, keep IM_FRAMEBOX metrics so layouts do not move) | VP patches frameiclass (V) and exposes "Borderless/Filled" | Low-medium. Keep IM_FRAMEBOX sizes or hand-laid windows break |
| GadTools gadgets (button, string, cycle, checkbox, listview) | gadtools.library (DrawBevelBox / own routines), not BOOPSI classes. Public calls: `DrawBevelBoxA` (gadtools LVO), plus internal drawing; strings via StringGadget border Image | SetFunction `gadtools.library` DrawBevelBoxA (public LVO, V exists in FD) + GT gadget creation (CreateGadgetA) for glyph/frame rendering. Everything inside gadtools internals is U | VP patches gadtools (V strings; which LVOs U) | Medium-high: gadtools internal drawing calls may bypass the public LVO (U) |
| Menus (pull-down, checkmark, submenu arrow) | Intuition internal, 3.2 draws submenu arrow (SUBMENU flag V intuition.h:116 "V47") | Pens only (BARDETAILPEN/BARBLOCKPEN/BARCONTOURPEN, V screens.h:109-113). Shape needs Intuition menu code patch (rig needed) | VP: "Full 3D screen titlebar" off etc.; unknown deeper (U) | High to change shape, pens low |
| Screen title bar (the bar itself) | Intuition internal; bar pens via DrawInfo | Pens (BARCONTOURPEN etc.) plus font; hide bevel by pens | VP: option; QuickFix fixed BARCONTOURPEN (V earlier doc) | Low (pens); high (shape) |
| Requesters / EasyRequest / ASL | Intuition + gadtools/ASL gadgets; use window border + GT/BOOPSI | No extra work if windows/gadgets are covered | same | Low once above covered |
| Window background fill | WA_BackFill hook, V intuition.h:1325 (app-supplied, not ours) | Not ours; Workbench background via our own wbpattern/ Amarchy | Birdie colours border fill only (V) | n/a |

## 2. Hook inventory, ranked by safety

1. Pens (DrawInfo array via SA_Pens / Preferences palette): zero patching, per screen; limited to colour. Existing Amarchy theme code already does this (see plan R1.25). V that pens exist (screens.h); that flat = bevel pens equal fill is visually sufficient is U.
2. BOOPSI class dispatcher replacement (`sysiclass`, `frameiclass`, `propgclass`, `imageclass` GM_RENDER side, `buttongclass`): public classes, supported by `OM_*`/`IM_*` method ids in headers (V), chain to the original `cl_Dispatcher` by saving `h_Entry`. Safe to restore: put old `h_Entry` back, but only when nobody else swapped it after us (same chain-repair logic VP implements for libraries, V above; for classes not seen in disasm, U). Needs MakeClass-style hook not SetFunction.
3. SetFunction on library LVOs (Intuition window/title, graphics primitives, gadtools): last resort, hot paths, strongest compatibility hazards.
4. Owning drawing entirely (replacing Intuition's window frame code by patching OpenWindow to attach our own border gadgets/images and set WFLG_BORDERLESS): changes window geometry, breaks apps that read `BorderTop/Left`: reject.

## 3. Live switch and clean uninstall

- Patches are removable: VP's remove routine implements chain repair (V, 0x16df6). If another patcher (MCP, PatchControl, ToolsDaemon, SetPatch) patched on top, we cannot unlink safely except by that JMP-slot rewrite; the stub memory must stay allocated (never freed) when `still_chained` is set. This is the realistic limit of "off = stock": off restores behaviour, RAM stays.
- Class dispatchers: our switch is a boolean checked at the top of our dispatcher ("off = call old dispatcher"). This is the safest live switch: no unpatching ever, one global flag, patch stays resident but inert. Recommend this for BOTH class and SetFunction patches: do not remove, make inert (U for SetFunction paths: a function that always JMPs the old pointer is safe and cheap).
- Patch order with other tools: since Birdie/VP must run before/after IPrefs in a specific order (earlier doc), an Amarchy patch started from User-Startup after IPrefs sits above VP/Birdie if both are present; coexistence rules unneeded because we are replacing them, but a user keeping Birdie must run theirs first: U.
- Reboot not required for flag flip; windows already open need `RefreshWindowFrame()` / `RefreshGList` (public calls) to redraw: U that every app cooperates.

## 4. RTG and AGA

- AGA planar 8 bit: Birdie only changes the fill colour and allocates pens (V from earlier doc; matches the primitives it patches); we avoid it by using DrawInfo pens only. Class-based drawing uses the RastPort pen API, works on any depth. Palette reallocation fights with FullPalette/ObtainPen are the known pain; our theme palette is set at screen open (plan R1.25).
- RTG (P96/CGX): drawing goes through graphics.library calls which P96/CGX replace with their own patches (SetFunction on the graphics base) (U, known behaviour but not checked here). Anything we patch in graphics.library sits above or below RTG depending on start order, and a Draw patch of ours would see 24-bit pen values differently: flat fill via pens is RTG-agnostic. Birdie gradients need hi/true colour and cybergraphics (V: strings include cybergraphics.library); we want none.
- 68060: every SetFunction path needs `CacheClearU` after writing the stub (VP and Birdie both do, V -636 exec call right after SetFunction) and the stub must be position independent or relocated before the flush.

## 5. 3.1 differences (cheap notes, all U unless stated)

- ICONIFYIMAGE (0x16) and IM_DRAWFRAME knob support are V47: absent on 3.1 (V headers). Dispatcher-replacement plan degrades gracefully: unknown SYSIA_Which falls to the old dispatcher.
- 3.1 has no `select.gadget`/BOOPSI-aware gadtools quirks (VP needs the former).

## 6. Biggest unknown and what to measure on a rig first (not run; do it before any phase 2)

Headers cannot prove which routines paint the window border and title bar. Required rig measurements, in order, on `--os 32` (3.2) and 3.1:
1. Dump `FindClass` list and each public class's `cl_Dispatcher.h_Entry` address range (is `sysiclass/frameiclass/propgclass` in Intuition ROM, or loaded from disk classes?), so the dispatcher-replacement plan is checked.
2. Snoop (wasabi) graphics `Draw/RectFill/Move/Text/BltBitMapRastPort` callers' return addresses while a window opens, to see whether border and title bar are painted by intuition ROM calling these LVOs (then Birdie-style filtering works) or by an internal inlined routine (then only pens and class dispatchers work).
3. Check whether the 3.2 window gadgets (close/depth/zoom) are created via `sysiclass` NewObject (then the dispatcher swap fully covers them) by logging `NewObjectA` (intuition -636 hits Birdie-style) args at window open.
4. With pens set so shine = shadow = fill on one test screen: screenshot the border, title bar, menus. If that already reads "flat enough", phase 1 needs zero patches.

## 7. Verdict

| Element | Feasible own patch | How |
|---|---|---|
| Colour of everything (border fill, bevels, bar, menus) | yes | DrawInfo pens, already ours; shine/shadow = fill for flat (U visually) |
| Close/depth/zoom/iconify/size glyphs | yes | `sysiclass` dispatcher replace (public, V in headers) |
| Button / BOOPSI frames | yes | `frameiclass` dispatcher replace, keep IM_FRAMEBOX metrics |
| Border scrollers, prop knobs | yes with care | `propgclass` GM_RENDER; classic Gadget-struct props remain U |
| GadTools gadgets/frames | probably | gadtools `DrawBevelBoxA` SetFunction + glyph LVOs; internal-routine bypass is the risk (U) |
| Window border shape, title bar shape | maybe | graphics primitive filter (Birdie way) or Intuition internal patch: only after rig measurement 2; reject if inlined |
| Menus shape | no for now | pens only until a rig shows a hookable path |
| Screen title bar shape | pens yes, shape unknown | |

Phases: P1 pens-only flat preset plus live switch (no patching, can land now, measures how far we get); P2 class dispatcher set (`sysiclass`, `frameiclass`, `propgclass`) with a global inert flag; P3 gadtools; P4 border/title shape only if rig measurement 2 shows a clean path, else drop and keep P1 look. Biggest unknown: whether Intuition 3.2 paints window borders and the title bar through patchable graphics/Intuition LVOs or entirely inline (section 6, measurement 2). Second: the exact LVO list VP patches (U) and which library each Birdie patch hits (U).
