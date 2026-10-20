---
date: 2026-10-19
status: draft
topic: native split panes in one vtcon console window (no tmux)
evidence: origin/main (fetched 2026-10-19); paths are repo-relative, line numbers from origin/main
---

# Native split panes

Research + plan only. No code changed. Owner scope: todos.md ~line 510 (NATIVE-SPLITS, after TUTORIAL-WINDOW W4).

## 1. Current architecture (evidence)

- **One process = one `con` = one window session.** `typedef struct con` at handler/vtcon_handler.c:143-331. It embeds `vtwin w` (:145, "the window: engine, renderer, fonts, frame clock"), the DOS-side state (reads queue :160, `in`/`le` line editor, `raw`/`ld` termios), the menu strip (`menustrip`), scroll bar, find/KingCON windows. Window state and session state are mixed in one 3700-line struct; this is the 1:1 assumption.
- **`vtwin`** (render/vtwin.h:69) holds `struct Window *win` (:109), `vt_term *t` (:120, the engine), `vr_render r` (:121, the renderer). The renderer takes the whole window as its target: `vr_render` (render/amiga_render.h:106) has `win`, `rp`, `ox/oy` ("top left of the text area in the window", :113), `cols/rows`, `inset_top` (:172).
- **vr_layout** (render/amiga_render.c:968-1000) derives `cols/rows` from `Window->Width/Height - Border*` and `ox = BorderLeft + lay_x`, `oy = BorderTop + lay_y + inset_top`. Only `inset_top` (top strip) exists as a carve-out; there is no left/right/bottom or origin offset beyond borders, and no rect object.
- **Shell attach:** the DOS packet path is a real handler. `ACTION_FINDINPUT` etc. on `XCON:` reach one handler process per Open'd window (ACTION_VTCON_* packets, handler/vtcon_packets.h). The reading shell is whichever task sends ACTION_READ (`reader_shell`, `comp_armed`, `words_owner` in con). A second shell on the same window cannot simply Open the same XCON: stream: DOS starts one handler process per Open (tabs plan, thoughts/shared/plans/2026-10-03-tabs.md:56).
- **Tabs already host several engines in one window (reuse candidate, the key finding).** Tabs plan chose "one process per tab" explicitly because one process serving N sessions means splitting every packet path (tabs.md:55-58). Mechanism:
  - Host owns the window (`own_win` :183); a tab is a process that `tab_register`s (vtcon_handler.c:6083-6122) by FindPort on `UPTermTabs.<hex>` and gets back the host's `Window*`, an inset, sets `w.own_rp = &c->tab_rp` (copy of window RastPort with its own pens/font), `w.inset_top`, `foreign_window=1`, `r.off=1`.
  - Each tab has a full `con`: its own engine, renderer, DOS stream, settings, menus. Spawn: `tab_spawn` (:5848) builds `NewShell "XCON:0/0/320/100/UP-Term/TAB <hostport>/PROFILE <p>"` and a `T:UP-Term-tab` FROM script that starts vsh (`tab_spawner` :5774-5818). `TAB_MAX 9` (:124).
  - Host routes everything: `tab_route` (:5954-6050): Right Amiga 1-9 raw codes 0x01-0x09 switch tab (:5962), NEWSIZE posted to every tab (:5969), REFRESH, tab-bar click (:5987), everything else forwarded to the active tab via `tab_post` (`TM_EVENT` carrying class/code/qual/mouse, struct `tab_msg` :5361).
  - Only ONE tab draws at a time: hide/show handshake (`tab_show_active` :5615, `r.off`, comment :5945 "the cursor from the other tab leaks"). Inactive tabs keep parsing, draw nothing.
