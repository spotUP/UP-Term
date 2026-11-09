# RIP core progress (branch gfx-rip-core, worktree vtcon-rip-core)

## RIP-1 byte parser: commit 666c3a21
- Files: engine/rip.h, engine/rip_parse.c, tests/test_rip.c, tests/rip/0{1,2,3}-*.dump (goldens), Makefile (TESTS list), tests/test_main.c (suite "rip"), engine/vtengine.{h,c} (inert stubs).
- API (rip.h): rip_init(st), rip_feed(st, p, n, sinks{ud,text,cmd}), rip_commands/rip_errors/rip_unknown(st), rip_dump(cmd, out, max). struct rip_cmd {level, sub, ch, nargs, arg[1040], ntext, text[256], truncated}.
- Stubs: vt_set_rip(t,on) and vt_rip_commands(t) in vtengine.h/.c, inert (return 0) until RIP-2.
- Tests (suite rip): fixture_01_line_grammar, fixture_02_continuation, fixture_03_escapes (golden dump, every split offset, byte at a time), text_lines_pass_through, malformed_commands_are_counted, level_and_pair_lists, reach_rip_through_vt_write (EXPECTED-PENDING RIP-2: prints PENDING, RIP2_LANDED=0 flips it on).
- Run: `nice -n 19 make test-one SUITE=rip` -> [OK] rip: 115 checks, 0 failed.

## RIP-3 canvas + palette: commit a4cf8372
- Files: engine/canvas.c, engine/rip_pal.c (API in rip.h), tests in tests/test_rip.c.
- Canvas: cv_open(w,h,npal), cv_close, cv_set_pal, cv_get (-1 outside), cv_pixel, cv_hspan (x1 incl), cv_rect (corners incl), cv_take_dirty (x1,y1 exclusive, cleared), cv_flush (serial bump if dirty), cv_map(cv_w,cv_h,win_w,win_h,&m) -> {out_w,out_h,num,den}. Palette change dirties the whole canvas.
- Palette: rip_ega_rgb(master), rip_pal_default, rip_pal_reset(p,cv), rip_pal_exec(p,cv,cmd) for level-0 Q (16 args) and a (2 args).
- Tests: palette_fixture_05 (test-side mini interpreter through the real parser), palette_commands_move_output, ega_master_levels, canvas_dirty_and_serial, canvas_placement.

## RIP-2 + RIP-4 (commit 70ce0e40, one commit: the adapter calls the executor and both build inside the vtengine TU)
- Files: engine/rip_vt.c (adapter), engine/rip_draw.c (executor), edits to engine/vtengine.c/.h, rip.h, rip_parse.c (rip_in_text, rip_bol), canvas.c (cv_open_pix, own flag), sources.mk (ENGINE_INC), Makefile (TESTS list), tests/test_rip.c.
- API: vt_set_rip(t, 0|1), vt_rip_commands(t); rip_gfx_init/handles/exec. vt_feed is a wrapper over vt_feed_core; RIP off = straight call, byte-identical.
- Canvas is image kind 4, z -1, 640x350 = 224000 bytes, charged via kit_room (O12); serial bumped after each feed that drew; freed image (ESC[2J) -> fresh blank canvas on next command.
- Reachability measured (80x25, cell 8x14): reply 14 bytes; 4 commands; 1 image; view 640x350 cw 8 ch 14; 51 line pixels; 961 bar pixels (31x31); (41,41) background.
- Tests: rip suite 287 checks 0 failed. Other suites run: xterm 475, amiga 109, reflow 147, sixel 451, kitty 470, kittyout 29, iterm2 85, pcansi 36, ansimusic 592, mirror 466, all 0 failed.
- Checks: engine/rip_*.c and canvas.c compile standalone with -std=c89 -pedantic -Wall -Wextra -Werror. `make amiga` exit 0, no errors.

## Open / UNVERIFIED
- Placement of text after a command line ("after" row) is not pinned: RIP-5 decides. A command needs a line start, so after ESC[2J add CR LF.
- ESC[!p (DECSTR) also draws a detection reply: UNVERIFIED stray reply.
- BGI fill pattern bytes, line-style bit patterns, O versus V, arc and thick-arc pixel exactness: UNVERIFIED; tests pin extreme points and counts only.
- Window, text, button and query commands are RIP-5 and later: the adapter ignores them.

## Older open items (RIP-1/3)
- vt_image_store wiring (image_install, image budget O12, z<0 placement, serial at kitty.c:1146) is NOT done: RIP-2 adapter. cv_map is the placement policy only.
- Reachability test cannot go green before RIP-2 (adapter) and RIP-4 (executor).
- Fixture README coordinates are off in places: 01 cyan rect is 7K..9G = (272,10)-(340,60) not 280; magenta 360..426 not 420; 02 yellow line y=212 not 210. Parser values are right; README approximate.
- Not checked: the new files against the Amiga cross build (only host build, plus standalone `cc -std=c89 -pedantic -Wall -Wextra -Werror -c`). Engine files are not yet in sources.mk / Amiga lists.
- Width table for levels 1 and 9 commands beyond fixtures 01-03/05/07/09 is from the plan/spec check, UNVERIFIED against more fixtures (1B, 1I, 1R, 1F, 1G, 9 ESC).
- Fixtures 04, 06-09 parse not golden-tested.

## RIP-5 / RIP-6 (done)
- Text window = scroll region rows + origin; `w` x0/x1 and its size flag unused (no DECSLRM) - exempt. Wrap flag is ONE digit (`w` x0 y0 x1 y1 wrap size).
- "After" row: the CRLF ending a RIP command line is consumed; text lands at the grid cursor (window row 0 after `w`/`g`). Pinned in text_window_commands_and_wrap.
- Canvas survives ESC[2J (refs bump around rip_run); tiles re-anchored after text.
- Mouse: 128 combined, last wins, clr flag = `*`. Exempt: clk invert not drawn, icons not drawn, `$VAR$` list unknown (all no-op) UNVERIFIED, font8x8 licence UNVERIFIED.
- Wired: render/vtwin.c vtwin_mouse (before vti_button) and vtwin_key. UNVERIFIED on a real Amiga window (build only).
