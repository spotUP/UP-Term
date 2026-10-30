---
date: 2026-10-29
topic: AMARCHY-FLAT
status: draft
author: spotUP
ledger: thoughts/spot/todos.md row AMARCHY-DESK-VISUALPREFS (3319); supersedes R1.25 of the P1b plan
depends_on: 2026-10-29-amarchy-p1a-native-theme.md (desk_core / desk_dos, pens R1.19, revert R1.16a)
research: thoughts/shared/research/2026-10-29-amarchy-flat-ui-patch.md (sections 1-8, rig measurements on Kickstart 3.2.3), thoughts/shared/research/2026-10-29-amarchy-visualprefs-birdie.md; DOpus 5 source read at /Users/spot/Code/Reference/dopus5 (git clone of BlitterStudio/dopus5, commit 96ede09, APL 1.1)
reviewed: Fable review 2026-10-29 (section at the end); facts marked V are read in headers / source, U are unmeasured
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

Status: draft. Nothing below is built. Owner rule (2026-10-29): plans only, no flat UI code, probes or rig tests yet; every probe (FLAT-R1, FLAT-R3) and the P2 residue measurement (FLAT-M1, FLAT-M2) is not started and waits for owner go. The earlier probe IDs FLAT-R2 (now FLAT-M1) and FLAT-R4 (RTG, now a note under Stacking) are folded in. 2026-10-30: FLAT-R3 done; FLAT-M1 and FLAT-M2 open for DOpus, the whole set on 3.1, scroller arrow pixels, a real drawer window.

## P1 mechanism: preferences only

### Flat DrawInfo pens (palette.prefs)

- palette.prefs carries the DrawInfo pen arrays (pen4 for 4-colour screens, pen8 for 8+), which `desk_palette` (amarchy/desk_core.h:43) already writes from `desk_pens` (desk_core.c:99, R1.19 map: SHINE=DK_FG, SHADOW=DK_DARKER, FILL=DK_ACCENT, BACKGROUND=DK_BG, BAR pens).
- Flat map `desk_pens_flat`: SHINEPEN = the surface it sits on (BACKGROUND pen, or FILL pen for selected / active surfaces; which one is OD-F4), SHADOWPEN = a thin line colour (theme border tone from `desk_darker`), so every 3D bevel reads as a 1-pixel outline on one side or both depending on how each class draws (measured in P2). Text, fill, bar pens keep the R1.19 roles.
- Live switch, two tiers (Fable review). (1) Theme-to-theme while flat stays on or stays off: only the colour VALUES of the pens change, the pen INDICES do not; colour values load live without a screen reset (desk_core.h:37 comment, V; IPrefs-RelNotes 44.19 names only the pen-array change as the reset case). `desk_pens_reset` already compares the live dri_Pens with the wanted array and reports no reset when the indices are equal (desk_core.c:116), so FLAT-P1-1 must make the flat and stock maps differ only where they have to, and keep the same indices for every flat theme. (2) Flat on <-> off changes the pen indices, and that needs the Workbench close / reopen: GetScreenDrawInfo autodoc BUGS "does not reflect changes in screen modes, depth, or pens" (V, NDK 3.2R4), dri_Pens is fixed at screen open. Existing path: `desk_dos_apply` writes the prefs, IPrefs resets the Workbench screen (hooks desk_dos_wb_close / _open), 15 s confirm requester, revert from T:amarchy-revert. Programs on their own screen are untouched until they reopen (DOpus on its own screen copies the Workbench pen array at screen open, see DOpus section).
- Menu glyphs, checkmark and Amiga key: drawn by `sysiclass` images (SYSIA_Which MENUCHECK 0x10, AMIGAKEY 0x11, MENUSUB new in 3.2, imageclass.h and intuition-RelNotes 47.17, V) in the pens above; they are in scope of the same P3 hook, not a separate route.
- GadTools: on 3.2 DrawBevelBoxA and every GadTools bevel goes through frameiclass FRAME_CONTEXT (gadtools-RelNotes 47.1 and 47.13, V), so P1 pens cover GadTools windows without any patch; on 3.1 GadTools draws its bevels itself from the same dri_Pens (U which code, same pens either way).

### Border and title bar size (IControl prefs)