- **Render path vs sub-rectangles.** Mostly rect-ready: all cell drawing uses `r->ox + x*cw`, `r->oy + y*ch` (amiga_render.c:3372-3577); `vr_scroll` (:3852) scrolls only `ox..ox+vis_cols*cw, oy+top*ch..` via ScrollRaster (:3946) or BltBitMap (:3920, `VTCON_DIRECT`, uses `win->LeftEdge + r->ox`). Dirty tracking is the engine's `damage(user,x0,y0,x1,y1)` callback (engine/vtengine.h:114) and scroll callback, per `vt_term`, so it is already per engine. What is NOT rect-ready: full-window clears and border strips use `win->Width/Height` (:3191-3198 padding fills, :3244-3290, vr_layout :970-979, `vtwin.c:1020,1053` EraseRect from BorderLeft/BorderTop, `vtwin.c:1845` font fit, `vtwin_set_size` :1873). Scroll regions (DECSTBM) are the engine's, independent of the window (vr_scroll(top,bottom,n)).
- **Menus:** `menu_ids.h:9` has `MENU_TAB_NEW/NEXT/PREV/CLOSE`; each tab builds its own strip, the active tab's is the window's (tabs.md:51-52). Right Amiga keys: Right Amiga F (find) at vtcon_handler.c:5255; Right Amiga 1-9 at :5962 (tab_route); other RAmiga keys in vtwin.c:1378-1388 (`vtwin_key`).
- **Mouse:** `vtwin_mouse` (vtwin.c ~1629) takes window pixel coords, does selection and mouse reporting (xterm 1000/1006) against `r->ox/oy`; wheel via IDCMP_EXTENDEDMOUSE or NewMouse raw keys. Host forwards mouse events to the active tab (tab_post). No divider/drag-resize code anywhere.
- **Resize:** IDCMP_NEWSIZE -> `vtwin_resize` (vtwin.c:1185) -> `vt_resize` (reflow), then `resized` callback; TIOCGWINSZ/SIGWINCH for ixemul programs via `winch_irq` input handler (con :186-189; rig test tools/rig/winch_rig.py, resize_rig.py proves tmux and nvim follow). Per-session size comes from `ACTION_VTCON_GWINSZ/SWINSZ` (vtcon_packets.h:29-35), so a pane smaller than the window needs the winsize to report the PANE size.
- **No split code today.** `git grep split|pane` over handler/ render/ engine/ finds only tmux comments (engine/vtwidth.h:6). The 4-way split seen by the owner is tmux (demo/updemo.c scene "a tmux split", demo/updemo.h:27). Research notes 2026-10-04_modern-terminal-gaps.md:185 and settings-gaps.md:132,309 list splits as "missing, tmux covers it".
- **Perf numbers in repo:** vt_feed 11.6 us/byte (A1200 020), ncurses frame of 4,268 bytes = 49.4 ms feed + ~61 ms render per write at 106 us/cell (thoughts/shared/research/2026-10-19_vtcon-feed-perf.md:23-29); tmux server start 63 s -> 12.9 s, `tmux ls` 17 s -> 3 s after ixemul speedups (plans/2026-09-28-vtcon.md:1050), "tmux started in 9.3 s only after a malloc cache" (2026-10-04-unix-tool-ports.md:280). No measured steady-state tmux-vs-single-window frame rate exists (unmeasured: must be measured in phase 0).

## 2. Design

Recommendation: **panes are tabs shown simultaneously.** Reuse the tab host/child process model; do not build N engines in one process (rejected by the tabs plan for a still-valid reason: one DOS stream needs one process per Open). Engines per window: 1 per pane, each in its own handler process, exactly as tabs.

### Data model
- New pure-C89 module `render/layout.[ch]` (portable, host-testable): a binary layout tree. Node = leaf(pane id) | split(dir H/V, ratio in 1/256ths or fixed cell count, child a, child b). Operations: `lay_solve(tree, x,y,w,h, cell_w, cell_h) -> rect per pane` (pixel rects snapped to whole cells; 1-cell divider line, in pixels `cw` or `ch` wide), `lay_split(pane, dir)`, `lay_close(pane)` (sibling takes the space), `lay_neighbour(pane, dir)`, `lay_hit_divider(x,y)`, `lay_drag(divider, px)`. Max panes = TAB_MAX-style constant (open question).
- `vr_render` gets a **viewport rect** (`vx, vy, vw, vh` in window pixels) replacing the implicit "whole inner window": `vr_layout` computes `ox/oy/cols/rows` from the rect; `inset_top` becomes the rect's top. Default rect = today's whole inner window, so tabs and CCON-style windows are unchanged.
- Host `con` gains `layout` (tree) and `pane_list[]` (the existing `tab_list` generalised: a tab is a pane of a single-leaf layout). A "tab" then = a layout of its own; one window may hold several tabs each with its own split tree (phase 5; phase 1-4 = splits within the active tab only, or splits only and tabs unchanged).

