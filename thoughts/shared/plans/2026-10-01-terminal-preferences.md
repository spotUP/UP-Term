---
date: 2026-10-01
topic: UP-Term terminal window preferences (iTerm2-style profiles)
tags: [vtcon, terminal, prefs, configuration, profiles]
status: draft
---

# Plan: User-configurable UP-Term windows (profiles, colours, font, cursor, bell, behaviour)

(Extensive plan; see also the tradeoffs asked at the end of the 2026-09-30 discussion.)

## Goal
Make every XCON: window (and CON:/RAW: replacement windows) user-configurable the way iTerm2/Terminal.app are: named profiles with default colors, palette, font, cursor, bell, scrollback, keys, mouse — stored in a plain-text file, settable per window, and eventually editable via a Prefs app.

## Non-goals / constraints
- console.device units (phase D, DD4) keep the ROM's appearance (Workbench pens, Amiga personality). Profiles apply to XCON:/CON:/RAW: windows only.
- The conformance matrix is the spec (RULES 2): behaviour changes update the matrix and a test in the same commit.
- The engine stays portable C89 (RULES 1); the new config module is portable (no OS calls) and host-testable.
- No per-app profiles in v1 (no reliable "which program is running" hook; title-based only).

## What exists today (evidence)
- Command-line options parsed in `parse_spec` (handler/vtcon_handler.c:462-577): x/y/w/h (px), title, CLOSE/WAIT/BACKDROP/NODRAG/NOBORDER/NOSIZE/NODEPTH/INACTIVE/AUTO, WINDOW 0x…, XTERM/AMIGA/PCANSI, LATIN1/CP437, DARK/LIGHT/FG/BG, SCREEN, FONT name.size + FONT1..9/FRAKTUR.
- Window creation (open_window, vtcon_handler.c:580-660): OpenWindowTagList, flags DRAGBAR|DEPTHGADGET|SIZEGADGET|SIZEBRIGHT|ACTIVATE|SMART_REFRESH, MinWidth 80/MinHeight 40, IDCMP includes MOUSEBUTTONS/MOUSEMOVE/RAWKEY/NEWSIZE/REFRESH/CLOSE/ACTIVE/INACTIVE — **no IDCMP_MWHEEL yet**.
- Scrollback: `vtwin.sb_lines`, 0 → 500 (render/vtwin.c:257-258). XCON: never sets it.
- Cursor: DECSCUSR block/underline/bar already drawn (cursor_flip, amiga_render.c:1311-1330), blink via ?12/DECSCUSR 1/3/5; colour is XOR complement (not configurable), no default-style setting.
- Palette: per-terminal `t->pal_set[256]` (OSC 4 mutates it), default xterm-256 table (vtengine.c:2994), `vt_resolve_colors` (engine:3701), pen_for uses nearest xterm-256 on AGA / scratch pens on truecolour (amiga_render.c:107).
- Bell: engine `cb.bell` → `DisplayBeep()` (render/vtwin.c:41-44); no visual, no mute.
- Meta key: Left Amiga = Meta hard-wired (vtwin.c:595-599).
- Mouse: drag-select; Shift+drag when program uses mouse; RAmiga C/V clipboard (clip.c); no copy-on-select; mouse wheel not received.
- Config storage precedent: `/ENV/up-term/` (ENVARC:up-term) for terminfo/termcap/ixpipe/unstartup (dist/Install:17-55); `vsh` reads ENVARC:vsh/vshrc + $HOME/.vshrc; history at ENVARC:vtcon.history; user-file preservation pattern (up-term-orig).
- File access from handler: only when idle or via worker (complete.c); never mid-packet (vtcon_handler.c:173-184).
- Line editor keys: fixed (lineedit.c/complete.c, README KEYS).

## Config model
File: `/ENV/up-term/up-term` (ini-like, profiles as sections):

```ini
[profile default]
font = TOPAZ:8.8.font
fg = C0C0C0
bg = 000000
scrollback = 2000
cursor = block            ; block|underline|bar
cursor-blink = on
cursor-color = inverse    ; inverse|RRGGBB
bell = visual             ; none|beep|visual
bold-bright = off         ; xterm only: SGR 1 uses colours 8-15
meta = amiga              ; amiga|alt
copy-on-select = on
wheel = scroll            ; scroll|ignore
palette = 1,0x00CD00,4,0x5C5CFF   ; remap 16 ANSI colours (optional)

[profile vim]
font = PARADISEC:8.8.font
cursor = bar
bell = none
```

