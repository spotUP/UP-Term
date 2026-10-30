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

## 8. Rig measurements (3.2)

Rig 7, `--os 32` (Kickstart 3.2.3 ROM, 512 KiB at 0xF80000, Intuition in ROM). Probe program (scratchpad only, not committed): SetFunction stubs on graphics Text/Move/Draw/RectFill/BltBitMapRastPort/BltTemplate/ClipBlit/SetAPen/WritePixel and intuition NewObjectA/DrawBorder/DrawImageState/PrintIText that log the caller return address, then OpenWindow (close, depth, zoom, size, drag bar, title, screen title), SetWindowTitles, RefreshWindowFrame, SetMenuStrip plus a held right mouse button (menu open), CloseWindow. Marks: VERIFIED (V, evidence given) / UNVERIFIED (U).

| # | Finding | Mark | Evidence |
|---|---|---|---|
| 1a | `frameiclass`, `propgclass`, `buttongclass`, `imageclass`, `gadgetclass`, `rootclass`, `strgclass`, `icclass`, `modelclass`, `fillrectclass`, `itexticlass` are public classes in RAM (IClass structs at 0x1000d0e8..0x1000d490, built at boot) whose dispatcher `cl_Dispatcher.h_Entry` is the SAME ROM address 0x00FE1118 for all of them (a Hook trampoline). | V | NewObjectA(NULL, name) then `OCLASS(obj)->cl_Dispatcher.h_Entry`, printed per class |
| 1b | `sysiclass` cannot be instantiated without SYSIA_Which and DrawInfo (NewObject returned NULL), so its h_Entry was not printed directly. It is the class of the window gadget images (see 3). Its dispatcher is expected to be the same trampoline. | U (entry address) | NewObject NULL; class id read from live gadget images in 3 |
| 1c | `gradientsliderclass`, `bitmapclass`, `textfieldclass`, `colorwheelclass`, `getfileclass`, `layoutclass` do not exist on this stock 3.2 rig (they are disk classes, not ROM). | V | NewObject NULL |
| 2 | Border, title and gadget painting is NOT inlined: every graphics call during OpenWindow, SetWindowTitles, RefreshWindowFrame and the menu comes from ROM code through the library LVO jump table (our SetFunction stubs saw them). Return addresses: border bevels `RectFill` at 0xFE6346, 0xFE6376, 0xFE63C4, 0xFE63EE (+`SetAPen` 0xFE6392), 4 sets per RefreshWindowFrame; title text `Move` 0xFE5692 / `Text` 0xFE56BE with `PrintIText` 0xFEB2C0; gadget glyphs `DrawImageState` 0xFDA582 (9 on open, 4 per refresh) and `BltBitMapRastPort` 0xFE5510; screen title bar `RectFill` 0xFF26AC, `SetAPen` 0xFF2558, `Move` 0xFF283A, `Text` 0xFF2952, `BltTemplate` 0xF9ACA8 (these fire on SetWindowTitles with a new screen title). Draw was never called (borders are RectFills). | V (calls and addresses); which routine is "border" vs "screen bar" is inferred from which phase fires it | per-phase caller histogram, probe output scratchpad `probe.out`, `probe2.out` |
| 3 | The 4 clickable window gadgets (depth 0x8015, zoom 0x8045, close 0x8065, 0x8085) have `GadgetRender` = BOOPSI image objects whose class id is `sysiclass`; the 5th (0x8025, size/drag) has no image. At open Intuition calls NewObjectA 5 times from ONE ROM site 0xFE5E1E, always with a class-ID STRING and a NULL class pointer: `buttongclass` x4 and `gadgetclass` x1 (the gadget bodies). The gadget images are not created by a logged NewObjectA call (internal creation), but the live objects report `sysiclass`. | V | NewObjectA stub logged a1 string; `OCLASS(GadgetRender)->cl_ID` printed "sysiclass" for 4 gadgets |
| 4 | Menu draw: pull-down uses `RectFill` 0xFE5314, 0xFE526A, 0xFE527C, 0xFE52A8, 0xFE52BA, `PrintIText` 0xFE46BA and 0xFE3BC0, `Move/Text` 0xFE5692/0xFE56BE, `DrawImageState` 0xFDA582 (checkmark/arrow images), plus the bar `RectFill` 0xFF26AC. | V | menu-phase histogram, right button held 3 s |
| 5 | Measurement 4 (screenshot with flat pens): not done in this run. | U | out of budget |
| 6 | graphics.library and intuition.library LVOs patched by SetFunction see ROM Intuition calls on a plain 68020/060 rig. Whether the A1200 (real 060, MMU/caches, other patches) behaves the same, and 3.1, were not measured. | U | rig only, 3.2 only |

