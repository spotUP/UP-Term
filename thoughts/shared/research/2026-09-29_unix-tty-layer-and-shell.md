---
date: 2026-09-29
topic: The Unix tty layer on AmigaOS 3.x (ixemul termios, signals, ptys) and what a Unix-style shell must solve
tags: [research, ixemul, termios, pty, fifo, sigwinch, job-control, shell, pdksh, tcsh, abcsh, clib2, clib4, libnix, aros]
status: final
---

# The Unix tty layer on AmigaOS 3.x, and the shell

Scope: phase P0 and phase S0 of `thoughts/shared/plans/2026-09-28-vtcon.md`. I read the source, and
every claim below cites a file and line under `build/research/` or gives a URL. **UNVERIFIED** marks a
claim I could not confirm from source or from a primary document.

Sources downloaded to `build/research/`:

| Dir | What | Origin |
|---|---|---|
| `ixemul-48.2/ixemul/` | ixemul **48.2** source, `version.in` = `48.2,6.4.01` | https://sourceforge.net/projects/amiga/files/ixemul.library/48.2/ (`ixemul-src.lha`) |
| `ixemul-sf-r64/` | ixemul SVN trunk r64 = **63.1** line (Bernd Roesch "bernd_afa", Diego Casorran), 68k + MorphOS | `svn export https://svn.code.sf.net/p/amiga/code/ixemul` |
| `ixemul-src/` | ixemul 47.2 (ADE, has the p.OS branches) | https://aminet.net/dev/gg/ixemul-src.lha |
| `fifolib-src/` | fifo.library + fifo-handler 38.4 (Matt Dillon, Joerg Hoehle) | https://aminet.net/dev/gg/fifolib-src.lha |
| `clib2/` | adtools/clib2 @ c0266593 (2021-05-14) | https://github.com/adtools/clib2 |
| `clib4/` | AmigaLabs/clib4 @ 1a37f0a2 (2026-08-03), OS4 only | https://github.com/AmigaLabs/clib4 |
| `libnix/` | AmigaPorts/libnix @ 0a1d18a0 (2026-09-26) | https://github.com/AmigaPorts/libnix |
| `abcsh/` | adtools/abcsh @ 145cae06 (2017-07-02) | https://github.com/adtools/abcsh |
| `aros/` | AROS `compiler/crt/posixc/{ioctl.c,posixc.conf}` @ 1a00437 (2026-09-29) | https://github.com/aros-development-team/AROS |
| `shells/` | pdksh 4.9 and tcsh 6.12 binaries (from `build/pkgs/`) | Aminet `dev/gg` |

The rig's library identifies itself as `ixemul 48.2 [68060, fpu, amigaos] (6.4.1)`
(`strings build/rig/ixemul.library`). That matches the 48.2 source's `version.in`, so all 48.2
behaviour below is the behaviour of the library the rig runs.

## Corrections to what we believed

1. **ixemul HAS pseudo-terminals, and has had them since 44.0.** "Implemented pseudo-terminals
   support using the FIFO device" (`ixemul-sf-r64/News.txt:472`, under `VERSION 44.0` at :393).
   `/dev/pty[p-u][0-9a-f]` and `/dev/tty[p-u][0-9a-f]` are mapped onto Matt Dillon's `FIFO:` handler
   (details in A2). Two earlier documents are wrong here: the plan's P0 line ("ixemul maps only
   /dev/tty and /dev/null") and `2026-09-29_68k-unix-ports-for-testing.md` ("has no pty support").
   The rig's binary contains the strings `/fifo/ptyXX/rweksm` and `/fifo/pty`.
2. **ixemul 48.2 does NOT use `CSI SP q` for TIOCGWINSZ.** It sends `ACTION_DISK_INFO`, then reads
   `ConUnit.cu_XMax/cu_YMax` through `id_InUse` (A1.2). The `CSI SP q` query appears in the
   **63.1** line (`ixemul-sf-r64/library/__tioctl.c:280`) and in clib4. It also appeared in the
   p.OS-only branch of 47.2. vtcon's `CSI SP q` answer is still useful for those consumers. On the
   rig's 48.2, however, the window size comes from the ROM console unit that
   `handler/vtcon_handler.c:1229-1255` opens on vtcon's window, and from `ACTION_DISK_INFO` at :1368.

## A. The Unix tty layer on AmigaOS 3.x today

### A1. ixemul.library