**Precedence (low→high):** built-in defaults < profile < window-spec options (`XCON:.../DARK`) < runtime (OSC 4/10/11/12, DECSCUSR). No file → byte-identical to current behaviour.

## Modules
1. `config/upconf.[ch]` — portable C89 parser/merger, no OS calls. API: `upconf_parse(buf,len)`, `upconf_get(conf, prof, key)`, `upconf_save(conf, path)`. Host-testable. Read in `handler_main` **before the first `GetMsg`** (idle, safe).
2. Handler: merge profile into `vtwin` spec (font, fg/bg, sb_lines, cursor*, bell, meta, copy-on-select, wheel, bold-bright, palette[16]). `PROFILE name` spec option. Wheel: `IDCMP_EXTENDEDMOUSE` + `IMSGCODE_INTUIWHEELDATA` (OS 3.9+; there is no IDCMP_MWHEEL on classic Amiga) → `vtwin_wheel`, which scrolls via `vr_set_view` or, in mouse-report mode, sends buttons 64/65. Copy-on-select: `copy_selection` on drag end.
3. Engine: expose `vt_set_palette(t,i,rgb)` (already has `t->pal_set`); `bold-bright` engine option (SGR 1 shifts 0-7→8-15 in xterm, inert for others/DCTelnet).
4. Renderer: cursor-colour draw (scratch/ObtainBestPen, glyph in bg colour), one-frame visual bell, wheel via view.
5. Prefs app `C:UP-Term Prefs` (phase 2): native Intuition two-page window (General/Colors), writes `/ENV/up-term/up-term` with `.orig` backup; Install offers it; icon via `tools/mkicon.py`. No live-apply (new windows pick it up).

## Phases
1. **P1 — config layer + all knobs**: `config/upconf.[ch]`, handler load, engine/renderer changes, `PROFILE` option, matrix, README, host tests + `tools/rig/prefs_rig.py`.
2. **P2 — Prefs app + kit**: Intuition app, save with backup, Install/Uninstall/icon, rig check.
3. **P3 — extras (approved)**: window menu (right-click: switch profile, copy, paste, clear, font ±), live-apply to open windows (IPC), scrollback search (Cmd-F).