- `struct IControlPrefs` in NDK 3.2R4 prefs/icontrol.h:52 has `UBYTE ic_GUIGeometry[4]; /* Titlebar and border geometry */`, valid when `ic_Flags` has ICB_VERSIONED (bit 31) and `ic_Version` >= IC_CURRENTVERSION (2) (V, header read 2026-10-29).
- The meaning of each of the 4 bytes is not in the header (U). Probe FLAT-R1 measures it by writing each byte and reading the window borders on the rig.
- Apply goes through IPrefs like palette.prefs (Workbench reset path; U whether IPrefs applies IControl live or needs reboot: R1 measures, and the result sets the D-TGT apply_kind).
- What the 3.2 release notes say the bytes carry (IControl-RelNotes 47.11, 47.17, 47.18, V as text, U as byte layout): window border aspect ratio correction (needs intuition 47.28+; also ic_Flags ICB_RATIO_LSB / MSB = ICF_RATIO_9_7, 9_8, 1_1, 8_9, icontrol.h), automatic border gadget scaling as a percentage of the title bar height or 1:1, and a title bar height increment 0..16 for screen and window title bars. So R1 can only make bars thicker, never thinner than stock, and it cannot remove the border. Flat thin borders stay a pen effect, not a geometry one.
- Exception list: IControl 47.17 writes `ID_IEXC` chunks (`struct IExceptionPrefs { struct TagItem ie_Tags[0]; }`, program / screen name with wildcards) that IPrefs reads to exempt programs from the new geometry (V text, U tag values: not in Include_H). Our writer must copy these chunks through unchanged (FLAT-P1-2 host test adds a file with an IEXC chunk).
- Sanctioned hook (3.2): intuition-RelNotes 47.17 adds an external hook installed through IntuitionControlA() for rearranging border arrow gadgets and scrollbars; IPrefs installs the default at boot and the note says it is meant for "third-party GUI enhancers". R1 also dumps what this hook receives (U). If it hands us the frame draw it is the clean route for OD-F6 corners and replaces FLAT-C6's missing hook point.
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
- Why a Hook swap and not a class replacement: AddClass autodoc (NDK 3.2R4) BUGS "no class replacement policy, no checking of like-named classes" (V), so MakeClass + AddClass of a second "sysiclass" is undefined and Intuition's internal callers (window gadgets are created from ROM 0xFE5E1E with sysiclass images made internally, research 8) would keep the ROM class anyway. The swap of the one Hook field is the only route that reaches every caller.
- Get the IClass by NewObjectA(NULL, "sysiclass", SYSIA_DrawInfo from GetScreenDrawInfo, SYSIA_Which CLOSEIMAGE) + OCLASS, dispose the object. Save the whole Hook, swap the field the trampoline dispatches through (h_Entry or h_SubEntry: FLAT-R3 decides) under Forbid, CacheClearU (68060).
- Our dispatcher: `if (!state->enabled) goto old`; only IM_DRAW (and IM_DRAWFRAME, same glyph in a frame box) for the SYSIA_Which values M2 lists is drawn by us: expected CLOSEIMAGE, DEPTHIMAGE, SIZEIMAGE, ZOOMIMAGE, ICONIFYIMAGE (3.2), MENUCHECK, AMIGAKEY, MENUSUB (3.2), and the arrow images UPIMAGE / DOWNIMAGE / LEFTIMAGE / RIGHTIMAGE if the Workbench scrollers show residue. Every other method and every other SYSIA_Which goes to the saved hook. IM_HITTEST / IM_FRAMEBOX / OM_NEW untouched, so gadget sizes never change. The per-instance SYSIA_Which is read with OM_GET on the object, never from the instance layout (U, ROM-private).
- Dispatcher register contract: a BOOPSI dispatcher is a Hook entry (a0 hook, a2 object, a1 message); the saved-hook call passes the same three through unchanged. Owner knob reads come from the state block only; no library base is opened inside the dispatcher.
- Interface: public semaphore `Amarchy.Flat` with a versioned MEMF_PUBLIC block: `enabled` word, glyph pens, glyph style (OD-F4), call counter (sentinel), `still_chained`. Amarchy writes it, then refreshes window frames (RefreshWindowFrame over a LockIBase walk, reusing amarchy/tile.c:290 / :308).
- Uninstall: restore the Hook only if it still holds our entry; else stay resident and inert, set `still_chained`, exit only when restored.

### Failure safety

- Prefs key `amarchy-flat` gates the startup block (default OD-F1).
- Crash marker `ENVARC:Amarchy/flat-starting` written before the swap, deleted 60 s after; present at start = start inert, STATUS `flat=held`. Reuses desk_marker_make / desk_marker_parse (amarchy/desk_core.h).
- Boot gesture per OD-F7.
- `amarchy-flat` has `__stack` + `$STACK` cookie. The dispatcher runs on the caller's stack: no libc, no recursion, frame <= 64 bytes (host-checked, FLAT-C3).

### Stacking, RTG, 3.1

