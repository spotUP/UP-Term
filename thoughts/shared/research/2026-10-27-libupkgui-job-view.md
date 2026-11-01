---
date: 2026-10-27
topic: Can the Amarchy text menu show a job view through libupkgui as it is today
status: research
---

Read: upterm-ports worktree .claude/worktrees/kit-main, detached HEAD 8b49328 (2026-10-27), pkgs/upkg-ui/src/ui.h (105 lines), ui.c (1045 lines).

## Verdict: NO (three missing pieces)

What exists and fits:
- Header: ui_opt.title / backtitle drawn by draw_masthead (ui.c:~490-500, draw_all).
- Body that follows the tail: K_PROGRAM + ui_program_begin/ui_program_line (ui.c:~850-880); `follow` (ui.c:243) pins tbtop to maxtop in draw_textarea (ui.c:369-382). ui_add_line (ui.c:894) also grows the buffer (no 200-line cap; buffer realloc doubles).
- Keys Up/Down/PageUp/PageDown/Home/End scroll: key_text (ui.c:613-630). Esc returns 255 from run (ui.c:~745). Enter closes via key_buttons (OK button).
- Tick hook: ui_on_tick (ui.h, ui.c:689), run loop calls tick_fn once per TICK_MS = 1000 ms (ui.c:38, 718-730); nonzero ends ui_run with UI_TICK.

Missing:
1. Redraw from the tick. run() sets `redraw = 1` only after a key (ui.c:~760); the tick path calls only paint_rail() then tick_fn. Lines added by ui_add_line inside on_tick stay invisible until the user presses a key. (ui_program_line redraws itself, but it is only usable while the caller owns the loop, not inside ui_run.) Also the tick is 1 s, coarse for live output.
2. Follow with scroll-up release. `follow` is set only by ui_program_begin and cleared by ui_program_done/clear_lines; key_text never clears it, and draw_textarea forces tbtop = maxtop while follow is 1. So in K_PROGRAM the user cannot scroll up while the job runs. In K_TEXTBOX follow is 0, so the view never tails. Nothing sets "follow until the user scrolls up, resume at the end".
3. Footer status line with own text and colour. draw_footer (ui.c:172) draws fixed key hints (H_TEXT / H_WAIT tables) in a_acc/a_dim. No caller-supplied status string and no way to pick an attribute. K_PROGRAM before done shows only H_WAIT (no Esc/Enter hints either; Enter is inert until ui_program_done, Esc still returns 255).

## Smallest addition (request to the UP-Term session)

```c
/* tick: a nonzero return from the tick callback OR this flag set by the caller redraws the widget */
void ui_invalidate(void);                 /* run() sets redraw = 1 on the next tick */
/* tail-follow that a scroll-up key releases and End / reaching the last row re-arms */
void ui_follow(int on);                   /* K_TEXTBOX too; key_text clears follow on UP/PPAGE/HOME,
                                             sets it again on END or when tbtop == maxtop after DOWN/NPAGE */
/* footer status */
enum ui_status { UI_ST_NONE, UI_ST_DIM, UI_ST_OK, UI_ST_FAIL };
void ui_status(const char *text, int st); /* draw_footer prints text in the matching attr, after
                                             (or replacing) the key hints; NULL clears */
```
Fit: run() already has `redraw`; after tick_fn returns 0, set `redraw = 1` when ui_invalidate was called (one flag, cleared in run). key_text already owns tbtop. draw_footer gets a_ok/a_fail via make_attr (see below). Optionally a TICK_MS setter (ui_tick_ms) if 1 s is too slow; a K_TEXTBOX with OK button and Esc then covers all the keys with no K_PROGRAM state machine.

## Footer colours and theme roles
- Possible: init_theme (ui.c:68-90) builds pairs with make_attr(fg,bg); a_col[0..7] are already plain colour pairs on ground 0 (a_col[2] = green, a_col[1] = red under the standard ANSI order), a_dim = pair(8,0) (bold black on 8-colour terminals). So Done = a_col[2], Failed = a_col[1], Running = a_dim need only a status call that selects among existing attrs; no new pairs needed. a_col is reachable today only via ui_opt.colors with \Zn codes in MENU rows (put(), ui.c:~135), not in the footer or textarea.
- menu.c maps NO theme roles to libupkgui: it includes gthemes_dos.h (line 23, the Amiga-side theme store) but makes no ui_ colour or theme call (grep: only ui_opt kind/title/backtitle/prompt/hotkeys/entries, ui_prepare, ui_on_tick, ui_run, ui_selected, ui_hotkey, ui_message, ui_osd, ui_start, ui_stop). libupkgui's palette is fixed in init_theme: chrome 6, ink 7, dim 8, accent 13, selection bg 5 / ink 0 ("Uprough Neon (muted)").
- menu.c's existing text view (viewer(), menu.c:~310-335) is the precedent: ui_clear_lines + ui_add_line, kind K_TEXTBOX, ui_on_tick(on_tick,0), ui_run; its on_tick (menu.c:127) returns 1 to end on Ctrl-C or watcher change.