## Risks / gotchas
- Config read before the first `GetMsg` (idle) — never mid-packet (deadlock).
- Font changes cell metrics and planar glyph extraction; set before window exists (same as FONT option today) — no re-layout needed in P1.
- AGA pen budget: custom fg/bg + 16-palette compete; `pen_for` already degrades (nearest index) — verify with cube check on AGA rig.
- Engine changes default to unchanged behaviour (bold-bright ON, matching today's SGR 1; cursor shape default unchanged; new host setters are no-ops until called).
- Wheel-delta sign (`WheelX < 0` = forward/up) is inferred from the Amiga's down-Y convention, not documented; verify on a wheel mouse at the rig check, and flip in `handler/vtcon_handler.c` (one line) if backwards.

## Files affected (indicative)
New: `config/upconf.c`, `config/upconf.h`, `tests/test_upconf.c`, `prefs/upprefs.c`, `dist/up-term`, `tools/rig/prefs_rig.py`. Changed: Makefile, handler/vtcon_handler.c, render/vtwin.h/c, render/amiga_render.c/h, engine/vtengine.c/h, clip.c, dist/Install, dist/README.txt, tools/mkicon.py, the matrix (thoughts/shared/research/2026-09-28_console-conformance-matrix.md).

## Progress (ledger — trust this over recollection)
- P1.1 upconf module: DONE — `config/upconf.h/.c`, host-tested (83 checks in `suite_upconf`, incl. the comment-only sample file shape). P2 added `upconf_hex`, `upconf_rmprof`, `upconf_palette_parse` (0x01RRGGBB convention, stops at a malformed pair), `upconf_palette_str` (`00,RRGGBB,...`, no NUL).
- P1.2 host tests + build wiring: DONE — `tests/test_upconf.c`, Makefile `CONF`/`TESTS`, all 13 host suites green.
- P1.3 engine: DONE — `vt_set_palette`, `vt_set_bold_bright` (+`bold_bright` field, xterm-only shift, default 1), `vt_set_cursor_style`, `vt_set_cursor_blink`, `vt_screen_reverse`; xterm test `host_settings_palette_bold_cursor()`; 68000 cross-compile clean.
- P1.4 renderer + vtwin: DONE —
  - `amiga_render.c/h`: `cursor_draw` (profile cursor colour: block filled with the colour, glyph in the default bg; underline/bar and double-size rows stay inverted), `vr_set_cursor_color` (pen/`VR_KEEP`), `cursor_ink` init in `vr_init`, pen release in `vr_free`.
  - `vtwin.c/h`: attach-time profile apply **after `report_defaults`** (bold_bright, cursor style/blink, palette via `vt_set_palette`, cursor colour via `vt_set_default_colors` + `vr_set_cursor_color`); `cb_bell` modes 0/1/2; copy-on-select at drag end; `vtwin_wheel(w, up, mx, my)` (3 lines/notch, mouse buttons 64/65 in mouse mode); `meta_alt` (Alt keys = Meta, LCOMMAND stripped only otherwise). 68000 cross-compile clean for `vtwin.c` + `amiga_render.c`.
  - **Visual bell is a renderer effect, not the engine's DECSCRM** (redone in review): `vr_bell_flash`/`vr_flash_tick` on `vr_render`; `cell_style` swaps fg/bg for one frame. Using `vt_screen_reverse` for the flash would have stolen the program's own `?5h` state and the un-reversed frame would linger when no blink kept frames coming. `vt_screen_reverse` stays in the engine (still host-tested).
- P1.5 handler: DONE — `config_load` reads `/ENV/up-term/up-term` in `handler_main` after the startup reply (idle, pre-GetMsg), per-process `upconf` (AllocVec, freed at exit; also fixed: `config_load` used `strlen(buf)` on the NULL from a failed AllocVec); `PROFILE name` spec option; `apply_profile` merges (font NAME:SIZE, fg/bg under `colours_spec`, scrollback, cursor*, bell, bold-bright, meta, copy-on-select, wheel, palette pairs — the palette through `upconf_palette_parse`); spec defaults set in `parse_spec` (bell=1, bold_bright=1, wheel_scroll=1, cursor_rgb=VR_KEEP); wheel via `IDCMP_EXTENDEDMOUSE`/`IntuiWheelData` in both IDCMP masks + dispatch. 68000 cross-compile clean for handler + upconf. All 13 host suites green.
- P1.6 matrix: DONE — BEL row (none|beep|visual per profile, default beep), SGR 1 bold-bright + Q6 resolved (ON default, profile-togglable), DECSCUSR row notes the per-profile start style/blink/block colour (profile default only; runtime sequences override).
- P1.7 dist: DONE — `dist/up-term` fully-commented sample (parses inert), `dist/Install` never-clobber copy to `ENVARC:up-term/` + `ENV:up-term/` (Uninstall already deletes both), `dist/README.txt` CONFIGURATION section.
- P1.8 rig: script WRITTEN (`tools/rig/prefs_rig.py`: profiles a/b, no-profile vs a vs b window, colour-family checks, SGR 32 palette, row-pitch for the font; `ink_at` takes the expected background and picks the pixel furthest from it), NOT RUN (rig down). Documented as not covered: wheel direction (needs a wheel mouse), visual-bell flash, copy-on-select clipboard content.
- COMMIT CHECKPOINT: Makefile is shared with the other agent's uncommitted WIP (upgetty/sigprobe rules). Decision needed: commit my non-overlapping files only (leaves committed tree without the CONF/TESTS handler wiring until their WIP lands) vs. hold. Flagged to owner.

## P2 progress (Prefs app + kit)
- P2.1 the app `prefs/upprefs.c`: WRITTEN, compiles clean for 68000 (-O2, -warnings-as-errors) and links. One Intuition window, one static gadget list, two pages (General / Colors) switched by disabling the other page's chain; the app's gadget ids are reported through `IAddress` (this NDK's `struct IntuiMessage` has no `GadgetID` field), `struct Gadget.GadgetID` is UWORD so the palette ids are `ID_PAL + i`.
  - NDK/API facts this NDK settled (do not re-derive): **no `dos/dosbase.h`** (use `dos/dosextens.h` + `proto/dos.h`); `WA_Left`/`WA_Top`, not `WA_LeftEdge`; the gadget-list tag is **`WA_Gadgets`** (WA_Dummy+0x09), this NDK has no `WA_GadTools`; classic gadget types are renamed — string = `GTYP_GADGET0002` (0x0002), label = `GTYP_PROPGADGET` (0x0003); `SetGadTools`/`SyncGadTools` are in **neither this NDK nor vbcc's clib** (a hand-declared prototype links to nothing usable — the clib emits a bare `jsr` with no library base in A6), so the app uses `RefreshGadgets(0, win, 0)`, which the clib does declare; `UnlockPubScreen` takes `(name, screen)`; `struct Window` here has no `UserPointer`; `startup.o` provides `_DOSBase` and `_SysBase` but **not** `_IntuitionBase`, so the app must define `struct IntuitionBase *IntuitionBase;` itself (defining DOSBase too is a duplicate-symbol link error).
  - Save: validates every field **before** touching the table (a bad value leaves the loaded file untouched), writes `up-term.orig` as the backup, creates `ENVARC:up-term` if missing. Palette fields are `RRGGBB` (0x/# accepted).
  - **Palette line width, fixed (owner: "fix and proceed")**: the value slot was 64 bytes, which holds about six remap entries (`II,RRGGBB,` is ten bytes), so the 16-field grid could not be written. Root cause was the table's value width, not the UI, so `UC_MAX_VALUE` is now **160** — a full grid is 159 bytes and fills a slot exactly (host-tested: `upconf: 83 checks`, and the new full-grid round-trip test fails on 64 and passes on 160). Cost: the `upconf` struct is 8*32*160 = **41 KB**, AllocVec'd once per handler session; a session without profiles pays it anyway. `UC_MAX_FILE` (16384) is now shared by the handler's read cap and the editor's write cap, so a file Prefs writes is always a file the handler reads whole (they were 16384 and 65536).
- P2.2 kit: Makefile `$(BUILD)/amiga/upprefs` (built as `vc -o` with `config/upconf.c`; the kit renames it to `UP-Term Prefs` because macOS make cannot hold a space in a target name), added to the `amiga:` list; dist copies it, generates `UP-Term-Prefs.info` (`tools/mkicon.py --tool "C:UP-Term Prefs" --plain`) and its script line; `dist/Install` copies the binary to `C:` and the icon to `SYS:Utilities`; `dist/Uninstall` removes both **and now keeps the user's profiles** in `ENVARC:up-term-orig/up-term` instead of deleting the drawer wholesale; `dist/README.txt` documents the window and the round trip.
- P2.3 rig: `tools/rig/prefs_rig.py --ui` added (same entry point, opt-in): puts the binary on the rig, writes a file with one profile, opens the window, clicks the font field, backspaces it, types a new value, clicks Save, reads `ENV:up-term/up-term` back and checks the edit, the untouched keys, the profile name and the `.orig` backup, then closes the window. NOT RUN (rig down). The rig uses `VTC:upprefs` because a space in the rig's `run` command would break its CLI parsing; the installed name is Install's business.
## P3 progress (extras)
- **P3.3 scrollback search — engine half DONE, host-tested**: `vt_find(t, q, from)` in `engine/vtengine.c` — the scrollback and the grid, oldest line first, ASCII-case-insensitive substring, `VT_ROW_NONE` when nothing matched, `from` lets the caller continue and wrap. A line the terminal wrapped is searched as one line, so a query crosses a wrap (`VT_FIND_MAX` 4096 covers the joined text; `VT_FIND_QUERY_MAX` 256 for a query, which is what a typed line is). Tests in `tests/test_xterm.c`: reading order, case folding, the wrap-spanning query, searching on does not look back, empty/one-byte/absent queries, UTF-8 folded on the ASCII part only. `xterm: 306 checks`.
- **P3.3 window half — compiled, NOT RUN**: `vtwin_find(w, q)` (q NULL = repeat the last query, kept in the window) scrolls the view to the hit: a grid match returns the view to the live output, a scrollback match puts the line on the last row of the window so what follows is readable. Bound to **Right Amiga F**; the prompt is the handler's own one-line window (`find_open`/`find_run`/`find_close`/`find_idcmp` in `handler/vtcon_handler.c`), Enter searches and closes, Escape closes, a query that matches nothing beeps. It has its own port and window so the console window's IDCMP is untouched: output, resize and the wheel keep working while you type.
  - NDK facts for the prompt (this NDK is old and stripped — do not re-derive): **no `REQ_STR_GETANSWER`** and no requester tag that asks for a string, so the string gadget is a hand-built `struct Gadget` + `struct StringInfo` filled field by field, the same shape the Prefs editor uses (`mk()` in `prefs/upprefs.c`). `struct StringInfo` exists in `intuition/intuition.h` and Intuition maintains `Buffer`/`MaxChars`/`BufferPos` itself. There is no `CreateGadget`, no `GADGET_GETINFO`, no `FreeGadget`, no `WFLG_NOVISIBILITY`, no `GACT_STRINGCLASS`, no `WA_NoActivate`/`WA_WindowSkeleton` here: the gadget goes in at `WA_Gadgets` (as the Prefs editor does) instead of `AddGadget`, and `OpenWindowTagList`'s first argument is a `struct NewWindow` — pass 0 and set `win->UserPort` after the open, before anything is waited on. `RefreshGadgets` is the 2.x-shaped `(gadget, window, refresh)` call. `DeletePort` is `DeleteMsgPort`, `DisplayBeim` does not exist (use `DisplayBeep(win->WScreen)`, as `cb_bell` does), and `struct Window` has `LeftEdge`/`TopEdge`/`Width`/`Height`, not `WBounds`.
- **Toolchain: the build no longer borrows another repo's NDK.** The AmigaOS3.2 SDK headers (`Include_H`, 4.1 MB) are vendored at `vendor/ndk-3.2r4-Include_H` and `tools/vbcc-aos68k.cfg` points at that relative path; it used to reach into `~/Code/dctelnet-petscii-recovered/.ndk`, so a checkout could not build from its own tree. All six cross-compiled sources plus `make amiga` and `make dist` are clean with it. **`vendor/` is gitignored** (Lars's call, 4.1 MB of third-party headers stay out of the repo) and the include path is `VTCON_NDK` in the Makefile, defaulting to `vendor/ndk-3.2r4-Include_H`: `make amiga VTCON_NDK=<path-to-Include_H>` builds against any other copy.
- **P2 leftovers closed** (all with a host test or a compile behind them):
  - `hex6` was local to `prefs/upprefs.c`, so the one thing that validates what the user typed could not be tested. It is now `upconf_hex6()` in `config/upconf.c` — a strict six-digits-and-nothing-else parser next to the lenient `upconf_hex()` that reads values already in a file — and `tests/test_upconf.c` has 14 checks on it (prefixes, tabs/spaces, a 7th digit, junk after, junk inside, too short, empty, NULL). Verified it fails without the trailing-junk check: 3 of the new checks fail, all 97 pass with it. `upconf: 97`.
  - Prefs Save is no longer a delete-then-write on the live file: it writes `up-term.new`, and only a complete file is renamed over `up-term`. A full disk or a short write now leaves the user's file exactly as it was, and the message says so. `Rename` needs the target gone first, so the delete is still there, but by then the new file is complete on disk and the old one is in `up-term.orig`.
  - Prefs `main()` had three exits after the libraries opened; two leaked the app block and both libraries. There is now one `fail:` label that undoes whatever was made, in reverse.
  - `dist/Install` copies the spaced `UP-Term Prefs` with quotes now, as `dist/Uninstall` already deleted it. Confirmed the file survives `make dist` + `lha -x` with its space.
- P2 remaining: rig runs (`prefs_rig.py` and `prefs_rig.py --ui`), and the visual read of the window (labels, page switch, tab grey/active) — a human's eye or the rig screenshots.

## Verification (RULES 4: one rig reachability test per feature, rest host-tested)
- Host: new `config` suite in `vttest_host` — parse/merge/precedence/defaults/save-roundtrip; engine tests for bold-bright, palette setter, cursor defaults.
- Matrix: update BEL row (now: none|beep|visual, per profile), note DECSCUSR default and SGR 1 bold-bright — matrix + tests in same commit.
- Rig: `tools/rig/prefs_rig.py` — write profile, open XCON:, check colours cell-exact, grid aspect after font change, scrollback, wheel, Prefs save → new window matches. install_rig.py and vttest_rig.py stay green.