- Other patchers: detect VisualPrefs / Birdie by task name and by a sysiclass Hook that already points outside the intuition ROM span (the span test of device/upconsole.c:304 `patched_vector`, used at :384, extracted to a shared helper, not copied). Policy OD-F5. MCP / PatchControl do not hook classes in the same way (U) and are left alone. Birdie never touches classes (8 SetFunctions on graphics / intuition LVOs, research note), so Birdie + our class hook do not collide at the hook level; they collide visually only, which is OD-F5's point.
- Stale-pointer safety when another patcher removes itself after us: our saved hook may then point into their unloaded code. Mitigation copied as a mechanism, not code, from VisualPrefs' remove routine (research note): at every dispatcher call compare the saved entry with the current class Hook of the *other* field; if the class Hook no longer points to us and our saved entry is outside ROM and outside any loaded seglist we know, call the ROM trampoline 0xFE1118 fallback recorded at install (R3 confirms the trampoline is stable per ROM version; else `still_chained` stays the only policy, U).
- 4-colour Workbench (pen4 array): the flat map needs background, text, outline and fill = 4 pens, so FILLTEXT and BAR pens must alias; FLAT-P1-1's pen4 golden encodes the aliasing and `desk_contrast` keeps the readable-text guard. Mark in FLAT-M1: one screenshot set on a 4-colour screen mode.
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
| ROM span / patched test | `patched_vector` | device/upconsole.c:304 (used at :384) |
| DOpus pens and layout | ARexx `dopus set pens <key> ...`, theme module `pen_settings[]`, button bank BORDERLESS flag | /Users/spot/Code/Reference/dopus5 source/Program/rexx_data.c:133, rexx_dopus.c:616 and :797, Modules/themes/themes_data.c:42, themes.c:810, Program/buttons_draw.c:44 |
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

- [x] **FLAT-R1 IControl geometry probe.** Status: not started, waits for owner go. Rig `--os 32`: write ic_GUIGeometry bytes one at a time (ICB_VERSIONED set, ic_Version 2, ic_VersionMagic 0), apply through IPrefs, read BorderTop / BorderLeft / BorderRight / BorderBottom and title bar height of a new window. Output: meaning, range and default of each byte; whether IPrefs applies it live (Workbench reset) or needs reboot; whether any value makes a border thinner than stock (release notes say no, V text). Also dump the IntuitionControlA() gadget-layout hook IPrefs installs (what it is called with; does it see the frame draw). `--os 31`: does IPrefs accept the versioned struct. Writes a section into the research note. Done e77add71: 3.2 bytes measured, 3.1 ignores geometry (research note section 9).
- [x] **FLAT-P1-1 flat pen map.** files: amarchy/desk_core.[ch] `desk_pens_flat(theme, pens[DK_SLOTS])` next to `desk_pens`; `desk_palette` takes the map as an argument. host test: golden palette.prefs (pen4 and pen8) for flat on / off in tests/data/desk/; every slot of the flat map moves the PALT pen bytes; desk_contrast still refuses an unreadable theme. rig `--os 32`: dri_Pens of a new window equal the flat map. 3.1: same on `--os 31`. revert: flat off -> palette.prefs bytes equal the P1a golden. A1200: owner looks at one window. Done cc062070: host test 543 checks 0 failed; 4 failed with the flat map stubbed. Rig pen check not run (unverified).
- [ ] **FLAT-P1-2 IControl writer.** files: desk_core `desk_icontrol(cur, geometry[4], out, cap)` (pure: reads current icontrol.prefs bytes, sets ICB_VERSIONED, ic_Version, ic_GUIGeometry, keeps every other field), desk_dos writes ENV: / ENVARC:. host test: round trip of a captured 3.2 icontrol.prefs, only the 4 geometry bytes and version fields change; a pre-V2 file is upgraded without losing fields. rig: border / title size of a new window equal the values chosen (OD-F4). 3.1: per R1 result (skip or no-op). revert: off -> icontrol.prefs md5 equal to the snapshot.
- [ ] **FLAT-P1-3 flat as a desk target.** Palette-row option `amarchy-flat-pens` and an IControl target row in D-TGT with the apply_kind R1 measured; both go through desk_dos_apply (snapshot, Workbench reset, confirm, revert). host test: desk_plan with the key on / off selects the maps and lists icontrol.prefs in the snapshot. rig: covered by X1. revert: confirm timeout restores both prefs files byte-equal.

### P2 measure the residue

