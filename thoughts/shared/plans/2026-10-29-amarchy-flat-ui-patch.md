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

# Amarchy flat-UI look (own code, replaces VisualPrefs / FullPalette / Birdie)

Owner decision (2026-10-29): pens first; patches only for the measured residue; no caller-filtered graphics patches.

## Goal

Amarchy makes the system GUI look flat with its own means, driven by the current Amarchy theme colours, switchable from the theme menu, and "off" gives the stock Workbench look. No third-party patch (VisualPrefs + VP-QuickFix, FullPalette, Birdie 2000) and no code copied from them; the research note records mechanisms only.

Order of means, cheapest first:
1. P1: preferences only (palette.prefs DrawInfo pens, IControl GUI geometry). No patch.
2. P2: measure on the rig what still looks beveled under P1.
3. P3: patch only that measured residue, behind one inert flag. Expected: one sysiclass dispatcher replacement for the close / depth / size glyphs.

Dropped by the owner decision: SetFunction patches of RectFill / SetAPen / Move / Text / PrintIText / DrawImageState filtered by caller address, and the self-calibration of caller sites they needed. Residue that P1 and a class hook cannot fix becomes an owner decision (OD-F9), not a graphics patch.

Done (reachability protocol, gate 3) = one rig test (FLAT-X1) drives Amarchy's top-level theme entry point (ARexx `DESK THEME <name>`, the verb the theme menu uses), proves the new code ran (sentinel from am_sentinel for P1; per-call counter in the shared state block for P3), and asserts the pen numbers / pixel colours the theme defines for each element in scope, for two themes and for off.

Status: draft. Nothing below is built. Owner rule (2026-10-29): plans only, no flat UI code, probes or rig tests yet; every probe (FLAT-R1, FLAT-R3) and the P2 residue measurement (FLAT-M1, FLAT-M2) is not started and waits for owner go. The earlier probe IDs FLAT-R2 (now FLAT-M1) and FLAT-R4 (RTG, now a note under Stacking) are folded in.

## P1 mechanism: preferences only

### Flat DrawInfo pens (palette.prefs)

- palette.prefs carries the DrawInfo pen arrays (pen4 for 4-colour screens, pen8 for 8+), which `desk_palette` (amarchy/desk_core.h:43) already writes from `desk_pens` (desk_core.c:99, R1.19 map: SHINE=DK_FG, SHADOW=DK_DARKER, FILL=DK_ACCENT, BACKGROUND=DK_BG, BAR pens).
- Flat map `desk_pens_flat`: SHINEPEN = the surface it sits on (BACKGROUND pen, or FILL pen for selected / active surfaces; which one is OD-F4), SHADOWPEN = a thin line colour (theme border tone from `desk_darker`), so every 3D bevel reads as a 1-pixel outline on one side or both depending on how each class draws (measured in P2). Text, fill, bar pens keep the R1.19 roles.
- Live switch: the existing path. `desk_pens_reset` says the pen array changed, `desk_dos_apply` writes the prefs, IPrefs resets the Workbench screen (close / reopen, hooks desk_dos_wb_close / _open), 15 s confirm requester, revert from T:amarchy-revert. There is no instant repaint; windows of programs that keep their own screen are untouched until they reopen.

### Border and title bar size (IControl prefs)

- `struct IControlPrefs` in NDK 3.2R4 prefs/icontrol.h:52 has `UBYTE ic_GUIGeometry[4]; /* Titlebar and border geometry */`, valid when `ic_Flags` has ICB_VERSIONED (bit 31) and `ic_Version` >= IC_CURRENTVERSION (2) (V, header read 2026-10-29).
- The meaning of each of the 4 bytes is not in the header (U). Probe FLAT-R1 measures it by writing each byte and reading the window borders on the rig.
- Apply goes through IPrefs like palette.prefs (Workbench reset path; U whether IPrefs applies IControl live or needs reboot: R1 measures, and the result sets the D-TGT apply_kind).
- No IControl writer exists in the tree yet (searched amarchy/, prefs/, tools/, tests/ for icontrol / ICTL: none). New IFF chunk writer goes next to desk_palette in desk_core and reuses its IFF FORM / PRHD writing; the other IControl fields are read from the current ENV:Sys/icontrol.prefs and written back unchanged.

## P3 mechanism: residue patch (expected: sysiclass only)

### Where the patch code lives: separate resident `amarchy-flat` (recommended) vs inside Amarchy

