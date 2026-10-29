---
date: 2026-10-29
topic: AMARCHY-FLAT
status: draft
author: spotUP
ledger: thoughts/spot/todos.md row AMARCHY-DESK-VISUALPREFS (3319); supersedes R1.25 of the P1b plan
depends_on: 2026-10-29-amarchy-p1a-native-theme.md (desk_core / desk_dos, pens R1.19, revert R1.16a)
research: thoughts/shared/research/2026-10-29-amarchy-flat-ui-patch.md (sections 1-8, rig measurements on Kickstart 3.2.3), thoughts/shared/research/2026-10-29-amarchy-visualprefs-birdie.md
supersedes: R1.25 (P1B-V0..V4, F1-F2, B1-B2) in thoughts/shared/plans/2026-10-29-amarchy-p1b-third-party-theme.md
---

# Amarchy flat-UI system patch (own code, replaces VisualPrefs / FullPalette / Birdie)

## Goal

Amarchy draws the system GUI flat with its own code, driven by the current Amarchy theme colours, switchable live, and with "off" giving the stock Workbench look. No third-party patch (VisualPrefs + VP-QuickFix, FullPalette, Birdie 2000) and no code copied from them; the research note records mechanisms only.

Owner: "this would give us full control over how it looks".

Done (reachability protocol, gate 3) = one rig test (FLAT-X1) drives Amarchy's top-level theme entry point (ARexx `DESK THEME <name>` on the Amarchy port, the same verb the theme menu uses), proves the patch code ran (sentinel counters in the shared state block, see FLAT-A3), and asserts the pixel colours / pen numbers the theme defines for each element in scope, for two themes and for off.

Status: draft. Nothing below is built. Owner decisions at the end are open.

## Architecture

### Where the patch code lives: separate resident `amarchy-flat` (recommended) vs inside Amarchy

| | Separate resident program `amarchy-flat` | In-process in the Amarchy commodity |
|---|---|---|
| Lifetime of patch code | Own process that never unloads while any vector or class hook is still chained (Wait forever, inert) | Tied to Amarchy: Amarchy quits, restarts on RELOAD, is replaced by upkg / `rig.py install` (warm-server swap, plan 2026-10-27-amarchy-gui-warm). Patch code inside an unloaded seglist = crash in every caller |
| Crash isolation | A bug in the commodity (menus, tiling, ARexx) cannot unload drawing code | Any commodity exit path must first unpatch, and unpatch can fail (patched on top) |
| Size in callers' context | Small, audited, no libc beyond what the draw code needs | Whole commodity linked in |
| Cost | One more program, one startup block, an interface (public semaphore + state block) | No IPC |
| Upgrade | Replacing the binary needs reboot (code chained) or a clean unpatch first | Same problem, worse: Amarchy is upgraded often |

Recommendation: the separate resident program. Decisive trade-off: one extra program and a small shared-state interface, against Amarchy's frequent restarts and replacements, each of which would otherwise be a system crash once any other patcher sits on top of our vectors.

### Interface Amarchy <-> amarchy-flat (FLAT-A3)

- Public semaphore `Amarchy.Flat` (AddSemaphore) embedding a versioned state block in MEMF_PUBLIC: `version`, `size`, `enabled` (the global inert flag, one word, read without locking by patch code), `elements` bit mask (border, title, glyphs, frames, props, screenbar, menus, bevelbox, corners), the flat pen table (per element: fill, line, text, active/inactive), look options (title alignment etc., see Owner decisions), sentinel counters (calls handled per element, calls passed through, calibration result), `still_chained` bits.
- Amarchy writes the block under ObtainSemaphore, then refreshes open windows (FLAT-A4). Patch code only reads; counters are incremented with single `addq.l` (no lock).
- `amarchy-flat` absent: Amarchy's theme apply skips the step silently and the STATUS reply says `flat=absent`.

### Hook mechanisms, ranked (from research section 2 and 8)