- [ ] **FLAT-M1 residue screenshots.** Status: PARTLY done 2026-10-30 (research 11, 12, 13): measured on 3.2 and 3.1 (own screen, SA_Pens): window frame, glyphs, GadTools gadgets, screen bar, menus, drawer-style scroller incl. arrows, 4- and 8-colour own screen; 3.2 also Prefs editor, theme B, sysiclass Which 0/0x1B/0x2A. OPEN: DOpus lister (no DOpus5: on the rig, not installed per rule: not measurable on rig), real Workbench drawer window under flat pens (3.1: OpenWorkbenchObjectA missing, Palette route hangs; 3.2: Palette USE did not apply on the Workbench screen in the 2026-10-30 run), flat Prefs editor and theme B on 3.1, menu '?' rows on 3.1. Original text: a real drawer window. Rig `--os 32` with P1 applied (theme A, then theme B): screenshot (a) active and inactive window frames, (b) close / depth / size / zoom / iconify glyphs, (c) a pull-down menu with checkmark, submenu arrow and separator, (d) the screen title bar, (e) a GadTools window (tests/amiga/ test window: button, string, cycle, checkbox, listview, bevel box) and a 3.2 Prefs editor, (f) a Workbench drawer window's scrollers, (g) DOpus Magellan II on the Workbench screen and on its own screen: a lister (title, toolbar, scrollers, status), a button bank with and without the BORDERLESS flag, the lister pop-up menu, a string field in a DOpus requester (route per DOpus section), (h) one 4-colour screen mode set of (a)-(c). A haiku subagent reads each picture with one question per element: "is there a two-tone bevel or a light highlight edge left?". Copy screenshots to ~/Desktop/upterm-shots, delete inbox files after the read.
- [ ] **FLAT-M2 residue list.** Status: PARTLY done 2026-10-30 (commit 3914c489), table below covers the elements measured so far (3.1 column added, research 13); open: DOpus own pen set, dual lister, button bank identity, real drawer window, flat Prefs editor on 3.1. Write the list into this plan (a table: element, what still looks beveled, which class or code draws it from research 8, fix route: none needed / sysiclass hook / owner decision). Elements drawn by a class we can swap (sysiclass, and frameiclass / propgclass only if listed) go to P3; anything drawn by Intuition or GadTools code directly goes to OD-F9.

  | Element | What still looks beveled (flat pens, 3.2; 3.1 identical unless noted) | Drawn by | Fix route |
  |---|---|---|---|
  | Window frame, title bar | nothing: outer edge vanishes, one dark line stays | Intuition ROM RectFill | none needed |
  | Close, depth, zoom, size glyphs | old shapes remain as outlines | sysiclass | sysiclass hook (P3) |
  | GadTools button, string, listview, bevel box | one-sided L shadow (right 2 px, bottom), no full outline | frameiclass / GadTools bevel code | OD-F9 owner decision (frameiclass hook or accept) |
  | Screen title bar | nothing beyond a 1 px line | Intuition | none needed |
  | Menus, checkmark, arrow, separator | nothing (single-pen glyphs, 2 px frame) | Intuition + sysiclass images | none needed |
  | Drawer scroller prop knob | nothing (1 px outline, dither fill) | propgclass / Intuition | none needed |
  | Prefs editor sliders, checkboxes, cycle, listview | one-sided L shadow, as GadTools | GadTools bevel code | OD-F9 (b) patch |
  | 4- and 8-colour screen, theme B | same as 3.2 depth 8 (pen indices decide) | n/a | none needed |
  | Scroller arrows (up/down/left/right) | nothing on 3.2 and 3.1: pen-2 shine and frame lines vanish, glyph and one divider remain | sysiclass images | none needed |
  | 3.1 whole set | same residue as 3.2 (table above); sysiclass present on 3.1 (class at 1000c758) | n/a | same routes |
  | DOpus lister (5.104, 3.2, research 14) | shine vanishes, shadow stays: one-sided L on path field, toolbar buttons, scroller knob and arrow boxes, column header boxes; frame and file list fine; sort arrow loses half | DOpus `L_DrawBox` / `draw_field_box` (Library/gui.c:27, :60), image_class IM_BBOX / arrows (Library/image_class.c:261, :109), lister_title.c:972-987 | pens only for frame, file list, title-bar gadgets; FLAT-D4 patch for path field outline, scroller, sort arrow; toolbar L acceptable or DrawBox patch; button bank unverified |


### P3 patch the measured residue (expected: sysiclass glyphs)

- [x] **FLAT-R3 sysiclass Hook layout.** Status: done 2026-10-30 (commit 3914c489, research 10; Which sweep in research 12). Rig `--os 32` and `--os 31`: dump `h_Entry`, `h_SubEntry`, `h_Data` of sysiclass (instance created with SYSIA_DrawInfo + SYSIA_Which CLOSEIMAGE), disassemble 32 bytes at the trampoline. Decides which field C1 swaps.
- [ ] **FLAT-C0 program skeleton and state.** files: amarchy/flat.c (OS), amarchy/flat_core.[ch] (pure, state block struct shared with Amarchy), Makefile rule `build/amiga/amarchy-flat`; `__stack` + `$STACK` cookie; `Amarchy.Flat` semaphore. host test: state struct offsets pinned, version / size mismatch refused. rig: starts, STATUS shows `flat=on|off|absent|held`, Ctrl-C exits when nothing is chained. Status 2026-10-30: host part and Amiga build done (amarchy/flat.c, flat_core.[ch], tests/test_flat_core.c `make test ONLY=flat_core`, `make amarchy-flat`, stack cookie checked in dist-check); STATUS text is `flat=<word> calls=N chained=0|1` (`flat=absent bad=version|size` for a foreign block); rig step open; kit / packaging row is FLAT-C4.
- [x] **FLAT-C1 sysiclass hook swap and remove.** `flat_class_hook` (save, swap, restore-if-ours, still_chained), pure model over a fake IClass / Hook. host test: install / clean remove; foreign hook on top -> still_chained + inert, our saved hook never written over theirs; double install refused. rig: swap with a pass-through dispatcher, 1000 window open / close cycles, glyph pixels unchanged; remove restores the Hook byte-equal. Status: host part and Amiga build done 2026-10-30 (flat_core swap model + 24-byte register/stack-preserving tail-jump trampoline, flat.c wiring, 83 checks); rig step done 2026-10-30 (tools/rig/amarchy_flat_rig.py, rig 5): OS 3.2 and 3.1 both PASS: swap installed, calls counter 0 -> 28030 (3.2) / 16018 (3.1) over 1000 flatwin open/close cycles, frame pixel hash identical off / on / after remove, Break restores h_SubEntry byte-equal, no Guru, no hang.
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