| | Separate resident program `amarchy-flat` | In-process in the Amarchy commodity |
|---|---|---|
| Lifetime of patch code | Own process that never unloads while the class hook is still chained (Wait forever, inert) | Tied to Amarchy: Amarchy quits, restarts on RELOAD, is replaced by upkg / `rig.py install`. Hook code inside an unloaded seglist = crash in every window open |
| Cost | One more program, one startup block, a small state block | No IPC |

Recommendation: the separate resident program (OD-F8). Decisive trade-off: one extra program against a crash on every Amarchy restart once anything is chained on top of the hook.

### Class hook swap

- sysiclass is a RAM IClass struct whose `cl_Dispatcher.h_Entry` points into ROM; on 3.2.3 the measured classes share the trampoline 0xFE1118 (research 8.1a, V for frameiclass / propgclass / buttongclass, U for sysiclass: it needs SYSIA_DrawInfo + SYSIA_Which to instantiate).
- Get the IClass by NewObjectA(NULL, "sysiclass", SYSIA_DrawInfo from GetScreenDrawInfo, SYSIA_Which CLOSEIMAGE) + OCLASS, dispose the object. Save the whole Hook, swap the field the trampoline dispatches through (h_Entry or h_SubEntry: FLAT-R3 decides) under Forbid, CacheClearU (68060).
- Our dispatcher: `if (!state->enabled) goto old`; only IM_DRAW for SYSIA_Which CLOSEIMAGE, DEPTHIMAGE, SIZEIMAGE (and ZOOMIMAGE / ICONIFYIMAGE if P2 lists them) is drawn by us; every other method and every other SYSIA_Which goes to the saved hook. IM_HITTEST / IM_FRAMEBOX / OM_NEW untouched, so gadget sizes never change.
- Interface: public semaphore `Amarchy.Flat` with a versioned MEMF_PUBLIC block: `enabled` word, glyph pens, glyph style (OD-F4), call counter (sentinel), `still_chained`. Amarchy writes it, then refreshes window frames (RefreshWindowFrame over a LockIBase walk, reusing amarchy/tile.c:290 / :308).
- Uninstall: restore the Hook only if it still holds our entry; else stay resident and inert, set `still_chained`, exit only when restored.

### Failure safety

- Prefs key `amarchy-flat` gates the startup block (default OD-F1).
- Crash marker `ENVARC:Amarchy/flat-starting` written before the swap, deleted 60 s after; present at start = start inert, STATUS `flat=held`. Reuses desk_marker_make / desk_marker_parse (amarchy/desk_core.h).
- Boot gesture per OD-F7.
- `amarchy-flat` has `__stack` + `$STACK` cookie. The dispatcher runs on the caller's stack: no libc, no recursion, frame <= 64 bytes (host-checked, FLAT-C3).

### Stacking, RTG, 3.1

- Other patchers: detect VisualPrefs / Birdie by task name and by a sysiclass Hook that already points outside the intuition ROM span (the span test of device/upconsole.c:302 `patched_vector`, extracted to a shared helper, not copied). Policy OD-F5. MCP / PatchControl do not hook classes in the same way (U) and are left alone.
- RTG vs AGA: P1 is pen indices only, depth-agnostic. The glyph code draws with SetAPen + RectFill / Move / Draw into the RastPort it is handed: works on planar and P96 / CGX alike. Rig check on RTG only if an RTG rig exists, else RTG stays U.
- 3.1: palette.prefs pens identical. ic_GUIGeometry is a V2 IControl field (3.2): on 3.1 no border / title size control (U whether 3.1 IPrefs ignores or rejects it; R1 checks on `--os 31`). sysiclass on 3.1: R3 dumps the Hook on `--os 31`. Scope OD-F3.

## Reuse map

