---
date: 2026-10-22
topic: UP-Term makes text bold where it should not (stray bold) - ranked code causes
status: research
---

# Stray bold: findings (code reading only, no probe run, no rig)

Owner report: "UP-Term seems to make text bold where it shouldn't." No repro. Nothing below is confirmed by a run.

## Checked and clean (no bug found)

- SGR 0 and empty SGR: `sgr()` engine/vtengine.c:2783 calls `sgr_reset` when np==0; `ESC[;1m` parses to np=2 (params 0,1) via parser engine/vtengine.c:5655-5668. sgr_reset (2323) clears attr (Amiga persona: to amiga_dattr, set only by CSI SP s, 3836).
- SGR 22 clears BOLD and FAINT (vtengine.c:2891). SGR 21 is double underline, not bold-off (2889), correct for xterm; PCANSI persona maps 2/21 to bold-off by design (2838).
- 38;5;N and 38;2;r;g;b: both the fast path (2816-2823) and `ext_colour` (2752) advance `i` past the colour params, so a `1` inside them is never read as SGR 1. Colon forms: sub-params skipped as "stray" (2813), 38:2 colour-space id counted (2766). Malformed `38;1` consumes the 1 (ext_colour default branch sets i=k+1).
- Erase / scroll / insert-delete: `blank_cell` (554-576) sets attr 0 for every persona except PCANSI, where only BLINK, and BOLD+INVERSE when inverse, carry. No bold leak in xterm or Amiga persona.
- DECSC/DECRC and 1048/1049 (2290-2321, 3050-3062): save and restore attr, as xterm. Alt screen switch does not reset the pen (same as xterm).
- Amiga renderer soft style: render/amiga_render.c:1740 sets `SetSoftStyle(rp, style_of(attr) & ~FSF_BOLD, BOLD|UNDERLINED|ITALIC)` on every text run, so a stale bold soft style cannot persist between runs; bold is drawn by a manual 1 px re-Text (1743-1747). Other bold sites: 1485 and 2289 (private rastport trp, set per call), 1986 and 2382 (template glyph paths, keyed on attr). Line 3612 resets the soft style at frame end.
- terminfo (terminfo/vtcon.terminfo:28,83-84): bold=\E[1m, sgr p6 -> ;1, sgr0=\E(B\E[m. setaf for 8-15 emits 9x, not bold. No bold set wrongly.
- upterm-ports 7600cda (tparm memo keyed by post-%i values) only corrupted `cup`-style %i formats; `sgr` has no %i, so it cannot have produced a wrong bold string. Very low.

## Ranked plausible causes

1. Bold-as-bright colour shift plus bold font on every SGR 1 (confidence: medium, about 40%). engine/vtengine.c:7538: `t->bold_bright && BOLD && f < 8` shifts colours 0-7 to 8-15; default is ON (5843) and the renderer also smears the glyph. A program that sets bold for "emphasis" together with a 256-colour index 1-7 (38;5;1..7) or palette colour 0-7 looks brighter AND bolder than the owner expects. Toggle: handler/vtcon_handler.c:1246 (conf key), menu MENU_SET_BOLD (2265). Confirm: ask owner to turn "bold as bright" off; if the complaint is about colour not weight it goes away. This is xterm-compatible behaviour, so a "fix" is a default change, owner's call.
2. A program really sends SGR 1 and never SGR 22 / 0 (confidence: medium, about 25%). ncurses/terminfo `bold` has no matching off string in vtcon.terminfo except sgr0 (`\E[m`); programs that use `rmso`/`rmul` (27/24) to end a mode they started with `bold` leave bold on. Also CSI 21m is not "bold off" in this engine (2889) though some old programs (and PCANSI/ANSI.SYS-era code) use it; in the xterm persona it becomes double underline and bold stays on. Confirm: capture the byte stream (vtcon raw log / `script`) for the offending program and look for ESC[1m without a following ESC[22m or ESC[m.
3. Algorithmic bold smear on Amiga looks heavy for the font (confidence: low-medium, about 15%). render/amiga_render.c:1743-1747 draws the glyph run twice 1 px apart; on thin or already-bold bitmap fonts every bold cell looks very heavy, and bold applied to ALL text in a faint/dim or inverse path (1986: `v |= v >> 1` template path) widens glyphs. Confirm: compare same text under a font with a real bold, or with `\e[1m` vs `\e[22m` side by side.
4. Parameter overflow (confidence: low, about 5%). engine/vtengine.c:43 VT_MAX_PARAMS 16 with the `;` drop at 5663: a CSI with more than 16 params drops the extra separators and the following digits glue onto param 16 (5649-5651 saturates at VT_PARAM_MAX). An SGR chain like `0;...;1` past the limit could turn a `0;1` ... into one big number or lose the reset. Rare in practice; confirm by looking for SGR with more than 16 items in the stream.
5. Amiga default attributes (confidence: low, about 5%). `CSI SP s` (3836) copies the CURRENT attr, including BOLD, into amiga_dattr; every later SGR 0 then resets TO bold instead of plain (2332). Only matters if a program sends `ESC[1m` then `ESC[ s`. Confirm: grep the stream for `ESC[ s`.

## Not investigated

No host probe was run (no repro; engine tests not touched). Faint (SGR 2) rendering at engine/vtengine.c:7545 was not read; a faint-as-dim colour is not bold but could be misread.

## Question that pins it

Which program (ls, a Shell prompt, an ncurses app such as amarchy-menu, vim), on which persona (xterm vs Amiga CON:), and is the text brighter, thicker, or both?
