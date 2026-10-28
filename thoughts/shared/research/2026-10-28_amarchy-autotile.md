---
date: 2026-10-28
topic: AMARCHY-AUTOTILE phase 1 research (what exists for a tiling commodity)
tags: [amarchy, win, commodity, arexx, autotile]
status: research
---

# AMARCHY-AUTOTILE: what exists today

All paths are relative to /Users/spot/Code/UP-Term/vtcon. Ledger row: thoughts/spot/todos.md:3240 (decisions) and :3241 (session note).

## 1. win_core / win.c

Pure layer, amarchy/win_core.h (no OS headers) and win_core.c:
- `win_box {left,top,width,height,minw,minh,maxw,maxh,screen}` win_core.h:33-38; maxw/maxh <= 0 means "screen is the limit". `win_screen {width,height,title}` win_core.h:40-44; `title` = BarHeight+1 (set in win.c:75).
- Functions: win_parse (win_core.c:30), win_move :106, win_size :137, win_fullwidth :156, win_fullscreen :166 (area below title bar), win_is_fullscreen_terminal :179, save ring :184-215, win_pick :218 (directional focus), win_ticket_valid :259.
- Private helpers `clamp` (:97) and `fit(v,min,max,room)` (:126: honours the window's own Min/Max, the room left, and a floor of 1). `fit` is static: a tiler needs it exposed (or a new `win_place(box, x,y,w,h, screen)` in win_core that calls it) so Min/Max clamping is not duplicated.
- No layout/tile function exists. No list-level API (only win_pick over a box array). Verb enum has no tile verb (WV_* win_core.h:12-14); win_parse is a closed-form text parser, host-tested.

OS layer, amarchy/win.c:
- `box_of` win.c:62-77 copies a Window into win_box + win_screen (Min/Max with 0xFFFF mapped to 0; screen width/height; title = BarHeight+1). Called with IBase locked.
- `apply_box` win.c:134-137 = `ChangeWindowBox(w, left, top, width, height)`; called inside the LockIBase section (win.c:228-230).
- Lock discipline: header comment win.c:3-8: lookup, membership check (`member` :37-47 walks IntuitionBase->FirstScreen -> FirstWindow) and queue-only calls (ChangeWindowBox, ZipWindow, WindowToFront/Back) under LockIBase(0); ActivateWindow, Screen*, ShowTitle, LockPubScreen, workbench.library after UnlockIBase. Foreign windows are never closed.
- Target window: `IntuitionBase->ActiveWindow` if `usable()` (:53-56: member and not "Amarchy" titled, `is_ours` :32-35 by title string compare); or the menu "ticket" (g_ticket, win.c:100-113 take, :215-217 use). Screen = `w->WScreen` (no named lookup in the win verbs).
- Window walk for focus/cycle: `for (x = w->WScreen->FirstWindow; x; x = x->NextWindow)` win.c:253-262, capped at WIN_MAXW 32 (:23), skipping is_ours.
- Borders/title/menu bar: nothing handled. ChangeWindowBox sets the outer box (Width/Height are outer incl. borders). Only the screen title bar is respected (win_fullscreen uses s->title). No Window->BorderTop/Left logic anywhere in amarchy/win*.c. No WFLG_SIZEGADGET / sizeable test exists anywhere in amarchy/*.c (grep WFLG_ only hits win.c:130 BACKDROP|BORDERLESS and gui.c/menu). Unconfirmed: whether Min==Max could serve as "not resizable" (not implemented either).
- Reusable directly for master-stack: win_box/win_screen types, box_of, apply_box, member/usable/is_ours, the FirstWindow walk, fit-clamping (once exposed). New: layout math (pure, in win_core), sizeable/requester/backdrop classification (new; WFLG_SIZEGADGET/WFLG_BACKDROP/IDCMP-less requesters have no precedent), tiling state.
- win_exec (win.c:187-290) is one static function dispatching via a verb switch; ARexx entry is `win_port(arg)` (:307-311) returning WIN_RC_OK 0 / REFUSED 5 / UNKNOWN 10 (win.h:6-8).

## 2. Commodities

Only one broker exists in the repo: amarchy/cx.c (`grep -rlE "CxBroker|NewBroker" --include='*.c'` finds only cx.c). cxdesc.c is the filter-description helper (CX_FILTER_DESC), not a broker. clipwatch.c and ixwatch.c are NOT commodities (see 4).
- Broker build: cx.c:63-111 `cx_open`: OpenLibrary commodities.library 37, CreateMsgPort, NewBroker {nb_Name "Amarchy", nb_Title, nb_Descr, nb_Unique NBU_UNIQUE|NBU_NOTIFY, nb_Flags COF_SHOW_HIDE, nb_Pri, nb_Port} (:71-80), CxBroker, ActivateCxObj(g_broker,1).
- Exchange events: `cx_next` cx.c:143-170: GetMsg + ReplyMsg, CXM_COMMAND switch CXCMD_ENABLE/DISABLE (ActivateCxObj), KILL, UNIQUE, APPEAR, DISAPPEAR. amarchy.c:341-350 handles KILL -> g_quit, UNIQUE -> reload; APPEAR/DISAPPEAR are returned but ignored in the loop (no window to show).
- Hotkey filter: one CxFilter with an IX (Left Amiga qualifier, any code) + CxCustom gate + CxSender + CxTranslate(NULL) (cx.c:90-110); gate lookup in the keymap. A tiler needs no hotkeys itself (Amarchy drives it over ARexx), so it needs only broker + port, not the filter network.
- Event loop shape: amarchy.c:323-358 `Wait(cxs|rxs|nts|lps|wrs|cws|SIGBREAKF_CTRL_C)` then per-signal serve. This is the loop to copy.
- Cleanup: cx_close cx.c:113-130 (DeleteCxObjAll, drain port with ReplyMsg, CloseLibrary).
- Closest template for a small standalone commodity binary: amarchy/cx.c (broker part, minus gate) + amarchy/amarchy.c main loop + its rexx_serve. Note: there is no existing small standalone commodity; Amarchy.c is itself large (launch port, notify, clip). Unconfirmed: nothing under tools/, tests/amiga, or addons builds another commodity (grep on *.c only; rexx/shell scripts not searched).

## 3. ARexx host code

- Host port implementation: amarchy/amarchy.c:203-233 `rexx_serve` and creation at :301-307 (OpenLibrary rexxsyslib.library; under Forbid FindPort("AMARCHY") ? 0 : CreateMsgPort; mp_Node.ln_Name; AddPort). Shutdown :362-366 (RemPort, final rexx_serve drain, DeleteMsgPort, CloseLibrary). Parsing: `starts(cmd,"STATUS")` etc. over `m->rm_Args[0]`; rc into rm_Result1; result string via CreateArgstring only if `rm_Action & RXFF_RESULT` (:227-228). Commands today: STATUS, RELOAD, RUN <row>, WIN <verb>, QUIT, SHOW; unknown rc 10. STATUS text is "hits=N rows=N enabled=N active=N wins=N" (amarchy.c:126-143; rig parses it, amarchy_window_rig.py:44-51). No shared rexx host module: it is inline in amarchy.c (vbcc, RexxSysBase global at amarchy.c:33). A new commodity would copy this ~30 lines, or extract it (a "single source" opportunity: a small amarchy/rexxhost.c used by both).
- Client (sending to another port): amarchy/leaf.c:427-466 `static long rexx_send(portname, line, result, cap)`: OpenLibrary rexxsyslib, CreateMsgPort, CreateRexxMsg, CreateArgstring, rm_Action = RXCOMM|RXFF_RESULT, FindPort+PutMsg under Forbid, WaitPort, rc = rm_Result1, DeleteRexxMsg. Static in leaf.c; used by send_win (leaf.c:469-481, "WIN <verb>" to AMARCHY) and the DOpus lister queries (:492, :499, :511, port DOPUS.1). leaf.c is linked into amarchy-menu and amarchy-gui (Makefile:786, 800), not into the Amarchy commodity itself.
- Menu/hotkey rows that send ARexx:
  - `win:` kind (KA_WIN config/keymap.h:79; dist/keymap:65-75 e.g. `super+shift+right = win:move right`): hotkey -> Amarchy do_row -> act_win (win.c:303) in-process; menu row -> LF_WIN (leaf.h:48) -> send_win -> port AMARCHY.
  - `rexx:` kind (KA_REXX config/keymap.h:75; keymap.c:127, :215; dist/keymap:32, :115-117, :134): hotkey -> amarchy.c:161 act_rexx (act.c:217-229) -> lf_rexx_command (leaf_core.c:219-241) builds `RX <script>` or `RX "<string>"` and act_run's it (async). Menu rows: leaf.c:569-576.
  - So an existing way to drive a new port with zero Amarchy C changes: a `rexx:` row with an inline ARexx string, e.g. `rexx:ADDRESS AUTOTILE TOGGLE` (string form is quoted by lf_rexx_command; any '"' becomes '\''). This spawns RX per press (cost; unmeasured). A native kind would be cleaner and reuse rexx_send (needs extracting rexx_send out of leaf.c to a shared file, since Amarchy.c does not link leaf.c).
  - win_parse closed forms are text-checked also in config/keymap.c `win_arg_ok` (comment win_core.h:1); adding a verb to win_core means updating that check too (unconfirmed exact location; grep win_arg_ok).

## 4. Window open/close detection

- Nothing watches Screen->FirstWindow, window IDCMP of foreign windows, or polls on a timer for windows. Existing watchers:
  - clipwatch.c: clipboard.device CBD_CHANGEHOOK hook that only Signals the main task (cw_entry :22-30, cw_start :32-68); loop handles `cws` (amarchy.c:345).
  - ixwatch.c: FindSemaphore of the indexer's shared struct under Forbid; polled by gui, not signal driven (ixwatch.c:8-48).
  - atiming.h: DateStamp marks to RAM:amarchy.timing only, no timer.device.
  - pty_handler.c:132-200 is the only timer.device user (timerequest, TR_ADDREQUEST via timer_proto copies, new_timer :168, timer_start :190, timer_stop :181): a handler, not a user program, but the pattern to copy: open timer.device once, copy a prototype IO request per timer, AddRequest, Wait on the port signal, AbortIO+WaitIO on stop.
  - Intuition event options for opens/closes (not used in repo; recommendations belong to the plan): poll FirstWindow on a timer (the ledger's chosen design), or a commodity-wide input handler/IDCMP NEWPREFS-style hook. Unconfirmed: availability of any 3.2 notification for window open/close (none known to me; not searched in docs).
- Window walks to reuse: win.c:253-262 (FirstWindow chain).

## 5. UP-Term screen

- Public screen name: "UP-Term" by default, "UP-Term.2".."UP-Term.9" if taken, or the PUBSCREEN option (handler/vtcon_handler.c:2718-2724, c->pubname at :201, :1434). PubScreenStatus makes it public (:2759) and later private again (:2773).
- Amarchy side: no Amarchy code looks the screen up by name. Existing helpers: `lock_front_screen()` / `front_screen_name()` handler/frontscr.c:12-45 (front screen's public name; linked into Amarchy via AMARCHY_SRC Makefile:721), `LockPubScreen(0)` default screen in gui.c:1508 and leaf.c:266; win.c verbs use `w->WScreen` of the active window. To tile "the UP-Term screen", the commodity must LockPubScreen("UP-Term") (+ the .N variants via LockPubScreenList; pattern at frontscr.c:21-34 walks psn_Screen/ln_Name). The IBase screen walk in win.c:37-47 is the alternative.

## 6. Build + packaging

- Cross compiler: `$(VC)` (vbcc) for plain Amiga programs. Amarchy target Makefile:717-724: `AMARCHY_LEAF_DEPS`, `AMARCHY_SRC := amarchy/taskalive.c ... amarchy/amarchy.c amarchy/cx.c amarchy/act.c amarchy/win.c amarchy/win_core.c ...`, rule `$(BUILD)/amiga/Amarchy: $(AMARCHY_SRC) <headers...>` -> `$(VC) -dontwarn=153,65 -I$(BUILD)/amiga -o $@ $(AMARCHY_SRC)`. amarchy-index (small, plain DOS): Makefile:807-814 is the shortest template. A new autotile binary would list amarchy/win_core.c in its own SRC var (shared source, no duplication). gcc/ixemul (amarchy-menu :786-791) is not needed.
- Host tests: win_core is tested by tests/test_win.c, which is part of the big `vttest_host` source list (Makefile:57-59 ends `tests/test_win.c`; the vttest_host rule is Makefile:209-211 which also compiles amarchy/win_core.c in its source list (verify with `grep -n win_core Makefile`: confirmed present on lines 721-722 for Amarchy and via the long line 211; exact vttest_host inclusion unverified since line 211 is truncated in my view). `make test ONLY=win` selects it (comment Makefile:89). New pure layout tests go in tests/test_win.c (or a sibling added to that list).
- dist-check: Makefile:1575-1579 runs tests/test_amarchy_docs.py and tests/test_amarchy_stack.py once per Amarchy binary; a new binary needs a line there and an entry in `MIN_BY_NAME`/`NO_SYMBOLS` in tests/test_amarchy_stack.py:12-14.
- dist-base prerequisites list Makefile:1400 (`dist-base: amiga jbmono $(BUILD)/amiga/amarchy-menu $(BUILD)/amiga/amarchy-gui $(BUILD)/amiga/amarchy-index ...`) and the copy into kit Files at Makefile:1448 area (cp line truncated; Amarchy binaries copied there; exact line to read in the plan phase).
- dist/parts.txt:52-55 lists `tools  Files/Amarchy`, `Files/amarchy-menu`, `Files/amarchy-gui`, `Files/amarchy-index`: add the new binary here.
- dist/install.dos:403: `Copy Amarchy amarchy-menu amarchy-gui amarchy-index UP-Term:bin CLONE` (add new name). Boot start: install.dos:428-449 writes a marked block in S:User-Startup: `Assign >NIL: UP-Term: EXISTS` / `If NOT WARN` / `  Run >NIL: UP-Term:bin/Amarchy` / `EndIf`, between ";BEGIN UP-Term Amarchy" and ";END UP-Term Amarchy"; a separate block for the index (:451-466). Uninstall removes blocks with `vsh unstartup.sh <name>` (unstartup.sh; the marker names matter). The helpers (amarchy-menu/gui) are not started at boot; Amarchy starts them on demand. An autotile commodity is opt-in (ledger decision), so whether to add a boot block is a plan decision; a new marker block would be needed (own BEGIN/END names) and an unstartup.sh handled name.
- Rig install: tools/rig/rig.py:961-968 `stage_upterm` copies `("Amarchy","amarchy-menu","amarchy-gui","amarchy-index")` from the build dir to the rig's UP-Term:bin; also rig.py:597-600 comment about the kit's User-Startup (RIG-WARM-SRV-RACE) and a recent commit d26d3636 "install_rig refuses up front, naming make rig-helpers, when a helper is not built" (the helper list there is another place to extend; grep install_rig / rig-helpers not read by me).
- Stack convention (RULES.md rule 6; memory feedback-amiga-stack-always): amarchy/amarchy.c:44-47:
  ```
  long __stack = 16384;
  const char amarchy_stack_cookie[] = "$STACK: 16384";
  ```
  also gui.c:61-65 (GUI_STACK 65536 with GUI_STR macro) and index.c:39 (cookie). Checked by tests/test_amarchy_stack.py (MIN_BY_NAME, symbol parser), run in dist-check.

## 7. Tests

- Host: tests/test_win.c (win_parse checks :19-28, win_move :34..., sizing, fullwidth/fullscreen, ring, win_pick). Pure layout tests go here via `make test ONLY=win`.
- Rig: tools/rig/amarchy_window_rig.py (header docstring :1-17). Pattern: resumable verdict files OUT/<step>.verdict (:30), `--only/--failed/--fresh`; `port(cmd)` :33-41 sends ARexx through `ami._m().arexx(conn, 'AMARCHY', cmd)` (wasabi MCP client in tools/rig/ami.py, same call as the MCP tool mcp__amiga__amiga_arexx); `status()` :44-51 parses STATUS key=value as the sentinel ("wins" counter = new code ran); `box()` :54 reads the window box via `ami.window(TARGET)` (same list as amiga_windows); known window made with `NewShell CON:40/40/320/120/amarchy-target` (SPEC :31); hotkeys pressed as real raw keys (RAW_RIGHT 0x4E, LAMIGA 0x40, LSHIFT 0x01 :29-30) in s_hotkey_move_right :97; menu path s_menu_move_left :110; deadlock probe `answers_within` :59 / `probe` :156; second ARexx target WORKBENCH via wb_port :178. Other related rigs: amarchy_rig.py (resumable base), amarchy_clash_rig.py, amarchy_menu_rig.py. Start: `UPTERM_RIG=<n> rig.py start --max --purpose "..."`.
- A reachability test for autotile would follow amarchy_window_rig.py: open N CON: windows, send `ON` to the new port, read boxes via ami.window, assert tiled numbers; sentinel = a counter in the new port's STATUS (like `wins`).

## Reuse summary / gaps for the plan

- Reuse: win_box/win_screen, box_of/apply_box/member FirstWindow walk, fit clamp (expose), broker code from cx.c, rexx host from amarchy.c:203-233 and :301-307, rexx client from leaf.c:427-466, loop shape amarchy.c:323-358, timer pattern pty_handler.c:168-200, stack cookie, amarchy_window_rig.py harness, Makefile/dist/install lists above.
- Gaps (nothing exists): tile layout in win_core; sizeable/requester/backdrop classification; window open/close watcher; screen lookup by UP-Term name for windows; border-aware math; a native ARexx-sending row kind (only `rexx:` via RX); a shared rexx host/client module (both are inline/static today).
- Unconfirmed: exact win_arg_ok location; whether vttest_host's source list includes win_core.c literally (Makefile:211 truncated in my read); dist-base cp line contents (Makefile ~1448).