| Need | Reuse | Where |
|---|---|---|
| Theme roles, pen map, contrast guard | `desk_pens`, DK_* roles, DrawInfo slots, `desk_darker`, `desk_contrast` | amarchy/desk_core.h:11-40, desk_core.c:99 |
| palette.prefs writer (pen4 / pen8) | `desk_palette` | amarchy/desk_core.h:43 |
| Workbench reset, confirm, revert, boot restore | `desk_pens_reset`, `desk_dos_apply`, desk_dos_wb_close / _open, T:amarchy-revert, desk marker | amarchy/desk_core.h:40, :85-99, desk_dos.c:15-231 |
| Theme load and switch entry | `gt_dos_load_current`, `gt_dos_select`, ARexx `DESK THEME` | amarchy/gthemes_dos.h:14-23, amarchy/amarchy.c |
| Target table and apply kind | D-TGT `desk_target`, apply_kind table | amarchy/desk_core.h, P1b plan Interface |
| ROM span / patched test | `patched_vector` | device/upconsole.c:302 |
| Hook save / restore skeleton | patchcon | tests/amiga/patchcon.c:27-31 |
| Host draw tests | gfx_fake | tests/render_host/gfx_fake.[ch] |
| Window walk | tiling walk | amarchy/tile.c:290, :308 |
| Sentinels | `am_sentinel`, `am_sentinel_str` | amarchy/sentinel_dos.c:5, :17 |
| Startup block | P1B-C1 `desk_startup_block_set/_remove`; Installer blocks | P1b plan; dist/Install.installer:86, dist/Uninstall:101 |
| Prefs rows | amconf AM_* keys, PG_PAGE_AMARCHY | amarchy/amconf_core.h:12-26, prefs/settings_def.c |
| Rig test skeleton | RX1 desk rig | tools/rig/amarchy_desk_rig.py |

Searched, not found (new code justified): no IControl prefs writer; no BOOPSI class hook swap in our code (`git grep cl_Dispatcher` outside thoughts/ finds only reference/kingcon/KingCON-handler.asm:17566, third-party asm, not reusable).

## Checklist

Rigs run as 060: `UPTERM_RIG=<n> rig.py start --max --os 32 --purpose "Amarchy flat: <item>"`; stop the rig at the end of the item. A1200 last, once per phase.

### P1 flat pens and geometry (no patch)

- [ ] **FLAT-R1 IControl geometry probe.** Status: not started, waits for owner go. Rig `--os 32`: write ic_GUIGeometry bytes one at a time (ICB_VERSIONED set, ic_Version 2), apply through IPrefs, read BorderTop / BorderLeft / BorderRight / BorderBottom and title bar height of a new window. Output: meaning, range and default of each byte; whether IPrefs applies it live (Workbench reset) or needs reboot. `--os 31`: does IPrefs accept the versioned struct. Writes a section into the research note.
- [ ] **FLAT-P1-1 flat pen map.** files: amarchy/desk_core.[ch] `desk_pens_flat(theme, pens[DK_SLOTS])` next to `desk_pens`; `desk_palette` takes the map as an argument. host test: golden palette.prefs (pen4 and pen8) for flat on / off in tests/data/desk/; every slot of the flat map moves the PALT pen bytes; desk_contrast still refuses an unreadable theme. rig `--os 32`: dri_Pens of a new window equal the flat map. 3.1: same on `--os 31`. revert: flat off -> palette.prefs bytes equal the P1a golden. A1200: owner looks at one window.
- [ ] **FLAT-P1-2 IControl writer.** files: desk_core `desk_icontrol(cur, geometry[4], out, cap)` (pure: reads current icontrol.prefs bytes, sets ICB_VERSIONED, ic_Version, ic_GUIGeometry, keeps every other field), desk_dos writes ENV: / ENVARC:. host test: round trip of a captured 3.2 icontrol.prefs, only the 4 geometry bytes and version fields change; a pre-V2 file is upgraded without losing fields. rig: border / title size of a new window equal the values chosen (OD-F4). 3.1: per R1 result (skip or no-op). revert: off -> icontrol.prefs md5 equal to the snapshot.
- [ ] **FLAT-P1-3 flat as a desk target.** Palette-row option `amarchy-flat-pens` and an IControl target row in D-TGT with the apply_kind R1 measured; both go through desk_dos_apply (snapshot, Workbench reset, confirm, revert). host test: desk_plan with the key on / off selects the maps and lists icontrol.prefs in the snapshot. rig: covered by X1. revert: confirm timeout restores both prefs files byte-equal.

### P2 measure the residue

- [ ] **FLAT-M1 residue screenshots.** Status: not started, waits for owner go. Rig `--os 32` with P1 applied (theme A, then theme B): screenshot (a) active and inactive window frames, (b) close / depth / size / zoom / iconify glyphs, (c) a pull-down menu with checkmark, submenu arrow and separator, (d) the screen title bar, (e) a GadTools window (tests/amiga/ test window: button, string, cycle, checkbox, listview, bevel box) and a 3.2 Prefs editor, (f) a Workbench drawer window's scrollers. A haiku subagent reads each picture with one question per element: "is there a two-tone bevel or a light highlight edge left?". Copy screenshots to ~/Desktop/upterm-shots, delete inbox files after the read.
- [ ] **FLAT-M2 residue list.** Status: not started, waits for owner go. Write the list into this plan (a table: element, what still looks beveled, which class or code draws it from research 8, fix route: none needed / sysiclass hook / owner decision). Elements drawn by a class we can swap (sysiclass, and frameiclass / propgclass only if listed) go to P3; anything drawn by Intuition or GadTools code directly goes to OD-F9.

