---
date: 2026-10-04
topic: UP-Term (vtcon xterm personality) vs modern Unix terminals -- gap list
tags: [research, conformance, xterm, terminfo, keyboard, mouse, unicode, gaps]
status: draft
---

# UP-Term vs modern Unix terminals: what is missing

Scope: the xterm personality (`VT_XTERM`) as a Unix terminal -- ssh/telnet, the local
vsh/ixemul pty path, tmux, vim/neovim, htop, less, fzf, git. Every status below comes from
the source: the parser and dispatch in `engine/vtengine.c`, the reply generators, the key
and mouse encoders, and the host side that feeds them (`render/vtwin.c`,
`handler/vtcon_handler.c`). Where a feature is absent I name the dispatch point that would
have handled it.

Reference terminals: xterm, kitty, WezTerm, foot, Ghostty, iTerm2, Alacritty, VTE (GNOME
Terminal). Their support column is the common case, from what they are known to ship (not
measured here).

**Status words.** *supported*: the engine handles it and the host delivers it. *partial*:
it works with a real limit, or the engine has it but the host never produces or uses it.
*missing*: no dispatch case (the sequence lands in `note_unhandled` / `note_value`, or is
swallowed as a string).

**VERIFIED** means a host test or a recorded stream asserts it: `tests/test_xterm.c`
(347 checks), `tests/test_keys.c` (63 checks), both green on 2026-10-04
(`make test ONLY=xterm`, `ONLY=keys`); the quirk streams in `tests/streams/` (diffed
against libvterm by `make test-ref`); program captures (`vim-*`, `tmux-*`, `top`,
`less-scroll`, `bash-readline`, `screen-*`, and their `ti-*` recaptures with TERM=vtcon,
`make test-terminfo`, which fails on any unhandled sequence); vttest menus 1, 2 and 8
(`vttest-m*-s*.bin`; ledger A6: 31 of 33 screens match on the rig). *impl.* means
implemented with no test found that asserts it.

Where the dispatch lives (for the citations):

| Piece | Location |
|---|---|
| Parser (Williams state machine) | `engine/vtengine.c:2758` `feed`, `:2971` `decode` |
| ESC dispatch | `engine/vtengine.c:1352` |
| CSI shared finals | `engine/vtengine.c:1803` `csi_common` |
| CSI xterm (private, intermediates) | `engine/vtengine.c:2092` `csi_xterm` |
| DEC modes | `engine/vtengine.c:1656` `set_mode`; DECRQM `:2037` `report_mode` |
| SGR | `engine/vtengine.c:1500` `sgr`, `:1469` `ext_colour` |
| OSC | `engine/vtengine.c:2462` `osc_dispatch` |
| DCS | `engine/vtengine.c:2715` `dcs_dispatch` (DECRQSS `:2623`, XTGETTCAP `:2663`) |
| Strings (OSC/DCS buffer) | `VT_STR_MAX 256`, `engine/vtengine.c:24`; APC/PM/SOS swallowed `:2747` |
| Key encoder | `engine/vtengine.c:4298` `vt_encode_key` |
| Mouse / paste encoders | `engine/vtengine.c:4472`, `:4536` |
| Host key/mouse/wheel/paste | `render/vtwin.c:979` `vtwin_key`, `:1092` `vtwin_mouse`, `:1144` `vtwin_wheel`, `:905` `paste` |
| Host IDCMP dispatch | `handler/vtcon_handler.c:3937` `dispatch` |

---

