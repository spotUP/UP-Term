---
date: 2026-09-28
topic: vtcon console conformance matrix (amiga / xterm / pcansi personalities)
tags: [vtcon, console.device, con-handler, xterm, ansi, conformance, spec]
status: final
---

# vtcon console conformance matrix

Every engine test is written against this file (RULES.md rule 2). It says what
each byte sequence does in each personality, and where that claim comes from.

- **amiga** reproduces the ROM console.device (V36-V47) as documented.
- **xterm** is what ixemul/libnix ports (bash, vim, less, mc) expect.
- **pcansi** is ANSI.SYS / BBS art as DCTelnet's `term-engine.c` renders it
  **today** (the column describes current behaviour, gaps included, so a test
  can pin it before anything is changed).

## 0. Sources and citation keys

All paths are absolute. "Node" = RKM/ADCD HTML node file.

| Key | File | What it is |
|-----|------|------------|
| RKM-80 .. RKM-99 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Devices_Manual_guide/node0080.html` .. `node0099.html` | RKM Devices, chapter 4 Console Device. 80 intro, 81 commands, 83/84 units+flags, 86 CSI note, 88 screen output, 8C ANSI table + SGR, 8D SGR notes + **Amiga private table**, 8E examples, 8F reading, 90 special key reports, 91 CPR, 92 window bounds report, 93 copy/paste, 94 raw event types, 95 input event reports + qualifiers, 96 CONU_LIBRARY, 98 caveats |
| AD20 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Includes_and_Autodocs_2._guide/node05B0.html` | Text_Autodocs/console.doc (V37): CMD_WRITE code tables, SGR list, OpenDevice units/flags, RawKeyConvert |
| AD31 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Includes_and_Autodocs_3._guide/node0102.html` (CMD_WRITE), `node0101.html` (CMD_READ), `node0103.html` (OpenDevice) | console.doc 3.1 (adds aSDSS V39, V39 notes) |
| NDK-DOC | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Autodocs/console.doc` | NDK 3.2 console.doc (identical in content to AD31 plus "It has changed with V44") |
| FD20 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Includes_and_Autodocs_2._guide/node057C.html`, `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/FD/console_lib.fd` | console library vectors |
| OFFS20 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Includes_and_Autodocs_2._guide/node0551.html` | References/Structure.offs: ConUnit offsets |
| H-CON | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/devices/console.h` | commands, SGR/DSR/CTC/TBC/mode constants |
| H-CU | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/devices/conunit.h` (+ `Include_I/devices/conunit.i`) | units, flags, struct ConUnit |
| H-IE | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/devices/inputevent.h` | IECLASS_*, IEQUALIFIER_*, struct InputEvent |
| H-KM | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/devices/keymap.h` -> `Include_H/libraries/keymap.h` | struct KeyMap, KCF_*, RAWKEY_* |
| H-IO | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/exec/io.h` | CMD_* values |
| H-DOS / H-DX | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/dos/dos.h`, `.../dos/dosextens.h` | InfoData, DOSTRUE, ACTION_* |
| H-RX | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Include_H/rexx/rexxio.h` | ACTION_STACK / ACTION_QUEUE |
| DOSDOC | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/Autodocs/dos.doc` | Open (l.3939), SetMode (l.5237), WaitForChar (l.6290) |
| RN-CH | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/ReleaseNotes/con-handler-RelNotes` | V47 con-handler changes (medium mode, UNDISK_INFO, ICONIFY, ...) |
| RN-CD | `/Users/spot/Code/dctelnet-petscii-recovered/.ndk/ReleaseNotes/console-RelNotes` | console.device 45-47 changes |
| AM-5D, AM-5F, AM-61, AM-62, AM-65, AM-5E, AM-7C, AM-2F, AM-30 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/AmigaMail_Vol2_guide/node005D.html` etc. | Amiga Mail: 5D "The 2.0 Con-handler", 5E packet spec summary, 5F basic I/O packets, 61 volume packets, 62 handler control, 65 **Console Only Packets**, 7C packet I/O under R2, 2F CLI/Shell notes, 30 Console notes |
| LIB-572, LIB-239 | `/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Libraries_Manual_guide/node0572.html`, `node0239.html` | RKM Libraries: Release 2 CON: changes; auto/close/wait example |
| DCT | `/Users/spot/Code/dctelnet-v2/src/third_party/retro32-term/term-engine.c` at dctelnet-v2 `8ab9e2e` | pcansi reference: header l.1-51, term_update_pens l.262, erase l.448/466, term_sgr l.503, term_ctl l.575, term_printable l.608, term_dsr l.639, term_csi l.679, term_feed l.818, term_reset l.927 |
| DCT-KBD | `/Users/spot/Code/dctelnet-v2/src/third_party/retro32-term/retro32term.c` l.259-290 | pcansi key output today |
| XT | xterm ctlseqs (XFree86/xterm "Xterm Control Sequences") | **(xterm ctlseqs, from knowledge)**: no local copy exists |

`/Users/spot/Code/amigadeveloperdocs/amigadev.elowar.com/` was checked: it holds
only `index.html`, `css/`, `img/` and an empty `read/` (a `.DS_Store`). There
is no local AmigaDOS Manual, so CON: details not covered by the files above
are marked **unverified**.

Notation: `CSI` = `ESC [` (1B 5B) or the single byte 9B. `SP` = 20. `Pn` a
decimal parameter (default 1 unless stated), `Ps` a selector (default 0), `Pm`
a list. Hex is given byte by byte.

### 0.1 Errata found in the sources (tests must follow the resolution column)

| # | Where | What it says | Resolution |
|---|-------|--------------|------------|
| E1 | RKM-8C table | `SHIFT IN 0E (undo SHIFT OUT)`, `SHIFT OUT 0F (set MSB)` | Labels swapped. AD20/AD31 give `00/14 SO` (0E) = shift G0->G1, `00/15 SI` (0F) = cancel. Follow the autodocs (and ASCII). |
| E2 | RKM-88 | "RETURN (0x13)" | Typo; the table in RKM-8C and AD20 give CR = 0D. |
| E3 | RKM-91 | Format `<CSI><row>;<column>R`, but example "cursor at column 40 and row 12" gives `9B 34 30 3B 31 32 52` (`40;12R`) | Format line and xterm agree on row;column. Example contradicts it. **Probe the ROM** (open question Q1). Until then: row;column. |
| E4 | RKM-8D table | WINDOW BOUNDS REPORT `9B 31 3B 31 3B <bot> 3B <right> 72` (no 20) | RKM-92 example `... 3B 36 30 20 72` and AD20 (` r`) include the SP. Final is `SP r` (20 72). |
| E5 | AD20/AD31 CMD_READ | raw key report `<CSI>1;0;<keycode>;<qualifiers>;0;0;<seconds>;<microseconds>q` | RKM-95 and the aIER row of the CMD_WRITE table give final `\|` (7C) and prev1/prev2 fields. Final is `\|`. |
| E6 | RKM-90 HTML | Shifted Left/Right rendered `<CSI>A` / `<CSI>@` "(notice the space after <CSI>)" | The HTML dropped the space (bytes checked with `od`). Values are `9B 20 41` / `9B 20 40`. |
| E7 | RKM-8C | IL, DL, ED, EL shown without a parameter | AD20 gives `#p = 1-` for all four (one optional parameter). ED/EL are "only to end of display/line". |

---

## 1. Frontmatter

See the YAML block at the top of this file.

---

## 2. C0 controls, ESC sequences, 8-bit C1

Byte ranges on the Amiga (AD20): `00-1F` C0, `20-7F` G0 (**DEL 7F is displayed
as a glyph**, not a control), `80-9F` C1 (ANSI 3.41), `A0-FF` G1 Latin-1.
RULES.md rule 3: input decoding is Latin-1 (amiga), UTF-8 (xterm), CP437 (pcansi).

### 2.1 C0

| Byte | Name | amiga | xterm (XT, from knowledge) | pcansi (DCT) | Source |
|------|------|-------|---------------------------|--------------|--------|
| 00-06, 10-17, 19, 1C-1F | - | Not listed; behaviour unverified (expected: ignored) | Ignored (05 ENQ may send answerback, default empty) | Ignored (`term_ctl` default) | AD20; DCT l.604 |
| 07 | BEL | Intuition `DisplayBeep()` (screen flash) | Bell (audible or visual); xterm makes the style a user resource: vtcon's xterm bell is per-profile `none` / `beep` / `visual` (a one-frame screen flash), default `beep` (plan 2026-10-01-terminal-preferences.md) | Ignored | RKM-8C, AD20; DCT l.604 |
| 08 | BS | Move left one column (behaviour at column 0 unverified) | Move left, stops at column 0 (reverse wrap only with `?45h`); cancels pending wrap | Left if x>0, never erases, clears pending wrap | RKM-8C, AD20; DCT l.577 |
| 09 | HT | Move right to next tab stop (`cu_TabStops`, set by HTS/CTC/TBC; default stops unverified, expected every 8) | Next tab stop (default every 8), stops at last column | Fixed stops every 8 columns, clamped to last column; HTS/CTC/TBC not implemented | RKM-8C; H-CU `MAXTABS 80`; DCT l.582 |
| 0A | LF | Down one line; with LNM set (`CSI 20h`) also CR. At bottom scrolls if ASM (`CSI >1h`, default on) | IND; with LNM (`CSI 20h`) also CR. Scrolls at bottom margin | Down one line, no CR; scrolls at bottom; clears pending wrap | RKM-88, RKM-8C, AD20; DCT l.564, l.591 |
| 0B | VT | **Up one text line** | Same as LF | Ignored | RKM-8C ("move up one text line"); DCT l.604. **COLLISION C-VT** |
| 0C | FF | **Clear the window** (equivalent of `CSI H CSI J`, i.e. home + clear); fills with the global background colour (V36) | Same as LF | Clear to current background and home (like the console it replaced) | RKM-8C, RKM-8E, RKM-8D; DCT l.595. **COLLISION C-FF** |
| 0D | CR | Column 1 | Column 1 | Column 0, clears pending wrap | RKM-8C, AD20; DCT l.601 |
| 0E | SO | **Shift G0 to G1**: every following 20-7F is displayed as the Latin-1 character with the MSB set (A0-FF) | LS1: invoke G1 into GL (G1 = whatever `ESC ) F` designated; default ASCII) | Ignored | AD20 (see E1); DCT l.604. **COLLISION C-SO** |
| 0F | SI | Cancel SO | LS0: invoke G0 into GL | Ignored | AD20 (E1) |
| 18 CAN, 1A SUB | - | Unverified | Abort the sequence in progress (SUB may show a glyph) | Executed mid-sequence as an ignored control; the sequence continues (does **not** abort) | DCT l.889 |
| 1B | ESC | Escape; `ESC [` = CSI | Escape | Escape; a second ESC restarts ESC state | RKM-8C, AD20; DCT l.846 |
| 7F | DEL | **Displayed as a glyph** | Ignored | Printed as a glyph (CP437 7F) | AD20; DCT l.828-836. **COLLISION C-DEL** |