1. Pens only (P1): no patch.
2. BOOPSI class hook swap (P2): `cl_Dispatcher` of the RAM IClass structs (V, research 8.1a: frameiclass, propgclass, buttongclass share ROM trampoline 0xFE1118; swap the class's Hook, never the trampoline).
3. SetFunction on graphics / intuition LVOs filtered to Intuition's own callers (P3): verified non-inlined on 3.2.3 (research 8.2).
4. SetFunction on gadtools `DrawBevelBoxA` (LVO -120, V from ndk/lib/fd/gadtools_lib.fd) (P4).
Rejected: owning the frame by borderless windows + own gadgets (changes BorderTop/Left, research 2.4).

### Caller selection without hard-coded addresses (FLAT-A5)

The 3.2.3 return addresses in research 8 are evidence, not constants. `amarchy-flat` self-calibrates at start:
1. Code range: intuition's ROM span `[FindResident("intuition.library"), rt_EndSkip)`, the same test device/upconsole.c:302 `patched_vector` uses for console.device (reuse: move it into a shared helper, do not copy).
2. Calibration run: with the patches installed but in "record" mode, open a hidden 1-window probe on a private screen (OpenWindow, SetWindowTitles, RefreshWindowFrame, a menu strip laid out with LayoutMenus... no user input), and record the return-address set per phase. Each RectFill / SetAPen / Move / Text / PrintIText / DrawImageState call site seen in the "frame" phase becomes a border site, "title" phase a title site, and so on. Pure classifier `flat_calib_classify` (host-tested).
3. Steady state: a call is ours only when its return address is in the recorded site set for that element AND the RastPort's layer belongs to a window border / the screen bar layer. Anything else: jump to the old vector at once.
Calibration that fails (no sites, sites outside the intuition span, a site set that differs between two runs) leaves that element inert and records why in the state block. Note: BltTemplate's caller 0xF9ACA8 (research 8.2) is outside the expected intuition span and is most likely graphics' own Text calling BltTemplate: it is never a selection site.

### Live switch, off and uninstall (FLAT-A2)

- Every patch entry first tests `enabled && (elements & bit)`; false = tail-jump to the saved old vector / old hook. Off is therefore instant and needs no unpatch (research 3).
- Uninstall (Ctrl-C to `amarchy-flat`, or `amarchy-flat QUIT`): for each vector, SetFunction(old) and compare the returned value with our entry; on mismatch (patched on top) put it back, stay chained and inert, set `still_chained` (mechanism described in research 0, written from scratch). Class hooks: restore only if `h_Entry` is still ours. The process exits only when nothing is chained, else it stays resident and inert until reboot and says so.
- 68060: CacheClearU after each SetFunction and each hook write, under Forbid.

### Refresh of open windows (FLAT-A4)

After a state change Amarchy walks screens/windows under LockIBase (reuse the walk in amarchy/tile.c:290 / :308, extract a shared iterator if it is not one already) and calls RefreshWindowFrame per window, and for the screen bar ShowTitle off/on or the IPrefs-free equivalent measured in FLAT-R2. Gadget imagery: RefreshGList on the window's gadget list. Apps that draw their own frames refresh at their next own redraw (documented limit).

### Failure safety (patch crash = system crash)

- Kill switch 1, prefs: amconf key `amarchy-flat` (default per OD-F1) gates the startup block; off = the program is not started.
- Kill switch 2, boot hold: holding both mouse buttons (or the left button, per OD-F7) during boot makes `amarchy-flat` print one line and exit before patching.
- Kill switch 3, crash marker: before patching, write `ENVARC:Amarchy/flat-starting`; delete it after calibration plus 60 s up. If the marker exists at start, start inert (patches not installed), set STATUS `flat=held (last start did not finish)` and show it in prefs. Pattern reused from the desk pending-revert marker (amarchy/desk_core.h desk_marker_make / desk_marker_parse, desk_dos.c revert path); no new marker format.
- Every patch entry is guarded by `enabled` and the per-element bit, so Amarchy can turn a misbehaving element off live without reboot.
- Stack: `amarchy-flat` gets `__stack` + `$STACK` cookie (memory rule). Patch code itself runs on the caller's stack: it uses no recursion, no libc, at most ~64 bytes of locals (checked by a host size test on the compiled object, FLAT-A6).

### Install / uninstall and stacking with other patchers (FLAT-A7)

- Startup block `;BEGIN Amarchy-Flat` ... `;END Amarchy-Flat` in S:User-Startup, after IPrefs and after any RTG / MCP / PatchControl lines (User-Startup runs after Startup-sequence, so after SetPatch and P96 init). Reuse P1B-C1 `desk_startup_block_set` / `_remove` (P1b plan; still needed for the DOpus rows) and the Installer block pattern (dist/Install.installer:86, dist/Uninstall:101). Kit row in dist/packages.txt as part of the Amarchy add-on.
- Detection of other patchers at start (reuse `patched_vector` span test): for each LVO we patch, record whether its vector already points outside the ROM module, and whose task (VisualPrefs, Birdie by task name; MCP, PatchControl generic). Policy when VisualPrefs or Birdie is present: Owner decision OD-F5. MCP / PatchControl / SetPatch: we chain normally (call old). A patcher installed above us that calls us with `jsr` instead of a tail jump changes the return address we see: our filter then matches nothing and that element simply stays stock (safe failure, recorded in counters as `passed through`).
- PatchControl / SnoopDOS-style listers see our patches under the task name `amarchy-flat`.

### RTG vs AGA

- All drawing goes through the pen-index API (SetAPen + RectFill, Move/Text) of the RastPort we were handed: depth-agnostic on AGA planar and on P96/CGX. No direct bitmap writes, no BltBitMap, no 24-bit colour; colours come from the theme palette loaded by P1a (desk_palette), pens obtained by the screen's existing pen allocation.
- RTG libraries SetFunction graphics LVOs at boot (U, known behaviour). We start later, so we sit above them and call down; nothing to special-case. Rig check on an RTG rig only if one exists (FLAT-R4); otherwise A1200 check covers AGA only and RTG stays U.
- 8-bit AGA: the flat look needs at most 8 theme pens (DK_ROLES), within P1a's palette.

### 3.1 notes

- P1 pens: identical mechanism on 3.1 (P1a already handles 3.1 pointer editor separately).
- P2: classes on 3.1 are ROM classes too (U); ICONIFYIMAGE and IM_DRAWFRAME knobs are V47, so on 3.1 those paths fall to the old dispatcher (research 5).
- P3: calibration makes the code Kickstart-independent in design; whether 3.1 Intuition reaches graphics through the LVOs is U and measured by FLAT-R1 on `--os 31`.
- Scope of 3.1 support is OD-F3.

## Reuse map

| Need | Reuse | Where |
|---|---|---|
| Theme roles, flat pen mapping | `desk_pens`, DK_* roles and DrawInfo slots, `desk_role`, `desk_darker`, `desk_contrast` | amarchy/desk_core.h:11-40, desk_core.c:99 |
| Pen change resets WB screen | `desk_pens_reset` + `desk_dos_apply` close/reopen path, `desk_dos_wb_close/open` hooks | amarchy/desk_core.h:40, desk_dos.c:132-231 |
| Snapshot / revert / confirm requester / boot restore | desk_dos_apply flow, T:amarchy-revert, marker | amarchy/desk_dos.c:15-231, desk_core.h:85-99 |
| Theme load and live switch entry | `gt_dos_load_current`, `gt_dos_select`, ARexx `DESK THEME` | amarchy/gthemes_dos.h:14-23, amarchy/amarchy.c (DESK verbs) |
| ROM-module span / "is this vector patched" | `patched_vector` | device/upconsole.c:302 (extract to a shared header, both use it) |
| SetFunction + restore skeleton | patchcon | tests/amiga/patchcon.c:27-31 |
| Host draw tests against a fake RastPort | gfx_fake (logs every graphics call, draws RectFill/Text into a pixel buffer) | tests/render_host/gfx_fake.[ch] |
| Window walk under LockIBase | tiling walk | amarchy/tile.c:290, :308 |
| Sentinels | `am_sentinel`, `am_sentinel_str` | amarchy/sentinel_dos.c:5, :17 |
| Startup block edit | P1B-C1 `desk_startup_block_set/_remove`; Installer blocks | P1b plan; dist/Install.installer:86, dist/Uninstall:101 |
| Option storage / prefs rows | amconf AM_* keys, PG_PAGE_AMARCHY | amarchy/amconf_core.h:12-26, prefs/settings_def.c |
| Rig test skeleton | RX1 desk rig, `ami.read_file` | tools/rig/amarchy_desk_rig.py (P1a), tools/rig/ami.py:518 |

Searched and not found (new code justified): no BOOPSI class hook swap in our own code (`git grep cl_Dispatcher` outside thoughts/ finds only reference/kingcon/KingCON-handler.asm:17566, a third-party class setup, not reusable C); no caller-address filter; no shared state block between Amarchy and a resident helper. SetFunction appears only in tests/amiga/patchcon.c and device/upconsole.c (detection only).

## Checklist

Columns per item: files; hook; reuse; host test; rig check (`--os 32`, plus a 3.1 note); revert proof; A1200 check. Rigs run as 060 (`UPTERM_RIG=<n> rig.py start --max --os 32 --purpose "Amarchy flat: <item>"`), and stop at the end of the item.

### Probes (rig, before any patch code)

- [ ] **FLAT-R1 caller sites on 3.1.** Rerun the research-8 probe on `--os 31`: does Intuition reach RectFill/SetAPen/Move/Text/PrintIText/DrawImageState through the LVOs, and do all frame/title/menu sites lie in `[intuition rt, rt_EndSkip)`. Also on `--os 32`: confirm every site from research 8.2 lies in that span (except BltTemplate). Output: thoughts/shared/research/ note, section per OS. Answers OD-F3 input.
- [ ] **FLAT-R2 flat pens screenshot (research 8.5, open).** With the P1 flat pen map on 3.2: screenshot border, title bar, gadgets, menus, screen bar, a GadTools requester. Haiku reads the picture. Decides how much P2-P4 changes visibly beyond P1.
- [ ] **FLAT-R3 class hook layout.** For frameiclass, propgclass, buttongclass and sysiclass (created with SYSIA_DrawInfo from GetScreenDrawInfo, SYSIA_Which CLOSEIMAGE): print `h_Entry`, `h_SubEntry`, `h_Data`; confirm the trampoline at 0xFE1118 dispatches through `h_SubEntry` (disassemble 32 bytes). Decides FLAT-C0 (swap h_Entry vs h_SubEntry). 3.1 note: same dump on `--os 31`.
- [ ] **FLAT-R4 RTG.** If a rig with P96 exists: whether RectFill / Text are already SetFunction'd at User-Startup time (span test). Else record RTG as U.

### Common (host first)

- [ ] **FLAT-A1 program skeleton.** files: new amarchy/flat.c (OS half), amarchy/flat_core.[ch] (pure), Makefile rule `build/amiga/amarchy-flat`. hook: none yet; Wait on CTRL_C / CTRL_F; `__stack` + `$STACK` cookie. host test: none (OS shell). rig: starts, prints version, exits on Ctrl-C. revert: n/a. A1200: start/stop once.
- [ ] **FLAT-A2 install / chain / inert / remove (pure model + OS).** flat_core: `flat_vec_install`, `flat_vec_remove` over an abstract vector table (host fakes SetFunction), returning installed / restored / still_chained. host test: install, remove clean; install, other patch on top, remove -> still_chained + inert + old target rewritten never; double install refused; class-hook variant with a fake Hook. rig: patch a harmless LVO, patchcon-style second patch on top, remove: system keeps running, STATUS says still_chained. revert proof: after a clean remove, vectors byte-equal the boot snapshot (span test passes again). A1200: same, once.
- [ ] **FLAT-A3 shared state block.** `Amarchy.Flat` semaphore, versioned struct in flat_core.h (one header both programs include). host test: version / size mismatch refused by Amarchy's writer; field offsets pinned. rig: Amarchy STATUS shows `flat=on|off|absent|held`. revert: n/a.
- [ ] **FLAT-A4 refresh walk.** Amarchy side `flat_refresh_all` reusing the tile.c LockIBase walk (extract a shared iterator). host test: walk order over a fake screen/window list, each window refreshed once. rig: two windows open, theme switch, both frames repaint (pixel read via the rig). A1200: visual owner check.
- [ ] **FLAT-A5 calibration.** `flat_calib_classify(phase, ret_addr[], n, span, out_sites)` pure; OS half opens the private probe screen. host test: a recorded 3.2.3 trace (research probe.out, copied into tests/data/flat/) classifies into the research-8 site sets; a trace with one site outside the span drops that element; two disagreeing runs drop it. rig `--os 32`: sites printed to the state block equal research 8.2 values; `--os 31` per FLAT-R1. revert: calibration leaves no window or screen open (screen count before == after).
- [ ] **FLAT-A6 caller-context budget.** host test: objdump of the patch entry objects, stack frame <= 64 bytes, no `jsr` into libc. rig: run under `Stack 4096` shell, open 20 windows, no crash.
- [ ] **FLAT-A7 install / stacking.** startup block via P1B-C1, kit row, Uninstall row; detection of VP / Birdie / foreign patches at start (shared `patched_vector`). host test: block text, detection table from fake vectors. rig: (a) stock, (b) with a patchcon-style foreign patch on RectFill below us, (c) VP installed (P1b rig image) -> behaviour per OD-F5. revert: Uninstall removes block and binary, User-Startup md5 equals pre-install. A1200: install + uninstall once.
- [ ] **FLAT-A8 kill switches.** prefs key, boot hold, crash marker (reuse desk marker make/parse). host test: marker present -> inert decision; marker removed after the 60 s mark (injected clock, no real sleep). rig: kill the rig mid-start (after marker write, before delete; never mid disk write: wait 5 s quiet), reboot: `flat=held`; boot with both buttons held (rig input) -> not started. A1200: boot hold once.
- [ ] **FLAT-A9 prefs rows.** PG_PAGE_AMARCHY: Flat system look (on/off), one row per v1 element (OD-F2), look options (OD-F4), status line (on / held / absent / VisualPrefs present). Full English labels. host test: rows present, each writes its key. rig: covered by X1.

### P1 flat pens + live switch (no patch)

- [ ] **FLAT-P1-1 flat pen map.** files: amarchy/desk_core.[ch]: `desk_pens_flat(pens[DK_SLOTS])` (shine = shadow = the border tone, fill = accent, bar pens per theme) next to `desk_pens`; desk_palette / palette.prefs writer takes the map as an argument instead of calling desk_pens directly. hook: none (palette.prefs pens, IPrefs). reuse: desk_pens, desk_palette, desk_pens_reset. host test: golden palette.prefs for flat on/off (tests/data/desk/), every slot in the flat map moves the PALT pen bytes, contrast guard (desk_contrast) still refuses an unreadable theme. rig `--os 32`: DrawInfo of a freshly opened window equals the flat map (dri_Pens dump). 3.1: same on `--os 31`. revert: flat off -> palette.prefs bytes equal the P1a non-flat golden. A1200: owner looks at one window.
- [ ] **FLAT-P1-2 flat as a desk target.** D-TGT row (or an option of the palette row, decided in implementation by whether it has its own files: it does not, so it is a palette-row option, amconf key `amarchy-flat-pens`). Live switch: `DESK THEME` with the key on goes through desk_dos_apply (WB close/reopen when desk_pens_reset says so, 15 s confirm, revert). host test: desk_plan with the key on/off selects the map. rig: covered by X1 step 1. revert: confirm timeout restores the previous palette.prefs bytes.

### P2 class dispatchers behind the global inert flag

Each item: hook = swap of that IClass's Hook (h_Entry or h_SubEntry per FLAT-R3), old Hook saved, `enabled && bit` test first, unknown methods / attributes and OM_NEW / OM_SET / IM_FRAMEBOX / IM_HITTEST always to the old hook. Drawing code is pure C against the RastPort API so it runs on the host under gfx_fake.

- [ ] **FLAT-C0 class hook swap primitive.** `flat_class_hook(name, which, entry)` finds the IClass by NewObjectA(NULL, name) + OCLASS (the research probe method), swaps under Forbid + CacheClearU. host test: fake IClass / Hook model in flat_core (same model as A2). rig: swap and restore frameiclass with a pass-through hook, 1000 window open/close cycles, no change in pixels (rig pixel read).
- [ ] **FLAT-C1 sysiclass glyphs.** files: amarchy/flat_glyph.[ch] (pure geometry: rect lists per SYSIA_Which and box size for CLOSE, DEPTH, ZOOM, SIZE, ICONIFY on 3.2; menu checkmark and submenu arrow per OD-F2), flat.c hook. host test: gfx_fake renders each glyph at 3 sizes into pixels, compared to goldens; selected / inactive states use the state's pen; unknown SYSIA_Which goes to old hook (call-count). rig `--os 32`: the 4 window gadgets' pixels equal the golden after open and after RefreshWindowFrame; off -> pixels equal the stock capture. 3.1: ICONIFY absent, the rest same. revert: flag off + RefreshWindowFrame -> stock pixels. A1200: owner looks.
- [ ] **FLAT-C2 frameiclass frames.** IM_DRAW / IM_DRAWFRAME draw a flat 1-pixel line (or none, per OD-F4) + fill; IM_FRAMEBOX passes to the old hook so layout metrics never move. host test: gfx_fake per frame type (DEFAULT, BUTTON, RIDGE, ICONDROPBOX), recessed / selected; IM_FRAMEBOX result equals the old hook's on a recorded table. rig: a ReAction-free BOOPSI button window (small test program in tests/amiga/) pixels and IM_FRAMEBOX numbers unchanged layout. revert: same as C1.
- [ ] **FLAT-C3 propgclass knob and container.** GM_RENDER: flat container fill + flat knob for PROPNEWLOOK; anything else to the old hook. host test: gfx_fake, horizontal / vertical, knob at 0 %, 50 %, 100 %. rig: window border scrollers (a Workbench drawer window) pixels flat, knob position equals stock (same rectangle). Classic non-BOOPSI props stay stock (U, research 1): recorded, not hooked. revert: as C1.
- [ ] **FLAT-C4 buttongclass.** First check on the rig whether buttongclass GM_RENDER only draws its GadgetRender image (then the images in C1/C2 cover it and this item closes with a written exemption, protocol section 2) or draws a highlight itself (complement / bevel): then hook GM_RENDER for the highlight only. host / rig per outcome.

### P3 border, title, screen bar, menus via drawing-call patches filtered to Intuition's callers

Hooks: SetFunction graphics RectFill (-306), SetAPen (-342), Move (-240), Text (-60), intuition PrintIText (-216), DrawImageState (-618). LVOs to be rechecked against ndk/lib/fd at implementation. Each entry: flag test, then site test (FLAT-A5), then layer test, else tail-jump old.

- [ ] **FLAT-B1 window border.** At border sites: substitute the pen in SetAPen by the element's flat pen (border line / fill, active vs inactive from the window's WFLG_WINDOWACTIVE), RectFill unchanged geometry. Border thickness stays Intuition's (research 8 verdict). host test: replay a recorded border call sequence (tests/data/flat/border-32.trace) through the patch logic on gfx_fake, assert the pixel colours of every border column. rig: border pixels of active and inactive window equal theme pens for theme A and B; off -> stock. 3.1 per FLAT-R1. revert: off + RefreshWindowFrame -> stock pixels. A1200: owner looks.
- [ ] **FLAT-B2 window title.** Text pen and background at title sites; title alignment (OD-F4) by shifting the Move x at the title site within the drag bar (TextLength of the title, bounded by the gadgets' left edges). host test: replay trace, left / centre positions, a title longer than the bar is left as stock. rig: title text pixels and x position for both alignments.
- [ ] **FLAT-B3 screen title bar.** RectFill / SetAPen / Move / Text at the screen-bar sites (research 8.2: RectFill 0xFF26AC, SetAPen 0xFF2558, Move 0xFF283A, Text 0xFF2952 on 3.2.3; never BltTemplate). host test: replay trace. rig: bar pixels after SetWindowTitles with a new screen title, both themes.
- [ ] **FLAT-B4 menus.** Pull-down box fill and outline (RectFill sites research 8.4), item text pen at PrintIText / Text sites, separator bars. Checkmark / submenu arrow via C1 if they are sysiclass images (U, research 8 verdict). host test: replay a menu trace. rig: hold the right button 3 s, read box pixels, both themes; menu stays usable (select an item, Amarchy STATUS shows the menu action ran).
- [ ] **FLAT-B5 rounded window corners (only if OD-F6 picks a or b).** See OD-F6 for the facts. (a) fake corners: after the last border RectFill of a window refresh (B1 site), paint the 2..3 corner pixels per corner in the screen's background pen. files: flat_glyph (corner masks, pure), B1 hook. host test: gfx_fake corner masks at radius 2 and 3. rig: corner pixels equal the backdrop pen over the plain Workbench backdrop; a window over another window shows the notch (recorded as expected, screenshot for the owner). revert: off -> square corners.

### P4 GadTools

- [ ] **FLAT-G1 DrawBevelBoxA.** SetFunction gadtools -120 (V: 16th entry after bias 30 in gadtools_lib.fd). Not caller-filtered: every bevel box (apps and GadTools itself) goes flat; GTBB_Recessed and GTBB_FrameType (BBFT_BUTTON, RIDGE, ICONDROPBOX) map to C2's frame drawing (shared code, one function). host test: gfx_fake per frame type and recessed flag. rig: a GadTools test window (tests/amiga/, button, string, cycle, checkbox, listview) and the 3.2 Prefs editors' bevel boxes flat.
- [ ] **FLAT-G2 GadTools internal frames.** Measure on the rig whether GadTools gadget frames call DrawBevelBoxA through the LVO, use frameiclass images (then C2 covers them) or draw internally. Only an LVO or class path is hooked; an internal path is recorded as a known limit with a screenshot, written exemption.

### Reachability (one rig test)

- [ ] **FLAT-X1 `test_flat_theme_reaches_system_gui`** in tools/rig/amarchy_desk_rig.py (extends RX1). Rig `--os 32 --max`, purpose "Amarchy flat UI: theme drives the system look". Setup: kit with amarchy-flat installed, prefs key per OD-F1. Steps:
  1. Flat off: `DESK THEME <A>`; flat counters in STATUS do not move; border / title / gadget / bar / menu / bevel-box pixels equal the stock capture.
  2. Flat on (all v1 elements): `DESK THEME <A>`; per-element counters > 0 (sentinel: the patch ran); pixels equal theme A pens for each element; IM_FRAMEBOX numbers and window BorderTop / BorderLeft unchanged.
  3. `DESK THEME <B>`: every element's pixels move to theme B (every knob moves output), plus each look option of OD-F4 toggled once moves its pixels.
  4. Element bits one by one off: only that element returns to stock.
  5. Flat off live: all stock again without reboot; counters stop moving.
  6. Uninstall: User-Startup md5 equals pre-install, after reboot no `Amarchy.Flat` semaphore.
  3.1 run of steps 1-5 if OD-F3 includes 3.1.

## Host tests (summary)

New suite `make test ONLY=flat`: A2 chain model, A3 struct, A4 walk, A5 classifier on recorded traces, A6 budget, A8 marker, C0 hook model, C1-C3 / G1 glyph and frame goldens on gfx_fake, B1-B4 trace replays, P1-1 palette goldens.

## Risks

- A patch bug crashes every program that draws: mitigated by the kill switches, the per-element live bits, minimal code in caller context, and A6.
- Third-party patchers above us with `jsr` defeat the caller filter: safe (stock look) but silent; STATUS shows `passed through` counts.
- Kickstart variants (3.2.x updates, 3.1, 3.9 / 3.1.4 ROMs) change sites: calibration handles relocation; a changed call pattern drops the element to stock.
- Classic Gadget-struct props and apps that draw their own frames stay stock.
- Pens-only P1 changes every 3D bevel on the screen (shine = shadow): R2 decides whether P1 alone is acceptable as the default.

## Order

1. FLAT-R1..R4 in one probe session (rig 3.2, then 3.1).
2. Owner decisions answered.
3. P1 (P1-1, P1-2) can land before any patch exists.
4. A1-A3, A5, A2, A8, A6, then P2 (C0, C1, C2, C3, C4), then P3 (B1-B4, B5 if chosen), then P4 (G1, G2), A7, A9 alongside.
5. FLAT-X1 last; A1200 checks once per phase.

## Owner decisions

All open. Recommended option first.

- **OD-F1 Default on or off after install.** (a) Off; Amarchy prefs turns it on. Trade-off: no risk to users who never asked, but the flat look is not seen out of the box. (b) On for P1 pens only, patches off. Trade-off: flat colours by default with zero patching risk, but every bevel on the screen changes on first install. (c) On for everything. Trade-off: full look at once, a patch bug hits every new user at first boot.
- **OD-F2 Elements in v1.** (a) P1 pens + P2 glyphs and frames (C1, C2) + P3 border and title (B1, B2). Trade-off: the visible window look with the measured paths; screen bar, menus, props and GadTools follow later. (b) Everything in P1-P4. Trade-off: complete look, longest time to first release and largest crash surface. (c) P1 pens only. Trade-off: zero patch risk, bevels stay 3D-shaped.
- **OD-F3 AmigaOS 3.1 support.** (a) 3.2 only for patches; P1 pens on 3.1 too. Trade-off: one OS to verify, 3.1 users get colours only. (b) 3.1 and 3.2 for all phases. Trade-off: every rig check twice and FLAT-R1 may show 3.1 needs a different path. (c) 3.2 only for everything.
- **OD-F4 Look options.** Border width: (a) Intuition's width, not changeable (only option the measured path supports). Title alignment: (a) left as stock, (b) centred as an option. Trade-off: centring needs the Move shift in B2 and a fallback for long titles. Frame line: (a) 1-pixel line in the theme's border tone, (b) no line, fill only. Trade-off: (b) is flatter but buttons on same-colour backgrounds lose their edge. Glyph style: (a) thin line glyphs in the text pen, (b) filled blocks.
- **OD-F5 VisualPrefs or Birdie present.** (a) amarchy-flat refuses to start and the prefs page says which patch to remove. Trade-off: no stacking unknowns, user must choose. (b) Start and patch on top. Trade-off: two patchers redraw the same pixels, untested order. (c) Start with only the elements VP/Birdie do not touch. Trade-off: partial look, per-element detection needed.
- **OD-F6 Rounded window corners.** Facts: AmigaOS 3.x layers are rectangles clipped through ClipRect lists (VERIFIED: NDK 3.2 clib/layers_protos.h 47.1 lists no shape call; its only clip / hook calls are InstallClipRegion, InstallLayerHook, InstallLayerInfoHook, DoHookClipRects, Create*HookLayer, all rectangular; no LAYA_Shape / ChangeLayerShape anywhere in the NDK 3.2 include tree). A clip region limits drawing inside a layer, it does not make the layer's damaged area or the windows behind show through (UNVERIFIED that no undocumented 3.2 mechanism exists; headers cannot prove absence). (c) Recommended: no rounding in v1. Trade-off: square corners, no notch artefacts; Amarchy's tiling places windows edge to edge, where fake corners would show a notch against the neighbour most of the time. (a) Fake corners: paint the 2-3 corner pixels per corner in the screen's background pen (B5). Trade-off: right over the plain desktop, a visible notch over every other window and over the backdrop pattern; cheap and safe. (b) True corners: the corner pixels must show what lies behind, which needs layer shape support (absent) or a patch of layers.library to backfill from the windows behind (simple-refresh layers keep no off-screen copy). Trade-off: correct look, but a layers.library patch is the highest-risk hook in the system and is weeks of work; not recommended.
- **OD-F7 Boot kill-switch gesture.** (a) Both mouse buttons held at boot. Trade-off: rarely pressed by accident, needs a two-button mouse (all targets have one). (b) Left button. Trade-off: easier, collides with other tools that use the left button at boot. (c) No gesture, crash marker only.
- **OD-F8 Process model.** (a) Separate resident `amarchy-flat` (recommended above). Trade-off: one more program and an interface vs crashes on every Amarchy restart once chained. (b) Inside Amarchy.