### Revised section 7 verdict

| Element | Patchable? | How / evidence |
|---|---|---|
| Window border (bevels, fill) | Yes, via graphics LVO | Painted by `RectFill`/`SetAPen` from fixed ROM sites 0xFE6346..0xFE63EE through the LVO (V, 2). A `RectFill`/`SetAPen` patch that matches caller range 0xFE6300..0xFE6400 (or window border rastport geometry) can recolour/flatten; shape (thickness) stays. Hot path: filter by return address range, not by every call. Pens alone also work (item 5 U). |
| Window title bar (text, background) | Yes, via graphics LVO plus PrintIText/Text | `Move`/`Text`/`PrintIText` from 0xFE5692/0xFE56BE/0xFEB2C0 (V, 2). Replacing the text path is possible; bar height stays from font. |
| Close/depth/zoom/size glyphs | Yes, via `sysiclass` class | Live gadget images are `sysiclass` objects (V, 3); class struct is in writable RAM and its dispatcher is the shared trampoline 0xFE1118 (V, 1a). Swap that class's `cl_Dispatcher` (not the shared 0xFE1118 itself, which all classes use). `DrawImageState` LVO (0xFDA582 caller) is a second hook. Entry address of sysiclass itself U (1b). |
| Button/prop frames (BOOPSI) | Yes, via class | `frameiclass`, `propgclass`, `buttongclass` exist as RAM classes with the shared trampoline (V, 1a). |
| Screen title bar | Yes, via graphics LVO | `RectFill` 0xFF26AC, `Move`/`Text`/`BltTemplate` from 0xFF283A/0xFF2952/0xF9ACA8 (V calls; attribution inferred). Pens alone remain the safe first step. |
| Menus | Yes, via graphics LVO and `PrintIText`/`DrawImageState` | Fixed ROM call sites listed in 4 (V). Shape is drawn with RectFills, so a caller-filtered RectFill patch can flatten it. Still U on checkmark/arrow images. |

Net change from section 7: the "maybe/reject if inlined" cases (border shape, title bar, menus, screen bar) are NOT inlined on 3.2; the graphics LVO path works and a caller-address filter is available as a precise selector. Caller addresses are for Kickstart 3.2.3 only and must not be hard-coded: select by "caller lies inside intuition.library's code range" plus rastport/bevel geometry. Not yet verified: A1200, 3.1, third-party patches (Birdie, VP) stacking on the same LVOs, and the flat-pens screenshot.

## 9. FLAT-R1 IControl geometry probe (rig 5, 2026-10-30)

Probe: `tests/amiga/icprobe.c` writes ENV:Sys/icontrol.prefs (FORM PREF, PRHD, ICTL 38 bytes) with ic_Flags = ICB_VERSIONED only, ic_Version 2, ic_VersionMagic 0, and ic_GUIGeometry[4] from the command line. It waits 4 s, then prints screen bar height, new-window borders and the system gadgets. Raw sweep: scratchpad `flat-r1-p1.md`.

Baseline `--os 32` (Kickstart 47.115, Workbench 47.2, intuition 47.53): screen bar 10, window BorderLeft 4, Top 11, Right 18, Bottom 2, title gadgets 11 high, size gadget 18 x 10.

| byte | meaning | measured | live? |
|------|---------|----------|-------|
| 0 | screen title bar height increment | bar = 10 + v (1 gives 11, 16 gives 26). Values 254 and 255 (negative) break LockPubScreen("Workbench"): rc 10 even after 8 s. Not usable for thinner. Writing 0 restores. | yes, same screen address, no reset |
| 1 | window title bar height increment, signed | BorderTop = 11 + v (not capped at 16: 100 gives 111). 250, 254, 255 clamp to -1: BorderTop 10, title gadgets 10 high. | live for new windows, but every change reopens the Workbench screen (new screen address) |
| 2 | no effect on borders or gadgets at 1, 4, 16, 100, 255; one screen reopen at 100. Meaning unknown. | | |
| 3 | border gadget scaling | right border 18 at 0, 20 at 1..100, 22 at 255; size gadget 18 x 10, 20 x 10, 22 x 11. Steps, not linear. | yes, no reset |

