---
date: 2026-10-04
topic: What one XCON window allocates, where each block comes from, and what was cut
tags: [research, memory, handler, upconf, lineedit, completion, W17]
status: draft
---

# Per-window memory of an XCON: window

Measured on the stock A1200 rig (2 MB chip, no fast RAM, about 690 KB free
after boot) with `tests/amiga/allocwatch.c`: one XCON: window, 640x200,
77x20 cells, no profile file, costs about 165 KB in the handler task (ROM
CON: 43 KB). The AllocMem calls the XCON task made while the window opened,
in order (AllocVec sizes carry 4 bytes of header):

`66130, 56244, 7094, 2004, 25 x 1252, 2052, 4782, 3 x 11088, 51204, 16248`

## The map

Sizes on the right are from the 68020 vbcc build itself: a scratch file that
includes `handler/vtcon_handler.c` (or `engine/vtengine.c`) and emits
`sizeof` of each struct as data, compiled with the handler's flags and read
back from the assembler output. Each logged size is the struct + 4.

| Logged | What | Source | Lifetime |
|-------:|------|--------|----------|
| 66130 | `con`, the per-window state (66126) | `handler/vtcon_handler.c:5350` `AllocVec(sizeof(con))` in `handler_main` | the process |
| 56244 | `c->conf`, the parsed profile file (`upconf`, 56240) | `handler/vtcon_handler.c:5404` | the process |
| 7094 | `vt_term` (7090) | `engine/vtengine.c:5035` `vt_new` via `render/vtwin.c:732` `vtwin_attach` | the window |
| 2004 | the scrollback ring, 500 pointers | `engine/vtengine.c:5056` (`sb_lines` 0 = 500, `render/vtwin.c:733`) | the window |
| 25 x 1252 | screen lines, 32 + 76 x 16 bytes (77 cells of 16 bytes) | 20 from `alloc_screen` `engine/vtengine.c:5011`; 5 from `line_new` on scroll `engine/vtengine.c:979` (the shell's first output scrolled 5 lines into the scrollback) | the window / the scrollback |
| 2052 | glyph table, 256 x 8 bytes (topaz 8) | `render/amiga_render.c:973` `extract_glyphs` from `vr_init` | the window (painter: speed path, kept) |
| 4782 | the menu strip | `handler/vtcon_handler.c:1396` `CreateMenusA` in `menu_add` (called right after `vtwin_attach`, `:2166`); GadTools allocates the strip as one block. **Inferred from the order and the size, not confirmed on the rig.** | the window |
| 3 x 11088 | `comp`, `check`, `hist` completion requests (`struct complete_req`, 11084 each: `names[8192]` + `extra[2048]` + three 256-byte words) | `handler/vtcon_handler.c:2597-2601` `ensure_worker`, from `history_load` at open (`:2169`) | the process |
| 51204 | the history file buffer, `HISTORY_KEEP * 2 * 256` | `handler/vtcon_handler.c:2819` `history_load` | until the load answers (`finish_completion`, freed) |
| 16248 | the completion worker process: `NP_StackSize 16000` + its Process | `handler/complete.c` `complete_start` `CreateNewProcTags` (made in the XCON task, so logged there) | until the worker ends |

Inside the 66126 bytes of `con`:

| Bytes | Member |
|------:|--------|
| 36116 | `le` (`le_line`): `hist[100][256]` 25600, `undo[8]` 8256, `buf` 1024, `before_search` 1032 |
| 8320 | `w` (`vtwin`), of which `vr_render` 6920 |
| 8192 | `menu[COMPLETE_NAMES]`, the last completion's names |
| 5196 | `ld` (`ldisc`), the termios line discipline |
| 4096 | `in[IN_MAX]`, bytes ready for Read (hot path) |
| 2048 | `obuf`, OPOST output of one chunk (hot path) |
| 512 | `hist_queue` |
| ~1650 | the rest |

Inside the 56240 bytes of `upconf`: `val[8][32][160]` 40960, `key[8][32][32]`
8192, `note[6144]` + indexes 640, `prof[8][32]` 256. The file it is parsed
from is at most `UC_MAX_FILE` = 16384 bytes (the handler refuses to read
more, the editor to write more): the table was 3.4 times the largest file
that can fill it.

### Allocated by the code but not in the measured list

- `handler/vtcon_handler.c:751` `watch_start`: a second `upconf` (56244) per
  window, the live-update watcher's table, and the watcher process
  (`NP_StackSize 8192`, alive as long as the window). Neither shows in the
  logged list, though both are made in the XCON task right after
  `c->conf`; the config worker (`:710`, 8192 stack, transient) is missing
  too. Either the rig ran a build from before live updates (2026-10-03),
  or the list was cut. **Rig: re-run allocwatch with MIN 1000 against
  this branch's build to settle it.**
- `handler/vtcon_handler.c:1566` `save_ask`: a third `upconf` (`save_work`)
  and `comp->data` (16385) on the first Save settings to profile, kept to
  the end of the process; `:1589` `theme_ask` keeps `comp->data` the same
  way; `:1604` `theme_apply` a fourth `upconf` for the moment it parses a
  theme.

### Not per open, but the biggest number

A scrollback line is the full row of 16-byte cells (1252 bytes at 77
columns): 500 lines of scrollback are 626 KB per window once full. That is
what ends a 2 MB machine after real use, not the open. Shrinking it means
work per scrolled row in the engine (copy the used cells into a smaller
block), which the speed race forbids without a measured design. Left open
(see the end).

## What was cut, and why it cannot slow conbench

conbench times writes: `vtwin_write` -> engine parse -> painter / row scan
-> glyph tables. None of the changes below touch that path.

1. **`upconf` packed** (`config/upconf.c`). Keys, values and comments live
   packed in one pool of `UC_MAX_FILE` bytes (an entry in the pool never
   takes more than its line in the file: `key\0value\0` against
   `key=value\n`, a comment plus NUL against the comment plus newline), with
   a 16-bit offset per key slot. Caps unchanged (8 profiles, 32 keys, 31-byte
   keys, 159-byte values, 160 comment lines of 6144 bytes); a set that would
   overflow the pool marks overflow, which only a table that could not be
   saved within `UC_MAX_FILE` anyway reaches. The table stays pointer-free,
   so `memcpy` copies (prefs_stage, the handler) still work. 56240 -> 17844
   bytes. Read at open, on a profile switch, a live update, a save: never
   per byte written.
2. **The watcher's table only while a change is on its way**
   (`handler/vtcon_handler.c` `watch_worker`/`watch_take`). The watcher
   allocates a table when the file changes, reads into it and hands it
   over; the window keeps it as its own and frees the one it had
   (`upconf_adopt`). One table per window instead of two. Packet loop:
   unchanged (the take runs where it ran).
3. **Save and theme buffers live for the operation only**: `save_work`
   becomes the window's table when the save lands (adopted, no copy) or is
   freed when it fails; `comp->data` is freed when the worker answers.
4. **Line editor history and undo packed** (`handler/lineedit.c`). History:
   one growing buffer of NUL-terminated lines (same 100 lines of up to 255
   bytes, same eviction of the oldest) + 100 offsets; undo: one growing
   buffer of the eight snapshots, each only its length. Nothing allocated
   until a line is entered; `le_free` at close. Keys in cooked mode only:
   conbench never reaches it (and Up/Down/suggest do the same comparisons
   through one offset).
5. **Completion requests sized to their job** (`handler/complete.c`): the
   names (8192) and the shell's words (2048) follow the request only for
   `comp` (Tab, ASL, theme, save); `check` and `hist` are 800 bytes. Each
   request is made when first needed (`hist` at open, `check` on the first
   typed word, `comp` on the first Tab). `c->menu` (8192) is made with
   `comp`.
6. **The history file buffer sized to the file** (`complete.c`
   `history_load`): the worker allocates file size + 1 (the old 51200 when
   the size is unknown); the window frees it as before.

## Result (68020 vbcc sizes, same probe)

| Block | Before | After |
|-------|-------:|------:|
| `con` | 66126 | 24402 (`le` 36116 -> 2580, `menu` 8192 -> pointer) |
| `c->conf` | 56240 | 17844 |
| watcher's table (if allocated, see above) | 56240 | 0 until the file changes, then it replaces `c->conf` |
| `check` + `hist` requests | 2 x 11084 | 2 x 852 |
| `comp` request + menu | 11084 + (8192 in `con`) | 0 until the first Tab, then 852 + 10240 + 8192 |
| history file buffer (transient) | 51200 | the file's size + 1 |
| history + undo (in `le`) | 33856 fixed | the bytes of the lines entered |

Per window at open, in the handler task, from the logged list: 66130 +
56244 + 3 x 11088 = 155,638 bytes become 24406 + 17848 + 2 x 856 =
43,966: **about 111 KB less** (with the history's 51 KB transient cut to the
file's size besides). If the watcher's second table is allocated on this
rig (the code says it is), **another 56 KB**: about 167 KB a window. Not
cut: the engine's grid, scrollback ring and lines (34 KB here), the glyph
table, the menu strip, the processes.

Host sentinel (`make test ONLY=winmem`, 64-bit host sizes): one window's
engine blocks + line editor + profile table, 145,501 bytes before, 73,633
after, bound 76,000. The 68k build fails at compile time if `con`,
`upconf` or a request without lists climbs past 26000 / 18500 / 1024
(`handler/vtcon_handler.c`, `con_size_bound`).

## Status

- [x] M1 map (this file)
- [x] C1 upconf packed + host tests (64b73e5)
- [x] C2 watcher / save / theme lifetimes (95ec1d4)
- [x] C3 line editor history and undo packed + host tests (3651b98)
- [x] C4 completion requests, menu, history buffer, 68k size bounds
- [x] C5 host sentinel: bytes per window open
- [ ] rig: allocwatch on this build, MIN 1000 (the watcher table and the
      menus' 4782 confirmed; the per-window total in Avail)
- [ ] rig: the lifetimes only the rig runs -- Save settings to profile and
      a live Prefs Use still apply; Tab, the KingCON window, the command
      colouring and the saved history still work; an AUTO window closed
      and opened again keeps its history
- [x] scrollback lines at full width (626 KB at 500 x 77 cells): W23 packs them, 1296 -> 36-93
      bytes a line on the 68k (plan ledger W23)