### Focus
- One focused pane gets keys; host forwards via `tab_post(TM_EVENT)` (already does for the active tab). Focus changes by: click in a pane, Right Amiga + arrow / bracket (open question). Focused pane draws a cursor in its normal style, others draw the unfocused hollow cursor (engine already has cursor styles). Active-window loss: all panes hide cursors (reuse IDCMP_INACTIVEWINDOW path :5284).
- Menus: the focused pane's strip is the window's (same rule as tabs, tabs.md:51).

### Resize
- Host receives NEWSIZE (and divider drags), runs `lay_solve`, posts `TM_RECT(x,y,w,h)` to each pane (new message beside `TM_INSET`, struct tab_msg gains rect); each pane runs `vr_set_rect` + `vtwin_resize` (reflow already supported) then sets its winsize for GWINSZ/SIGWINCH (`post_sizewindow`/winch path must name the PANE size). Pane smaller than minimum (e.g. 10 cols x 3 rows): layout refuses the split / drag.
- Window-level things that assume the whole window (DECCOLM `want_cols` window resize vtwin.c:1156, `vtwin_set_size`, fullscreen, `vtwin_set_font` fit :1845) must act on the focused pane's rect or be disabled for panes; list in phase 2.

### Per-pane shell
- Pane spawn = `tab_spawn` unchanged: NewShell on `XCON:.../TAB <host>/PROFILE p` + FROM script running vsh (`tab_spawner`). Each pane has its own DOS handler instance, own stream, own TIOCGWINSZ; no packet-path split needed. Second shell per window needs only: a pane-aware REGISTER answer (rect instead of inset) and the host's layout tree. Pane close = its shell exits -> `tab_unregister` -> `lay_close`; last pane ends window (host_only rule, tabs.md:48-50 kept).
- Spawning a pane in the same cwd: reuse `termurl_cwd_dir` path already in tab_spawner (`m->cwd`).

### Keys and menus
- New menu items `MENU_SPLIT_RIGHT`, `MENU_SPLIT_DOWN`, `MENU_PANE_NEXT`, `MENU_PANE_PREV`, `MENU_PANE_CLOSE`, `MENU_PANE_ZOOM` in handler/menu_ids.h, resolved by the focused pane, routed through `tab_command` like MENU_TAB_*. Menu text full words ("Split Right", "Split Down", "Next Pane", "Close Pane", "Zoom Pane").
- Right Amiga keys: reuse the raw-code dispatch at :5962 (layout-independent raw codes, noted in the comment there). Candidate: RAmi+D split down? (conflict check against `vtwin_key` :1378-1388 and Amiga menu shortcuts) -- owner decision.
- Pane-sized help text: add to slash/help and the tutorial.

### Mouse dividers
- Host hit-tests `IDCMP_MOUSEBUTTONS` against `lay_hit_divider` before forwarding (extends the tab-bar hit-test at :5987). SELECTDOWN on a divider starts a drag; MOUSEMOVE during drag calls `lay_drag`, throttled to the frame clock, resize applied on release or at 10 Hz. Pointer position -> pane: otherwise forward to the pane under the pointer (not "the active tab") with coordinates relative to its rect; click also sets focus. Selection and mouse reports (1000/1006) remain per pane because they use that pane's `ox/oy`.