- Thinner than stock: yes, by exactly 1 px, byte 1 only (BorderTop 10 against 11). Nothing thins the left (4), right (18) or bottom (2) border: they never changed.
- No reboot needed on 3.2: IPrefs applied each write to new windows inside the 4 s wait. The file is deleted afterwards and the baseline returns.
- The IntuitionControlA layout hook: the NDK says "No public tags defined so far" and the NDK3.2R4 files carry no documentation of a hook tag. Not probed. Unverified.
- 3.1 (`--os 31`, intuition 40.85): the same file is accepted without error but every window and screen metric stayed at the baseline for geometry bytes 4/0/0/0, 0/4/0/0, 0/254/0/0 and 0/0/0/100. 3.1 has no route for the geometry. Per OD-F3 this goes back to the owner as a question.

## 10. FLAT-R3 sysiclass Hook layout (rig 5, 2026-10-30)

Probe: `tests/amiga/sysiprobe.c` (NewObjectA sysiclass with SYSIA_DrawInfo, OCLASS, dump Hook; sweep SYSIA_Which 0..24).

| | 3.2 (intuition 47.53) | 3.1 (intuition 40.85) |
|---|---|---|
| sysiclass h_Entry | 0x00FE1118 | 0x00FDE028 |
| sysiclass h_SubEntry | 0x00FF02F0 | 0x00FD9938 |
| h_Data | 0x1000bdc4 | 0x1000b04c |
| h_Entry shared with imageclass, frameiclass, gadgetclass, propgclass | yes | yes |
| h_Entry code | `jmp 0xFF4BE2` (shared ROM dispatcher) | `move.l a6,-(sp); move.l a1,-(sp); move.l a2,-(sp); move.l a0,-(sp); movea.l 16(a0),a6; movea.l 12(a0),a0; jsr (a0); lea 12(sp),sp; movem.l (sp)+,a6; rts`-style thunk: calls h_SubEntry with C stack args (hook, obj, msg) and a6 = h_Data |
| h_SubEntry | per class; sysiclass starts `suba.w #228,sp; movem.l ...; movem.l a0-a2,104(sp)` (register arguments) | per class; sysiclass starts `link a5,#-120` (C stack arguments) |

Answer for FLAT-C1: swap `h_SubEntry`, never `h_Entry`, on both versions (h_Entry is one routine for five classes). The replacement must match the calling convention of the version: stack (hook, obj, msg) on 3.1 via the thunk; on 3.2 the original sub takes a0-a2 (the 0xFF4BE2 dispatcher body was not disassembled, so the convention is unverified: C1 must call the saved original with the same registers it receives, i.e. a register-preserving trampoline, and test both OS).

SYSIA_Which sweep (class pointer equal to sysiclass for every created object): 3.2 creates 1,2,3,5,8,10..17,22; 3.1 creates 1,2,3,5,10..17. NOOBJ in the sweep: 3.2 0,4,6,7,9,18..21,23,24; 3.1 0,4,6..9,18..24. Value 0 (DEPTHIMAGE) shows NOOBJ because of a probe bug (`which ? t : t+2` treats 0 as no tags): not re-tested. Values 0x1B (menu MX) and 0x2A (shift key) are outside the 0..24 sweep: not tested. Both close/depth/zoom/size and the checkmark, MX, Amiga-key and arrow images are therefore drawn by sysiclass and covered by one hook.

3.1 flat palette: `SYS:Prefs/Palette FROM file USE` hung the rig (3.1 Palette is not the 3.2 command line tool), so the 3.1 pixel dump with flat pens was not taken; only the 3.1 stock dump exists. The 3.1 route to apply a palette (no ic_GUIGeometry either, see section 9) goes back to the owner (OD-F3).

## 11. FLAT-M1 residue pixel dump (rig 5, `--os 32`, 2026-10-30)

