---
date: 2026-11-09
topic: RIP-7b handler glue -- level-9 block transfer on the Amiga
status: design (no code yet)
base: main 6d3df261
---

# RIP-7b handler glue: design

## Finish line

A RIP `!|9<ESC>` download sent through an UP-Term console window ends as
`ENVARC:UP-Term/Downloads/<host>/<NAME83>` on rig 68, the screen shows no protocol garbage,
and the terminal is usable afterwards. One reachability check proves it through the real
handler (section 6).

## 1. Findings (evidence)

| Question | Answer | Evidence |
|---|---|---|
| Does the handler run ZMODEM for plain `rz` autodetect (`**\x18B00`)? | **No.** Nothing in `handler/` calls `zm_receive`, builds a `zm_line`, or sniffs ZRQINIT. The only Amiga ZMODEM is the standalone `C:rz`/`C:sz` over the program's own Input()/Output(). | grep zm_receive/zm_line/autodetect/B00 in handler/ (only ripxfer.c); `zm/zm_amiga.c:37-80` (aline, a_getc via WaitForChar+Read), `:210` zm_receive |
| ZMODEM autostart anywhere? | Only in DCTelnet (the client program's stream sniffer), not the console. | `thoughts/shared/research/2026-10-03_dctelnet-port.md:55`, `plans/2026-10-03-screens-and-dctelnet.md:74` |
| Who writes `ENVARC:UP-Term/Downloads` today? | **Nobody.** The handler never calls `vt_set_download` or `vt_set_download_sink`; the iTerm2 writer is an open item "shared with RIP-7b". | `thoughts/shared/s66-evidence/gfx-iterm2.md:24-25`; grep vt_set_download* outside tests: none |
| Is RIP even on in the handler? | **No.** `vt_set_rip` is never called outside the engine, so `cb.rip_block` cannot fire in a window today. | grep `vt_set_rip(` in *.c: only `engine/rip_vt.c:485` (definition) |
| Is `cb.rip_block` wired in the window callbacks? | No. `vtwin_callbacks` sets reply/bell/title/.../music only; it memsets the rest to 0. | `render/vtwin.c:997-1012` |
| Where do program bytes enter the engine? | ACTION_WRITE -> `output()` -> `vtwin_write()` -> `vt_feed()`. The packet is replied right after. | `handler/vtcon_handler.c:8571-8601`, `:3809-3825`; `render/vtwin.c:1287-1306` |
| Where do terminal-originated bytes reach the program? | `c->in` queue, drained by `service_reads()` into queued ACTION_READ packets; raw mode passes whole. Engine reports come in via `vtwin_host.reply`, raw input via `vtwin_host.input`. | `handler/vtcon_handler.c:4055-4101`; `render/vtwin.h:40-45` |
| Where does rip_block fire relative to the buffer? | Inside `vt_feed` -> `rip_run` -> `rip_pass` -> RIP parser -> `rip_on_cmd`; the rest of the buffer is parsed afterwards as RIP/ANSI text. No stop mechanism; `vt_feed` is void. | `engine/rip_vt.c:216-227`, `:411-468`, `:470-484`; `engine/vtengine.h:273` |
| Does the receiver need the bytes already in the buffer? | Not for protocol correctness: `zm_receive` opens with ZRINIT (`send_rinit`) before reading, so a ZRQINIT lost in the buffer remainder is harmless; the sender cannot send ZFILE before it hears our ZRINIT. The remainder must still not be RENDERED. | `zm/zmodem.c:773-781` |
| Handler threading | One handler process serves every window and tab; long blocking inside it freezes all of them. Worker-process precedent: `config_worker`, `watch_worker` with a msg port, CreateNewProcTags, PutMsg/WaitPort, CTRL_C to stop. | `handler/vtcon_handler.c:1489-1590` |
| Directory creation precedent | `CreateDir(CACHE_DIR)` | `handler/complete.c:500` |

## 2. Chosen data path

Handler-side receive in a **worker process**, fed from the ACTION_WRITE packets, answering
through the existing input queue. The engine gets one small root-cause change: it stops
consuming a buffer when the host says the bytes after the command are not terminal output.

```
 telnet client  --ACTION_WRITE-->  handler
                                     | normal: output() -> vt_feed
                                     | xfer:   packet held, pointer+len -> worker (no vt_feed)
                                     v
                           worker "vtcon rip receive"
                             ripxfer_receive(zm_line{getc: next held chunk, write: ->handler})
                             sink = dl_save (ENVARC:UP-Term/Downloads/<host>/<name>)
                                     |
            handler <-- msg: "send these bytes" -- worker.write
            appends to c->in, service_reads() -> ACTION_READ -> telnet client -> BBS
            handler <-- msg: "done rc, delivered" -- worker exit -> back to normal feed
```

Why this and not the alternatives:
- **Blocking zm_receive inside the handler** (getc pumps the handler's own packet port): freezes
  every window/tab for the whole transfer and needs a deferral queue for every other packet
  type. Rejected.
- **Let the client program do it** (DCTelnet-style sniffer): the console cannot reach the
  client's code; RIP is parsed in the console's engine. Not applicable.
- **Write a push-model ZMODEM receiver**: reinvents `zm_receive`. Rejected (reuse rule).

Flow control comes for free: the handler does not reply an ACTION_WRITE packet while the
worker still reads its bytes, so the client's Write blocks exactly like a slow serial line.

## 3. Reused, unchanged

- `ripxfer_receive`, `ripxfer_name83`, `RIPXFER_MAX` (`handler/ripxfer.c/.h`).
- `zm_receive` and the whole protocol (`zm/zmodem.c`).
- `dl_clean_name` (already applied by the engine before `rip_block`).
- `zm_line` contract (`zm/zmodem.h:23-27`); the `aline` buffering shape from `zm/zm_amiga.c:39-71`
  (pos/len over a chunk, refill on empty) -- same structure, chunk source differs.
- Worker lifecycle pattern of `watch_start`/`watch_stop` (`vtcon_handler.c:1525-1590`).
- `c->in` + `service_reads()` for every byte the receiver sends.
- `vt_dl_sink` signature for the writer; the same writer becomes the iTerm2 sink (closes the
  open item in `gfx-iterm2.md:24`).

## 4. Edits by file

### engine (root cause: the engine must give back the bytes that are not its own)
- `engine/vtengine.h`: add
  `void vt_feed_stop(vt_term *t);` -- callable only from inside a callback during `vt_feed`;
  `vt_feed` returns after the byte that ended the current command.
  `long vt_fed(const vt_term *t);` -- bytes of the last `vt_feed` buffer consumed (== len unless stopped).
  Document on `rip_block`: "call vt_feed_stop to take the rest of the stream".
- `engine/vtengine.c`: `vt_term` gets `int feed_stop; long fed;`. `vt_feed_core` honours the
  flag at its byte loop (only reachable via RIP today, but generic).
- `engine/rip_vt.c`: `rip_run` / `rip_pass` / the parser call must stop at the command's last
  byte when `t->feed_stop` is set and set `t->fed = <index after it>`; `vt_feed` clears
  `feed_stop` on entry, sets `fed = len` when not stopped. Check `engine/rip_parse.c`'s byte
  loop takes a stop (it processes a chunk `buf+from..i`; the stop has to come back out of it
  with a position). Auto mode (`rip_auto_byte`, held line): the `!|9` line is replayed from
  `ad->line`; the stop index is then the end of the current input byte (the held line ends at
  the newline that is `buf[i]`), so `fed = i + 1`.

### render
- `render/vtwin.h`: `vtwin_host` gets `void (*rip_block)(void *user, int mode, int proto, int type, const char *name);` (may be 0).
- `render/vtwin.c`: `vtwin_callbacks` sets `cb->rip_block = cb_rip_block` forwarding to
  `w->host->rip_block` when set. `vtwin_write` returns `long` = `vt_fed(w->t)` (callers that
  ignore it are unchanged in behaviour). `device/upcon_unit.c:222` keeps ignoring it (no RIP xfer there).

### handler
- **New `handler/dlsave.c/.h`** (pure-ish, DOS only): `int dl_save(const char *host, const char *name, const vt_u8 *data, long len)`:
  builds `ENVARC:UP-Term/Downloads/<host>/<name>` (host "" -> "local"), `CreateDir` each missing
  level (`ENVARC:UP-Term`, `.../Downloads`, `.../<host>`; precedent `complete.c:500`),
  `Open(MODE_NEWFILE)`, `Write`, returns 1 when all bytes went. Overwrites an existing file
  (same as iTerm2 semantics; see unknown U4). Path builder split out as a pure function
  `dl_path(char *out, int n, host, name)` for the host test.
- **New `handler/ripglue.c/.h`** (pure C89, no OS calls; the testable state machine):
  - `ripglue_arm(g, mode, proto, name)` -> returns 1 when this block is one we receive
    (mode = BBS sends, proto = ZMODEM; section 7 U1/U2), else 0 (handler notifies "not supported").
  - the chunk queue the worker's `getc` drains (pointer/len of the held packet, or the
    remainder of the triggering buffer), `ripglue_getc(g, timeout)` returning the next byte
    or "need chunk"; `ripglue_put(g, b, n)` collecting bytes to send.
  - the worker body `ripglue_run(g, line, sink, host)` = `ripxfer_receive(...)` -- the glue
    adds no protocol logic.
- `handler/vtcon_handler.c`:
  1. Window setup (next to `vt_set_file_reader`, `:3484`): `vt_set_rip(c->w.t, <setting>)`
     (prerequisite, owner decision O1) and `vt_set_download_sink(c->w.t, dl_save, <host>)`
     (iTerm2 gets the writer too).
  2. Host callbacks: `rip_block` handler `con_rip_block(c, mode, proto, type, name)`:
     if `ripglue_arm` accepts and no transfer runs: `vt_feed_stop(c->w.t)`, set
     `c->xfer = 1`, start the worker (CreateNewProcTags like `watch_start`, name
     "vtcon rip receive", stack 16384 as `zm_amiga.c:34`).
  3. `case ACTION_WRITE` (`:8571`): if `c->xfer`, do NOT call `output`; queue the packet to the
     worker (pointer `dp_Arg2`, len `dp_Arg3`) and do not reply; it is replied when the worker
     hands the chunk back. Otherwise call `output`; if `vtwin_write` returned `fed < len`
     (the rip_block stopped it mid-buffer), copy the remainder into the worker's first chunk
     (the packet is replied now as usual -- the copy owns the bytes).
     OPOST path (`:8588-8596`) the same: the transfer is raw, so with `c->xfer` the OPOST
     translation is skipped entirely.
  4. Main loop: the worker's port signal joins the Wait mask (`:9097-9102`). Messages:
     `XFER_SEND(b, n)` -> append to `c->in` (bypassing the line editor, as raw input via the
     `vtwin_host.input` path), `service_reads(c)`, reply; `XFER_CHUNK_DONE(packet)` -> `reply(p, len, 0)`;
     `XFER_DONE(rc, delivered)` -> `c->xfer = 0`, `cb.notify`-style status line
     ("RIP download: NAME saved" / "failed: <rc>"), re-render.
  5. Close/exit while `c->xfer`: CTRL_C to the worker (getc returns `ZM_LINE_ERROR`), wait for
     XFER_DONE, reply every held packet with -1 -- as `watch_stop`.
  6. Keys typed during the transfer: Ctrl-X x5 / a menu "Cancel transfer" -> worker getc gets
     `ZM_LINE_ERROR` (zm_receive then cancels). Other keys are dropped while `c->xfer` (they
     would corrupt the ZMODEM stream). Raw mode required: if the reader is cooked, the handler
     forces raw delivery of `c->in` during the transfer (ZRINIT has no newline).
- `Makefile`: add `ripxfer.o zmodem.o dlsave.o ripglue.o` to the handler link; host test
  targets for `test_ripglue`, `test_dlsave`.

### Worker zm_line (Amiga)
- `getc(timeout_ms)`: byte from the current chunk; when empty, give the chunk back
  (XFER_CHUNK_DONE) and wait on its port for the next chunk with a timer.device request of
  `timeout_ms` (own signal; `zm_amiga.c` uses WaitForChar, which a worker without a stream cannot).
  CTRL_C -> `ZM_LINE_ERROR`.
- `write(b, n)`: copy into a MEMF_PUBLIC message, PutMsg to the handler, WaitPort for the
  reply (so order is kept and the buffer can be reused). Batch: zm writes are small; one
  message per write call is fine at telnet speeds (measure on rig, U5).

## 5. Host tests

- `tests/test_ripglue.c`:
  - arm: (mode 0, ZMODEM id, "x.icn") accepted; other proto/mode refused (refusal counted).
  - feed-stop through the real engine: `vt_new` + `vt_set_rip(t,1)` + `cb.rip_block` calling
    `vt_feed_stop`; one buffer = RIP `!|9` line + a real ZRQINIT + 2 KB of ZMODEM bytes from
    `zm_send` (as `test_ripxfer.c` does). Assert `vt_fed(t)` == offset just after the command,
    `vt_rip_blocks(t) == 1`, and the grid shows no `**` / `B00` (the remainder was not drawn).
    Same with RIP auto mode (2). Mutation: drop the stop check in `rip_run` -> grid assertion fails.
  - full glue loop on host: a fake handler loop (single-thread, the worker's getc/write
    replaced by queues) moves the remainder + later chunks into `ripglue_run`, the sender
    `zm_send` on the other side; fake sink receives (host, "MYPICTUR.ICO", bytes) identical to
    the source. Sentinel: sink call count 1 and `vt_rip_blocks == 1`.
- `tests/test_dlsave.c`: `dl_path` -> `ENVARC:UP-Term/Downloads/local/A.TXT` for host "",
  `.../bbs.example.org/A.TXT` for a host; overlong host/name truncated, never a `/` or `:`
  from input (already sanitised upstream; asserted again).
- Existing `tests/test_ripxfer.c`, `test_iterm2.c` stay green (no behaviour change there).
- Run: `ONLY=ripglue`, `ONLY=dlsave`, `ONLY=ripxfer`, `ONLY=rip` only (section 6a rule).

## 6. Reachability: the real handler on rig 68

Entry point = the shipped handler: a program Writes to an UP-Term console window.
Sentinel = the file on disk plus a handler counter.

- Add a counter `c->xfer_done` exposed through the existing debug/status path (whatever the
  handler's rig probes already read; if none fits, a `VTCON_DEBUG` `dbg("RIPXFER rc/delivered")`
  line, read from the debug log -- text check, no screenshot).
- Rig 68 steps (`rig.py start --max --purpose "<session>: RIP-7b level-9 receive check"`):
  1. Build the handler + `C:sz` from the branch; install on rig 68.
  2. RIP on for the window (setting from O1, or `echo "*e[2!"` to switch it on in-stream).
  3. `S:rip9` script, run with `execute S:rip9` from an UP-Term window:
     ```
     type S:rip9.txt          ; one line: !|9<ESC>0<Z-proto>000000hello.bin + CRLF
     sz RAM:hello.bin         ; the standard sender over the same console, raw
     ```
     (A script, not two typed lines: the worker's ZRINIT lands in `c->in` at once; only `sz`
     must read it, not the shell.)
  4. PASS: `ENVARC:UP-Term/Downloads/local/HELLO.BIN` exists and `cmp`/`sum` equals
     `RAM:hello.bin`; the debug line shows rc 0 delivered 1; the window shows no `**B0000`
     text; afterwards the shell prompt answers a typed `echo ok`.
  5. Negative: same with a 5 MB file -> refused (RIPXFER_MAX), no file, terminal usable.
  6. Cancel: start with a 2 MB file, close the window mid-transfer -> no hang, no partial file,
     handler still serves another window.
  7. iTerm2 riding along: `OSC 1337;File=name=dGVzdA==;inline=0:...` -> file appears under
     `ENVARC:UP-Term/Downloads/local/`.
- Real BBS check (owner, later): a RIP BBS that offers icon download over ZMODEM via telnet
  client in UP-Term. Unverified until done.

## 7. Unknowns and owner decisions

**Owner answers (s68, 2026-11-09):**
- O1 DECIDED: a per-profile RIP setting, default Auto; the handler calls `vt_set_rip(t, 2)` for Auto (0 Off, 1 On). The setting row goes through the existing settings_def / prefs path like other per-profile rows.
- O2 DECIDED: `<host>` = host part of OSC 7 (`vt_cwd`), else "local".
- U2 stays open: accept any protocol id when the ZMODEM start header follows; no hard-coded id until the spec text is in the repo.

- **O1 (owner): how RIP gets turned on in the handler.** Today it never is. Options: a profile
  setting `RIP=Off|Auto|On` (recommended default Auto = `vt_set_rip(t,2)`), or only the
  in-stream `ESC [ 2 !`. Without this, nothing in this plan is reachable from a real session.
- **O2 (owner): `<host>` for the download path.** The console does not know the BBS. Options:
  (a) "local" always; (b) the host part of OSC 7 (`vt_cwd`) when a program set it, else
  "local" (recommended -- uses what exists); (c) a new ACTION_VTCON packet so a telnet client
  tells the console its host.
- **U1 (unverified): mode values.** The engine's comment says mode 0 = BBS sends, 1 = BBS wants
  (`engine/vtengine.h:171-172`). No spec text in the repo (grep BLOCK_MODE/ENTER_BLOCK: only
  our own files). From memory of RIPscrip 1.54 `RIP_ENTER_BLOCK_MODE`: args mode:1 proto:1
  file_type:2 res:4 filename -- matches the parser; mode semantics unverified.
- **U2 (unverified): protocol ids.** From memory of RIPscrip 1.54: 0 XMODEM (checksum),
  1 XMODEM (CRC), 2 XMODEM-1K, 3 XMODEM-1K-G, 4 Kermit, 5 YMODEM (batch), 6 YMODEM-G,
  7 ZMODEM. **Conflict:** `tests/test_ripxfer.c:162` uses proto `5` for a ZMODEM transfer.
  Either the test's id is arbitrary or my memory is wrong. Need the 1.54 spec text
  (`RIPSCRIP.DOC`) before `ripglue_arm` hard-codes an id. Safe interim: accept the ZMODEM id
  AND also accept any proto when the first chunk carries ZRQINIT (`**\x18B00`); refuse with a
  notify otherwise.
- **U3: mode 1 (upload to BBS).** Out of scope (receive only, as RIP-7b says); refused with
  a notify. Confirm with owner if uploads are wanted later (zm_send exists).
- **U4: name clash.** Overwrite or keep both (`NAME.1`)? Default here: overwrite, as `rz OVERWRITE`
  is opt-in on the CLI tool but the download folder is a cache. Owner may veto.
- **U5: speed.** One message per `zm_line.write` and per chunk round trip; fine at telnet rates
  in theory, measure on rig 68 (ZM_STATS-style counters exist in `zm_amiga.c:43-45`).
- **U6: auto-mode stop index** in `rip_run` when the line is held (`ad->nline`) -- the replay
  path `rip_auto_done` must report the stop; verify in the host test (section 5).
- **U7: raw requirement.** A telnet client reading the console cooked would never see ZRINIT.
  Real clients read raw; the handler forcing raw delivery while `c->xfer` is a design choice
  to confirm on rig.