## 1. Escape sequences and modes

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| DA1 `CSI c` | all | supported, VERIFIED | `vtengine.c:2164` replies `CSI ?62;22c`; test `device_reports` | VT220 + ANSI colour. No `4` (sixel) and no `52`, consistent with the engine. |
| DA2 `CSI >c` | all | supported, VERIFIED | `vtengine.c:2152` `CSI >1;10;0c` | Says "VT220 firmware 10"; vim's xterm detection keys on DA2 (see gap list, unconfirmed). |
| DA3 `CSI =c` | xterm, VTE, foot, kitty | missing | `csi_xterm` sends any `=` private to `note_unhandled` (`vtengine.c:2157-2158`) | Rarely used; no harm (no reply is a valid answer). |
| XTVERSION `CSI >q` | xterm, kitty, foot, WezTerm, Ghostty, iTerm2 | supported, VERIFIED | `vtengine.c:2142-2144` `DCS >|vtcon 1.0 ST`; test `decrqss_decrqm_and_version` | tmux/neovim do not know the name, so no extra features are switched on from it. |
| DECRQM `CSI ?Ps$p` / `CSI Ps$p` | xterm, kitty, foot, WezTerm, Ghostty, VTE | supported, VERIFIED | `vtengine.c:2037-2088` | Answers 1/2 for every mode `set_mode` keeps except ?66 (DECNKM) and ?1048 (answer 0 though both are settable). Unknown modes answer 0, correct. |
| DECRQSS `DCS $q` | xterm, foot, kitty, WezTerm | partial, VERIFIED | `vtengine.c:2623-2660`: `m`, `r`, ` q`, `"p`, `"q` | No `s` (DECSLRM), `t`, `*|`, `"q` fine. SGR answer (`put_sgr` `:2564`) leaves out SGR 58 underline colour and SGR 10-20 fonts. `"p` answers `64;1"p` (VT420 level) while DA1 says VT220 and the VT420 features (DECSLRM, rectangles, DECIC/DECDC) are absent: an inconsistent claim. |
| XTGETTCAP `DCS +q` | xterm, kitty, foot, WezTerm, Ghostty | partial, VERIFIED | `vtengine.c:2665-2668`: only `TN`, `name`, `Co`, `colors`, `RGB` | Every other name answers `DCS 0+r`, although vtcon.terminfo carries ~150 capabilities (keys, Smulx, Setulc, Ss/Se...). Programs that probe instead of reading terminfo (kakoune, vim's xterm path, neovim over ssh without the entry) learn little. |
| DSR `CSI 5n`, CPR `CSI 6n`, DECXCPR `CSI ?6n` | all | supported, VERIFIED (5n, 6n) | `vtengine.c:1939-1949`, `:2121`, `report_cursor :1787` | Origin-relative row honoured. |
| DSR `?996n` colour scheme + `?2031` | kitty, foot, Ghostty, WezTerm, VTE (recent) | supported, VERIFIED | `vtengine.c:2123`, `:2432` | test `scheme_updates_when_asked`. |
| Other DSRs (`?15n`, `?25n`, `?26n`, `?53n`) | xterm | missing | only 5/6/996 in `csi_common`/`csi_xterm` | Not used by the target programs. |
| DECSCUSR `CSI Ps SP q` | all | supported, VERIFIED | `vtengine.c:2105-2108`; drawn `render/amiga_render.c:1727` | 0-6; blink via `render/amiga_render.c:1453`. |
| DECSLRM `CSI l;r s` + DECLRMM `?69` | xterm, foot, WezTerm, kitty, Ghostty, VTE | missing | `?69` not in `set_mode` (falls to `note_value(t,'M',p)` `:1768`); `CSI s` is always SCOSC (`:2178`) | tmux uses margins only if the terminal says it has them; with DA1=62 it does not, so nothing breaks, scrolling a vertical split is just slower. |
| REP `CSI b` | all but Alacritty (has it now) | supported, VERIFIED | `vtengine.c:1911-1918`; test `repeat_last_character`, streams `quirk-rep-*` | Not advertised in terminfo (see 7). |
| DECSTBM `CSI t;b r` | all | supported, VERIFIED | `vtengine.c:2166-2176`; streams `quirk-region-*`, `quirk-decstbm-reset`, vttest | |
| DECOM `?6` | all | supported, VERIFIED | `set_mode` `:1683`; streams `quirk-origin-*`, test `origin_mode_positions_relative_to_region` | |
| IRM `CSI 4h`, LNM `CSI 20h` | all | supported, VERIFIED | `set_mode` `:1777-1785`; streams `quirk-irm-*` | |
| DECIC / DECDC `CSI Ps ' }` / `' ~` | xterm, foot, WezTerm, Ghostty | missing | `csi_xterm` sends intermediate `'` to `note_unhandled` (`:2114-2117`) | Rare in ncurses output. |
| DECFRA/DECERA/DECCRA/DECSERA/DECCARA/DECRARA (`$x $z $v ${ $r $t`) | xterm, foot, WezTerm, Ghostty | missing | only `$p` handled (`:2110`); other `$` finals to `note_unhandled` | ncurses/tmux do not need them (tmux's `Rect` is optional). |
| ECH, ICH, DCH, IL, DL, SU, SD, ED, EL, ED 3 | all | supported, VERIFIED | `csi_common :1803-1960`; many `quirk-*` streams; `E3` test `clear_scrollback_keeps_the_screen` | DECSED/DECSEL `CSI ?J/?K` treated as ED/EL (no protected cells). |
| Tab stops HTS / TBC / CHT / CBT | all | supported, VERIFIED | HTS `:1331`, TBC `:1922`, CHT `:1851`, CBT `:1908`; streams `quirk-tab-*`, `quirk-cht-cbt`, `quirk-cbt-at-col0` | |
| Title OSC 0 / 2 (and 1) | all | supported, VERIFIED | `osc_dispatch :2548-2560` (1 ignored at `:2497`); test `osc_title_ends_on_bel_and_st` | Title shown as Latin-1 (`render/vtwin.c:81-90`); capped at 255 bytes by `VT_STR_MAX`. |
| Title stack `CSI 22;0t` / `23;0t` | xterm, foot, kitty, WezTerm, VTE | supported, VERIFIED | `window_op :2003-2016`; test `window_reports_and_title_stack` | Depth 4. Not cleared by RIS (`vt_reset :3163` leaves `n_titles`). |
| XTWINOPS reports 14/16/18/19 t | xterm, foot, kitty, WezTerm, Ghostty | supported, VERIFIED | `window_op :1987-2000`; host gives cell size `render/vtwin.c:579,630` | 13 (position), 15 (screen px), 20/21 (icon/title report: deliberately absent in most terminals) and the manipulation ops (1-9) are not done (`note_unhandled :2017-2019`). |
| Alternate screen 47 / 1047 / 1048 / 1049 | all | supported, VERIFIED | `set_mode :1730-1750`; streams `quirk-alt-*`, test `alt_screen_1049_saves_and_restores` | Allocated on first use. |
| Synchronized output `?2026` | kitty, foot, WezTerm, Ghostty, iTerm2, Alacritty, (VTE no) | supported, VERIFIED (engine) | `set_mode :1718`, DECRQM `:2057`; renderer holds `render/vtwin.c:242`, max 3 frames `:31` | Hold is at most 3 frames (~60 ms at 50 Hz); others wait ~150 ms-1 s. Not advertised in terminfo (`Sync`). |
| Grapheme clustering `?2027` | foot, WezTerm, Ghostty, kitty (implicit), Contour | missing | not in `set_mode` (`note_value :1768`) | Cells hold one BMP code point (`vtengine.h:76`); see area 4. |
| Bracketed paste `?2004` | all | supported, VERIFIED | `set_mode :1707`; `vt_encode_paste :4536`; host `render/vtwin.c:905-937`; test `bracketed_paste_only_when_asked` | Paste is Latin-1 only (Amiga clipboard), at most 8 KB (`vtwin.c:907`); pasted ESC bytes are not filtered inside the brackets (kitty/foot/WezTerm sanitise). |
| Focus events `?1004` | all | missing (mode stored, never reported) | bit set at `set_mode :1707`; no encoder in the engine; host asks for `IDCMP_ACTIVEWINDOW` (`handler/vtcon_handler.c:2021`) but `dispatch` (`:3937-3976`) has no case for it | Advertised by terminfo (`kxIN`/`kxOUT`). vim/neovim `FocusGained` autocommands, tmux `focus-events`, autoread never fire. |
| Mouse X10 `?9`, normal `?1000` | all | supported, VERIFIED | `vt_encode_mouse :4472`; host clicks `render/vtwin.c:1092-1120`; test `mouse_reports_follow_the_modes` | Left and right buttons only (`vtwin.c:1104-1107`): no middle button. Modifiers always 0 (`vtwin.c:1111`); Shift reserved for local selection (as xterm). |
| Button-motion `?1002` / any-motion `?1003` | all | partial (engine only) | encoder handles `kind == 2` (`:4482`, test asserts); host never calls it: `vtwin_mouse` with `move` only extends the local selection (`vtwin.c:1100-1102`), and `ReportMouse` is on only during a local drag (`:1129`) | Dragging in vim (visual select), tmux pane-border resize, htop/fzf drag do nothing. |
| UTF-8 mouse `?1005` | xterm, most | supported, VERIFIED | `:4510-4522`; test `utf8_mouse_reaches_past_column_223` | |
| SGR mouse `?1006` | all | supported, VERIFIED | `:4500-4508` | |
| urxvt mouse `?1015` | xterm, kitty, WezTerm, VTE, foot | missing | not in `set_mode` | Legacy; programs prefer 1006. |
| SGR-pixel mouse `?1016` | xterm, kitty, foot, WezTerm, Ghostty | missing | not in `set_mode` | |
| Wheel reports (buttons 64/65) | all | partial -- BUG | `render/vtwin.c:1153` passes `mx, my` -- window **pixel** coordinates from `handler/vtcon_handler.c:4001` (`im->MouseX/Y`) -- straight to `vt_encode_mouse`, which expects cells; clicks convert with `vr_cell_at` (`vtwin.c:1099`, `render/amiga_render.c:1862`) | In SGR mode the report names a cell far right/below (tmux picks the wrong pane, vim may ignore it); in legacy mode any pixel > 222 makes the encoder return 0 (`:4525`) and the wheel scrolls UP-Term's own scrollback instead. Horizontal wheel (66/67) not produced. |
| Alternate scroll `?1007` | xterm, kitty, foot, WezTerm, VTE, Ghostty (default on in several) | missing | not in `set_mode`; `vtwin_wheel` with no mouse mode moves the local view (`vtwin.c:1158`) | `less`, `man`, `git log` on the alternate screen: the wheel scrolls the primary scrollback instead of the pager. |
| `?7727` application escape | xterm, mintty | supported, VERIFIED | `set_mode :1751`; encoder `:4387-4393`; test `meta_escape_and_modify_other_keys` | |
| DECCKM `?1` | all | supported, VERIFIED | `:1662`; test `xterm_cursor_keys_follow_decckm` | |
| DECKPAM / DECKPNM `ESC = / >`, DECNKM `?66` | all | supported, VERIFIED | `esc_dispatch :1440-1445`, `set_mode :1724`; test `keypad_follows_deckpam` | Host maps keypad raw codes only in DECKPAM (`vtwin.c:1044`). |
| modifyOtherKeys `CSI >4;Ps m`, query `CSI ?4m` | xterm, kitty, WezTerm, foot, Ghostty, iTerm2 | partial | engine: `:2146-2150`, encoder `:4304-4325`, VERIFIED; host: character keys reach the encoder with Alt only (`render/vtwin.c:1066`, "the keymap already applied Ctrl") | So Ctrl+1, Ctrl+Shift+A, Ctrl+; etc. are never encoded -- the keymap's byte (or nothing) goes out. Also Return/Tab/Backspace/Escape ignore modifiers (`:4356-4393`): no Ctrl+Enter, Shift+Enter, Ctrl+Tab. Level not reset by RIS (`vt_reset` leaves `mok`). |
| kitty keyboard protocol (`CSI >u`, `CSI <u`, `CSI =u`, query `CSI ?u`) | kitty, foot, WezTerm, Ghostty, Alacritty, iTerm2 | missing | `CSI ?u` goes to `note_unhandled` (`:2138`); `>u` to `:2154`; `<`/`=` to `:2157-2158` | Query unanswered, so neovim/helix/fish fall back cleanly (they send DA1 after the query). |
| fixterms / `CSI code;mod u` (formatOtherKeys=1) | kitty, foot, WezTerm, Ghostty, xterm (option) | missing | only the `CSI 27;m;c~` form exists (`:4314-4324`) | |
| DECSTR `CSI !p` | all | supported, VERIFIED | `:2101`, `soft_reset :1107`; test `soft_reset_restores_modes` | |
| RIS `ESC c` | all | supported (minor leaks), VERIFIED | `vt_reset :3163` | Does not reset modifyOtherKeys level, title stack, cursor style or charset (UTF-8/Latin-1 is a host setting). |
| DECSCNM `?5`, DECAWM `?7`, DECTCEM `?25`, DECARM `?8`, `?12`, `?45`, DECCOLM `?3` + `?40` | all (varies) | supported, VERIFIED | `set_mode :1667-1766`; tests `screen_reverse_video_inverts_every_cell`, `modes_of_phase_a5`, `deccolm_only_when_allowed` | `?1034` meta-8bit also there. |
| `?1036` / `?1039` metaSendsEscape / altSendsEscape | xterm | missing (default behaviour already ESC prefix) | not in `set_mode` | Harmless. |
| DECBKM `?67` (Backspace sends BS) | xterm, VTE (as option) | missing | not in `set_mode`; Backspace is fixed DEL (`:4370-4372`) | |
| In-band resize `?2048` | kitty, foot, Ghostty, WezTerm, neovim uses it | missing | not in `set_mode` | Local pty path has SIGWINCH via `ACTION_VTCON_SWINSZ` (`handler/pty_handler.c:704`); over ssh resizes depend on the client (bebbossh: unconfirmed, research 2026-10-03). |

## 2. SGR

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| 16 colours, 90-97 / 100-107 | all | supported, VERIFIED | `sgr :1544-1557, 1630-1648`; stream `quirk-sgr-bright` | Bold-as-bright for 0-7 is a host setting (`vt_resolve_colors :4140`). |
| 256 colours `38;5;n` / `38:5:n` | all | supported, VERIFIED | `ext_colour :1469-1476`; stream `quirk-sgr-256-and-rgb`; rig `cube_rig.py` | |
| 24-bit `38;2;r;g;b`, `38:2:r:g:b`, `38:2:cs:r:g:b` (and `38:2::r:g:b`) | all | supported, VERIFIED | `ext_colour :1477-1493` counts sub-parameters for the colour-space slot | Exact on RTG true-colour screens; nearest xterm-256 entry on palette screens (`render/amiga_render.c:124-132`). Test `rgb_colour_keeps_all_24_bits`. |
| Bold 1, faint 2 | all | supported / faint partial | `sgr :1563-1566`; `vt_resolve_colors :4148` | Faint only greys default/7/15 text; a coloured faint cell draws at full intensity. |
| Italic 3 / 23 | all | supported (algorithmic) | `:1567`, `:1607`; drawn with `FSF_ITALIC` `render/amiga_render.c:677` | |
| Underline 4 / 24, double 21 and 4:2, curly 4:3, dotted 4:4, dashed 4:5, 4:0 | kitty, foot, WezTerm, Ghostty, iTerm2, VTE (curly etc.); xterm double only | supported, VERIFIED | `sgr :1569-1580, 1594-1597, 1613-1615`; drawn `render/amiga_render.c:715-735`; test `underline_styles_and_colour` | 21 is double underline (ECMA/xterm), not "bold off". |
| Underline colour 58 / 59 (both forms) | kitty, foot, WezTerm, Ghostty, iTerm2, VTE | supported, VERIFIED | `:1623-1626`; `vt_cell_underline_color :1047`; renderer `amiga_render.c:1127` | |
| Blink 5, rapid blink 6, 25 | xterm, VTE, foot (5), kitty (5 since 0.28), WezTerm | supported | `:1581-1584`, `:1600`; renderer two rates `amiga_render.c:1132-1135, 1492` | |
| Inverse 7 / 27, conceal 8 / 28, strike 9 / 29 | all | supported, VERIFIED | `:1585-1606` | Stream `quirk-sgr-reset-forms`. |
| Overline 53 / 55 | kitty, foot, WezTerm, Ghostty, VTE, xterm (recent) | supported, VERIFIED | `:1617-1622`; test `overline_frames_scripts_ideograms_blink` | |
| Default colours 39 / 49 | all | supported | `:1636-1645` | |
| Resets 22-29 | all | supported, VERIFIED | `:1598-1606` | 22 clears bold and faint; 23 also clears Fraktur. |
| Extras: fonts 10-20, framed/encircled 51/52/54, ideogram 60-65, super/subscript 73-75 | mintty, some | supported (beyond most) | `:1608-1629` | |

## 3. OSC

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| OSC 4 set / query, OSC 104 reset | all but Alacritty query (has it now) | supported, VERIFIED | `osc_dispatch :2499-2524, 2472-2495`; test `colour_queries_and_changes` | Reply mirrors BEL/ST. |
| OSC 10 / 11 / 12 set / query, 110-112 reset | all | supported, VERIFIED | `:2526-2546`; host applies via the colours callback (`render/vtwin.c:118-122`) | OSC 11 query is how vim/neovim pick `background`: works. |
| OSC 5 / 105 special colours, OSC 13-19 (pointer, highlight / selection colours) | xterm, some | missing | fall to `note_value(t,'O',cmd)` `:2548-2551` | |
| OSC 7 working directory | VTE, kitty, foot, WezTerm, Ghostty, iTerm2 | missing | `:2548` | New tab cannot open in the shell's directory; no cwd in the title. |
| OSC 8 hyperlinks | VTE, kitty, foot, WezTerm, Ghostty, iTerm2, (xterm no) | missing (text shows, link dropped) | `:2548` swallows it cleanly | `ls --hyperlink`, `gcc`, `systemd`, `delta` print links: text is fine, no click. URIs > 255 bytes are truncated in the buffer but the terminator is still found (no garbage). |
| OSC 9 / 777 / 99 notifications | iTerm2, kitty (99, 9), foot (777), WezTerm, Ghostty | missing | `:2548` | Could map to a requester / screen flash; low value. |
| OSC 52 clipboard set / query | xterm (opt.), kitty, foot, WezTerm, Ghostty, iTerm2, Alacritty | missing (by decision) | `:2548`; terminfo comment "no Ms ... on purpose" | tmux `set-clipboard`, neovim `"+y` over ssh, vim OSC 52 plugins do nothing. Payloads would also overflow `VT_STR_MAX 256` (`vtengine.c:24`): a set larger than ~190 bytes of text cannot be held even if handled. |
| OSC 133 semantic prompts | kitty, WezTerm, foot, Ghostty, iTerm2, VTE (partial) | missing | `:2548` | Jump-to-prompt / select-output unavailable. |
| OSC 1337 (iTerm2: images, SetUserVar, CurrentDir...) | iTerm2, WezTerm, (kitty no) | missing | `:2548`; payload capped at 256 | |
| OSC 22 pointer shape | xterm, kitty, foot, Ghostty | missing | `:2548` | |
| OSC 50 font, OSC 3 X property | xterm | missing | `:2548` | Not needed. |

## 4. Text and Unicode

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| UTF-8 decode (incl. 4-byte) | all | supported, VERIFIED | `decode :3018-3042` | 4-byte sequences decode, then become U+FFFD (next row). Test `utf8_decodes_to_cells`. |
| Code points beyond U+FFFF | all | missing | `put_char :1174` replaces with U+FFFD; `vt_cell.ch` is `vt_u16` (`vtengine.h:76`) | Every emoji, CJK Ext B, math alphanumerics, Nerd Font v3 icons in plane 15 show as one replacement cell. |
| wcwidth / East Asian wide | all (Unicode 15/16 tables) | partial, VERIFIED (BMP) | `engine/vtwidth.h:12-31`: Markus Kuhn ranges, BMP only | Width disagreements with glibc/musl/macOS on the remote side shift the cursor: BMP emoji-presentation characters (U+231A, U+23E9-23EC, U+25FD, U+2614, U+26A1, U+26BD, U+2705, U+274C, U+2B50 ...) are 2 there and 1 here; all non-BMP wide characters are 2 there and 1 (U+FFFD) here. Missing zero-width ranges (e.g. Thai U+0E47-0E4E, Devanagari/Indic marks, Hangul jungseong U+1160-11FF). Tests `wide_glyphs_take_two_cells`, `quirk-wide-*`. |
| Emoji (presentation selectors VS15/VS16, ZWJ sequences, skin tones, flags) | kitty, WezTerm, foot, Ghostty, iTerm2 (full); xterm/VTE partial | missing | non-BMP -> U+FFFD (`:1174`); U+FE0F and U+200D are width 0 and dropped (`:1178`) | |
| Combining marks | all (composed in the cell) | partial | `put_char :1178-1179`: width-0 code points are discarded | Alignment stays right (program counts 0, engine adds 0), but the mark is lost on screen and in copy (`vt_copy_text`): decomposed "e + U+0301" shows "e". Ledger U3 open. |
| Grapheme clusters / `?2027` | foot, WezTerm, Ghostty, kitty, Contour | missing | one code point per cell | |
| Glyph coverage | system font + fallback chain | partial | `render/glyphmap.h:1-32` (Latin-1 or CP437 bitmap font; box/block/DEC lines drawn); one outline-font fallback `render/outline.h` | Anything else is `?`. Ledger U2 (Unifont pages) open. |
| DEC special graphics `ESC ( 0`, UK `A`, SO/SI, G0-G3 designation | all | supported, VERIFIED | `esc_dispatch :1356-1364`, `put_char :1170-1173`; streams `quirk-dec-graphics-box`, `quirk-so-si-g1`; test `dec_graphics_draw_boxes` | Only `0`, `A`, `B` are known; any other final (DEC Supplemental `<`, Technical `>`, NRCS) is silently ASCII (`:1363`). |
| SS2 / SS3 | xterm, VTE | supported | `exec_c1 :1339-1344`, ESC N/O `:1430-1435` | |
| LS2 / LS3 / LS1R-LS3R (`ESC n o ~ } |`) | xterm | missing | not in `esc_dispatch` (`:1447` `note_unhandled`) | Irrelevant under UTF-8. |
| `ESC % G` / `ESC % @` | xterm | supported, VERIFIED | `:1365-1374`; test `modes_of_phase_a5` | |
| DECDHL / DECDWL / DECSWL `ESC # 3-6`, DECALN | xterm, VTE (dwl), foot no, kitty no | supported, VERIFIED | `:1375-1401`; renderer `amiga_render.c:1155-1209, 1331`; test `double_width_lines_hold_half_the_columns`, vttest m8 by eye | Better than most modern terminals. |
| BiDi | VTE (opt-in), iTerm2, mlterm | missing | none | Expected; most terminals lack it. |

## 5. Graphics

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| Sixel | xterm (opt.), foot, WezTerm, iTerm2, mlterm, VTE (0.76+), Konsole | missing | `dcs_dispatch :2715-2723` handles only `$q`, `+q`; DCS payload capped at 256 bytes (`feed :2786-2798`) and then dropped | Realistic on RTG and AGA: sixel is palette-based and maps onto pens; costs a streaming DCS path (no 256-byte buffer) and per-cell image storage. The most realistic of the three. |
| kitty graphics (APC `_G`) | kitty, Ghostty, WezTerm, Konsole (partial) | missing | APC swallowed as a string (`enter_string :2738`, `end_string :2754`) | Needs base64 + zlib/PNG decode, image placement and z-order: heavy for a 68020, realistic only on accelerated machines; low value. |
| iTerm2 inline images (OSC 1337 File=) | iTerm2, WezTerm, mintty | missing | `osc_dispatch :2548`; OSC buffer 256 bytes | Realistic via datatypes.library (decodes PNG/JPEG/GIF on the Amiga) on RTG; needs a streaming OSC path. |

## 6. Behaviour, UI and keyboard

| Feature | Modern terminals | UP-Term | Evidence | Notes |
|---|---|---|---|---|
| Reflow on resize | kitty, WezTerm, foot, Ghostty, iTerm2, Alacritty, VTE (xterm no) | partial | engine `vt_set_reflow` (`vtengine.c:3346`, tests `reflow_*`); only `device/upcon_unit.c` turns it on -- XCON/UP-Term windows never call it | Ledger W1 open (default on, plus scrollback reflow). |
| Scrollback | all | supported | `vt_set_scrollback :3113`; default 500, profile `scrollback =` (`handler/vtcon_handler.c:880-884`); test `scrollback_size_changes_live` | 16 B/cell, so big scrollbacks cost real RAM. Not reflowed. |
| Scrollback search | kitty, WezTerm, foot, Ghostty, iTerm2, VTE | partial | `vt_find :4064` (ASCII case-fold substring, crosses wraps), Right Amiga F (`vtcon_handler.c:3941-3947`) | No regex, no highlight-all; tested `find_*`. |
| Selection: stream drag | all | supported | `render/vtwin.c:1121-1140` | |
| Selection: double-click word / triple-click line | all | missing | `vtwin_mouse` has no click counting (`vtwin.c:1092-1141`) | |
| Rectangular selection | xterm, kitty, WezTerm, foot, VTE, iTerm2 | missing | `vr_select` is linear | |
| Copy / paste | all | partial | copy `vtwin.c:874-902` (UTF-8 -> Latin-1, others `?`, 16 KB); paste `:905-937` (Latin-1, 8 KB); Right Amiga C/V, copy-on-select option | No middle-click paste, no primary selection; non-Latin-1 text cannot round-trip through the clipboard. |
| URL detection / open | kitty, WezTerm, foot, Ghostty, iTerm2, VTE | missing | nothing in vtwin/handler | |
| Ligatures | kitty, WezTerm, Ghostty, iTerm2 | missing | bitmap fonts | Not realistic, not needed. |
| Font fallback | all (chains) | partial | one outline font (`render/outline.h`) | |
| True colour on palette screens | n/a (they are all true colour) | supported (quantised) | `render/amiga_render.c:124-132` nearest xterm-256 entry; exact on RTG | |
| Bell | all | supported | `render/vtwin.c:56-62`: none / `DisplayBeep` / visual flash | No urgency hint (no taskbar); could bring the screen to front. |
| Tabs | kitty, WezTerm, iTerm2, VTE, Ghostty | supported | menu `handler/vtcon_handler.c:1129-1132`, `tab_route` | |
| Splits | kitty, WezTerm, iTerm2, Ghostty, Terminator | missing | none | tmux covers it. |
| Session restore | iTerm2, WezTerm, kitty (opt.) | missing | none found (grep for session/restore in handler) | Not confirmed beyond that grep. |
| Alt/Meta as ESC prefix | all | supported, VERIFIED | `vt_encode_key :4342-4343`; Left Amiga is Meta, `meta_alt` makes Alt Meta (`render/vtwin.c:1033-1041`) | The Amiga's "Option as Meta". `?1034` 8-bit meta also. |
| Meta with Backspace / Return / Tab / Escape | all (`ESC DEL`, `ESC CR`, ...) | missing | `vt_encode_key :4356-4393` ignores mods for these | Alt+Backspace (readline/zsh/fish backward-kill-word) sends a plain DEL. Ctrl+Backspace is also DEL (xterm sends BS). |
| F1-F12 with modifiers | all | supported, VERIFIED | `:4450-4468`; test `xterm_function_and_editing_keys` | Classic Amiga keyboards have F1-F10 only; F11/F12 arrive from PC-style keyboards (raw 0x4B/0x6F, `vtwin.c:836-838`). |
| F13-F24 | xterm, kitty, foot, WezTerm | partial | no `VT_KEY_F13+` in `vtengine.h` | Only Shift+F1-F12 gives terminfo's kf13-kf24. |
| Cursor keys, Home/End/Insert/Delete/PgUp/PgDn with modifiers | all | supported, VERIFIED (engine) | `:4424-4449`; tests `xterm_cursor_keys_follow_decckm`, `xterm_function_and_editing_keys` | Shift+PgUp/PgDn are always taken for the scrollback (`console_key`, `vtwin.c:952-977`), even on the alternate screen, so terminfo's kPRV/kNXT never reach programs. Home/End/PgUp/PgDn need a keyboard that has them (raw 0x48/0x49/0x70/0x71). |
| Backspace / Delete convention | all (DEL / `CSI 3~`) | supported, VERIFIED | `:4370-4372`, `:4438-4443` | matches `kbs=^?`, `kdch1`. |
| Keypad (numeric / application) | all | supported, VERIFIED | `:4398-4409`; host `vtwin.c:849-869` | No modifier encoding on keypad keys. |
| Mouse middle button, mouse modifiers | all | missing | `vtwin.c:1102-1105`, `:1111` | |

## 7. terminfo (`terminfo/vtcon.terminfo`) vs the engine

Advertised and right: `am bce ccc km mir msgr npc xenl AX RGB XT`, colors#256, acsc (DEC
graphics), cup/csr/ich/dch/il/dl/ech/indn/rin/hpa/vpa/E3, `flash` (?5), sgr/sgr0, `dim`,
`sitm/ritm`, `smxx/rmxx`, `Smol/Rmol`, `Smulx`, `Setulc`, `Ss`/`Se`, `setrgbf/setrgbb`,
`initc`/`oc`, `tsl/fsl/dsl` (+ `hs`), `smcup/rmcup` (1049), `smkx/rmkx`, `BE/BD/PS/PE`,
`XM`/`xm`/`kmous` (1006), `u6-u9` (CPR, DA1 pattern matches `CSI ?62;22c`), all key
strings `kcuu1..kf24`, `kDC kEND kHOM kIC kLFT kRIT kind kri khlp kent kcbt`.

Mismatches, terminfo claims what the engine/host does not deliver:

| Capability | Problem | Evidence |
|---|---|---|
| `kxIN` / `kxOUT` (focus in/out keys) | No focus report is ever sent. | Section 1, `?1004` row |
| `kPRV` / `kNXT` (Shift+PgUp/PgDn) | The console takes these keys for the scrollback. | `render/vtwin.c:952-977` |
| `km` (has meta) | True only with Left Amiga (or Alt with `meta_alt`); Meta+Backspace/Return send no ESC. | `vt_encode_key :4356-4393` |
| `XM` mouse with motion (`%p1` = 1 sets ?1000 only) | fine; but if a program sets ?1002/?1003 itself (vim `ttymouse=sgr` + drag, tmux) no motion arrives. | Section 1 |

Engine supports, terminfo does not advertise:

| Capability | Engine | Effect of the omission |
|---|---|---|
| `rep` | `CSI b` `:1911` | ncurses repeats characters by hand (more bytes over a 9600-115200 line). |
| `Sync` | `?2026` `:1718` | tmux and neovim do not wrap redraws; tearing on a slow 68k redraw. |
| `Cs` / `Cr` (cursor colour) | OSC 12 / 112 `:2526`, `:2472` | tmux/neovim cannot set the cursor colour. |
| `Ms` | not supported (consistent) | -- |
| `Tc` | `RGB` boolean present, `Tc` absent | tmux >= 3.2 and neovim read `RGB`; older tmux needs `Tc` or `terminal-overrides`. Unconfirmed which tmux the users run. |
| Modified-key extended caps (`kUP3..kUP7`, `kLFT5`, `kRIT5`, `kDC5`, `kHOM5`, `kEND5`, `kNXT5`, `kPRV5`, ...) | engine emits `CSI 1;5D` etc. (`xterm_cursor :4202`) | ncurses programs that read terminfo (not vim/neovim, which know xterm's scheme) cannot name Ctrl/Alt+arrows. |
| `fe`/`fd` or tmux `Enfcs`/`Dsfcs`, `Enbp`/`Dsbp` | `?1004` mode bit / `?2004` | `?2004` works through `BE/BD`; the tmux-specific names are unconfirmed (tmux may use built-in strings). |
| `Smulx` style values 4/5 | engine has dotted / dashed | fine (Smulx is parametric). |
| `E3` | present, works | -- |

Outside terminfo itself: **TERM over ssh.** vsh exports `TERM=vtcon` (`shell/vsh.c:1257-1260`).
No remote host has a `vtcon` entry (it is not in ncurses' database), so `ssh host` with
TERM=vtcon breaks vim/tmux/htop/less there ("unknown terminal type"). bebbossh sends
`xterm-amiga` unless a local TERM is set; with `xterm-256color` the measured session was
clean (`thoughts/shared/research/2026-10-03_ssh-for-up-term.md:33-37`). xterm-256color is
the right remote TERM; everything it asks for the engine handles except focus events and
OSC 52 (xterm-256color does not include Ms either).

---

## Prioritised gap list (by what breaks or degrades in real programs today)

1. **Mouse wheel reports carry pixel coordinates** (bug). `render/vtwin.c:1153` -> convert
   with `vr_cell_at` like clicks do; one-line fix plus a host test on the cell math.
   Breaks wheel in tmux (wrong pane / falls back to local scrollback), neovim, htop, fzf.
2. **No emoji / non-BMP characters, and width tables older than the remote's.**
   `vtengine.h:76` (`vt_u16 ch`), `vtengine.c:1174`, `vtwidth.h`. Remote glibc says 2, the
   engine says 1: cursor drift in zsh/fish prompts with icons, fzf lists, git log
   graphs with emoji, tmux status lines, neovim. Takes: widen `ch` to 21 bits (cell is
   16 B; `pad` byte + `ch` rearranged), Unicode 15 width tables generated from
   EastAsianWidth/emoji-data (shared with tmux-amiga via `vtwidth.h`), glyph source (U2).
   Ledger U1/U2.
3. **Button-motion / any-motion mouse never reported** (?1002/?1003). Host:
   `vtwin_mouse` must encode `kind 2` while a reporting mode is on, and keep
   `ReportMouse` on in those modes. Breaks vim visual drag, tmux pane resize/selection,
   htop/fzf drag.
4. **TERM=vtcon leaks to remote hosts.** vsh / kit should set `xterm-256color` for ssh
   (or ship `vtcon` with an `infocmp | ssh host tic -x -` helper). Breaks every
   full-screen program over ssh.
5. **Focus events (?1004) never sent, though terminfo advertises them.** Handle
   `IDCMP_ACTIVEWINDOW/INACTIVEWINDOW` in `dispatch` (`handler/vtcon_handler.c:3937`) and
   add `vt_encode_focus` in the engine. Degrades vim/neovim autoread/FocusGained, tmux
   `focus-events`.
6. **Meta/Alt with Backspace, Return, Tab, Escape send no ESC prefix; Ctrl/Shift with
   Return/Tab not encoded.** `vt_encode_key :4356-4393`. Breaks Alt+Backspace (kill word)
   in bash/zsh/fish, Alt+Enter in fish/neovim mappings, Shift+Enter/Ctrl+Enter in modern
   TUIs.
7. **modifyOtherKeys unreachable for Ctrl combinations** (host passes Alt only for
   character keys, `render/vtwin.c:1066`). Needs the raw key + qualifier to reach the
   encoder when `mok` is on (decode the unmodified keymap character, pass Ctrl/Shift).
   neovim/vim mappings like `<C-;>`, `<C-S-x>`, Ctrl+digits do nothing.
8. **Alternate scroll (?1007) / wheel on the alternate screen.** With no mouse mode, the
   wheel on the alternate screen should send cursor up/down (as kitty/foot/VTE do by
   default). Degrades less, man, git log/diff, systemd pagers.
9. **OSC 52 clipboard** (decided out). Needs a streaming OSC path (the 256-byte
   `VT_STR_MAX` cannot hold a payload), base64 decode to the Amiga clipboard, and an
   opt-in setting. Degrades tmux copy-mode to system clipboard, neovim over ssh.
10. **Clipboard and paste are Latin-1, 8/16 KB.** UTF-8 text cannot round-trip; larger
    pastes are cut. Takes a UTF-8 (CHRS with charset) clipboard path or a private
    UTF-8 buffer for in-window copy/paste; streaming paste.
11. **Reflow off in UP-Term windows** (ledger W1). Takes the profile switch and scrollback
    reflow. Degrades every shell session after a resize.
12. **terminfo omissions: `Sync`, `rep`, `Cs`/`Cr`, Ctrl/Alt arrow caps; `kPRV/kNXT`
    unreachable.** Edit the file (each with its test in `make test-terminfo`), and pass
    Shift+PgUp/PgDn through on the alternate screen.
13. **Combining marks discarded.** Draw over the previous cell (ledger U3); needs a
    per-cell combining slot (or the style table trick `ext` uses). Degrades
    decomposed text (macOS file names over ssh, Vietnamese, IPA).
14. **Selection: no word/line double/triple click, no rectangular, no middle-click
    paste, no URL open.** Host-only work in `vtwin_mouse`.
15. **XTGETTCAP answers 5 names.** Serve it from the same table the terminfo is built
    from (one source of truth). Helps neovim/kakoune/vim probing over ssh when TERM
    lacks an entry.
16. **Faint only for default/grey colours** (`vt_resolve_colors :4148`). Dim the resolved
    RGB (true colour) or pick the darker palette entry. Degrades git/zsh-autosuggestions
    and neovim's dimmed text in colour.
17. **DECRQSS `"p` claims VT420 while VT420 features are absent; DECRQM ?66/?1048 answer 0.**
    Answer `62;1"p` (or implement DECSLRM/rectangles). Low impact.
18. **kitty keyboard protocol / CSI u.** Unanswered query is a clean fallback, so this only
    limits key disambiguation in neovim/helix/fish; large work (press/release, all keys).
19. **OSC 7 / 8 / 133 / notifications.** Swallowed cleanly; new tabs do not follow cwd, links
    are not clickable, no prompt jumps.
20. **Sixel / iTerm2 images / kitty graphics.** Missing; sixel is the realistic one on
    AGA/RTG (palette native), iTerm2 via datatypes.library on RTG, kitty graphics not worth
    it on a 68k.
21. **DECSLRM, DECIC/DECDC, rectangles, LS2/LS3, other charsets, DA3, ?1015/?1016, ?2048,
    F13+ keys, RIS leaving mok/title stack/cursor style.** Each small; none blocks the
    target programs.

## Not confirmed (looked, could not settle from the source)

- vim's mouse type with TERM=vtcon: vim's `ttymouse` detection uses the TERM name and
  the DA2 answer (`>1;10;0c` says VT220); whether vim picks `sgr` automatically was not
  checked (no vim source here). `set ttymouse=sgr` would force it.
- Which terminfo names tmux uses for focus/bracketed paste on the outer terminal
  (`Enfcs`/`Enbp` vs built-ins) -- not checked against tmux-amiga's source.
- Whether neovim's startup XTGETTCAP probes ask for more than `RGB` -- not checked.
- Window resize reaching a remote over bebbossh (it asks for Amiga raw event 12 with
  `CSI 2;11;12 {`; the engine routes 8-bit `$9B ... {` to the Amiga meaning,
  `amiga_collision :1959`, and the host reports event 12 on resize, `render/vtwin.c:805`;
  the 7-bit form would not work). Not run.
- Session restore: only a grep of handler/render for session/restore; nothing found.
- Programs not captured in `tests/streams`: htop, fzf, neovim, git's own output. Their rows
  above are inferred from the sequences they are known to send, not from a capture.