Method: `tests/amiga/flatprobe.c` opens an active and an inactive window (220x120) with close/depth/zoom/size/drag gadgets plus GadTools button, string, checkbox, cycle, listview and a DrawBevelBox, then dumps the screen pixels as pen characters (ReadPixel; no picture read). Run once on the stock pens (`0 1 1 2 1 3 1 0 2 1 2 1`) and once on the flat pen8 palette (`tests/data/desk/palette-flat-pen8.prefs`, pens `0 1 2 0 1 3 0 0 2 2 0 1`, shine == background).

Findings with flat pens (theme A, active window):
- Window frame: stock uses pen 2 (shine) for the top and left outer edge and pen 1 (shadow) for the inner edge. With flat pens shine equals background, so the outer edge disappears and one 1-pixel dark line remains between title bar and body. Pens alone are enough here.
- Close, depth, zoom, size glyphs (sysiclass): the shine fill becomes background; close is an outlined square, depth and zoom are outlines of overlapping rectangles, size an outlined staircase. Readable, but the old shapes stay. The gadget divider changes from a 2/1 two-tone pair to one line. Needs the sysiclass hook (P3) for a redrawn flat glyph.
- GadTools button, string, bevel box, listview (frameiclass / GadTools bevel code): the light top and left edge becomes background, but the dark right (2 px) and bottom shadow stay. Result is a one-sided L-shaped shadow, no full outline. Pens cannot give a full outline. Fix route: frameiclass hook or owner decision OD-F9.
- Screen title bar: bottom row is a pen 1 line, top rows pen 2 (stock edge) over a 0 background; the bar is flat enough with pens.

Not measured (unverified): pull-down menu (checkmark, submenu arrow, separator), theme B, inactive window rows (dumped, not analysed), Workbench drawer scrollers, 3.2 Prefs editor, DOpus Magellan II, 4-colour screen mode, and the whole set on `--os 31`. Raw dumps are in the session scratchpad, not committed.

## 12. FLAT-M1 remaining elements (rig 5, 2026-10-30)

Pixel dumps with flat pens `0 1 2 0 1 3 0 0 2 2 0 1` (Workbench screen via Palette prefs, or own screen via SA_Pens). Probe: tests/amiga/flatprobe.c (modes BASIC, MENU, SCROLL, SCAN, OWN FLAT depth) and tests/amiga/sysiprobe.c. Evidence files: scratchpad flat-m1r-*.txt.

- sysiclass SYSIA_Which sweep 0..0x2C (3.2): objects are created for 0 (DEPTHIMAGE), 1, 2, 3, 5, 8, 10..17, 22, 0x1B (27, menu MX) and 0x2A (42), all with the same class pointer 0x1000d618, so the R3 hook covers them. The earlier "0 NOOBJ" was a probe bug (tags were not passed).
- Menus (3.2, depth 8): the menu box edge is a 2 px pen-2 line on the left, pen-1 and pen-0 lines at the bottom, with no shine/shadow pair; checkmark, submenu arrow and the stippled separator are single-pen glyphs (pen 0 on pen 1). Nothing beveled remains.
- Drawer-style scroller window (3.2 and own 4-colour screen): the prop knob is a 1 px pen-1 outline with a dither fill in a pen-0 container; no two-tone edges. Arrow gadgets lie below the dumped 120 rows, so their pixels are not measured (they are sysiclass images, covered by the R3 hook).
- Prefs editor (SYS:Prefs/Input, 3.2): sliders, checkboxes, cycle gadgets and listview boxes show the same one-sided L shadow as the GadTools gadgets (pen-1 lines on top/left or bottom/right only). Same fix route as GadTools bevel boxes.
- 4-colour screen (own screen, depth 2) and 8-colour (depth 3): BASIC, MENU, SCROLL dumps equal the depth-8 shapes; the flat pens map to pens 0..3 so no extra residue.
- Theme B (same pen map, other colours): window pixel rows identical to theme A; only the screen-bar clock/text rows differ. Pen indices, not colours, decide the residue.
- NOT measured: DOpus (no DOpus5: assign on this rig), everything on `--os 31` (rig start was cut off by the disk-space stop; the known `Palette ... USE` hang also blocks the Workbench-screen route there), arrow gadget pixels of the scroller, a real drawer window.