Controls received **inside** a CSI sequence: pcansi executes them and keeps
parsing (ECMA-48, DCT l.889). xterm does the same (XT). Amiga: unverified.

### 2.2 ESC sequences (7-bit)

| Sequence | Hex | amiga | xterm (XT) | pcansi (DCT) | Source |
|----------|-----|-------|------------|--------------|--------|
| `ESC c` | 1B 63 | RIS: reset the console display; also resets aSDSS defaults and the V39 scrolled-plane count | RIS: full reset | `term_reset`: SGR 0, home, save slot to 0,0, cursor visible, clear with pen 0 | RKM-8C, AD20, AD31 (V39 notes); DCT l.851, l.927 |
| `ESC 7` / `ESC 8` | 1B 37 / 1B 38 | Not documented; behaviour unverified | DECSC / DECRC: save/restore cursor, SGR, charsets, wrap flag, origin mode | Save/restore **position only**, same slot as `CSI s`/`CSI u` | DCT l.853-859 |
| `ESC D` | 1B 44 | IND: down one line (scroll at bottom) | IND | Dropped | AD20 ("Code or Esc" column); DCT l.862 |
| `ESC E` | 1B 45 | NEL: first column of next line | NEL | Dropped | AD20 |
| `ESC H` | 1B 48 | HTS: set tab stop at cursor column | HTS | Dropped | AD20 |
| `ESC M` | 1B 4D | RI: up one line (scroll down at top, unverified) | RI: up; at top margin scroll down | Dropped | AD20 |
| `ESC [` | 1B 5B | CSI | CSI | CSI | RKM-86, AD20; DCT l.841 |
| `ESC ( 0` / `ESC ( B` (and `ESC ) F`) | 1B 28 30 / 1B 28 42 | Not documented; unverified | Designate G0 (G1): `0` DEC Special Graphics (line drawing), `B` US-ASCII. Needed by mc/ncurses `acsc` unless the terminfo uses UTF-8 box glyphs | Swallowed (intermediate state 2 up to the final) | DCT l.864 |
| `ESC =` / `ESC >` | 1B 3D / 1B 3E | Not documented | DECKPAM / DECKPNM (keypad application / numeric) | Dropped | DCT l.862 |
| `ESC ]` ... `BEL` or `ESC \` | 1B 5D ... | Not documented | OSC: `0;t`/`2;t` set title, `1;t` icon name, `4;n;rgb` palette, `10/11` colours, `52` clipboard. Terminated by BEL (07) or ST (`ESC \`) | **Not parsed**: `]` returns to plain state and the OSC payload is printed as text | DCT l.848-862 |
| `ESC P` / `ESC ^` / `ESC _` ... `ESC \` | DCS / PM / APC | Not documented | Parsed and consumed to ST (DECRQSS, XTGETTCAP ... in DCS) | Not parsed (payload printed) | DCT |
| `ESC SP F` / `ESC SP G` | 1B 20 46 / 47 | Not documented | S7C1T / S8C1T: 7-bit vs 8-bit C1 in replies | Swallowed | DCT l.855 |

### 2.3 8-bit C1 (80-9F)

| Byte | Name | amiga | xterm | pcansi | Source |
|------|------|-------|-------|--------|--------|
| 84 | IND | Down one line | Only as a C1 control in 8-bit mode; vtcon's xterm decodes UTF-8 first, so a raw 84 is a UTF-8 continuation byte (invalid alone -> U+FFFD) | CP437 glyph | RKM-8C, AD20 |
| 85 | NEL | Next line, column 1 | As above | CP437 glyph | RKM-8C, AD20 |
| 88 | HTS | Set tab at cursor | As above | CP437 glyph | RKM-8C, AD20 |
| 8D | RI | Up one line | As above | CP437 glyph | RKM-8C, AD20 |
| 9B | CSI | Control sequence introducer (output: 9B or `ESC [`; input: console sends 9B unless the user typed `ESC [`) | As above (xterm honours 8-bit C1 only outside UTF-8 mode, XT) | **CSI** (not the CP437 glyph U+00A2) | RKM-86, RKM-8C, AD20; DCT l.830, l.887. **COLLISION C-C1** |
| other 80-9F | - | "ANSI 3.41 control characters"; which others act is unverified (expected: ignored) | As above | CP437 glyphs | AD20 |

**COLLISION C-C1**: amiga treats 84/85/88/8D/9B as controls; xterm (UTF-8)
treats 80-BF as continuation bytes; pcansi treats all of 80-9F as CP437
glyphs **except 9B**, which DCTelnet takes as CSI although CP437 9B is a glyph
(U+00A2). A BBS screen containing a CP437 9B byte therefore misparses today.

---

## 3. CSI final bytes

Parser rules per personality:

- **amiga**: parameters are decimal, `;` separated, omitted = 1 for counts
  (RKM-8C). `>` may appear **inside** the list as a prefix of one parameter
  (`CSI 1;33;40;>0m`, the `>` item last, AD20 SGR notes) and as a leading
  marker (`CSI >1h`). `?` leads `?7h/l`. Intermediate `SP` precedes the final
  in the private `SP p/q/r/s/v` forms.
- **xterm (XT)**: leading private markers `? > = <`, intermediates 20-2F
  (`SP ! " # $ ' *`) before the final, `:` sub-parameters in SGR.
- **pcansi (DCT l.868-903)**: only a leading `?` is kept as a marker; with `?`
  only `?25h/l` acts (l.683). Any other marker (`> = <`), any intermediate
  (20-2F), or `:` sets `p_ignore`: the sequence is parsed to its final and does
  nothing. Parameters above 9999 saturate (`v < 1000` guard). At most 16
  parameters (`P_MAX`), extra ones overwrite the last slot. Count parameters
  use `pn1`: 0 and omitted both mean 1 (l.656). Unknown finals: ignored.

`COLLISION` marks rows where the same bytes mean different things in amiga and
xterm (the reason a window carries exactly one personality).

| Sequence (hex) | amiga | xterm (XT, from knowledge) | pcansi (DCT) | Source |
|----------------|-------|----------------------------|--------------|--------|
| `CSI Pn @` (9B [N] 40) | ICH: insert N spaces at cursor, rest of line shifts right | ICH | Insert N blank cells (current bg) | RKM-8C, AD20; DCT l.742 |
| `CSI Pn SP @` (9B [N] 20 40) | Output: not defined. **Input**: Shift+Right key report is `9B 20 40` | SL: scroll left N columns | Ignored (intermediate) | RKM-90 (E6). Input-side clash, see §5 |
| `CSI Pn A` (9B [N] 41) | CUU: up N | CUU (stops at top margin) | Up N, clamped | RKM-8C, AD20; DCT l.694 |
| `CSI Pn SP A` (9B [N] 20 41) | Output: not defined. **Input**: Shift+Left key report `9B 20 41` | SR: scroll right N columns | Ignored | RKM-90 (E6) |
| `CSI Pn B` (42) | CUD: down N | CUD | Down N, clamped | RKM-8C, AD20; DCT l.698 |
| `CSI Pn C` (43) | CUF: forward N | CUF | Right N, clamped | RKM-8C, RKM-8E, AD20; DCT l.702 |
| `CSI Pn D` (44) | CUB: backward N | CUB | Left N, clamped | RKM-8C, AD20; DCT l.706 |
| `CSI Pn E` (45) | CNL: down N, to column 1 | CNL | Column 0, down N | RKM-8C, AD20; DCT l.710 |
| `CSI Pn F` (46) | CPL: up N, to column 1 | CPL | Column 0, up N | RKM-8C, AD20; DCT l.715 |
| `CSI Pn G` (47) | **Not documented** (CHA is absent from RKM-8C and AD20); ROM behaviour unverified | CHA: column N | Column N | DCT l.720 |
| `CSI Pr ; Pc H` (9B [N] [3B M] 48) | CUP, 1-based row;column. `CSI H`, `CSI 1;1H`, `CSI ;1H`, `CSI 1;H` all home. The `;` must be present when row is omitted (`CSI ;4H` = row 1 col 4) | CUP (relative to scroll region when DECOM `?6h`) | CUP, clamped | RKM-8C, RKM-8E, AD20; DCT l.725 |
| `CSI Pn I` (49) | CHT: forward to the Nth tab stop | CHT | Ignored | RKM-8C, AD20 |
| `CSI Ps J` (9B 4A) | ED **only to end of display** (from cursor). What `1J`/`2J` do is unverified | ED 0 below, 1 above (incl. cursor), 2 all (cursor does **not** move), 3 scrollback; fill = current bg (BCE) | 0 to end, 1 to start incl. cursor cell, **2 and 3 clear all and home** (ANSI.SYS); fill = current bg | RKM-8C, AD20 (E7); DCT l.448. xterm/pcansi differ on 2J homing |
| `CSI ? Ps J` | Not defined | DECSED (selective erase) | Ignored (`?` accepted, only ?25 acts) | DCT l.683 |
| `CSI Ps K` (9B 4B) | EL **only to end of line**; 1K/2K unverified | EL 0 right, 1 left (incl. cursor), 2 whole line | 0/1/2 as xterm, fill = current bg | RKM-8C, AD20 (E7); DCT l.466 |
| `CSI Pn L` (4C) | IL: insert line(s) above the cursor line | IL (inside scroll region, cursor to column 1) | Insert N lines from cursor row down | RKM-8C, AD20 (E7); DCT l.736 |
| `CSI Pn M` (4D) | DL: remove line(s), lines below move up, blank bottom | DL | Delete N lines | RKM-8C, AD20 (E7); DCT l.739 |
| `CSI Pn P` (50) | DCH: delete the character under the cursor and N-1 to the right | DCH | Delete N cells | RKM-8C, AD20; DCT l.745 |
| `CSI Pr ; Pc R` (52) | CPR, **read stream only** (reply to DSR 6) | CPR (reply to DSR 6) | Not parsed on output; generated by `term_dsr` | RKM-91 (E3), AD20 |
| `CSI Pn S` (53) | SU: remove N lines at top, blank N at bottom. **Input**: Shift+Down key report `9B 53` | SU (in scroll region). `CSI ? Pi;Pa;Pv S` = XTSMGRAPHICS | Scroll up N | RKM-8C, RKM-90, AD20; DCT l.748 |
| `CSI Pn T` (54) | SD: remove N lines at bottom, blank N at top. **Input**: Shift+Up key report `9B 54` | SD with one parameter; with 5 parameters = start highlight mouse tracking; `CSI > Ps;Ps T` = XTRMTITLE | Scroll down N | RKM-8C, RKM-90, AD20; DCT l.751 |
| `CSI Ps W` (57) | CTC: 0 set tab at cursor, 2 clear tab at cursor, 5 clear all tabs (`CTC_HSETTAB 0`, `CTC_HCLRTAB 2`, `CTC_HCLRTABSALL 5`) | Bare CTC not in ctlseqs (from knowledge); `CSI ? 5 W` = DECST8C (reset stops to every 8) | Ignored | RKM-8C, AD20, H-CON |
| `CSI Pn X` (58) | Not documented | ECH: erase N cells from cursor, no shift | Ignored | - |
| `CSI Pn Z` (5A) | CBT: back to the Nth previous tab stop | CBT | Ignored | RKM-8C, AD20 |
| `` CSI Pn ` `` / `CSI Pn a` / `CSI Pn b` / `CSI Pn d` / `CSI Pn e` (60/61/62/64/65) | Not documented | HPA / HPR / REP (repeat previous glyph N times) / VPA / VPR | Ignored | - |
| `CSI Ps c`, `CSI > Ps c`, `CSI = Ps c` (63) | Not documented | DA1 / DA2 / DA3 (reply formats: open question Q8) | Ignored (the `>`/`=` forms set p_ignore) | - |
| `CSI Pr ; Pc f` (66) | HVP: same as CUP | HVP | Same as CUP | AD20; DCT l.724 |
| `CSI Ps g` (67) | TBC: 0 clear tab at cursor, 3 clear all (`TBC_HCLRTAB 0`, `TBC_HCLRTABSALL 3`) | TBC 0 / 3 | Ignored | AD20, H-CON |
| `CSI 20 h` / `CSI 20 l` (9B 32 30 68 / 6C) | LNM set: LF acts as CR+LF / reset: LF only. Default at console open: unverified (CON: behaviour suggests set) | LNM (mode 20) same meaning | Ignored | RKM-8C, AD20, H-CON `M_LNM 20` |
| `CSI 4 h` / `l` | Not documented | IRM insert/replace | Ignored | - |
| `CSI > 1 h` / `CSI > 1 l` (9B 3E 31 68 / 6C) | ASM auto scroll on (default) / off: when off, output at the bottom does not scroll (what happens instead: unverified) | Not defined as SM/RM (`CSI > Ps;Ps t/T` are title modes, `CSI > Ps p` XTSMPOINTER). Must be ignored | Ignored (p_ignore) | RKM-8D, AD20, H-CON `M_ASM ">1"` |
| `CSI ? 7 h` / `CSI ? 7 l` (9B 3F 37 68 / 6C) | AWM auto wrap on (default) / off | DECAWM same meaning (xterm wraps deferred: last-column flag) | **Ignored**: pcansi always wraps (deferred, DCT l.608) | RKM-8D, AD20, H-CON `M_AWM "?7"`; DCT l.683 |
| `CSI ? 25 h` / `l` | Not documented (cursor on/off is `CSI SP p`) | DECTCEM show / hide cursor | Show / hide cursor | DCT l.686 |
| `CSI ? Pm h` / `l`, other modes | Not documented | DECSET/DECRST: `?1` DECCKM, `?6` DECOM, `?12` blink, `?47`/`?1047`/`?1049` alternate screen (1049 also saves cursor), `?1000/1002/1003` mouse, `?1004` focus in/out, `?1006` SGR mouse, `?2004` bracketed paste | Ignored | DCT l.683 |
| `CSI Pm m` (6D) | SGR, see §4. `>n` background item allowed as last item | SGR, see §4 | SGR, see §4 | RKM-8C, AD20; DCT l.754 |
| `CSI > Ps ; Ps m` | **Global background colour** when written `CSI >4m` etc. (the `>n` item, V36) | **XTMODKEYS**: `CSI > 4 ; 2 m` = modifyOtherKeys level 2 (vim sends this) | Ignored (p_ignore) | AD20 SGR notes; XT. **COLLISION C-SGR>** |
| `CSI 6 n` (9B 36 6E) | DSR: console inserts CPR `CSI row;col R` into the read stream (introducer 9B) | DSR 6 -> `ESC [ row ; col R` (7-bit unless S8C1T); `CSI 5 n` -> `ESC [ 0 n`; `CSI ? 6 n` -> DECXCPR `ESC [ ? row ; col R` | 6 -> `ESC [ row ; col R`, 5 -> `ESC [ 0 n` (sent via `send_data`) | RKM-8C, RKM-91, AD20, H-CON `DSR_CPR 6`; DCT l.639. Same meaning, **different reply introducer** (9B vs 1B 5B) |
| `CSI Ps SP p` (9B [N] 20 70) | aSCR set cursor rendition: `9B 30 20 70` invisible, `9B 20 70` visible. Other N unverified | Not defined in ctlseqs (from knowledge). `CSI ! p` = DECSTR soft reset; `CSI Ps $ p` = DECRQM; `CSI > Ps p` = XTSMPOINTER; `CSI Ps ; Ps " p` = DECSCL | Ignored (intermediate) | RKM-8B, RKM-8D, AD20 |
| `CSI Ps q` (71) | Not defined | DECLL (load LEDs) | Ignored | - |
| `CSI Ps SP q` (9B 30 20 71) | **aWSR window status request**: console answers with aWBR into the read stream | **DECSCUSR cursor style**: 0/1 blinking block, 2 steady block, 3 blinking underline, 4 steady underline, 5 blinking bar, 6 steady bar (vim sends `CSI 2 SP q`). vtcon xterm: the starting style, blink (`?12`, off by default) and block colour are per-window-profile (`cursor`, `cursor-blink`, `cursor-color`; plan 2026-10-01-terminal-preferences.md), a profile default only — runtime sequences override | Ignored | RKM-8D, RKM-92, AD20 (`#p = 0`); XT. **COLLISION C-SPq** |
| `CSI 1;1;Pb;Pr SP r` (9B 31 3B 31 3B .. 3B .. 20 72) | aWBR window bounds report, **read stream only**: bottom margin = rows, right margin = columns; e.g. 20x60: `9B 31 3B 31 3B 32 30 3B 36 30 20 72` | Not defined (`CSI Pt;Pb r` bare = DECSTBM) | Ignored | RKM-8D (E4), RKM-92, AD20 |
| `CSI Pt ; Pb r` (72) | Not defined | DECSTBM: set scroll region (cursor homes) | **Ignored** (no scroll regions) | - |
| `CSI s` (73) | Not defined | SCOSC save cursor (when DECLRMM `?69` off; otherwise DECSLRM) | Save cursor position | DCT l.775 |
| `CSI SP s` (9B 20 73) | aSDSS (V39): current pen, cell colour, text style and reverse become the defaults for `SGR 0`, `39`, `49`; `ESC c` restores the factory defaults | Not defined | Ignored | AD31, NDK-DOC |
| `CSI Pn t` (74) | **aSLPP set page length**: how many text lines fit ("in character raster lines ... using current font", wording ambiguous: Q3). No parameter = back to automatic | **XTWINOPS** window ops: e.g. `8;h;w t` resize, `14 t` / `18 t` report pixel / character size (`CSI 8;rows;cols t`), `22/23 t` title stack | Ignored | RKM-8D, AD20; XT. **COLLISION C-t** |
| `CSI Pn u` (75) | **aSLL set line length** in character positions. No parameter = back to automatic | **SCORC** restore cursor (bare `CSI u`) | Restore saved position | RKM-8D, AD20; DCT l.779. **COLLISION C-u** (also amiga vs pcansi) |
| `CSI 0 SP v` (9B 30 20 76) | aRAV Right-Amiga-V pressed, **read stream only** (V37, when ConClip runs) | Not defined | Ignored | RKM-8D, RKM-93, AD20 |
| `CSI Pn x` (78) | **aSLO set left offset** in raster columns (pixels) from the window's left edge. No parameter = automatic | **DECREQTPARM** (request terminal parameters; `CSI 0 x` / `CSI 1 x` -> `CSI 2;1;1;128;128;1;0 x` style report) | Ignored | RKM-8D, AD20; XT. **COLLISION C-x** |
| `CSI Pn y` (79) | **aSTO set top offset** in raster lines from the top of the window's RastPort. No parameter = automatic | Bare `CSI Ps y` not defined in ctlseqs (from knowledge; `CSI ... * y` is DECRQCRA). Must be ignored | Ignored | RKM-8D, AD20. Near-collision C-y |
| `CSI Pm {` (7B) | **aSRE set raw events**: enable each listed class (§5.1) | Bare form not defined (`CSI # {` XTPUSHSGR, `CSI ... $ {` DECSERA, `CSI Ps ' {` DECSLE) | Ignored | RKM-8D, RKM-94, AD20 |
| `CSI c;s;k;q;x;y;sec;usec \|` (7C) | **aIER input event report**, read stream only, 8 parameters (§5.2) | Bare form not defined (`CSI # \|` XTREPORTSGR, `CSI Ps ' \|` DECRQLP, `CSI Ps $ \|` DECSCPP, `CSI Ps * \|` DECSNLS) | Ignored | RKM-8D, RKM-95, AD20 (E5) |
| `CSI Pm }` (7D) | **aRRE reset raw events** | Bare form not defined (`CSI # }` XTPOPSGR, `CSI Ps ' }` DECIC) | Ignored | RKM-8D, RKM-94, AD20 |
| `CSI Pn ~` (7E) | **aSKR special key report**, read stream only (§5.3) | Output: bare form not defined (`CSI Ps ' ~` DECDC). **Input**: xterm's own key encodings use `CSI Pn ~` with different numbers | Ignored | RKM-8D, RKM-90, AD20. **COLLISION C-~** (input side) |

### 3.1 Collision list

| ID | Bytes | amiga | xterm |
|----|-------|-------|-------|
| C-t | `CSI Pn t` | set page length | XTWINOPS (resize / size reports / title stack) |
| C-u | `CSI Pn u` | set line length | SCORC restore cursor (pcansi also restores) |
| C-x | `CSI Pn x` | set left offset (pixels) | DECREQTPARM |
| C-y | `CSI Pn y` | set top offset (pixels) | undefined bare (DEC DECTST family); xterm must ignore it |
| C-SPq | `CSI Ps SP q` | window status request, produces `CSI 1;1;r;c SP r` in the input | DECSCUSR cursor shape |
| C-SGR> | `CSI > n m` (and `>n` inside an SGR list) | global background pen n | XTMODKEYS (`CSI >4;2m`) |
| C-~ | `CSI Pn ~` in the **input** stream | F1 = `CSI 0~`, F3 = `CSI 2~`, F4 = `CSI 3~`, F6 = `CSI 5~`, Insert = `CSI 40~` | Insert = `CSI 2~`, Delete = `CSI 3~`, PgUp = `CSI 5~`, F5 = `CSI 15~` |
| C-arrowS | `CSI S`, `CSI T`, `CSI SP A`, `CSI SP @` in the input | Shift+Down, Shift+Up, Shift+Left, Shift+Right | (as output) SU, SD, SR, SL; xterm's shifted arrows are `CSI 1;2A..D` |
| C-VT | 0B | cursor up | LF |
| C-FF | 0C | clear window | LF |
| C-SO | 0E / 0F | set MSB (Latin-1 upper half) / cancel | LS1 / LS0 (G1 / G0, e.g. DEC line drawing) |
| C-DEL | 7F | displayed glyph | ignored |
| C-C1 | 80-9F | 84/85/88/8D/9B controls | UTF-8 continuation bytes |
| C-CPR | reply to `CSI 6n` | introducer 9B | introducer `ESC [` |

Not collisions (same meaning): CUU..CPL, CUP/HVP, CHT, CBT, IL, DL, ICH,
DCH, SU, SD, TBC, LNM 20, AWM ?7, SGR 0/1/3/4/7/8/22-28/30-37/39/40-47/49
(colour **semantics** still differ, §4), `ESC c`, IND/NEL/HTS/RI.

---

## 4. SGR per personality

### 4.1 Parameter table

| Param | amiga | xterm (XT) | pcansi (DCT l.503-557, l.754-773) |
|-------|-------|------------|-----------------------------------|
| 0 | Normal colours and attributes: fg pen 1, cell pen 0, plain, reverse off. V39+: the aSDSS-saved defaults instead | All attributes off, default fg/bg | fg 7, bg 0, bold/blink/inverse/underline off |
| 1 | **Bold font style** (algorithmic style of the RastPort, `cu_AlgoStyle`); colour does not change | Bold. xterm resource `boldColors` (default true) also brightens 30-37 to 90-97 when rendering: vtcon decision (Q6, resolved: ON as default, togglable per window profile `bold-bright`) | **Bright foreground** (fg + 8). No font change |
| 2 | Faint ("secondary color"); the pen it uses is unverified | Faint | Bold off (treated as intensity off) |
| 3 | Italic | Italic | Ignored |
| 4 | Underscore | Underline (`4:0..5` styles with colon) | Underline |
| 5, 6 | Not documented | Blink (6 rapid; xterm renders as blink) | **Bright background** (bg + 8, iCE colours) |
| 7 | Reverse character/cell colours | Inverse | Inverse (swap resolved fg/bg, l.262) |
| 8 | Concealed. Before V39 masked all RastPort output (avoid); V39+: text hidden by pen choice, scrolling/clearing unaffected | Invisible | Ignored |
| 9 | Not documented | Crossed out | Ignored |
| 21 | Not documented | Doubly underlined | Bold off |
| 22 | Normal colour, not bold (V36) (`SGR_NORMAL`) | Normal intensity (not bold, not faint) | Bold off |
| 23 | Italic off (V36) | Not italic | Ignored |
| 24 | Underscore off (V36) | Not underlined | Underline off |
| 25 | Not documented | Steady (blink off) | Blink off (bright bg off) |
| 27 | Reversed off (V36) (`SGR_POSITIVE`) | Positive | Inverse off |
| 28 | Concealed off (V36) | Visible | Ignored |
| 29 | Not documented | Not crossed out | Ignored |
| 30-37 | **Character colour = pen 0-7** (`SGR_CLR0`..`SGR_CLR7`; the header says the colour names "refer to the ANSI standard, not the implementation") | ANSI colours 0-7 (black, red, green, yellow, blue, magenta, cyan, white) | CGA colours 0-7 in ANSI order (+8 while bold) |
| 38 | Not documented | `38;5;n` 256-colour, `38;2;r;g;b` direct colour, colon forms `38:5:n`, `38:2::r:g:b` | `38;5;n` consumed (3 items) and `38;2;r;g;b` consumed (5 items) with no effect; colon forms make the whole SGR a no-op (p_ignore) |
| 39 | Default character colour (pen 1, or aSDSS default) (`SGR_DEFAULT`) | Default foreground | fg 7 |
| 40-47 | **Character cell colour = pen 0-7** (`SGR_CLR0BG`..) | ANSI background 0-7 | CGA bg 0-7 (+8 while blink) |
| 48 | Not documented | As 38 for background | As 38 |
| 49 | Default cell colour (pen 0, or aSDSS) (`SGR_DEFAULTBG`) | Default background | bg 0 |
| `>0`..`>7` | **Global background colour** pen 0-7 (V36). Must be the last item(s): "issue the digit only parameters first, followed by any prefixed parameters". Vacated areas (erase, scroll) fill with it, not with the cell colour (V36 change) | A leading `>` makes the whole sequence XTMODKEYS (C-SGR>) | Whole SGR ignored |
| 90-97 | Not documented | Bright foreground 8-15 | fg = n-90 **and sets bold**, so a later 30-37 stays bright until 22/0 |
| 100-107 | Not documented | Bright background 8-15 | bg = n-100 **and sets blink** |

Sources: amiga RKM-8C, RKM-8D, AD20 SGR notes, AD31/NDK-DOC (V39 aSDSS,
concealed), H-CON (SGR_* names), AM-30 / LIB-572 (V36 vacated areas);
Amiga bold as a font style: AD20 ("Set bold"), H-CU (`cu_AlgoStyle`), DCT
header l.3-5 and README (observed: "SGR 1 selects a bold font instead of the
bright palette half").

### 4.2 Colour model per personality

- **amiga**: colours are **screen pens**, not colours. Only pens 0-7 are
  addressable for fg, cell and background (SGR 30-37, 40-47, `>0-7`); on a
  deeper screen pens 8+ are unreachable (DCT README, consistent with the SGR
  ranges). Defaults: fg pen 1, cell pen 0, background pen 0 (AD31 V39 note:
  "the defaults of PEN color 1, and cell color 0"). Three separate colours
  exist: character, cell (behind each glyph), global background (vacated
  areas). The console guide: "In most cases, the character cell color and
  the background color should be the same" (RKM-8C). Smart-refresh vs
  character-mapped refill differences on resize: RKM-8D.
- **xterm**: default fg and default bg are **distinct from** palette entries
  (SGR 39/49 select "default", not index 7/0). Palette 0-15, 16-231 6x6x6
  cube, 232-255 greys, plus 24-bit direct colour. Erase uses the current bg
  (BCE; terminfo `bce`).
- **pcansi**: 16-colour CGA palette in ANSI order; bold folds into fg+8,
  blink into bg+8; resolved in `term_update_pens` (DCT l.262): `fg = atr_fg +
  8*bold`, `bg = atr_bg + 8*blink`, swapped when inverse. Erase and scroll
  fill with the current bg (BCE); `ESC c` clears with pen 0.

RULES.md rule 3: all three resolve in `vt_resolve_colors` only.

---

## 5. Console input side

### 5.1 Raw input event classes (`CSI n {` / `CSI n }`)

`CSI 7;8;11{` enables several at once; `CSI 7;8;11}` disables them. Events may
still be queued after a reset (RKM-94). Class number = IECLASS value (H-IE).

| n | RKM description | IECLASS (H-IE) |
|---|-----------------|----------------|
| 0 | No-op (used internally) | `IECLASS_NULL` 0x00 |
| 1 | RAW keyboard input | `IECLASS_RAWKEY` 0x01 |
| 2 | RAW mouse input ("Intuition swallows all except the select button") | `IECLASS_RAWMOUSE` 0x02 |
| 3 | Private Console Event | `IECLASS_EVENT` 0x03 |
| 4 | Pointer position | `IECLASS_POINTERPOS` 0x04 |
| 5 | (unused) | - |
| 6 | Timer | `IECLASS_TIMER` 0x06 |
| 7 | Gadget pressed | `IECLASS_GADGETDOWN` 0x07 |
| 8 | Gadget released | `IECLASS_GADGETUP` 0x08 |
| 9 | Requester activity | `IECLASS_REQUESTER` 0x09 |
| 10 | Menu numbers | `IECLASS_MENULIST` 0x0A |
| 11 | Close Gadget | `IECLASS_CLOSEWINDOW` 0x0B |
| 12 | Window resized | `IECLASS_SIZEWINDOW` 0x0C |
| 13 | Window refreshed | `IECLASS_REFRESHWINDOW` 0x0D |
| 14 | Preferences changed | `IECLASS_NEWPREFS` 0x0E |
| 15 | Disk removed | `IECLASS_DISKREMOVED` 0x0F |
| 16 | Disk inserted | `IECLASS_DISKINSERTED` 0x10 |
| 17 | Active window | `IECLASS_ACTIVEWINDOW` 0x11 |
| 18 | Inactive window | `IECLASS_INACTIVEWINDOW` 0x12 |
| 19 | New pointer position (V36) | `IECLASS_NEWPOINTERPOS` 0x13 |
| 20 | Menu help (V36) | `IECLASS_MENUHELP` 0x14 |
| 21 | Window changed (zoom, move) (V36) | `IECLASS_CHANGEWINDOW` 0x15 (`IECLASS_MAX`) |

Routing (RKM-94): requester, window refreshed, active, inactive, resized and
changed go to the console that **owns the window**, active or not; all other
classes go to the active console only, if it asked for them.

`cu_RawEvents` holds one bit per class: `(IECLASS_MAX+8)/8` = 3 bytes (H-CU).

### 5.2 Input event report (aIER)

```
CSI <class>;<subclass>;<keycode>;<qualifiers>;<x>;<y>;<seconds>;<microseconds>|
```

All fields decimal ASCII; CSI is the single byte 9B (RKM-95).

- `<class>`: §5.1. `<subclass>`: usually 0; 1 for the mouse in the right
  port; `IESUBCLASS_COMPATIBLE 0 / PIXEL 1 / TABLET 2 / NEWTABLET 3` for
  NEWPOINTERPOS (H-IE).
- `<keycode>`: raw key number; +128 (`IECODE_UP_PREFIX` 0x80) on release;
  also carries mouse button codes (`IECODE_LBUTTON 0x68`, `RBUTTON 0x69`,
  `MBUTTON 0x6A`, `NOBUTTON 0xFF`) (H-IE).
- `<x>;<y>`: coordinates, or for some classes an Intuition address as
  `x<<16+y`. For RAWKEY they are `<prev1>;<prev2>`: previous and second-previous
  down key, high byte = key code, low byte = qualifier (dead keys).
- `<seconds>;<microseconds>`: system time stamp (longwords).
- Caps Lock: a code only on press; "keycode 62 (Caps Lock pressed)" when the
  LED lights, "keycode 190 (Caps Lock released)" when it goes out (RKM-95).
  The RKM pair is consistent in decimal (62 + 128 = 190), but H-KM defines
  `RAWKEY_CAPSLOCK 0x62` (= 98, release 226 = 0xE2). The field is decimal
  ASCII, so the report most likely carries 98 / 226 and the RKM wrote the hex
  value as if decimal. **Q5**, probe.

Qualifiers (RKM-95 = H-IE `IEQUALIFIER_*`):

| Bit | Mask | Meaning |
|-----|------|---------|
| 0 | 0001 | Left Shift |
| 1 | 0002 | Right Shift |
| 2 | 0004 | Caps Lock |
| 3 | 0008 | Ctrl |
| 4 | 0010 | Left Alt |
| 5 | 0020 | Right Alt |
| 6 | 0040 | Left Amiga |
| 7 | 0080 | Right Amiga |
| 8 | 0100 | Numeric pad |
| 9 | 0200 | Repeat |
| 10 | 0400 | Interrupt (not used) |
| 11 | 0800 | Multibroadcast |
| 12 | 1000 | Middle button |
| 13 | 2000 | Right button |
| 14 | 4000 | Left button |
| 15 | 8000 | Relative mouse |

Example (RKM-95), A pressed and released with Left Shift + Right Amiga:
`CSI 1;0;32;32769;14593;5889;421939940;316673|` then
`CSI 1;0;160;32769;0;0;421939991;816683|`. (32769 = 0x8001 = relative mouse +
Left Shift, not Left Shift + Right Amiga (that would be 0x0081 = 129): the
RKM prose and its bytes disagree; the key code 32 = 0x20 = A matches.)

Window events: class 11 (close gadget) is what a RAW: window delivers for the
close gadget (AM-2F, LIB-572: "the Close Gadget raw event ESC seq with RAW:").
Class 12 (resize) and 21 (move/zoom) are the resize notifications; the exact
subclass/keycode/x/y contents for window classes are **unverified**. The
bounds after a resize are read with `CSI 0 SP q` -> `CSI 1;1;<rows>;<cols> SP r`
(RKM-92).

### 5.3 Special key reports (amiga, default state, no raw events)

These come from the keymap (the ROM usa keymap), not from console code
(RKM-89/90). Hex is the full read-stream byte string.

| Key | Rawkey (H-KM) | Unshifted | Hex | Shifted | Hex |
|-----|---------------|-----------|-----|---------|-----|
| F1 | 0x50 | `CSI 0~` | 9B 30 7E | `CSI 10~` | 9B 31 30 7E |
| F2 | 0x51 | `CSI 1~` | 9B 31 7E | `CSI 11~` | 9B 31 31 7E |
| F3 | 0x52 | `CSI 2~` | 9B 32 7E | `CSI 12~` | 9B 31 32 7E |
| F4 | 0x53 | `CSI 3~` | 9B 33 7E | `CSI 13~` | 9B 31 33 7E |
| F5 | 0x54 | `CSI 4~` | 9B 34 7E | `CSI 14~` | 9B 31 34 7E |
| F6 | 0x55 | `CSI 5~` | 9B 35 7E | `CSI 15~` | 9B 31 35 7E |
| F7 | 0x56 | `CSI 6~` | 9B 36 7E | `CSI 16~` | 9B 31 36 7E |
| F8 | 0x57 | `CSI 7~` | 9B 37 7E | `CSI 17~` | 9B 31 37 7E |
| F9 | 0x58 | `CSI 8~` | 9B 38 7E | `CSI 18~` | 9B 31 38 7E |
| F10 | 0x59 | `CSI 9~` | 9B 39 7E | `CSI 19~` | 9B 31 39 7E |
| F11 (101-key) | 0x4B | `CSI 20~` | 9B 32 30 7E | `CSI 30~` | 9B 33 30 7E |
| F12 (101-key) | 0x6F | `CSI 21~` | 9B 32 31 7E | `CSI 31~` | 9B 33 31 7E |
| HELP | 0x5F | `CSI ?~` | 9B 3F 7E | `CSI ?~` (same) | 9B 3F 7E |
| Insert (101-key) | 0x47 | `CSI 40~` | 9B 34 30 7E | `CSI 50~` | 9B 35 30 7E |
| Page Up (101-key) | 0x48 | `CSI 41~` | 9B 34 31 7E | `CSI 51~` | 9B 35 31 7E |
| Page Down (101-key) | 0x49 | `CSI 42~` | 9B 34 32 7E | `CSI 52~` | 9B 35 32 7E |
| Pause/Break (101-key) | 0x6E | `CSI 43~` | 9B 34 33 7E | `CSI 53~` | 9B 35 33 7E |
| Home (101-key) | 0x70 | `CSI 44~` | 9B 34 34 7E | `CSI 54~` | 9B 35 34 7E |
| End (101-key) | 0x71 | `CSI 45~` | 9B 34 35 7E | `CSI 55~` | 9B 35 35 7E |
| Up | 0x4C | `CSI A` | 9B 41 | `CSI T` | 9B 54 |
| Down | 0x4D | `CSI B` | 9B 42 | `CSI S` | 9B 53 |
| Left | 0x4F | `CSI D` | 9B 44 | `CSI SP A` | 9B 20 41 |
| Right | 0x4E | `CSI C` | 9B 43 | `CSI SP @` | 9B 20 40 |

Other amiga input bytes:

- Right-Amiga-V with ConClip running: `CSI 0 SP v` (9B 30 20 76) (RKM-93,
  AD20 OpenDevice). Without ConClip the console pastes the snip as if typed.
  Right-Amiga-C/V are swallowed unless raw key events are on (AD20). Beware
  confusing `CSI 0 SP v` with F1 `CSI 0~` (AM-30).
- A user-typed `ESC [` arrives as 1B 5B, console-generated CSI as 9B (RKM-86);
  the V47 con-handler also accepts `ESC [` as CSI on input (RN-CH 47.1).
- Plain keys from the usa keymap, **from knowledge (no local keymap dump)**:
  Backspace 08, Del 7F, Tab 09, Shift+Tab `CSI Z` (9B 5A), Return and Enter
  0D, Esc 1B, Ctrl+letter 01-1A, Alt+key = Latin-1 A0-FF per keymap.

### 5.4 xterm key encodings vtcon must emit in the xterm personality

(xterm ctlseqs, from knowledge.) All 7-bit (`ESC [` = 1B 5B, `SS3` = `ESC O`
= 1B 4F). Modifier parameter `m = 1 + (Shift 1 | Alt 2 | Ctrl 4 | Meta 8)`;
map Amiga Left/Right Amiga to Meta (vtcon decision, Q7).

| Key | Normal | Application (DECCKM `?1h`) | With modifier m |
|-----|--------|----------------------------|-----------------|
| Up / Down / Right / Left | `ESC [ A` / `B` / `C` / `D` | `ESC O A` / `B` / `C` / `D` | `ESC [ 1 ; m A` .. `D` |
| Home / End | `ESC [ H` / `ESC [ F` | `ESC O H` / `ESC O F` | `ESC [ 1 ; m H` / `F` |
| Insert | `ESC [ 2 ~` | same | `ESC [ 2 ; m ~` |
| Delete (Amiga Del key) | `ESC [ 3 ~` | same | `ESC [ 3 ; m ~` |
| Page Up / Page Down | `ESC [ 5 ~` / `ESC [ 6 ~` | same | `ESC [ 5 ; m ~` / `6 ; m ~` |
| F1 / F2 / F3 / F4 | `ESC O P` / `Q` / `R` / `S` | same | `ESC [ 1 ; m P` .. `S` |
| F5 / F6 / F7 / F8 | `ESC [ 15 ~` / `17 ~` / `18 ~` / `19 ~` | same | `ESC [ 15 ; m ~` etc. |
| F9 / F10 / F11 / F12 | `ESC [ 20 ~` / `21 ~` / `23 ~` / `24 ~` | same | `ESC [ 20 ; m ~` etc. |
| Shift+Tab | `ESC [ Z` | same | - |
| Backspace | `7F` or `08`: xterm's `backarrowKey` default sends 08 and the stock xterm terminfo has `kbs=^H`; many distributions use 7F. **Decision Q4**; must match `terminfo/` | same | - |
| Return / Enter | 0D (Enter in keypad application mode `ESC =`: `ESC O M`) | | |
| Keypad digits in DECKPAM | `ESC O p`..`ESC O y`, `ESC O j/k/l/m/n/o` for `* + , - . /` | | |
| Alt+key | `ESC` prefix (`metaSendsEscape`), vtcon default (Q7) | | |
| Ctrl / Shift + character keys under modifyOtherKeys (G1 K2) | the keymap applies Ctrl (its control characters) | same | while `CSI > 4 ; 1/2 m` is on the window converts the key without Ctrl and passes Ctrl+Shift: level 1 `ESC [ 27 ; m ; c ~` for Ctrl+Shift+x and Ctrl on keys with no control character (Ctrl+; Ctrl+1), level 2 for every Ctrl/Meta combination; Shift alone is never reported (the character says it). Test `modify_other_keys_takes_ctrl_combinations_from_the_host` |
| Alt / Ctrl / Shift + Return, Tab, Backspace, Escape (G1 K1) | Alt: `ESC` + the plain key (`ESC 7F`, `ESC CR`, `ESC HT`, `ESC ESC`); Ctrl+Backspace `08`; Shift+Tab `ESC [ Z`; other modifiers: the plain key | same | modifyOtherKeys 1: Ctrl or Shift (not Shift+Tab, not Ctrl+Backspace) `ESC [ 27 ; m ; c ~` with c = 13 / 9 / 127 / 27; level 2: any modifier but Shift+Tab. Test `modify_other_keys_reports_modified_return_and_tab` |
| HELP (no xterm key) | Proposed `ESC [ 28 ~` (DEC Help, as xterm maps it on LK keyboards) | | |
| Shift+F1..F10 (Amiga has distinct codes) | Encoded with the modifier form, e.g. Shift+F1 `ESC [ 1 ; 2 P`, Shift+F5 `ESC [ 15 ; 2 ~` | | |

Other xterm input reports:

- Mouse, SGR mode (`?1000h` + `?1006h`): press `ESC [ < b ; x ; y M`, release
  `... m`, 1-based cells; b = 0/1/2 button, +4 Shift, +8 Meta, +16 Ctrl, +32
  motion, 64/65 wheel.
- Focus (`?1004h`): `ESC [ I` in, `ESC [ O` out (source: Amiga classes 17/18).
- Bracketed paste (`?2004h`): `ESC [ 200 ~` ... `ESC [ 201 ~` around pasted
  text (the Amiga equivalent is `CSI 0 SP v` + clipboard read).
- Window size: xterm has no in-band resize notification; ports learn it via
  the pty (`TIOCGWINSZ` / `SIGWINCH`, supplied by the handler / ixemul layer),
  or ask with `CSI 18 t` -> `ESC [ 8 ; rows ; cols t`.
- Replies to DSR/DA/DECRQM/XTWINOPS are 7-bit `ESC [` unless S8C1T.

### 5.5 pcansi input today

DCTelnet converts rawkeys with `RawKeyConvert(..., NULL)` (default keymap)
and rewrites each 9B byte to `ESC [`, passing everything else verbatim
(DCT-KBD l.259-290). So pcansi sends the Amiga key strings of §5.3 in 7-bit
form: arrows `ESC [ A..D` (ANSI.SYS compatible), but F1 `ESC [ 0 ~`, Shift+Up
`ESC [ T`, HELP `ESC [ ? ~`.

---

## 6. console.device API

### 6.1 Commands (H-CON, H-IO; semantics AD20, NDK-DOC)

| Command | Value | io_Data / io_Length | Semantics |
|---------|-------|---------------------|-----------|
| `CMD_READ` | 2 | buffer / size | Next input as ANSI bytes. Not satisfied while no input is pending; satisfied with **what is available** if less than io_Length. Sets io_Actual. |
| `CMD_WRITE` | 3 | buffer / length, or -1 for NUL-terminated | Interpret and render. Results: io_Error (none reported as of V36), io_Actual = bytes written, **io_Length = 0**, **io_Data = original + io_Actual**. The window RastPort is in use while pending; may wait internally even with SendIO (RKM-87). Layers are locked while writing: 256 bytes per write recommended (RKM-8B) |
| `CMD_CLEAR` | 5 | - | Remove pending reports/input from the console input buffer |
| `CD_ASKKEYMAP` | `CMD_NONSTD+0` = 9 | `struct KeyMap *` / `sizeof(struct KeyMap)` (32) | Copy this unit's keymap out |
| `CD_SETKEYMAP` | 10 | same | Set this unit's keymap from io_Data |
| `CD_ASKDEFAULTKEYMAP` | 11 | same | Copy the device default keymap (used for new units and `RawKeyConvert(..., NULL)`) |
| `CD_SETDEFAULTKEYMAP` | 12 | same | Set the default. Since V36 the structure is **not copied**: it must stay in memory until replaced |
| `CD_SETUPSCROLLBACK` | 13 | undocumented; `struct ConsoleScrollback { APTR cs_ScrollerGadget; UWORD cs_NumLines; }` in H-CON | 3.2 addition, no autodoc (Q10) |
| `CD_SETSCROLLBACKPOSITION` | 14 | undocumented | 3.2 addition, no autodoc (Q10) |
| `NSCMD_DEVICEQUERY` | 0x4000 | NSDeviceQueryResult | Supported; since 45.4 it lists itself (RN-CD) |

CMD_INVALID 0, CMD_RESET 1, CMD_UPDATE 4, CMD_STOP 6, CMD_START 7,
CMD_FLUSH 8 exist in H-IO; whether console.device accepts them is not
documented (Q10). CD_ASK/SETDEFAULTKEYMAP work on any unit, cheapest on
CONU_LIBRARY (RKM-96).

### 6.2 Units, flags, OpenDevice (H-CU, AD20 OpenDevice, RKM-83/84)

`error = OpenDevice("console.device", unit, IOStdReq, flags)` (d0 = a0, d0, a1,
d1). `io_Data` = `struct Window *` (required for units 0, 1, 3), `io_Length` =
`sizeof(struct Window)` (RKM-84 example). Fills `io_Device` and `io_Unit`
(`io_Unit` points to the `struct ConUnit`, AM-65 notes it can be NULL for
some CON: consoles).

| Name | Value | Meaning |
|------|-------|---------|
| `CONU_LIBRARY` | -1 | No console; only fills io_Device = ConsoleDevice base for the functions. Must still be closed |
| `CONU_STANDARD` | 0 | Standard unmapped console; typically SMART_REFRESH window |
| `CONU_CHARMAP` | 1 | (V36) Character map kept (chars, attributes, style); redraws on reveal/resize. **SIMPLE_REFRESH window required** |
| `CONU_SNIPMAP` | 3 | (V36) Character map + mouse drag-select, Right-Amiga-C copy |
| `CONFLAG_DEFAULT` | 0 | Redraw window on resize (charmap units) |
| `CONFLAG_NODRAW_ON_NEWSIZE` | 1 | (V37) Do not redraw on resize; ignored for unit 0; ignored under V36 |

Also from H-CU: `PMB_ASM = M_LNM+1 = 21`, `PMB_AWM = 22` (bit numbers in
`cu_Modes`), `MAXTABS 80`. Caveats (RKM-98): one console per window;
do not mix graphics.library rendering in the console's area; character map is
private; with charmap units get Intuition events via raw events, not IDCMP.
Closing the window before the console crashes (AM-30).

### 6.3 Library vectors (FD20, console_protos.h)

`##bias 42`, base `_ConsoleDevice`. Standard device vectors precede them
(Open -6, Close -12, Expunge -18, Reserved -24, BeginIO -30, AbortIO -36;
exec device layout, from knowledge).

| LVO | Function | Registers | Result |
|-----|----------|-----------|--------|
| -42 | `CDInputHandler(events, consoleDevice)` | a0, a1 | a0/d0: events not consumed. Historical; prefer input.device `WriteEvent` |
| -48 | `RawKeyConvert(events, buffer, length, keyMap)` | a0, a1, d1, a2; ConsoleDevice in a6 | d0 = bytes placed, **-1 on overflow** (buffer contents then partly invalid). keyMap NULL = device default. Converts IECLASS_RAWKEY only |
| -54, -60, -66, -72 | `consolePrivate1..4` (V36) | - | private |

### 6.4 struct ConUnit (H-CU; offsets verified in OFFS20, identical to a hand count)

| Offset | Size | Type | Field | Note |
|--------|------|------|-------|------|
| 0x000 | 34 | `struct MsgPort` | `cu_MP` | |
| 0x022 | 4 | `struct Window *` | `cu_Window` | read-only from here to cu_YCCP |
| 0x026 | 2 | WORD | `cu_XCP` | character position |
| 0x028 | 2 | WORD | `cu_YCP` | |
| 0x02A | 2 | WORD | `cu_XMax` | max character position |
| 0x02C | 2 | WORD | `cu_YMax` | |
| 0x02E | 2 | WORD | `cu_XRSize` | character raster size |
| 0x030 | 2 | WORD | `cu_YRSize` | |
| 0x032 | 2 | WORD | `cu_XROrigin` | raster origin |
| 0x034 | 2 | WORD | `cu_YROrigin` | |
| 0x036 | 2 | WORD | `cu_XRExtant` | raster maxima |
| 0x038 | 2 | WORD | `cu_YRExtant` | |
| 0x03A | 2 | WORD | `cu_XMinShrink` | smallest area intact from resize |
| 0x03C | 2 | WORD | `cu_YMinShrink` | |
| 0x03E | 2 | WORD | `cu_XCCP` | cursor position |
| 0x040 | 2 | WORD | `cu_YCCP` | |
| 0x042 | 32 | `struct KeyMap` | `cu_KeyMapStruct` | read/write from here (writes must be protected) |
| 0x062 | 160 | UWORD[80] | `cu_TabStops` | "0 at start, 0xffff at end of list" |
| 0x102 | 1 | BYTE | `cu_Mask` | must match RastPort order |
| 0x103 | 1 | BYTE | `cu_FgPen` | |
| 0x104 | 1 | BYTE | `cu_BgPen` | |
| 0x105 | 1 | BYTE | `cu_AOLPen` | |
| 0x106 | 1 | BYTE | `cu_DrawMode` | |
| 0x107 | 1 | BYTE | `cu_Obsolete1` | was cu_AreaPtSz |
| 0x108 | 4 | APTR | `cu_Obsolete2` | was cu_AreaPtrn |
| 0x10C | 8 | UBYTE[8] | `cu_Minterms` | |
| 0x114 | 4 | `struct TextFont *` | `cu_Font` | |
| 0x118 | 1 | UBYTE | `cu_AlgoStyle` | (bold/italic/underline soft styles) |
| 0x119 | 1 | UBYTE | `cu_TxFlags` | |
| 0x11A | 2 | UWORD | `cu_TxHeight` | |
| 0x11C | 2 | UWORD | `cu_TxWidth` | |
| 0x11E | 2 | UWORD | `cu_TxBaseline` | |
| 0x120 | 2 | WORD | `cu_TxSpacing` | |
| 0x122 | 3 | UBYTE[(PMB_AWM+7)/8] | `cu_Modes` | bit 20 LNM, 21 ASM, 22 AWM |
| 0x125 | 3 | UBYTE[(IECLASS_MAX+8)/8] | `cu_RawEvents` | bit per class |
| 0x128 | - | - | sizeof = 296 | ALIGNWORD in conunit.i |

struct KeyMap (H-KM): `km_LoKeyMapTypes`, `km_LoKeyMap`, `km_LoCapsable`,
`km_LoRepeatable`, `km_HiKeyMapTypes`, `km_HiKeyMap`, `km_HiCapsable`,
`km_HiRepeatable` (8 pointers, 32 bytes). Key types: `KC_NOQUAL 0`,
`KCF_SHIFT 1`, `KCF_ALT 2`, `KCF_CONTROL 4`, `KC_VANILLA 7`, `KCF_DOWNUP 8`,
`KCF_DEAD 0x20`, `KCF_STRING 0x40`, `KCF_NOP 0x80`.

---

## 7. Console-handler contract (CON: / RAW: / XCON:)

### 7.1 Packets

Types: BPTR, BSTR, BOOL (DOSTRUE -1 / DOSFALSE 0), CODE (dos.h error), ARG1 =
the handle's `fh_Arg1` (AM-5E). Numbers from H-DX unless noted.

| Packet | Number | DOS call | Args | Results | Console semantics / source |
|--------|--------|----------|------|---------|----------------------------|
| `ACTION_STARTUP` | 0 | mount / first open | handler startup: name BSTR, startup, DeviceNode (from knowledge) | DOSTRUE | Handler start; H-DX |
| `ACTION_FINDINPUT` | 1005 | `Open(name, MODE_OLDFILE)` | ARG1 BPTR FileHandle, ARG2 LOCK, ARG3 BSTR name | RES1 BOOL, RES2 CODE | Opens a console on the spec in ARG3 (§7.3). Handler sets `fh_Arg1`; for interactive consoles `fh_Port` (fh_Interactive) nonzero (from knowledge). AM-5F |
| `ACTION_FINDOUTPUT` | 1006 | `Open(name, MODE_NEWFILE)` | as above | as above | Same as FINDINPUT for a console. AM-5F |
| `ACTION_FINDUPDATE` | 1004 | `Open(name, MODE_READWRITE)` | as above | as above | Same. AM-5F |
| `ACTION_READ` | 82 ('R') | `Read()` | ARG1, ARG2 APTR buffer, ARG3 LONG length | RES1 bytes, 0 = EOF, -1 = error; RES2 CODE | Cooked: returns at most one line when Return is pressed. Raw: returns what is available (blocks for at least 1). EOF = Ctrl-\ or close gadget (cooked). AM-5F, AM-5D, AM-2F |
| `ACTION_WRITE` | 87 ('W') | `Write()` | ARG1, ARG2 APTR, ARG3 LONG | RES1 bytes written; RES2 if != ARG3 | Pass through the engine. V47 forwards output-device errors (RN-CH 47.1). AM-5F |
| `ACTION_END` | 1007 | `Close()` | ARG1 | RES1 DOSTRUE | Close the handle; with WAIT keep the window until close gadget / Ctrl-\. AM-5F, AM-5D |
| `ACTION_SEEK` | 1008 | `Seek()` | ARG1, ARG2 pos, ARG3 mode | RES1 old pos or -1 | Handlers need not support it (AM-5F); console: RES1 -1, RES2 `ERROR_ACTION_NOT_KNOWN` 209 (from knowledge) |
| `ACTION_SCREEN_MODE` | 994 | `SetMode(fh, mode)` | ARG1 LONG mode | RES1 BOOL, RES2 CODE | 0 cooked, 1 raw, **2 medium (V47)**: lines buffered, but TAB, Shift+TAB, Up and Down produce immediate CSI sequences for the shell. AM-65, DOSDOC l.5237, RN-CH 47.1 |
| `ACTION_CHANGE_SIGNAL` | 995 | sendpkt only | ARG1 fh_Arg1, ARG2 APTR MsgPort of process to signal, ARG3 0 | RES1 BOOL, RES2 CODE | Redirect Ctrl-C/D/E/F break signals (default: the opener). AM-65 |
| `ACTION_WAIT_CHAR` | 20 | `WaitForChar(fh, timeout)` | ARG1 ULONG timeout (microseconds) | RES1 DOSTRUE if a char is available within the timeout, else DOSFALSE; V47: RES2 = number of buffered lines, 0 result on EOF, early 1 when paste/queue lines exist | AM-65, DOSDOC l.6290, RN-CH 47.1 (calls it ACTION_WAIT_FOR_CHAR) |
| `ACTION_DISK_INFO` | 25 | sendpkt only (for consoles) | ARG1 BPTR InfoData | RES1 BOOL | **`id_VolumeNode` = `struct Window *`**, **`id_InUse` = the console's `IOStdReq *`** (its `io_Unit` may be NULL). Window may be NULL (AUTO not yet open, AUX:). V47: forces the window open, then always returns the IO pointer (47.3); clears VolumeNode for no window / AUX (47.13); disables AUTO until UNDISK_INFO (47.1). **Measured 2026-09-30 (tools/rig/autoprobe_rig.py, ROM con-handler 40.x):** an AUTO/CLOSE window's close gadget shuts only the window (no read pending) and the next write reopens it; after DISK_INFO the gadget does nothing; ACTION_UNDISK_INFO (513) is unknown there (res2 209). XCON: does the same and also answers UNDISK_INFO as V47 documents (the gadget shuts the window again). AM-65, AM-61, AM-7C, H-DOS InfoData |
| `ACTION_UNDISK_INFO` | 513 | sendpkt only | - | RES1 BOOL | (V47) Re-enable AUTO after DISK_INFO. RN-CH 47.1, H-DX |
| `ACTION_FORCE` | 2001 (number **unverified**: not in the NDK headers) | ARexx / shell | buffer, length (unverified) | - | Insert text into the input as if typed; V47 opens the AUTO window first; 47.14 fixes zero length. RN-CH (names ACTION_FORCE and ACTION_FORCE_INPUT) |
| `ACTION_STACK` | 2002 | ARexx `PUSH` | line (unverified layout) | - | Push a line in front of the input (LIFO). H-RX "private DOS packet types"; RN-CH |
| `ACTION_QUEUE` | 2003 | ARexx `QUEUE` | line (unverified layout) | - | Append a line to the input (FIFO). H-RX; RN-CH |
| `ACTION_IS_FILESYSTEM` | 1027 | `IsFileSystem()` | - | RES1 DOSFALSE | A console is not a file system. AM-62 |
| `ACTION_DIE` | 5 | sendpkt | - | RES1 DOSTRUE | Release resources, fail later packets. AM-62 |
| `ACTION_FLUSH` | 27 | sendpkt | - | RES1 DOSTRUE | Finish pending writes before replying. AM-62 |
| `ACTION_EXAMINE_FH` 1034, `ACTION_PARENT_FH` 1031, `ACTION_CHANGE_MODE` 1028, `ACTION_FH_FROM_LOCK` 1026 | | | | RES1 DOSFALSE, RES2 `ERROR_ACTION_NOT_KNOWN` (209) | Not required of a console (from knowledge; which of these the ROM CON: answers: unverified) |
| `ACTION_READ_RETURN` 1001, `ACTION_WRITE_RETURN` 1002, `ACTION_TIMER` 30 | | | | | Handler internal: replies of the handler's own IO requests dressed as packets (AM-5E "Handler Internal") |
| Removed in V47 | - | - | - | - | `ACTION_GET_VARS`, `ACTION_REPLACE`, `ACTION_PEEK`, `ACTION_SIZE_HIST`, `ACTION_SET_HIST`, `ACTION_GET_HIST` (history moved to the shell). Numbers not in the NDK. RN-CH 47.1 |

Any other packet: RES1 DOSFALSE, RES2 `ERROR_ACTION_NOT_KNOWN` 209 (H-DOS).

Related DOS behaviour: `Open("*")` = the process's current console
(`pr_ConsoleTask`); from V36 also `CONSOLE:` and `CONSOLE:spec` (the ROM
con-handler ignores the spec) (DOSDOC l.3960-3965). `GetConsoleTask` /
`SetConsoleTask` read/set `pr_ConsoleTask` (DOSDOC l.2685, l.5052). Ctrl-C/D/E/F
send `SIGBREAKF_CTRL_C..F` (H-DOS) to the opener or the CHANGE_SIGNAL port;
a System()-launched command only receives them while it has I/O pending on
the window (AM-5C, `.../AmigaMail_Vol2_guide/node005C.html`).

### 7.2 Raw / cooked / medium

| Aspect | Cooked (mode 0, CON:) | Raw (mode 1, RAW:) | Medium (mode 2, V47) |
|--------|-----------------------|--------------------|----------------------|
| Line buffering | Yes; Read returns on Return | No; bytes as typed | Yes |
| Echo / line editing | Handler echoes and edits (BS, Ctrl-X etc.: exact editing keys unverified) | None | As cooked |
| Special keys | Consumed by the line editor (history removed in V47: the shell does it) | Passed as CSI sequences (§5.3), raw event reports passed through | TAB, Shift+TAB, Up, Down produce immediate CSI sequences (exact bytes unverified, Q9) |
| Close gadget | EOF (Read returns 0; getchar -1) | Raw event class 11 report | unverified |
| Ctrl-\ | EOF (AM-5D) | byte 1C (unverified) | unverified |
| Paste (`CSI 0 SP v`) | Handler reads the clipboard and inserts as typed (AM-30, RKM-93 "CON: provides you with free PASTE support") | unverified | unverified |

Sources: AM-65, AM-5D, AM-2F, LIB-572, DOSDOC SetMode, RN-CH 47.1 (raw->cooked
re-enables the close event).

### 7.3 CON: / RAW: open string

```
CON:x/y/width/height/title/OPTION/OPTION ...
RAW:x/y/width/height/title/OPTION ...
```

Keywords follow the title in any order, one per `/` field (AM-5D). Example:
`CON:/0/0/640/200/My Title/AUTO/CLOSE/WAIT` (AM-5D) and
`CON:0/0/640/200/auto/close/wait` (LIB-239; keywords are case-insensitive).
Terminating the string after the title keeps 1.3 compatibility
(`/Users/spot/Code/amigadeveloperdocs/ADCD_2.1/Libraries_Manual_guide/node0023.html`). V47: missing coordinates open the window under the mouse pointer
(RN-CH 47.15); `\/` in the title is a literal `/`, `\\` a backslash (47.6).
Default refresh: SIMPLE_REFRESH with a character-mapped (snip) console;
SMART selects the old unmapped console (LIB-572, AM-30).

| Option | Meaning | Status |
|--------|---------|--------|
| `AUTO` | Open the window only when input or output happens | Verified AM-5D |
| `CLOSE` | Add a close gadget | Verified AM-5D |
| `WAIT` | Hold off Close until the user clicks Close or types Ctrl-\ | Verified AM-5D |
| `WINDOW 0xaddr` | Attach to an already open Intuition window (hex address; may be on a custom screen) | Verified AM-5D |
| `SCREEN name` | Open on the named public screen | Verified AM-5D |
| `BACKDROP` | Backdrop window | Keyword verified AM-5D; semantics from knowledge |
| `NODRAG` | No drag bar | Keyword verified AM-5D; semantics from knowledge |
| `NOBORDER` | Borderless window | Keyword verified AM-5D; semantics from knowledge |
| `NOSIZE` | No sizing gadget | Keyword verified AM-5D; semantics from knowledge |
| `SIMPLE` | SIMPLE_REFRESH, character-mapped console (default since V36) | Keyword verified AM-5D; LIB-572 |
| `SMART` | SMART_REFRESH, unmapped console (CONU_STANDARD) | Keyword verified AM-5D; LIB-572 |
| `ICONIFY` | Iconify gadget (needs V47 ConClip) | Verified RN-CH 47.8 |
| `INACTIVE` | Do not activate the window on open | **Unverified** (not in local docs) |
| `NOCLOSE` | No close gadget (e.g. on shells) | **Unverified** |
| `NODEPTH` | No depth gadget | **Unverified** |
| `ALT x/y/w/h` | Alternate (zoom) size | **Unverified** |
| `KEEPCLOSED`, `SHELL` | seen in later con-handlers | **Unverified** |

Where the keyword argument goes (`/SCREEN MYSCREEN` in one field vs a
separate field) is not stated in AM-5D beyond "WINDOW 0xaddr" / "SCREEN
name": treat space-separated within one field as the documented form
(unverified).

---

## 7a. Measured on the ROM console (2026-09-29)

`tests/amiga/romprobe` on the rig (KS 3.1 40.068, CON: 79x25), cases in
`tests/probes/amiga_cases.txt`, compared with `tools/probe_compare.py`. The
engine's amiga personality now gives the same answer on all 50 cases, and so
does XCON:/AMIGA through DOS (identical output, window size included).

- **Q1 resolved:** the cursor report is `row;column` (the RKM-91 example is wrong).
- **Cursor motion is linear:** BS at column 1 goes to the last column of the row
  above (on row 1: its own last column); CUB past column 1 continues in the row
  above; CUF past the right edge continues in the rows below, stopping on the
  bottom row without scrolling; HT at the last column goes to the next line's
  first tab stop.
- **Wrap is immediate:** the 79th character of a row puts the cursor on the next
  row at once (no deferred wrap); at the bottom right that scrolls.
- **Charmap units re-wrap on resize** (measured 2026-09-30 on the SNIPMAP unit, all
  four Kickstarts; research `2026-09-30_console-device-replacement.md` section 7):
  100 characters typed into 45 columns, cursor 3;11; widened to 90 columns the
  window shows 90 + 10 characters and the cursor is at 2;11. Engine:
  `vt_set_reflow(t, 1)` (D1.8; off by default, so XCON: keeps cutting or padding
  rows) joins wrap-linked rows into logical lines on a width change and types them
  again at the new width with the personality's wrap rule: amiga wraps at once, so
  a line ending exactly at the margin keeps the empty row after it; xterm keeps a
  pending wrap pending. A hard newline is never joined, attributes stay per cell,
  the cursor stays on its character (or as far past the text as it was).
  Not reflowed: the alternate screen (xterm does not), scrollback lines (they keep
  their width; the amiga personality keeps none), DEC double-size rows.
  Narrowing was not measured on the ROM; the engine treats it as the inverse.
- **Not implemented by the ROM:** CHA (`CSI G`), `ESC 7` / `ESC 8`, `CSI s` / `CSI u`
  (u is set-line-length anyway).
- **IL / DL keep the cursor column** (xterm homes it).
- Confirmed as documented: LNM set at open (LF = CR LF), `CSI 20 l` turns it off;
  VT moves up; FF clears and homes; DEL takes a cell; SO/SI; ?7l stops at the
  last column; CHT/CBT; SU/SD leave the cursor.

## 8. Open questions / not found

| Q | Question | Where I looked |
|---|----------|----------------|
| Q1 | RESOLVED 7a: row;column. CPR field order: RKM-91 format says row;col, its own example gives `40;12R` for row 12 col 40 (E3). | RKM-91, AD20 (only "CPR 2 params"). Needs a ROM probe on the rig |
| Q2 | Amiga behaviour for anything not in the tables: CHA `CSI G`, ECH, VPA, `?25`, `ESC 7/8`, `ESC ( 0`, unknown finals, unknown SGR values, SGR 2 pen, ED/EL with 1/2, BS at column 0, default tab stops, default LNM at console open, what ASM-off does at the bottom, other C1 bytes, controls inside a CSI. | RKM-8C/8D, AD20, AD31, NDK-DOC. Not documented: probe on the rig |
| Q3 | aSLPP unit: RKM-8D says "in character raster lines" but also "how many text lines will fit"; AD20 gives no unit. | RKM-8D, AD20 |
| Q4 | Backspace byte in the xterm personality (08 vs 7F) and the matching `kbs` in `terminfo/`. | xterm ctlseqs from knowledge; vtcon plan has no decision |
| Q5 | Caps Lock report keycode: RKM-95 "62" / "190" vs `RAWKEY_CAPSLOCK 0x62` (98). | RKM-95, H-KM |
| Q6 | RESOLVED (2026-10-01): ON by default (xterm's `boldColors` default), togglable off per window profile (`bold-bright = off`). Engine `bold_bright` field, default 1, xterm personality only; the P1.3 host test pins it. | XT from knowledge; plan 2026-10-01-terminal-preferences.md |
| Q7 | Amiga key to xterm modifier mapping (Left/Right Amiga as Meta? Alt as ESC prefix or 8-bit?). | XT from knowledge |
| Q8 | DA1/DA2 replies the xterm personality sends (must match `terminfo/`). | XT from knowledge |
| Q9 | Exact CSI bytes V47 medium mode sends for TAB / Shift+TAB / Up / Down. | RN-CH 47.1, DOSDOC SetMode, NDK `ReleaseNotes/shell-RelNotes` (nothing) |
| Q10 | CD_SETUPSCROLLBACK / CD_SETSCROLLBACKPOSITION semantics; which CMD_* besides READ/WRITE/CLEAR console.device accepts. | H-CON (definitions only), NDK-DOC, RN-CD: no documentation |
| Q11 | CON: options INACTIVE, NOCLOSE, NODEPTH, ALT, KEEPCLOSED, SHELL; editing keys in cooked mode; raw-mode Ctrl-\ and close-gadget report field contents; ACTION_FORCE number and argument layout; ACTION_STACK/QUEUE argument layout. | AM-5D, AM-65, RN-CH, H-DX, H-RX, DOSDOC, Libraries manual; the AmigaDOS Manual is not local and `amigadev.elowar.com/read` is empty |
| Q12 | Exact field contents of input event reports for window classes (11, 12, 13, 17, 18, 21). | RKM-95 (generic format only) |
| Q13 | Whether xterm personality honours 8-bit C1 decoded from UTF-8 (U+0080-U+009F). Recommendation: no. | XT from knowledge |
| Q14 | pcansi: CP437 byte 9B is taken as CSI (C-C1), OSC payloads print as text, DECSTBM ignored, HT/HTS fixed 8. Keep as today's behaviour or fix: an owner decision before tests pin it. | DCT l.830, l.848-862, l.582 |