Versions: 48.2 (2003-08-02 on SourceForge, built 6.4.2001), 49.x, 50.0, 61.1, 62.1 and **63.1
(2010-05-05, `ixemul-63.1-m68k.lha`)**. See https://sourceforge.net/projects/amiga/files/ixemul.library/.
The 61-63 line is Bernd Roesch's (bernd_afa) and Diego Casorran's (diegocr) 68k continuation. It merges
MorphOS changes by Jacek Piszczek, "jDc/jacaDcaps" (`ixemul-sf-r64/changelog.txt:1-60`,
`__tioctl.c:97,114,119`). A "48.3 fork from megacz" is mentioned in `changelog.txt`. Its source was not
found (**UNVERIFIED**). Whether old 48.x GG binaries run unchanged on 63.1 is **UNVERIFIED**.

The key design fact: **ixemul keeps no line discipline.** The tty state is a few bits per file
descriptor (`f->f_ttyflags`: RAW, ICRNL, INLCR, OPOST, ONLCR, PKT). The console handler does the rest
through the two modes AmigaDOS knows, cooked and raw (`SetMode`).

#### A1.1 termios (`library/__tioctl.c`, 48.2)

- `tcgetattr`/`tcsetattr` are `ioctl(TIOCGETA/TIOCSETA[W|F])` (`general/termios.c:56-73`). `__tioctl`
  returns ENOTTY unless `IsInteractive(fh)` (:84).
- **TIOCSETA** (:133-174): `makeraw = (c_lflag & (ICANON|ECHO)) != (ICANON|ECHO)` (:140). If *either*
  flag is cleared, the handle becomes `SetMode(fh,1)`, which is raw with no echo. Otherwise it becomes
  `SetMode(fh,0)` (:147, :152). The source comment reads: "if ICANON is disabled, we disable ECHO too,
  no matter what the user wanted". So `ICANON+!ECHO` (a password prompt) and `!ICANON+ECHO` are both
  plain raw. **ISIG, VMIN/VTIME, IEXTEN and every c_cc character are ignored on set.**
- ICRNL and INLCR are remembered. `__read.c:127-145` maps CR to NL only in raw mode, because the
  console's cooked mode already does it. OPOST+ONLCR are remembered, and
  `__write.c:74-94` turns `\n` into `\r\n` only when RAW|OPOST|ONLCR are all set. **Performance note
  for vtcon:** `__write` splits every write into lines of at most 256 bytes, and in raw+ONLCR mode it
  sends each `\n` as its own 2-byte `Write` (`__write.c:76-94`). A full-screen redraw is therefore many
  small DOS packets. This belongs to ixemul, but vtcon pays the per-packet cost.
- **TIOCGETA** (:90-131) reports a fixed picture: `c_lflag = ECHOCTL | (raw ? 0 : ICANON|ECHO)`
  (:111). **ISIG is never reported**, and VSUSP=0, VQUIT=0, VERASE=8, VEOF=^\ , VINTR=^C
  (:115-127). The 63.1 line changed this to Linux-like values: ISIG|IEXTEN, VSUSP=^Z, VERASE=127,
  VEOF=^D (`ixemul-sf-r64/library/__tioctl.c:97-131`). It also propagates stdin's flags to stdout
  (:180-192).
- Old sgtty `TIOCGETP/TIOCSETP` map RAW/CBREAK onto the same SetMode (:176-212).
- `TIOCPKT` sets IXTTY_PKT, and `__read` prefixes a 0 byte (`__read.c:63`). `TIOCOUTQ` returns 0. Every
  other request, **including TIOCSWINSZ, TIOCSCTTY and TIOCNOTTY**, "is no error, but ... we don't
  take any actions" (:297-305).

#### A1.2 TIOCGWINSZ (`__tioctl.c:214-255`, 48.2)

The call sends `ACTION_DISK_INFO` (:227) and requires `dp_Res1 == -1`. It takes
`w = id_VolumeNode` as a `struct Window*` and `ios = id_InUse` as an `IOStdReq*` (it must be non-zero
and even), then reads `cu = ios->io_Unit`. It checks `cu->cu_Window == w` ("paranoid"), and sets
`ws_col = cu_XMax+1`, `ws_row = cu_YMax+1` (:247) and the pixel sizes from the window's borders. On
any failure it `break`s with **result -1 and errno 0**. This is the "DevCon notes" contract. AROS
documents that its own CON: does not honour it (`aros/ioctl.c:50-55`: "mainline CON stores a use count
in id_InUse").

- vtcon today: `ACTION_DISK_INFO` returns `c->win` plus a real ROM `console.device` CONU_STANDARD unit
  opened on vtcon's window (`handler/vtcon_handler.c:1229-1255, 1368-1379`). ixemul therefore gets the
  **ROM console's** idea of the grid, computed from `win->RPort`'s font. vtcon sets that font at
  `vtcon_handler.c:557`. **UNVERIFIED:** (a) that the ROM grid equals vtcon's grid (margins and cell
  size), and (b) that the ROM unit updates `cu_XMax/YMax` after a resize before the program re-queries.
  Both need a rig probe (D, P2).