## DOpus Magellan II (added 2026-10-29)

Source read: /Users/spot/Code/Reference/dopus5 (BlitterStudio/dopus5, commit 96ede09). Grep and range reads only; nothing run. Rule from the ledger: any DOpus-side change goes upstream as a PR to BlitterStudio/dopus5, never a private fork; our parts are modules, ARexx and config.

How DOpus draws (V, source):
- Every bevel is DOpus's own code from the screen DrawInfo, not frameiclass: `L_DrawBox` (source/Library/gui.c:27, SetAPen dri_Pens[SHINEPEN] / [SHADOWPEN], Move / Draw) and `L_DrawFieldBox` (gui.c:60, double bevel for string fields), exported by dopus5.library as DrawBox / DrawFieldBox; `buttons_draw_border` (source/Program/buttons_draw.c:29) for button banks, skipped when the bank window has `BTNWF_BORDERLESS` (:44); lister title / toolbar / dual-lister frames read SHINEPEN / SHADOWPEN directly (Program/lister_title.c:972-987, lister_dual.c:1422-1428, lister_toolbar.c:244-249). Its own BOOPSI classes (button, listview, image, palette) come from `MakeClass(name, super, 0, size, 0)` + AddClass inside dopus5.library; they are not sysiclass and our P3 hook never sees them.
- Its own screen takes the Workbench pens: display.c:170-174 copies `workbench_draw_info->dri_Pens` into `screen_pens` and opens with `SA_LikeWorkbench` (:200) and `SA_Pens, screen_pens` (:243); `GUI->draw_info = GetScreenDrawInfo(GUI->screen_pointer)` (:508) is read again at every screen open. So P1 flat pens flatten DOpus on the Workbench screen at once and on its own screen after its screen reopens (`dopus set pens` / palette only change DOpus's own lists and screen palette, V).
- Glyph images: source/Library/image_class.c:271-355 draws DOpus's own arrow / check images in SHINEPEN and SHADOWPEN. With SHINE = background they lose one edge; with both pens on the surface they vanish. This is the DOpus residue.
- Hardcoded pens: 5 `SetAPen(rp, 1|2)` sites in Program + Library (grep count, U which elements). One `DrawBevelBoxA` call site (U path; covered by frameiclass on 3.2 either way).
- Configurable without code: `dopus set pens <icons|files|dirs|selfiles|seldirs|devices|assigns|source|dest|gauge|user> ...` (Program/rexx_data.c:133, rexx_dopus.c:616 and :797) is what the stock theme module saves (Modules/themes/themes.c:810, keys in themes_data.c:42); button banks have the BORDERLESS window flag per bank.

What the system plan covers for DOpus: all bevels (listers, toolbars, button bank borders, requesters, dual-lister frame) via P1 pens; its window frames and close / depth / size glyphs via P1 + P3 like every other window; nothing more is needed on our side than the pen map. Coverage V by source, pixel result U until FLAT-M1 (g).

What needs a DOpus-side change (upstream PR, item FLAT-D1):
- `L_DrawBox` / `L_DrawFieldBox` / `buttons_draw_border`: when `dri_Pens[SHINEPEN] == dri_Pens[BACKGROUNDPEN]` (the flat signal, no new config) draw one outline in SHADOWPEN instead of a two-tone bevel, and the string field box as one line. Zero behaviour change for stock pens.
- image_class.c: draw glyphs in TEXTPEN when SHINE equals the surface pen. Same signal.
- Optional, owner's call: an `environment` flag `flat` written by the theme module, so a DOpus theme can switch it without the pen heuristic.

Our side (modules, ARexx, config), items FLAT-D2 / D3:
- [x] **FLAT-D1 upstream PR to BlitterStudio/dopus5. DROPPED by the owner 2026-10-30 (runtime patch FLAT-D4 instead).** A DOpus setting (default off) turns on flat drawing (owner 2026-10-30: flat must be optional inside DOpus); flat drawing in gui.c, buttons_draw.c, image_class.c as above; test on the rig with the stock DOpus theme (no pixel change) and with P1 flat pens. Status: not started, waits for owner go (OD-F10).
- [ ] **FLAT-D2 Amarchy DOpus theme hook.** `desk_dos_apply` sends `dopus set pens <key> ...` for the 11 keys from the theme colours when a DOpus ARexx port exists (ports named by `GUI->rexx_port_name`, DOPUS.1 by default, V display.c:1167) and `dopus set pens` is already known to work on Workbench (owner note). Pure mapping theme -> 11 pen strings in desk_core, host golden; rig: DOpus lister colours equal the theme after `DESK THEME <A>`.
- [ ] **FLAT-D3 button banks borderless.** Document in the theme README how to set the BORDERLESS flag per bank (config, not code) and ship it in the Amarchy DOpus button bank files the kit installs, if any (U whether the kit installs banks; grep dist/ when implementing).

## Fable review 2026-10-29

Verdict: the plan's shape is right and is the most system-friendly route available: a pen map that every drawing path on the machine already honours (Intuition frames, titles, menus, screen bar, GadTools via frameiclass on 3.2, DOpus's own bevel code, every app that reads dri_Pens), plus one class Hook for the only elements that are pictures rather than pens (sysiclass glyphs). No graphics-library patch, no caller filter, no class replacement. It beats VisualPrefs and Birdie on stability, memory, speed, boot safety, clean removal and 3.1/3.2 parity by construction; it does not try to beat them on gradients, pictures or custom title-bar images, which is correct for a flat look.

