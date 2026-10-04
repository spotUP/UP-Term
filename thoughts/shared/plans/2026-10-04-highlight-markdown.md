---
date: 2026-10-04
topic: Syntax highlighting (hl) and Markdown rendering (mdv) as UP-Term commands
tags: [vtcon, tools, highlight, markdown, view]
status: implemented
---

# hl and mdv: coloured sources and rendered Markdown

Owner 2026-10-04: "we dont have any syntax highlighting to speak of for common sources etc etc
and we dont support markdown send agents to fix the gaps". On Unix: bat / highlight /
pygmentize (coloured cat, also in a pager) and glow / mdcat (Markdown in the terminal).

## What exists (searched before building)

- shell/, handler/, tools/, dist/, the vtcon ledger: no highlighter, no Markdown, no pager.
  The only colouring is lineedit's green/red command word (handler/lineedit.h le_set_command),
  a different problem (one word, known commands).
- `less` is not in the kit: the GG-built less runs on the rig (ledger V2) but a user has it
  only from Geek Gadgets. vshrc can define a pager wrapper only when `which less` finds one.
- Width: engine/vtwidth.h (vt_char_width, header-only, "for programs that lay text out for
  it") -- reused for every width measurement.
- Window size: ACTION_VTCON_GWINSZ (handler/vtcon_packets.h, ttyprobe's DoPkt pattern).
- Glyphs (render/glyphmap.c): U+2500-257F (box drawing) drawn natively, U+2022 bullet shows as
  the middle dot, U+25CB as 'o'; anything else outside Latin-1 is '?' without a fallback font.
  Tables, rules and quote bars use box drawing; bullets are U+2022 / U+25CB / '-'.
- OSC 8: the engine parses OSC strings and ignores 8 (osc_dispatch -> note_value), so the
  hyperlink is swallowed cleanly in the xterm and amiga dialects (pcansi prints OSC payloads,
  conformance matrix l.116). The URL is therefore also shown as text.
- Pattern: portable core + thin platform files (demo/updemo.c + updemo_amiga.c/_posix.c,
  zm/zmodem.c + zm_amiga.c). Commands ship in SYS:UP-Term/bin beside sz/rz (install.dos).

## Decisions

- D1 Commands `hl` (highlighter, bat-like) and `mdv` (Markdown viewer, glow-like); Unix-style
  flags (they live in SYS:UP-Term/bin with coreutils and run from vsh).
- D2 Code in `view/`: hl_lex.[ch] engine, hl_langs.c tables + detection, hl_style.[ch]
  themes/SGR/colour depth, vw_text.[ch] output buffer + UTF-8 width, md.[ch] renderer,
  hl_main.c / md_main.c CLIs, vw_plat.h + vw_plat_amiga.c / vw_plat_posix.c.
- D3 Lexer: line at a time with a carried state (block comment depth, multi-line string kind,
  Lua long-bracket level, markup/markdown/CSS context), no regex; keywords as space-separated
  strings hashed once per language (open addressing). Line-oriented modes for diff, ini/toml,
  markup (HTML/XML), Markdown source, 68k asm (label/mnemonic/operands).
- D4 Colours: themes map token classes to a style (fg, bold, italic, underline, dim); colours
  are ANSI 0-15, 256 indexes or #rrggbb. Built-in `ansi` (default, the 16 colours, so they
  follow the profile's palette), `mono`, `rich` (truecolour). Depth from COLORTERM
  (truecolor/24bit) and TERM (*256color, vtcon = 256); higher colours are downconverted.
  Theme files: `class = spec ...` lines (`--theme FILE`, env HL_THEME).
- D5 Console vs not: on a console hl colours and numbers lines; to a pipe or file it is cat
  (plain, no numbers) unless --color=always / -n.
- D6 UTF-8 output (box drawing) when TERM is vtcon/xterm*/screen*/tmux* or LANG says UTF-8;
  ASCII otherwise or with --ascii.
- D7 mdv reads the whole document (reference definitions are collected first); output is
  wrapped to the window width (GWINSZ, then ACTION_DISK_INFO's window, then COLUMNS, then 80).

## Checklist (19 of 20; R1 is the owner's)

- [x] H1 hl_lex engine: classes, state across lines, keyword hash, numbers, strings + escapes,
      line/block comments (nesting), preprocessor, $variables, function calls, modes -- d342f3c
- [x] H2 languages: C/C++, 68k asm (vasm/Devpac), AmigaE, Python, sh/vsh, AmigaDOS script,
      ARexx, Lua, JavaScript/TypeScript, JSON, YAML, TOML/INI/up-term.conf, Makefile,
      Markdown source, HTML/XML, diff/patch, CSS, Rust, Go, Java (22 rows) -- d342f3c
- [x] H3 detection: extension, file name (Makefile*, Startup-Sequence, up-term, .vshrc), #!
      (env, python3.11 -> python), first line (<?xml, .KEY, diff), -l, fence names -- d342f3c
- [x] H4 styles: themes ansi/mono/rich, theme file, depth 16/256/24, rgb -> 256 / 16 by hue
      -- d342f3c
- [x] H5 hl CLI: files/stdin, -n/-N/-p, --color, -l, --list, --theme, --colors, -T, ^C;
      cat fast path when neither colour nor numbers -- d342f3c
- [x] H6 host timing + 68020 estimate (below) -- measured 2026-10-04
- [x] M1 md blocks -- 62909ee
- [x] M2 md inlines -- 62909ee
- [x] M3 wrap and tables fitted to the width (md_fit_columns) -- 62909ee
- [x] M4 mdv CLI: -w, -L, -U, colour options; width through a pipe -- 62909ee, 74d21cb
- [x] T1 suite `hl` (1762 checks) in make test -- d342f3c
- [x] T2 suite `md` (130 checks) in make test -- 62909ee
- [x] A1 `make amiga` builds hl and mdv clean (full `make amiga`, exit 0) -- 958b048
- [x] I1 install.dos copies hl, mdv to SYS:UP-Term/bin; make dist; Uninstall needs nothing
      (Delete SYS:UP-Term ALL) -- 958b048
- [x] I2 vshrc: hlp / mdp through ${PAGER:-less -R}; `# alias cat='hl -p'`; tested through
      the real vshrc in suite sh_exec -- 958b048
- [x] I3 dist/README.txt VIEWING FILES; README.md table; RULES.md Commands -- 958b048
- [ ] R1 manual checks on the rig (owner; steps below)

## Decisions made while building

- Keyword lists are blank-separated strings, two per class (C89 caps a literal at 509 bytes),
  hashed once per language into an open-addressed table; a per-language byte table (CT_*)
  marks what any rule looks at, so plain runs and names go in one step.
- Tiny helpers that run per byte are macros (is_alpha, is_digit, is_hex, is_space); `at()`
  stays a function: its macro copies nested in big expressions made vbcc's optimizer give
  up (warning 172 is an error under -warnings-as-errors).
- vo_raw and vo_text copy bytes in a loop into the buffer (the ledger: vbcc's memmove moves
  bytes; a call per token is the cost on a 68020).
- The gutter's number counts up as decimal text (no divu.l per line).
- 16-colour downconversion by hue sector (nearest-by-distance turned purples grey).
- mdv: tight lists have no blank lines, loose ones do, a list opening after text gets one;
  the blank line before a block draws only the quote bars both blocks share.
- Headings H3-H6 show their #s when there are no colours (piped output stays readable).

## Timings (host: Apple M1 Pro, clang -O2; own work, output discarded)

Instructions retired per input byte (macOS `time -l`, 11 runs minus 1 run, over 10):

| input | per byte |
|-------|----------|
| hl, 100 KB of C (engine/vtengine.c), lexer alone | 43 |
| hl, same, colours | 63 |
| hl, same, colours + line numbers | 79 |
| hl, Makefile / Python / 68k asm, colours + numbers | 56 / 77 / 104 |
| mdv, 93 KB ledger (2026-09-28-vtcon.md) | 110 |
| mdv, README.md of this repo | 150 |

Wall clock: hl 6 ns a byte (100 KB in ~0.6 ms), mdv 9 ns a byte.
Before the byte table, cached SGR and the buffer copies the lexer alone was 120 a byte.

68020 estimate (not measured -- no emulator was run for this work): the engine's own
calibration in this ledger (2026-09-28-vtcon.md l.431-441) is 5-10 ns a byte on the Mac
against 24 us a byte on the stock A1200 rig, a factor of 2400-4800. Applied here:
hl on 100 KB of C with colours and numbers ~1.5-3.4 s CPU on a stock A1200, mdv on a
100 KB document ~2.2-4.3 s, a typical 10-20 KB README ~0.2-0.9 s; an 030/040 or fast RAM
is several times quicker. The console drawing the output (2.2x the input in bytes with
colours) comes on top. The upper end of the hl range does NOT meet "well under a few
seconds" on a stock A1200; R1 measures it. Next levers if it is short: fewer calls per
token (vbcc passes arguments on the stack and inlines nothing), the identifier hash fused
with the name scan.

## Manual checks (R1, the owner, on the rig or the A1200)

1. Install the kit (or copy build/amiga/hl and build/amiga/mdv to SYS:UP-Term/bin), open
   UP-Term, vsh.
2. `hl S:Startup-Sequence`: AmigaDOS colours, line numbers. `hl --list`: 22 languages.
3. `hl --color=always -n RAM:big.c >NIL:` with a 100 KB C file (e.g. engine/vtengine.c cut
   to 100 KB), timed (`date` before and after, or a stopwatch): the H6 estimate.
4. `mdv SYS:UP-Term/README.md`-like file (copy the repo's README.md over): headings,
   the table in box lines fitted to the window, links with addresses; resize the window
   and run again: the text and the table follow the width.
5. `hlp file.c` and `mdp README.md` with a less on the path: colours inside less.
6. A ROM CON: window (NewShell CON:...): `hl -p x.c` shows colours, `mdv` draws + - |.

## Found

- OSC 8: the engine swallows OSC 8 (osc_dispatch -> note_value), so links are not clickable
  in UP-Term; the URL is printed after the link text, so nothing is lost. In the PCANSI
  dialect OSC payloads print as text: mdv sends OSC 8 only with colours on and UTF-8 out;
  -L turns it off.
- less is not in the kit; hlp/mdp need one from Geek Gadgets or Aminet.
- There are no AmigaGuide docs in the project; documentation is dist/README.txt.
- Characters above U+FFFF (emoji in READMEs) are width 1 by engine/vtwidth.h, so tables
  line up in UP-Term (which draws them as one cell) and not in a Mac terminal.
- vsh's `which` answers "is a command" for any name (no lookup), so the vshrc cannot test
  for less; the functions are always defined.
- GNU make here compares whole-second mtimes: a sed and a rebuild in the same second kept
  the old binary once; `rm` the target when switching a change back and forth.
