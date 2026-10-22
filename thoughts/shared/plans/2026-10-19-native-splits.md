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
- Clip: `InstallClipRegion` per pane RastPort (own layer clip rect) as a safety net so a bug in padding fills (:3191-3198, :3244-3290 use window edges) cannot paint outside; those fills must use the rect anyway.
- Dividers + pane titles drawn by the host in `bar_draw`-style code (tab bar precedent) with theme tokens (config/upconf theme colours), full-pixel lines, no emoji.
- Gadget: the scroll bar (sbar_gad) is a window-border gadget showing the active tab's knob via TM_KNOB (:5372): becomes the focused pane's knob; per-pane scroll bars are out of scope.

### Perf budget on 68k (A1200 020 baseline, feed-perf research)
- Cost model: vt_feed 11.6 us/byte; render 106 us/cell. Panes add no per-byte cost; they split the cell count. Full 80x24 repaint is already ~204 ms; a 4-pane layout of the same area has the same total cells, so a full-window repaint cost is unchanged and a typical pane update (one pane busy) is cheaper.
- Versus tmux: tmux means every pane's output goes shell -> pty -> tmux server (ixemul, 12.9 s start) -> its own redraw escapes -> vt_feed -> render; native removes one pty hop, the tmux server process, its start time and the extra escape bytes (each tmux redraw re-emits cup/sgr). Expected win: no 13 s startup, one fewer 020 process, ~2x less vt_feed bytes per visible change. UNMEASURED; phase 0 measures tmux 4-pane vs native 4-pane using engbench workload 7 and tests/amiga/phaseprobe.c.
- Budgets (pass/fail, to be set after phase 0): pane split latency < 1 s (spawn dominated by NewShell); focus switch < 100 ms; divider drag keeps >= 5 resizes/s; idle CPU with 4 panes equal to 4 tabs (cursor blink timers: one frame clock per pane today -> share the host's? check `frame_port` per vtwin).
- Memory: each pane = one full con + engine + scrollback (sb_lines default 500). 4 panes on a 2 MB-chip/stock machine: measure Avail before/after; panes inherit sb_lines but default to 200 when more than 2 panes (open question).

## 3. Phases (checklist). Every item ends with a reachability test through the real entry point: the rig `XCON:` window, driven by a script in tools/rig/ (new `splits_rig.py`, modelled on tools/rig/resize_rig.py and the tab rigs), with a sentinel proving the new code ran (PROF/`VTCON_DEBUG` serial counter e.g. `splits/rects_applied`) and the numbers asserted. Pure-C layout logic: one host-suite test (`make test ONLY=layout`) where it lives. Per-item: ONE rig test per feature, not per assertion.

### Phase 0 - measure and spike (no product code)
- [ ] NS0.1 Baseline: tmux 4-pane (`tmux split-window` x3, tmux_rig.py) vs single window, on the rig: frames/s of engbench 7, tmux idle CPU, tmux startup s. Test: rig script records numbers to build/rig/splits/baseline.txt; asserts file has the 4 figures.
- [ ] NS0.2 Spike: two tab processes draw into disjoint rects of one window at the same time (hack: both `r.off=0`, `inset`/rect hardcoded). Test: rig screenshot-as-text (clipboard read as step 13 of amiga-pi rig does) shows both panes' sentinel strings; no corruption over 60 s of output in both.

### Phase 1 - viewport rect in the renderer
- [ ] NS1.1 `vr_render` rect (`vr_set_rect`), `vr_layout` honours it, all fills/borders/padding use the rect not window edges (amiga_render.c:3191-3290, vtwin.c:1020,1053). Host test: render_host fake RastPort (tests/render_host/gfx_fake.c) logs every call; assert no call outside the rect for a full repaint, scroll, selection, cursor, sixel. Reachability: `make test ONLY=render_host` plus rig XCON: full-window default rect unchanged (existing rig suites still pass).
- [ ] NS1.2 `InstallClipRegion` per pane RastPort. Test: fake-gfx logs the region; rig sentinel draws past the rect and nothing lands outside.

### Phase 2 - pure layout tree
- [ ] NS2.1 `render/layout.[ch]` C89: solve/split/close/neighbour/hit-divider/drag, cell snapping, min size, max panes. Test: host suite `layout` covers numbers (rect sums = area minus dividers; ratios stable across resize). Reachability for this module is the Phase 3 rig test (not duplicated).

### Phase 3 - host hosts several simultaneously drawn panes
- [ ] NS3.1 `TM_RECT` message and pane-aware REGISTER answer; hide/show handshake skipped for panes in the same tab; host runs `lay_solve` on NEWSIZE. Rig test: open XCON:, menu Split Right via the real menu (rig injects menu pick), assert two panes each report `stty size`/TIOCGWINSZ = pane cols x rows from `lay_solve`, typed lines land in the focused pane only, output in both visible (clipboard text read).
- [ ] NS3.2 Per-pane winsize/SIGWINCH correct after a window resize (extend winch_rig.py, resize_rig.py): both panes' `stty size` follow within 1 s.
- [ ] NS3.3 Pane close: shell `exit` in one pane returns its space; last pane exit closes the window; host's own shell ending keeps window while panes live (tabs rule). Test: rig script.
- [ ] NS3.4 Focus: click in other pane and Right Amiga focus keys move keys and the hollow/solid cursor. Test: rig injects mouse click via input.device, types, reads each pane's text.

### Phase 4 - menus, keys, mouse dividers
- [ ] NS4.1 Menu items and Right Amiga bindings (`menu_ids.h`, `tab_command`, :5962 dispatch). Test: each menu id picked on the rig changes the pane count/focus as asserted; a count of menu ids vs items built from the menu builder (not grep).
- [ ] NS4.2 Mouse divider drag with `lay_hit_divider`/`lay_drag`. Test: rig sends press/move/release on the divider; both panes' `stty size` change by the dragged cells; a click not on a divider still reaches the pane (mouse reporting test with `?1000` program).
- [ ] NS4.3 Mouse wheel and selection per pane; selection never crosses a divider. Test: rig scrollback in pane B while A keeps output.

### Phase 5 - polish and scope decisions
- [ ] NS5.1 Zoom pane (toggle one pane to full rect and back). Test: rig.
- [ ] NS5.2 Interaction with tabs: each tab owns its own layout (or splits only in the first tab: per owner answer). Test: rig, switch tabs and see layout preserved.
- [ ] NS5.3 DECCOLM, View > 80x24 / 132x43 menu items, fullscreen, font bigger/smaller act on the focused pane or are disabled while split (decided by owner). Test: rig per item.
- [ ] NS5.4 Perf: measure against Phase 0 budgets (rig, 4 panes): record to build/rig/splits/perf.txt; all budgets asserted. Includes `top`/ps idle check.
- [ ] NS5.5 Persistence if chosen: layout saved in profile (upconf), restored on window open. Test: rig reopen.
- [ ] NS5.6 Docs: tutorial lesson, slash help, conformance matrix note (panes are not a sequence; matrix unaffected), man page, README feature list; remove "splits missing" rows in research notes? (research notes are history: leave).
- [ ] NS5.7 Remove superseded demo claim? No: the updemo tmux-split scene stays; add a native-split scene only if owner wants (demo/updemo.c).

Plan count: 17 items (NS0.1-NS0.2, NS1.1-1.2, NS2.1, NS3.1-3.4, NS4.1-4.3, NS5.1-5.7). Execution rule: tick in the open with running count; partial = failed plan.

## 4. Risks

1. **Concurrent drawing into one layer by several processes** (graphics.library). Mitigation: NS0.2 spike first; fall back to host-serialised drawing token.
2. **Hardcoded window-edge assumptions** in render and vtwin (listed in section 1) - missing one makes pane A paint over B. Mitigation: fake-gfx rect assertion (NS1.1) plus clip region (NS1.2).
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