Changed by this review and why:
1. Live switch split into two tiers (P1): theme-to-theme is colour values only and loads live; only flat on/off changes pen indices and needs the Workbench reopen. The draft treated every switch as a reopen. Source: GetScreenDrawInfo BUGS text and desk_core.h:37.
2. Menu glyphs (checkmark, Amiga key, 3.2 MENUSUB) and the Workbench arrow images are sysiclass too and join the P3 hook scope; the draft only named the window gadgets. Source: imageclass.h, intuition-RelNotes 47.17.
3. GadTools recorded as covered by P1 on 3.2 through frameiclass (gadtools-RelNotes 47.1 / 47.13); OD-F9 no longer needs a DrawBevelBoxA LVO patch on 3.2.
4. IControl geometry: the release notes say the bytes carry aspect ratio, border gadget scaling and title bar increments 0..16, so geometry can only grow bars; thin borders stay a pen effect. R1 keeps the byte layout as U and also dumps the sanctioned IntuitionControlA() layout hook, the only candidate for a clean frame hook (OD-F6).
5. IExceptionPrefs (IEXC) chunks must be copied through by the IControl writer; P1-2 host test extended.
6. Class replacement via MakeClass / AddClass ruled out explicitly (AddClass BUGS) so the Hook swap is justified, not assumed; dispatcher register contract and OM_GET for SYSIA_Which stated; stale-pointer fallback when a later patcher unloads.
7. 4-colour Workbench aliasing named as a P1-1 golden and an M1 screenshot.
8. DOpus Magellan II section, items FLAT-D1..D3, M1 (g), OD-F10.
9. Owner decisions reordered: recommended option first everywhere, each with its decisive trade-off; unverified claims marked U.

Comparison, per front (ours = this plan as written; "not yet" = after P1 + P3 as planned):

| Front | VisualPrefs | Birdie | Ours |
|---|---|---|---|
| Look: window frames, title, screen bar | Full: frames, custom title images, gradients | Title / border gradients and pictures via graphics SetFunctions | Flat only, from theme pens, no patch. Wins for a flat look; never does gradients or pictures (by design) |
| Look: window and menu glyphs | Yes, own image sets via class hooks | No | Yes via one sysiclass hook (P3), theme-coloured. Not yet: pixel style unmeasured until M1 / C2 |
| Look: GadTools, requesters | Yes (gadtools + frameiclass hooks) | Fills only, pen heuristics | 3.2: yes via pens (frameiclass). 3.1: pens only, bevel code unpatched (U residue) |
| Look: DOpus Magellan II | Partly (its own bevel code ignores class hooks) | Fills only | Bevels and colours yes via pens; glyph images and string boxes need upstream FLAT-D1. Not yet until that PR lands |
| Look: apps with hardcoded pens 1 / 2 or own SA_Pens | No | Partly (fills) | No, and will not (no caller-filtered patches). OD-F9 |
| Rounded corners | No | No | No clean route in NDK (OD-F6); R1 checks the 3.2 layout hook |
| Stability | Hooks 6+ classes, gadtools, OpenWindow; needs VP-QuickFix on 3.2; 47.17 border gadget interaction noted in intuition-RelNotes | 8 SetFunctions on graphics hot paths incl. RectFill / Text; Forbid / CacheClearU | P1 zero patch; P3 one Hook field, inert flag, crash marker, boot gesture. Wins |
| Compatibility with other patches | Chain-repairing remove, SafetyLevel / BirdieMode env | Stacks on graphics LVOs | Refuses or stays inert next to VP / Birdie (OD-F5); never unloads while chained. Wins on safety, not on coexistence |
| RTG and AGA | Both (cybergraphics for gradients) | Gradients need cybergraphics | Pens are depth-agnostic; glyphs draw into the given RastPort. RTG rig U |
| 3.1 and 3.2 | 3.1-era, 3.2 needs QuickFix | 3.1-era | Pens on both; geometry 3.2 only; hook per R3 on 3.1 (OD-F3) |
| Speed 68020-060 | Every class dispatch and gadtools call goes through hooks | Every RectFill / Text filtered | P1 zero cost; P3 one compare per sysiclass IM_DRAW only. Wins |
| Memory | Image sets, prefs, hooks | Pictures / gradient buffers | One small resident, a state block, no images. Wins |
| Clean install / remove | Prefs editor + startup; remove repairs chain | Startup line | Kit row, startup block, Uninstall md5 check; remove restores or stays inert. Wins; "stays resident inert" is honest, not a leak |
| Live switch | Prefs editor save = Workbench reset | Restart Birdie | Colours live; flat on/off = one Workbench reset with confirm and revert. Wins on theme switch, equal on on/off |
| Boot safety | None beyond prefs | None | Crash marker, boot gesture, prefs key gate. Wins |