- 63.1: writes `9B 20 71` and reads the `CSI 1;1;rows;cols SP r` reply. If `WaitForChar` fails it
  switches to raw around the query and parses it with `sscanf` (`ixemul-sf-r64/library/__tioctl.c:279-311`).
  The pixel values are faked as `col*8` and `row*8`.

#### A1.3 SIGWINCH (`library/ix_sigwinch.c`, 48.2)

SIGWINCH **is** delivered. At program start (`_main.c:344`, `ix_exec_entry.c:93`) ixemul sends
`ACTION_DISK_INFO` to `pr_ConsoleTask` (:71-77) to get the window. It then adds an **input.device
handler at priority 10** (:107; comment: "must be before console.device"). That handler raises
`SIGWINCH` for every `IECLASS_SIZEWINDOW` event whose `ie_EventAddress == window` (:47-49). The
handler is removed on execve and exit (`stdlib/execve.c:298`, `ix_startup.c:129`). No console request is
involved, only raw input event class 12 (`IECLASS_SIZEWINDOW`) from Intuition. For vtcon this works
as long as `DISK_INFO` returns the real window, which it already does. The window must also be
resizable. Tested on the rig: **no, UNVERIFIED**.

#### A1.4 Signals from keys

- **Ctrl-C only.** The console handler sends `SIGBREAKF_CTRL_C` to its break task. On the next
  context switch ixemul's `machdep.c:211-233` maps a newly received CTRL_C to `SIGINT`, sent to the
  **session's foreground process group** (`_psignalgrp(u_session->pgrp, SIGINT)`, :225). It then
  clears the Exec bit. `ix_sleep.c:86-109` and `select.c:157,289` wake on CTRL_C for the same purpose.
- **Ctrl-\ and Ctrl-Z generate nothing.** No code produces SIGQUIT or SIGTSTP from input. The only
  SIGQUIT and SIGTSTP code is in the kill path (`kern_sig.c:650-655, 968`). In raw mode the program
  receives the byte 0x1C or 0x1A as data. Ctrl-D/E/F reach an ixemul program only as raw
  `SIGBREAKF_*` bits, available through `SIGMSG` if it catches that (`machdep.c:235-245`).

#### A1.5 Job control

- `setpgid`/`getpgrp` are bookkeeping only (`misc.c:44-66`). `setsid` allocates a `struct session`
  (`misc.c:69-90`). **Every ixemul program started from an AmigaDOS Shell calls `setsid()`**
  (`_cli_parse.c:436`). A vfork child *shares* its parent's session (`vfork.c:136-138`).
- `tcsetpgrp` becomes `TIOCSPGRP`, which just stores `u_session->pgrp` (`__tioctl.c:257-266`). The
  "controlling terminal" is therefore the session struct and not the console. A second program on the
  same console that was started separately has its own session.
- **Stopping does not exist for the default action.** In `issig()` the stop branch for
  SIGTSTP/SIGTTIN/SIGTTOU/SIGSTOP with SIG_DFL is inside `#if notyet` and just `break`s. The signal is
  ignored (`kern_sig.c:840-852`). Only a *traced* process can be stopped
  (`stopped_process_handler`, `kern_sig.c:676-697, 760-782`). SIGTTIN/SIGTTOU are never generated.