### P3 patch the measured residue (expected: sysiclass glyphs)

- [ ] **FLAT-R3 sysiclass Hook layout.** Status: not started, waits for owner go. Rig `--os 32` and `--os 31`: dump `h_Entry`, `h_SubEntry`, `h_Data` of sysiclass (instance created with SYSIA_DrawInfo + SYSIA_Which CLOSEIMAGE), disassemble 32 bytes at the trampoline. Decides which field C1 swaps.
- [ ] **FLAT-C0 program skeleton and state.** files: amarchy/flat.c (OS), amarchy/flat_core.[ch] (pure, state block struct shared with Amarchy), Makefile rule `build/amiga/amarchy-flat`; `__stack` + `$STACK` cookie; `Amarchy.Flat` semaphore. host test: state struct offsets pinned, version / size mismatch refused. rig: starts, STATUS shows `flat=on|off|absent|held`, Ctrl-C exits when nothing is chained.
- [ ] **FLAT-C1 sysiclass hook swap and remove.** `flat_class_hook` (save, swap, restore-if-ours, still_chained), pure model over a fake IClass / Hook. host test: install / clean remove; foreign hook on top -> still_chained + inert, our saved hook never written over theirs; double install refused. rig: swap with a pass-through dispatcher, 1000 window open / close cycles, glyph pixels unchanged; remove restores the Hook byte-equal.
- [ ] **FLAT-C2 flat glyphs.** files: amarchy/flat_glyph.[ch] (pure geometry: rect / line list per SYSIA_Which and box size; states normal / selected / inactive; style per OD-F4). Only the SYSIA_Which values M2 lists; others to the old hook. host test: gfx_fake renders each glyph at 3 box sizes into pixel goldens; unknown SYSIA_Which and non-IM_DRAW methods reach the old hook (call count). rig `--os 32`: glyph pixels equal the golden after open and after RefreshWindowFrame, for theme A and B; flag off + RefreshWindowFrame -> stock pixels. 3.1: per R3 and OD-F3. A1200: owner looks.
- [ ] **FLAT-C3 caller-context budget.** host test: the dispatcher object's stack frame <= 64 bytes, no libc calls (objdump check). rig: 20 windows opened from a `Stack 4096` shell, no crash.
- [ ] **FLAT-C4 kill switches and install.** Crash marker (desk marker reuse), boot gesture (OD-F7), prefs key, startup block `;BEGIN Amarchy-Flat` via P1B-C1, kit row, Uninstall row, VisualPrefs / Birdie detection (OD-F5). host test: marker -> inert decision with injected clock; startup block text; detection table from fake hooks. rig: kill the rig after the marker write (5 s quiet first, never mid disk write), reboot -> `flat=held`; install + Uninstall -> User-Startup md5 equals pre-install. A1200: install, boot gesture, uninstall once.
- [ ] **FLAT-C5 prefs rows.** PG_PAGE_AMARCHY: Flat system look (on / off), Flat window gadgets (on / off), border and title size, glyph style, status line (on / held / absent / VisualPrefs present). Full English labels. host test: each row writes its key. rig: covered by X1.
- [ ] **FLAT-C6 rounded window corners (only if OD-F6 picks painted corners).** After the frame draw, paint 2-3 pixels per corner in the screen background pen. Hook point: U; the sysiclass hook does not draw the frame, so this needs a frame-draw hook the owner decision excluded unless M2 finds a class-level route. If none: the item closes and OD-F6 records it. host test: corner masks at radius 2 and 3 on gfx_fake. rig: corner pixels equal the backdrop pen over the plain backdrop.

### Reachability (one rig test)

- [ ] **FLAT-X1 `test_flat_theme_reaches_system_gui`** in tools/rig/amarchy_desk_rig.py (extends RX1). Rig `--os 32 --max`, purpose "Amarchy flat UI: theme drives the system look". Steps:
  1. Flat off: `DESK THEME <A>`; dri_Pens equal the P1a map; glyph counter does not move; frame / glyph pixels equal the stock capture.
  2. Flat on: `DESK THEME <A>` (Workbench reset, confirm); P1 sentinel set; dri_Pens equal the flat map; border / title size equal the chosen geometry; glyph counter > 0; glyph pixels equal theme A.
  3. `DESK THEME <B>`: pens, geometry and glyph pixels move to theme B; each look option toggled once moves its output.
  4. Flat window gadgets off live: glyphs stock after RefreshWindowFrame, pens stay flat.
  5. Flat off: stock pens, stock geometry, stock glyphs after the Workbench reset.
  6. Uninstall: User-Startup md5 equals pre-install; after reboot no `Amarchy.Flat` semaphore.
  3.1 run of steps 1-5 if OD-F3 includes 3.1.