Fronts where ours does not yet win: DOpus glyphs and string boxes (needs FLAT-D1 upstream), apps with hardcoded pens or own pen arrays (never, by owner decision), 3.1 GadTools bevel residue (U until M1 on `--os 31`), RTG (U, no rig), rounded corners (no route unless R1's layout hook gives one).

## Host tests (summary)

`make test ONLY=desk` gains P1-1 / P1-2 / P1-3 / D2; new suite `make test ONLY=flat`: C0 struct, C1 hook model, C2 glyph goldens on gfx_fake, C3 budget, C4 marker and detection.

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
5. D2 with P1 (pens only); D1 upstream PR after M1 (g) shows the residue; D3 when the kit ships banks.
6. FLAT-X1 last; A1200 check once per phase.

## Owner decisions

DECIDED 2026-10-29: pens first; patches only for the measured residue; no caller-filtered graphics patches. Open ones below, recommended option first.

ANSWERED 2026-10-30 (owner, through the question tool). These override the recommendations listed per item below.

- OD-F1: (b) flat pens on, glyph patch off after install.
- OD-F3: (b) everything on both 3.1 and 3.2: pens, IControl geometry and the glyph patch. The owner did not take the recommendation. Every rig check runs on `--os 31` and `--os 32`. Where R1 / R3 show 3.1 has no route (ic_GUIGeometry is a V2 field, the 3.1 sysiclass Hook layout), that finding goes back to the owner as a new question; it is not dropped silently.
- OD-F4: (a) shine = background pen, (a) stock border and title size, (a) thin line glyphs in the text pen.
- OD-F5: (a) refuse next to VisualPrefs / Birdie; P1 pens still apply.
- OD-F6: (c) no rounded corners. FLAT-C6 closes.
- OD-F7: (a) both mouse buttons held at boot.
- OD-F8: (a) separate resident `amarchy-flat`.
- OD-F9: (b) patch the residue. The owner did not take the recommendation. For DOpus this means a runtime patch of DOpus's own drawing (glyphs in image_class.c, string double bevel) from `amarchy-flat`, not a DOpus fork build (the owner chose the runtime patch over the fork when asked to pick one). Constraints carried over: no caller-filtered graphics patches; the patch identifies the DOpus code it hooks by version and checksum and stays inert on any DOpus build it does not know, so a DOpus update turns the patch off instead of crashing. Risk recorded: breaks (goes inert) on every DOpus update; crash surface inside DOpus.
- OD-F10: pens now (FLAT-D2) plus the runtime patch FLAT-D4. No upstream PR: the owner ruled FLAT-D1 out on 2026-10-30 ("if we do the runtime patch we dont need to do the pr"). Risk recorded: every DOpus update can change the hooked code, so FLAT-D4 goes inert until its version table is updated for that build.
- New item FLAT-D4 (from OD-F9 / OD-F10): runtime DOpus glyph and string-bevel patch in `amarchy-flat`. Version-gated, inert on unknown DOpus builds. Host test: version gate table; rig: DOpus lister arrows and string fields flat with the patch, stock with it off, stock on a DOpus build with an unknown checksum. Status: not started, waits for owner go.

- **OD-F1 Default after install.** (Recommended) (b) Flat pens on, glyph patch off. Trade-off: the flat colours are the product and cost zero patch risk; the price is that every bevel on the screen changes at first install, which the 15 s confirm / revert already guards. (a) Off; Amarchy prefs turns it on. Trade-off: nothing changes for users who never asked, but the flat look is never seen out of the box. (c) All on. Trade-off: the class hook runs on a machine nobody has watched yet.
- **OD-F3 AmigaOS 3.1.** (Recommended) (a) Pens on 3.1 and 3.2; geometry and glyph patch on 3.2 only. Trade-off: one OS to verify for the patch; 3.1 keeps stock border size and glyphs; 3.1 GadTools bevel residue is U (no frameiclass route there). (b) Everything on both. Trade-off: every rig check twice; R1 / R3 may show 3.1 needs other paths. (c) 3.2 only. Trade-off: the 3.1 pen path is free and already works (palette.prefs pens identical, V), dropping it buys nothing.
- **OD-F4 Look options.** Shine pen: (Recommended) (a) background pen. Trade-off: flattest; a two-tone bevel reads as a single 1-pixel SHADOWPEN outline, and the same SHINE == BACKGROUND equality is the flat signal DOpus FLAT-D1 keys on, so one choice drives both. (b) fill pen. Trade-off: keeps a faint edge on selected surfaces, but DOpus and others see no flat signal. Border and title size: (Recommended) (a) one Amarchy default from R1 (stock, since geometry can only grow bars, V text). Trade-off: no prefs row. (b) a size option in prefs. Trade-off: one more row and more rig states; only thicker bars are available anyway. Glyph style: (Recommended) (a) thin line glyphs in the text pen. Trade-off: matches a flat outline look, 1-pixel lines are faint on AGA interlace. (b) filled blocks. Trade-off: bolder, less like Omarchy.
- **OD-F5 VisualPrefs or Birdie present.** (Recommended) (a) amarchy-flat refuses to start and the prefs page names the patch to remove; P1 pens still apply. Trade-off: no stacking unknowns; the user must choose one look layer. (b) Start and hook on top. Trade-off: two patchers drawing the same glyphs, untested order, and VisualPrefs' remove routine rewrites hook slots it believes are its own (research note). (c) Pens only, no glyph patch. Trade-off: same as (a) without telling the user why glyphs stay stock.
- **OD-F6 Rounded window corners.** Facts: NDK 3.2R4 has no shaped-layer or window-decoration API (clib/layers_protos.h 47.1: layers are rectangles of ClipRects; only InstallLayerHook / InstallLayerInfoHook backfill hooks, no LAYA_Shape / ChangeLayerShape in the include tree, V). Only painted corners are possible. (Recommended) (c) No rounding. Trade-off: square corners, no artefacts; Amarchy tiles windows edge to edge, where painted corners show a notch against the neighbour most of the time. (a) Painted corners: 2-3 pixels per corner in the background pen. Trade-off: right over the plain desktop, a visible notch over other windows and over a backdrop pattern; needs a frame-draw hook point (C6); the only sanctioned candidate is the 3.2 IntuitionControlA() layout hook (U what it exposes, R1 dumps it).
- **OD-F7 Boot kill-switch gesture.** (Recommended) (a) Both mouse buttons held at boot. Trade-off: rarely pressed by accident; needs the gesture read before the swap in the startup block. (b) Left button. Trade-off: collides with other boot-time tools. (c) None, crash marker only. Trade-off: a crash before the 60 s marker delete is already covered; a wrong-but-not-crashing look is not.
- **OD-F8 Process model for the patch.** (Recommended) (a) Separate resident `amarchy-flat`. Trade-off: one more program and an interface, against a crash on every Amarchy restart once chained. (b) Inside Amarchy. Trade-off: no IPC, but the hook code lives in a seglist that upkg / RELOAD / `rig.py install` unload.
- **OD-F9 Residue that pens and the sysiclass hook cannot fix** (filled from M2). Per element: (Recommended) (a) accept it beveled. Trade-off: no new patch surface; on 3.2 this is expected to be only apps with hardcoded pens or their own SA_Pens, since GadTools goes through frameiclass (V). (b) A targeted patch for that element only (class hook or library LVO such as gadtools DrawBevelBoxA on 3.1, never a caller-filtered graphics patch). Trade-off: more look, more crash surface per element.
- **OD-F10 DOpus Magellan II flat.** (Recommended) (a) Pens from P1 now (FLAT-D2 sends the 11 `dopus set pens` keys from the theme), and an upstream PR to BlitterStudio/dopus5 (FLAT-D1) that draws one outline and TEXTPEN glyphs when SHINEPEN == BACKGROUNDPEN. Trade-off: the full flat DOpus waits for an upstream merge and a new DOpus build on the user's machine; stock-pen users see zero change. (b) Pens only, accept DOpus's own glyph and string-box residue. Trade-off: nothing upstream, listers and banks flat, arrows and string fields stay two-tone or faint. (c) Private patched DOpus in the kit. Ruled out by the ledger rule (no private fork); listed so it is not re-asked.