- Result: pdksh and tcsh have their job-control code compiled in (strings "job control requires
  tty", "Stopped", "There are stopped jobs" in `shells/bin/ksh`). They can background with `&` and
  wait with `fg`, but **Ctrl-Z can never suspend anything.**

#### A1.6 /dev/tty, isatty, ttyname

- `open("/dev/tty")` becomes `"*"`, the process's console (`open.c:169-170`). `__plock.c:167-168` treats
  `console:`, `/console` and `/dev/tty` the same way. `/dev/null` and `nil:` are special-cased
  (`__plock.c:152-157`).
- `isatty(fd)` = `f_type == DTYPE_FILE && IsInteractive(fh)` (`library/isatty.c`).
  `ttyname()` "now always returns /dev/tty if the device is a tty" (`News.txt:187`).

#### A1.7 select, pipes, fork

- `select()` on a DOS handle sends an async `ACTION_WAIT_CHAR` with a 10 s timeout and re-arms it
  (`__fselect.c:50-100`). Write is always "ready". vtcon must answer `ACTION_WAIT_CHAR` asynchronously
  and correctly. When closing a pty with a pending select, ixemul aborts it with
  `ACTION_STACK "\n"` (`__close.c:44-48`).
- `pipe()` is an in-library ring buffer (`DTYPE_PIPE`, `pipe.c:61-84`), not `PIPE:`. AF_UNIX sockets
  (`bind/listen/accept/connect`, `unp.c:265-421`) and `socketpair` (`socket.c:233`) are also in-library.
- `fork()` = **ENOSYS** (`misc.c:156-161`). `mkfifo()` = ENOSYS (`misc.c:165`). Only `vfork()` exists.
  The child runs on the parent's frame until `execve`/`_exit`. The extensions `ix_vfork()` and
  `ix_vfork_resume()` let the parent resume early with shared data ("EXTREMELY careful",
  `vfork.c:408-475`).
- **Native Amiga commands exec'd by an ixemul program** run through `RunCommand()` inside the vfork
  child (`stdlib/execve.c:407-620`) with the arguments re-quoted (`quote()`, :680-713). stdin and
  stdout are the real DOS handle only for plain files. Pipes, sockets and **ptys** go through the
  `IXPIPE:` handler (`dup2_BPTR`, :636-652). The source comment says: "I do wish I knew why pty's need
  to go though ixpipe:. But if I don't do this, the output simply disappears" (:504-505).

### A2. Pseudo-terminals

**ixemul (44.0 through 63.1): BSD-style ptys on FIFO:.**

- `is_pseudoterminal()` accepts `/dev/` or `dev:` + `pty|tty` + `[p-u]` + `[0-9a-f]`
  (`__plock.c:76-90`). That gives `IX_NUM_PTYS = 6*16 = 96` pairs (`ixemul.h:175-181`).
- `open.c:171-190` rewrites the name to `/fifo/ptyXX/rweksm`. For the slave (`/dev/ttyXX`) the last
  flag is replaced by `c`, giving `rweksc`. It records master/slave occupancy in `ix.ix_ptys[]`, and a
  second open of the same side fails with EIO. `__close.c:63-72` frees the pair when both sides are
  closed, and `stat.c:165-174` reports them as `S_IFCHR`.
  - Defect in the source: `name = "/fifo/ptyXX/rweksm"; memcpy(name+7, ...)` **writes into a string
    literal** shared by every caller of the library (`open.c:176-178`). Two tasks opening ptys at the
    same moment race.
- What FIFO: gives the pair (`fifolib-src/sysdep/fifolib/fifo.doc:75-150`, `fifo-handler.c:276-334`):
  `rw` full duplex, `e` EOF on close, `k` keep data, `s` "SHELL mode": its own message port so `*`,
  `WaitForChar` and `SetMode` work (fifo.doc:143-150). `m` selects the master side. `c` means **cooked
  on the slave**: FIFO: does its own tiny line editing with echo to the master. That editing handles
  only CR/LF, BS (8) and EOF. `^C..^F` become `SIGBREAKF_CTRL_C..F` sent to every slave handle's
  signal port (`fifo-handler.c:1042-1082`). `SetMode` switches cooked and raw only if the handle was
  opened with `c` (`fifo-handler.c:728-760`).
- So an ixemul program on `/dev/ttyXX` sees the following:
  - `isatty` = true.
  - `tcsetattr` raw/cooked works.
  - Ctrl-C from the master becomes CTRL_C, which ixemul turns into SIGINT.
  - `/dev/tty` = `*` works thanks to `s`.
  - **TIOCGWINSZ fails:** FIFO: answers `ACTION_DISK_INFO` with `ERROR_ACTION_NOT_KNOWN`
    (`fifo-handler.c:844-848`), so ioctl returns -1.
  - **TIOCSWINSZ on the master is a no-op**, so no SIGWINCH reaches the slave's programs.
  - No Ctrl-Z or Ctrl-\ signals. termios c_cc is fixed. No VMIN/VTIME.
- Requirements: `L:fifo-handler` must be *run* from the startup-sequence ("THERE IS NO MOUNTLIST",
  fifo.doc:23-31), and `LIBS:fifo.library` must be present. Whether the rig's `sys.hdf` has them is
  **UNVERIFIED**: the image contains the string `fifo.library` but no `fifo-handler`.
- `openpty`, `forkpty`, `posix_openpt`, `grantpt` and `ptsname` do **not exist** in any ixemul
  version I read (a grep of 48.2 and r64 finds none). Ports must scan `/dev/pty[p-u][0-f]` the BSD way.

**Other Amiga platforms:**

- **AROS:** `posix_openpt`, `grantpt`, `ptsname` and `unlockpt` are declared but commented out
  (`aros/posixc.conf:649,664,665,680`). TIOCGWINSZ on DOS handles returns ENOTTY "until a
  terminal-control protocol is available" (`aros/ioctl.c:80-81`). No ptys.
- **AmigaOS 4 (clib2/clib4):** no pty functions (a grep of clib4 finds none). clib4 TIOCGWINSZ =
  `CSI SP q` on `Output()` in raw mode (`clib4/library/socket/ioctl.c:60-98, 229-247`).
- **MorphOS:** its ixemul is the source of the jDc changes and presumably has the same FIFO ptys.
  Whether a native MorphOS pty or terminal multiplexer exists is **UNVERIFIED**; a web search found
  nothing.
- **68k third-party:** `comm/tcp/ttyhandler.lha` ("Telnetd as a TTY: device", AmiTCP) is a console
  handler over a socket. That is prior art for a DOS console endpoint backed by a byte stream. I did
  not read its source. `FIFO:`/RemCLI (`fifolib-src/.../remcli.c`) is the canonical "remote shell on a
  handler pair". No handler named `PTY:` was found on Aminet (search "pty").
- **No GNU screen or tmux** on any Amiga platform (web search; the earlier report found 0 Aminet hits).

**What a screen port would hit on 48.2:**

| Area | Status |
|---|---|
| Opening ptys | Works through FIFO: |
| Sockets | AF_UNIX sockets exist in-library (screen can use sockets instead of `mkfifo`, which is ENOSYS) |
| `select` on the master | Uses WAIT_CHAR, which FIFO `s` supports |
| Window children | vfork+exec already fits (setsid, open slave, dup2, exec) |
| **Missing: fork** | screen's server daemonises with `fork()`; that must become `ix_vfork`/`ix_vfork_resume` or a no-detach design |
| **Missing: winsize** | TIOCGWINSZ/TIOCSWINSZ plumbing and SIGWINCH on the slave |
| **Missing: signals** | Ctrl-Z and job control |

tmux adds libevent, `forkpty` and a forked daemon on top of that. It is the harder port.

### A3. libnix and clib2 (brief)

- **libnix** (the bebbo/AmigaPorts toolchain's `-noixemul` libc): `tcsetattr` only works on fds 0-2
  and emulates ECHO-off by writing SGR 8 (concealed) and SGR 28. It never calls `SetMode`
  (`libnix/sources/nix/misc/termios.c:14-40`). `ioctl()` returns -1 for everything
  (`sources/nix/stdio/ioctl.c:5-9`). `isatty` = `IsInteractive` (`sources/nix20/stdio/__initstdio.c:61-71`).
  No TIOCGWINSZ, no SIGWINCH, no pty. Native programs (vim 9.1 and similar) do their own console
  handling.
- **clib2** (68k and OS4): its termios *emulates* a line discipline in the fd hook. It uses raw
  `SetMode` for non-canonical mode and emulates echo, ICRNL, OPOST/ONLCR itself
  (`clib2/library/termios_console_fdhookentry.c:77,232-373`, `termios_tcsetattr.c:60-68`). No
  TIOCGWINSZ, no pty.
- **clib4** (OS4 PPC only): TIOCGWINSZ/TIOCSWINSZ via `CSI SP q` (see above). Not usable on 68k.

## B. What a pty for AmigaOS could look like

A Unix pty is a **kernel line discipline plus a stored winsize between two fds**. On AmigaOS the only
place that can own shared per-terminal state is a **DOS handler**, because ixemul keeps per-fd bits
only. There are three levels a fix could live at:

1. **Reuse ixemul's mapping unchanged: install FIFO:.** Cost: zero code. Covered: ptys open, raw and
   cooked, Ctrl-C. Not covered: window size, SIGWINCH, Ctrl-Z/\, real termios. It is a test bed, not
   the answer.
2. **A `PTY:` handler of our own, with ixemul unchanged.** Covered: our slave can answer
   `ACTION_DISK_INFO` with a **fake `IOStdReq` -> `ConUnit` and a fake `Window`**
   (`cu_Window == w`, `cu_XMax = cols-1`, `cu_YMax = rows-1`). That satisfies 48.2's TIOCGWINSZ
   *without an ixemul change*, based on the code path at `__tioctl.c:214-255`. Not covered: nothing
   can open it as `/dev/ptyXX` unless ixemul maps the name. It cannot be mounted as `FIFO:` without
   breaking real FIFO users. TIOCSWINSZ is still a no-op, and there is still no SIGWINCH or SIGTSTP.
   It works only for programs that open `PTY:` by Amiga name.
3. **PTY: handler plus a patched ixemul (the root fix).** A terminal's state belongs in the terminal
   (the handler). ixemul should become a thin client of it. That is the Unix model, and it lets **the
   same protocol serve vtcon's XCON: and PTY:**, giving one line discipline instead of two.

Recommended shape for level 3:

- **Handler** `PTY:` (one process, many pairs). `PTY:<n>/m` is the master and `PTY:<n>/s` is the slave.
  Both are ordinary DOS filehandles.
  - The **slave** is a complete console endpoint: `ACTION_SCREEN_MODE`/`SetMode`,
    `ACTION_WAIT_CHAR`, `ACTION_CHANGE_SIGNAL` (break target), and `ACTION_DISK_INFO` with the fake
    ConUnit/Window for 48.2 binaries. It also answers `CSI SP q` itself from the stored winsize, for
    63.x/clib4-style callers. That last behaviour is a design choice: on Unix the query bytes would
    pass to the master. **UNVERIFIED** which behaviour screen-hosted programs expect.
  - The **master** holds the winsize (set through a new packet) and receives the slave's output.
  - The handler runs a **real line discipline** from a full `struct termios` per pair: ICANON editing
    (VERASE/VKILL/VWERASE), ECHO independent of ICANON, ISIG, VMIN/VTIME, OPOST/ONLCR, IXON.
  - For ISIG it signals the foreground process group. An Amiga handler can only `Signal()` tasks, so
    SIGINT goes out as `SIGBREAKF_CTRL_C`, which ixemul already maps. SIGQUIT and SIGTSTP need a new
    path (next list).
- **ixemul changes.** The library is LGPL (`COPYING.LIB`), and the 48.2 source matches the rig binary.
  1. Map `/dev/ptyXX` and `/dev/ttyXX` to `PTY:` instead of `/fifo/...` (`open.c:171-190`,
     `__close.c:44-72`, `execve.c:504-560`). Fix the string-literal race. Add `openpty`, `forkpty`,
     `posix_openpt`, `grantpt`, `unlockpt` and `ptsname` to libc.
  2. `__tioctl.c`: when the handler answers a new *termios packet*, pass TIOCGETA/SETA through
     whole. Otherwise fall back to today's SetMode mapping, so the change is backward compatible with
     CON:, KingCON and FIFO:. Also pass through TIOCGWINSZ, TIOCSWINSZ, TIOCSCTTY and
     TIOCSPGRP/TIOCGPGRP. **TIOCSWINSZ must `killpg(fg, SIGWINCH)`**.
  3. **Key signals beyond Ctrl-C.** The handler needs a way to post SIGQUIT and SIGTSTP to ixemul
     processes. One option is a documented ixemul entry point the handler calls with a pgrp and a
     signal, since `_psignal` is callable from interrupt context (`kern_sig.c:582`). Another is an
     "ixemul signal port" message. **UNVERIFIED** which is cleaner; ixemul has no existing hook.
  4. **Job control:** implement the `#if notyet` default-stop branch (`kern_sig.c:840-852`) using the
     existing `stopped_process_handler` machinery. Add SIGCONT resume, and SIGTTIN/SIGTTOU when a
     background pgrp touches the tty. The controlling tty must move from the per-process
     `u_session` into the handler (session id = handler pair).
  5. Keep `fork()` ENOSYS. Ports are written against vfork+exec.
- **vtcon (XCON:)** implements the same termios, winsize and signal packets. The line discipline
  module is then shared between XCON: and PTY: as a single source of truth. ixemul programs in a vtcon
  window get ISIG in raw mode (Ctrl-C in `less` or `nano` raw mode today is data only), ECHO without
  ICANON, Ctrl-Z and a correct winsize.
- **Distribution risk:** a patched `ixemul.library` replaces a system-wide library that every ixemul
  program uses. Version numbering (for example 48.4) and install policy are the owner's call.

## C. Shells

| Shell | 68k binary | Runtime | Notes |
|---|---|---|---|
| pdksh 4.9 | `dev/gg/pdksh-bin.lha` (1997, `bin/ksh` = `bin/sh`, 104 KB) | ixemul >= 47 | emacs editing mode; job-control code present ("Job control not enabled", "job control requires tty") |
| tcsh 6.12.00 | `dev/gg/tcsh-6.12.00-b.lha` (2004) | ixemul >= 48 (binary string) | "lots of stack"; OSTYPE amiga; "No job control in this shell" string present |
| bash | **source only** `dev/gg/bash-src.lha` (2.01) | - | no 68k binary found |
| zsh, mksh, dash, ash | **none found** on Aminet (`ZShell` in util/shell is an unrelated native shell) | - | - |
| abc-shell (abcsh) | **PPC OS4 only** on Aminet (`util/shell/abc-shell.lha` 53.3) | clib2 | pdksh-derived. Source has `#ifndef __amigaos4__` 68k paths (`abcsh/amigaos.c:727-760`); a 68k build is **UNVERIFIED** |

What the ixemul shells lack on AmigaOS:

- **fork.** They run on vfork+execve (A1.7). How the ADE builds handle subshells `( )`, `$(...)` and
  builtins inside pipelines without fork is **UNVERIFIED**. I did not read the pdksh-src diff.
- **Job control:** background and `fg` wait work. **Suspend never works**: default stop is not
  implemented, and no key produces SIGTSTP (A1.4, A1.5).
- **Ctrl-C** works through CTRL_C to SIGINT to the session's foreground pgrp. It reaches the right
  job only if the shell's `tcsetpgrp` ran and the console's break target is a process in that session.
- **Pipes:** ixemul-internal between ixemul programs. A native command in a pipeline gets an
  `IXPIPE:` handle, and `IXPIPE:` must be mounted (`execve.c:496-560, 636-652`).
- **Native commands:** `RunCommand` in the vfork child with re-quoted args (`execve.c:407-620`).
  Shell-resident AmigaDOS builtins (`cd`, `path`, `assign` as Resident segments) are not files. A
  Unix shell cannot run them unless it walks the Resident list or uses `SystemTags`.
- **Globbing:** the Unix shell globs. Separately, **every ixemul program started from an AmigaDOS
  command line globs its own arguments** (`_main.c:329-331` -> `__ix_cli_parse`). A native shell that
  launches ixemul programs as plain Amiga commands gets double expansion unless it quotes.
- **Paths:** ixemul maps `/volume/...` to `volume:`. Amiga filesystems are case-insensitive, and
  `O_CASE` exists only as an opt-in (`open.c`).

Prior art for a fork-less shell: abcsh (`abcsh/amigaos.c`).

| Construct | How abcsh does it |
|---|---|
| `pipe()` | a named `/PIPE/<id>/4096/0` (:143-179) |
| `fork()` | a stub that fails (:183-188) |
| Subshells | `CreateNewProcTags(NP_Entry=execute_child, ...)` sharing the interpreter (:727-760) |
| Commands | `SystemTags(..., SYS_UserShell)` or `LoadSeg`+`RunCommand` (:557-600) |

**What a new shell has to solve** (input for the S1 scope decision):

1. **Process model without fork:**
   - native commands via `SystemTags`/`CreateNewProc` (asynchronous for `&` and pipelines)
   - subshells as a new process running the interpreter on a copied tree (abcsh's approach)
   - Resident builtins (`FindSegment`)
2. **Two program worlds:** native (ReadArgs, exit codes 0/5/10/20, `SIGBREAKF_*`) and ixemul
   (argv, globbing in `__ix_cli_parse`, signals). The shell must quote for ixemul, map exit status and
   pass the environment through ENV:/local vars.
3. **Pipes:** `PIPE:` (queue-handler) or a pipe handler of our own. The AmigaDOS Shell's `_pchar`
   needs a third-party `pipe` command in the path (https://datagubbe.se/adosmyst/).
4. **Job control:**
   - achievable now: background jobs, a job table, `fg`/`bg` as "who owns the terminal", Ctrl-C to
     the foreground job
   - suspend: only for ixemul jobs, and only after the ixemul fixes in B
   - native Amiga tasks cannot be stopped safely (**UNVERIFIED**; no documented API exists)
5. **Line editing:** use vtcon's cooked-line editor (H7), or put the terminal in raw mode and edit in
   the shell (zsh ZLE-style). The latter needs a working termios round-trip, which 48.2 cannot give an
   ixemul shell beyond raw and cooked. A native shell calls `SetMode` itself.

## D. Recommended plan (ordered)

Tty layer (P):

1. **P1 Fix the premise.** Update plan items P0 and S0 and the ports report: ixemul has FIFO-backed
   BSD ptys, and 48.2 TIOCGWINSZ = DISK_INFO/ConUnit.
2. **P2 Rig probe of today's path in vtcon.** Run an ixemul `TIOCGWINSZ` test, resize the window and
   check the value again. Also verify that SIGWINCH arrives (A1.2, A1.3 **UNVERIFIED**) and that the
   ROM-console grid equals vtcon's grid. If they differ, replace the ROM unit in `DISK_INFO` with a
   vtcon-owned `ConUnit` whose `cu_XMax/YMax` vtcon updates on resize. That trades away the "callers
   can CMD_WRITE to it" property in `vtcon_handler.c:1231-1233`, so it is the owner's call if
   anything depends on it. Add a regression test.
3. **P3 FIFO: baseline on the rig.** Install fifo-handler and fifo.library, open
   `/dev/ptyp0` + `/dev/ttyp0` from a small ixemul program, and record what works. The ixemul
   cross-compiler availability in this repo is **UNVERIFIED**.
4. **P4 Define the termios/winsize/signal packet protocol** (one header). Implement it in vtcon first,
   as the shared line-discipline module.
5. **P5 `PTY:` handler** on the same module, including the fake-ConUnit `DISK_INFO` so unpatched 48.2
   gets the size.
6. **P6 Patch ixemul 48.2** (B, items 1-4): pty mapping to PTY:, termios pass-through, TIOCSWINSZ
   raising SIGWINCH, key signals, default stop, SIGCONT. Base choice: **48.2** (the rig's version and
   the ABI the GG binaries target) rather than adopting 63.1. 63.1's compatibility with old binaries is
   **UNVERIFIED**. **Owner decision:** shipping a modified system `ixemul.library`.
7. **P7 GNU screen port** on vfork (server daemonisation redesigned) as the multiplexer proof. Then
   tmux, which also needs libevent, forkpty and a forked daemon, or a multiplexer of our own inside
   vtcon. **Owner decision.**

Shell (S):

1. **S1 Owner scope decision:** native AmigaOS shell (like abcsh: SystemTags/CreateNewProc, runs both
   worlds) or ixemul program (like pdksh/tcsh: POSIX for free, bound to vfork and ixemul's tty).
   Recommended: **native**. A shell that must run Amiga commands, Resident builtins and ixemul
   programs has to own process creation, and ixemul's vfork model cannot run Resident builtins. It
   should speak the P4 packets for job control and termios.
2. **S2** Language subset: POSIX sh grammar, with zsh-class interactive editing on vtcon.
3. **S3** Process layer: async spawn, pipes over PIPE: or our handler, subshell-as-process, job table.
4. **S4** Job control on top of P4 and P6: foreground pgrp through the handler, Ctrl-Z for ixemul jobs.

## Sources (web)

- ixemul releases: https://sourceforge.net/projects/amiga/files/ixemul.library/ (48.2, 49.17, 50.0,
  61.1, 62.1, 63.1). SVN: https://sourceforge.net/p/amiga/code/HEAD/tree/ixemul/
- ixemul 47.2 (ADE): https://aminet.net/dev/gg/ixemul-src.lha
- ixemul 48.0 git mirror: https://www.ranger.innolan.net/github/amiga-ixemul/src/branch/master/README.md
- fifolib: https://aminet.net/dev/gg/fifolib-src.lha, https://aminet.net/util/misc/fifolib38_4upd.readme
- ttyhandler: https://aminet.net/comm/tcp/ttyhandler.readme
- pdksh: https://aminet.net/dev/gg/pdksh-bin.readme. tcsh: https://aminet.net/dev/gg/tcsh-6.12.00-b.readme
- abc-shell: https://aminet.net/util/shell/abc-shell.readme, https://github.com/adtools/abcsh
- clib2: https://github.com/adtools/clib2. clib4: https://github.com/AmigaLabs/clib4
- libnix: https://github.com/AmigaPorts/libnix
- AROS posixc: https://github.com/aros-development-team/AROS/blob/master/compiler/crt/posixc/ioctl.c
- `_pchar` pipes: https://datagubbe.se/adosmyst/