## Host tests (summary)

`make test ONLY=desk` gains P1-1 / P1-2 / P1-3; new suite `make test ONLY=flat`: C0 struct, C1 hook model, C2 glyph goldens on gfx_fake, C3 budget, C4 marker and detection.

## Risks

- P1 alone changes every 3D bevel on the screen at once (shine and shadow); some apps rely on the bevel to show edges. M1 shows how bad that is before any patch.
- ic_GUIGeometry byte meaning is unmeasured; R1 may find it limited (for example title bar only).
- A sysiclass hook bug crashes every window open: kill switches, live inert flag, small code (C3).
- Residue drawn directly by Intuition or GadTools (no class, no prefs) stays beveled unless the owner chooses otherwise (OD-F9).

## Order

1. FLAT-R1, then P1 (P1-1, P1-2, P1-3) lands on its own.
2. FLAT-M1, M2: residue list into this plan.
3. Owner answers OD-F9 for residue P3 cannot fix.
4. R3, C0, C1, C2, C3, C4, C5 (C6 only if chosen and routable).
5. FLAT-X1 last; A1200 check once per phase.

## Owner decisions

DECIDED 2026-10-29: pens first; patches only for the measured residue; no caller-filtered graphics patches. Open ones below, recommended option first.

- **OD-F1 Default after install.** (a) Off; Amarchy prefs turns it on. Trade-off: no change for users who never asked, flat look not seen out of the box. (b) Flat pens on, glyph patch off. Trade-off: flat colours by default with zero patch risk, but every bevel on the screen changes at first install. (c) All on.
- **OD-F3 AmigaOS 3.1.** (a) Pens on 3.1 and 3.2; geometry and glyph patch on 3.2 only. Trade-off: one OS to verify for the patch; 3.1 keeps stock border size and glyphs. (b) Everything on both. Trade-off: every rig check twice, R1 / R3 may show 3.1 needs other paths. (c) 3.2 only.
- **OD-F4 Look options.** Shine pen: (a) background pen, (b) fill pen. Trade-off: (a) flattest, (b) keeps a faint edge on selected surfaces. Border and title size: (a) one Amarchy default from R1, (b) a size option in prefs. Trade-off: (b) is one more row and more rig states. Glyph style: (a) thin line glyphs in the text pen, (b) filled blocks.
- **OD-F5 VisualPrefs or Birdie present.** (a) amarchy-flat refuses to start and the prefs page names the patch to remove. Trade-off: no stacking unknowns, the user must choose. (b) Start and hook on top. Trade-off: two patchers drawing the same glyphs, untested order. (c) Pens only, no glyph patch.
- **OD-F6 Rounded window corners.** Facts: NDK 3.2R4 has no shaped-layer or window-decoration API (clib/layers_protos.h 47.1: layers are rectangles of ClipRects; only InstallLayerHook / InstallLayerInfoHook backfill hooks, no LAYA_Shape / ChangeLayerShape in the include tree). Only painted corners are possible. (c) No rounding. Trade-off: square corners, no artefacts; Amarchy tiles windows edge to edge, where painted corners show a notch against the neighbour most of the time. (a) Painted corners: 2-3 pixels per corner in the background pen. Trade-off: right over the plain desktop, a visible notch over other windows and over a backdrop pattern; needs a frame-draw hook point (C6), which the no-caller-filter decision may leave without a route.
- **OD-F7 Boot kill-switch gesture.** (a) Both mouse buttons held at boot. Trade-off: rarely pressed by accident. (b) Left button. Trade-off: collides with other boot-time tools. (c) None, crash marker only.
- **OD-F8 Process model for the patch.** (a) Separate resident `amarchy-flat`. Trade-off: one more program and an interface, against a crash on every Amarchy restart once chained. (b) Inside Amarchy.
- **OD-F9 Residue that pens and the sysiclass hook cannot fix** (filled from M2). Per element: (a) accept it beveled. Trade-off: no new patch surface. (b) A targeted patch for that element only (class hook or library LVO such as gadtools DrawBevelBoxA, never a caller-filtered graphics patch). Trade-off: more look, more crash surface per element.