### Rendering
- All panes draw at once (drop the single-shown-tab handshake for splits; keep it between tabs). Each pane process draws only into its rect through its `tab_rp`; no ScrollRaster/BltBitMap crosses the rect (vr_scroll already limits to `ox..ox+vis_cols*cw`; `VTCON_DIRECT` path uses `win->LeftEdge+r->ox` which stays valid). Needs: LockLayer discipline already in :3912; confirm two processes drawing the same layer concurrently is safe (phase 0 spike on the rig) -- else host serialises with a token message.
- Clip: NO clip region. Corrected 2026-10-19 after the phase 0 spike: `InstallClipRegion` sets the region of the window's one Layer, not of a RastPort, so two pane processes would install and restore over each other. Every draw (text, fills, scrolls, padding strips at :3191-3198, :3244-3290) stays inside the rect on its own; the fake-gfx tests prove it.
- Dividers + pane titles drawn by the host in `bar_draw`-style code (tab bar precedent) with theme tokens (config/upconf theme colours), full-pixel lines, no emoji.
- Gadget: the scroll bar (sbar_gad) is a window-border gadget showing the active tab's knob via TM_KNOB (:5372): becomes the focused pane's knob; per-pane scroll bars are out of scope.

### Perf budget on 68k (A1200 020 baseline, feed-perf research)
- Cost model: vt_feed 11.6 us/byte; render 106 us/cell. Panes add no per-byte cost; they split the cell count. Full 80x24 repaint is already ~204 ms; a 4-pane layout of the same area has the same total cells, so a full-window repaint cost is unchanged and a typical pane update (one pane busy) is cheaper.
- Versus tmux: tmux means every pane's output goes shell -> pty -> tmux server (ixemul, 12.9 s start) -> its own redraw escapes -> vt_feed -> render; native removes one pty hop, the tmux server process, its start time and the extra escape bytes (each tmux redraw re-emits cup/sgr). Expected win: no 13 s startup, one fewer 020 process, ~2x less vt_feed bytes per visible change. UNMEASURED; phase 0 measures tmux 4-pane vs native 4-pane using engbench workload 7 and tests/amiga/phaseprobe.c.
- Budgets (pass/fail, to be set after phase 0): pane split latency < 1 s (spawn dominated by NewShell); focus switch < 100 ms; divider drag keeps >= 5 resizes/s; idle CPU with 4 panes equal to 4 tabs (cursor blink timers: one frame clock per pane today -> share the host's? check `frame_port` per vtwin).
- Memory: each pane = one full con + engine + scrollback (sb_lines default 500). 4 panes on a 2 MB-chip/stock machine: measure Avail before/after; panes inherit sb_lines but default to 200 when more than 2 panes (open question).

## 3. Phases (checklist). Every item ends with a reachability test through the real entry point: the rig `XCON:` window, driven by a script in tools/rig/ (new `splits_rig.py`, modelled on tools/rig/resize_rig.py and the tab rigs), with a sentinel proving the new code ran (PROF/`VTCON_DEBUG` serial counter e.g. `splits/rects_applied`) and the numbers asserted. Pure-C layout logic: one host-suite test (`make test ONLY=layout`) where it lives. Per-item: ONE rig test per feature, not per assertion.

### Phase 0 - measure and spike (no product code)
- [x] NS0.1 Baseline (f50c9571, thoughts/shared/research/2026-10-19-splits-baseline.md, 3 runs each, rig 4): tmux startup to pane prompt 9.7 s; tmux idle with 2 panes 7.1 % CPU vs plain vsh 0.4 %; `Type` of 138 KB in a tmux pane 3.04 s vs 1.72 s without tmux (1.77x). Changed 2026-10-19: engbench is engine-only and has no tmux path, so the throughput figure is the timed `Type`, and the file holds 3 figures, not 4.
- [x] NS0.2 (attempt 3, 2026-10-19, ledger: per-pane SGR bands, 60 s streaming, 0 pixels across the seam in runs 2-4; the wrapped-text seam moved into splits_rig `paint`) Spike: two tab processes draw into disjoint rects of one window at the same time (hack: both `r.off=0`, `inset`/rect hardcoded). Test: rig screenshot-as-text (clipboard read as step 13 of amiga-pi rig does) shows both panes' sentinel strings; no corruption over 60 s of output in both.

### Phase 1 - viewport rect in the renderer
- [x] NS1.1 (8d7bb42d, d1d8fa4b: render_host asserts no draw outside the rect, planar and direct) `vr_render` rect (`vr_set_rect`), `vr_layout` honours it, all fills/borders/padding use the rect not window edges (amiga_render.c:3191-3290, vtwin.c:1020,1053). Host test: render_host fake RastPort (tests/render_host/gfx_fake.c) logs every call; assert no call outside the rect for a full repaint, scroll, selection, cursor, sixel. Reachability: `make test ONLY=render_host` plus rig XCON: full-window default rect unchanged (existing rig suites still pass).
- [x] NS1.2 (fdba16a7 host test: no region installed, nothing outside the rect after an inset move; rig: splits_rig `paint` and `race` PASS on rig 4, 2026-10-20) No layer clip region; rect-confined drawing only (corrected 2026-10-19). Host test: no region is ever installed, nothing is drawn outside the rect, also after an inset move. Rig: two panes draw at the same time and neither paints into the other.

### Phase 2 - pure layout tree
- [x] NS2.1 (df1a589e, suite `layout`) `render/layout.[ch]` C89: solve/split/close/neighbour/hit-divider/drag, cell snapping, min size, max panes. Test: host suite `layout` covers numbers (rect sums = area minus dividers; ratios stable across resize). Reachability for this module is the Phase 3 rig test (not duplicated).

### Phase 3 - host hosts several simultaneously drawn panes
- [x] NS3.1 (rig 4, 2026-10-20: splits_rig sentinel, size, focus, paint, race PASS) `TM_RECT` message and pane-aware REGISTER answer; hide/show handshake skipped for panes in the same tab; host runs `lay_solve` on NEWSIZE. Rig test: open XCON:, menu Split Right via the real menu (rig injects menu pick), assert two panes each report `stty size`/TIOCGWINSZ = pane cols x rows from `lay_solve`, typed lines land in the focused pane only, output in both visible (clipboard text read).
  Design note (2026-10-19, from the NS0.2 rig leak): the pane rect lives in the tab's vtwin state and bind reapplies it after vr_init, the same way inset_top is reapplied. A rect set only on the renderer is lost on any re-init or reset.
  NS3.1 design (2026-10-19, decided, do not re-litigate):
  - Host data: `c->tab_list[]` becomes `c->tabs[TAB_MAX]`, one `tab_slot { lay l; con *pane[LAY_MAX_PANES]; int focus; }` per bar entry. `tab_at(c, i)` = that slot's focused pane; every old `tab_list[i]` use goes through it, so keys, the bar title, TM_KNOB and the menu strip follow the focused pane with no other change. Broadcasts (STATE, insets, NEWSIZE, quit) loop over every pane of every slot.
  - Split = spawn first, commit at REGISTER. The host records a pending split `{serial, from con, dir}` and spawns `XCON:.../TAB <port>/PANE <serial>/PROFILE ...`; the spawner writes a per-spawn script `T:UP-Term-tab.<serial>` (fixes the shared-script race). REGISTER carries the serial; the host finds `from`'s slot, runs `lay_split`; refused (too small, 4 panes, from gone) -> it becomes a new tab as today. No empty hole while NewShell starts.
  - Geometry: the host lays out every multi-pane slot with `lay_solve` over the inner area below the bar, `LAY_DIV_PIXEL`, cell size of the splitting pane. It sends `TM_RECT x y w h` (inner-area pixels); `w == 0` means "whole area below the inset" (single pane, the old inset path). The REGISTER answer carries the rect. The tab keeps it in `vtwin` (`rect_x/y/w/h`, `rect_on`); `bind` reapplies it after `vr_init`; `vtwin_set_rect` = `vr_set_rect` + `vtwin_resize` (winsize + SIGWINCH follow). One function `tab_layout(c)` sends rects/insets for NEWSIZE, bar change, split, close.
  - Show/hide: switching bar tabs hides every pane of the old slot; the host counts outstanding HIDE replies (`c->hides`) and shows every pane of the new slot when it reaches 0. Focus moves inside a slot need no handshake. TM_SHOW `what=1` = focused (sets the menu strip); `what=0` = drawn, no strip.
  - Dividers: host draws 1-pixel lines from `lay_divider_rect` with a theme pen after each layout and on REFRESHWINDOW; no titles.
  - Pane exit (needed already here, NS3.3 tests it): `lay_close`, focus = returned id, `tab_layout`; the last pane of a slot removes the slot as today.
  - Menu: Split Right / Split Down items added now (the NS3.1 rig test picks them); keys and the rest are NS4.1.
- [ ] NS3.2 Per-pane winsize/SIGWINCH correct after a window resize (extend winch_rig.py, resize_rig.py): both panes' `stty size` follow within 1 s.
- [ ] NS3.3 Pane close: shell `exit` in one pane returns its space; last pane exit closes the window; host's own shell ending keeps window while panes live (tabs rule). Test: rig script.
- [ ] NS3.4 Focus: click in other pane and Right Amiga focus keys move keys and the hollow/solid cursor. Test: rig injects mouse click via input.device, types, reads each pane's text.

### Phase 4 - menus, keys, mouse dividers
- [ ] NS4.1 Menu items and Right Amiga bindings (`menu_ids.h`, `tab_command`, :5962 dispatch). Test: each menu id picked on the rig changes the pane count/focus as asserted; a count of menu ids vs items built from the menu builder (not grep).
- [ ] NS4.2 Mouse divider drag with `lay_hit_divider`/`lay_drag`. Test: rig sends press/move/release on the divider; both panes' `stty size` change by the dragged cells; a click not on a divider still reaches the pane (mouse reporting test with `?1000` program).
- [ ] NS4.3 Mouse wheel and selection per pane; selection never crosses a divider. Test: rig scrollback in pane B while A keeps output.

### Phase 5 - polish and scope decisions
- [x] NS5.1 (rig 4, 2026-10-20: splits_rig zoom PASS) Zoom pane (toggle one pane to full rect and back). Test: rig.
- [ ] NS5.2 Interaction with tabs: each tab owns its own layout (or splits only in the first tab: per owner answer). Test: rig, switch tabs and see layout preserved.
- [ ] NS5.3 DECCOLM, View > 80x24 / 132x43 menu items, fullscreen, font bigger/smaller act on the focused pane or are disabled while split (decided by owner). Test: rig per item.
- [ ] NS5.4 Perf: measure against Phase 0 budgets (rig, 4 panes): record to build/rig/splits/perf.txt; all budgets asserted. Includes `top`/ps idle check.
- [x] NS5.5 (rig 4, 2026-10-20: splits_rig restore PASS) Persistence if chosen: layout saved in profile (upconf), restored on window open. Test: rig reopen.
- [x] NS5.6 (8528997f README + window lesson, d4706f6a slash help, d4af863a matrix note; man/UPTerm.1 exempt: it names no setting one by one and points at /help) Docs: tutorial lesson, slash help, conformance matrix note (panes are not a sequence; matrix unaffected), man page, README feature list; remove "splits missing" rows in research notes? (research notes are history: leave).
- [x] NS5.7 (a3e078a5: updemo `splits` scene; owner 2026-10-20: tmux scene stays) Remove superseded demo claim? No: the updemo tmux-split scene stays; add a native-split scene only if owner wants (demo/updemo.c).

Plan count: 17 items (NS0.1-NS0.2, NS1.1-1.2, NS2.1, NS3.1-3.4, NS4.1-4.3, NS5.1-5.7). Execution rule: tick in the open with running count; partial = failed plan.

## 4. Risks

1. **Concurrent drawing into one layer by several processes** (graphics.library). Mitigation: NS0.2 spike first; fall back to host-serialised drawing token.
2. **Hardcoded window-edge assumptions** in render and vtwin (listed in section 1) - missing one makes pane A paint over B. Mitigation: fake-gfx rect assertion (NS1.1) plus the no-region rect test (NS1.2). No layer clip exists to catch a miss, so the fake-gfx test is the guard.
3. **Memory/CPU on stock 68k**: N full cons (each with line editor, KingCON state, 2 KB obuf, scrollback). Mitigation: measure in NS0/NS5.4; cap panes.
4. **Cursor/frame-clock timers per pane** (frame_port, blink) multiply timer traffic.
5. **Menus per pane**: each tab builds its own GadTools strip; swapping on focus change flickers and is slow on 68k.
6. **Scroll bar and find/KingCON windows** are window-level; shared owner must be the focused pane.
7. **Host death**: panes depend on the host process (tab precedent); closing the host kills all panes -> must behave like closing the window.
8. **Programs reading window size once** (no SIGWINCH) see pane size at start only; fine, as in tmux.
9. **Scope creep into tmux features** (detach/attach, copy mode, named sessions): out of scope.

## 5. Open questions for the owner (product only)

1. Key bindings: Right Amiga + which keys for split right / split down / next pane / close pane / zoom? Arrow keys or brackets for directional focus? Keep mouse click focus?
2. Maximum panes per window (suggest 4; tab limit is 9)?
3. Splits per tab (each tab its own layout) or splits only in a single-tab window?
4. Layout persistence in the profile (restore panes and which programs on open), or always start single?
5. What runs in a new pane: vsh in the same directory (as tabs), the pane's profile, or the same program as the source pane?
6. Divider look: one-pixel line, a cell-wide gap, or titled? Pane titles shown?
7. Scrollback default for panes (500 as XCON: today, or smaller when many panes)?
8. Window-level commands while split (font size, 80x24/132x43, fullscreen, DECCOLM): act on focused pane, on the whole window, or disabled?
9. Is tmux kept as the documented way for detach/attach, with native splits positioned as "layout only"?
10. Ship as part of 1.x or a later release; start only after TUTORIAL-WINDOW W4 merged (todos.md).

## 6. Owner answers (2026-10-19)

- Go: full plan.
- Splits per tab (each tab owns its layout). Max 4 panes per tab.
- New pane runs vsh in the same directory (tab_spawn as for tabs).
- Keys: split right RA+D, split down RA+Shift+D, focus RA+arrows, close pane RA+Shift+W, zoom RA+Shift+Enter; mouse click also focuses.
- Window-level commands (font size, 80x24 / 132x43, fullscreen) act on the whole window, panes keep proportions; DECCOLM from a program is ignored while split.
- Persistence (NS5.5): a user setting in the profile. Values off / layout / layout+programs, default off. All three ship in the first release (owner 2026-10-19), so NS5.5 includes restarting the program each pane ran.
- Dividers: 1-pixel line, no pane titles; focus shown by solid vs hollow cursor.
- Still open: pane scrollback default, tmux positioning in docs, release target.

## 7. Added findings (peer session, verified on origin/main 2026-10-19)

- Spawn race: tab_spawner writes one shared script name `T:UP-Term-tab` (handler/vtcon_handler.c:5796, used as `FROM T:UP-Term-tab` at :5809). Two panes spawned close together can overwrite each other's script. Fold into NS3.1: unique script name per spawn (host + sequence), deleted by the script itself; rig test spawns two panes back to back and asserts each starts in its own directory.
- ACTION_DISK_INFO hands back the whole window (`id->id_VolumeNode = (BPTR)c->w.win`, vtcon_handler.c ~6909). A program in a pane that draws straight into that Window draws over other panes. Same as tmux has no answer for. Decision for NS5.3: keep returning the window (V47 compatibility), document that raw-window programs see the whole window, (no pane clip region exists, NS1.2, so nothing to exempt). Rig check: a raw-window program in a pane still works.
