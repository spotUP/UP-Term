# The test rig

An emulated Amiga (FS-UAE) that the host drives through wasabid. Every
`*_rig.py` here boots nothing itself: it needs the rig up (`rig.py start`) and
then types into it, reads its files and screen, and prints PASS or FAIL.
No real Amiga is needed to develop UP-Term; the rig is how the Amiga code is
tested. Do not run two scripts at once on one rig: they share its emulator (a second rig: see below).

Where this fits: step 3 of "Set up the whole thing" in the `upterm` repo's
README. Build the Amiga programs first (`make amiga` in vtcon).

## What the rig needs, and where each piece comes from

Nothing below is in any repository (all of it is licensed material or large);
`rig.py` stops with a one-line message when a piece is missing.

| Piece | Where `rig.py` looks | Where it comes from |
|---|---|---|
| FS-UAE | `/Applications/FS-UAE.app` (started with `open -a`) | fs-uae.net, macOS build. Not checked by `upterm-doctor`. |
| Kickstart 3.1 ROM (40.068, A1200) | `$UPTERM_ROM`, else `$UPTERM_ROOT/Up_Rough_Demo_System/web/maker/public/puae/kick40068.A1200` (another repo of the owner's, not in `repos.lock`); `--kick <file>` for one boot | Your own legal copy (Amiga Forever, or dumped from your A1200). Set `UPTERM_ROM=<file>`. |
| System disk | `~/Downloads/nyhd2.hdf`, copied once to `build/rig/sys.hdf` (1.5 GB) | A hard-disk image of a working Workbench 3.x install (the owner's own; there is no public source). Set `UPTERM_SYSTEM_HDF=<image>` (default `~/Downloads/nyhd2.hdf`). What it must hold, not verified for a smaller image: C:Assign, S:Startup-Sequence, Libs/Devs/Fonts, the Classes and MUI drawers the rig adds to `LIBS:`. |
| AmigaOS 3.2 tree (only for `--os32`) | `~/Downloads/AmigaOS-3.2-full (1)` (a host drawer, copied to `build/rig/os32`), ROM `~/Desktop/KICK_323.rom` (3.2.3) | An installed AmigaOS 3.2 tree and its Kickstart 3.2.3 ROM, from Hyperion (licensed). Set `UPTERM_OS32_TREE=<drawer>` and `UPTERM_OS32_ROM=<rom>` (defaults as shown). Also needs `media/AmigaOS3.2CD/ADF/Extras3.2.adf` inside the tree and `xdftool` (`pip install amitools`) to make SYS:L once. |
| wasabid | `build/rig/boot/wasabid`, copied from `build/wasabi/wasabid` (`make wasabi`, pinned) on every `setup`/`start` | https://github.com/creep-ltx/wasabi (MIT). The rig key is `rigwasabi`, set in `go` with `SetEnv wasabi.key`; it listens on 1234 + N - 1 (`paths.wasabi_port`). `--stock` no longer changes the daemon. |
| ncurses 5.5, less, nano, BitchX (what the userland and tmux/screen rigs and the tmux and screen builds use) | `build/rig/vtc/pkgs/ncurses-5.5-1-p-bin-m68k` and siblings | Aminet `dev/gg/ncurses-5.5-1-bin-m68k` (Geek Gadgets) unpacked there. The `-p-` in the directory name is the owner's; whether it marks a modified unpack is not verified. tmux-amiga and screen-amiga read the same directory (`NCURSES=`), so build the rig tree before them. |
| Python 3.14 | the host | `python3 --version` must be 3.14 or newer (the upterm doctor checks). The rig scripts use the standard library plus Pillow (`import PIL`, for screenshots: `python3 -m pip install pillow`; found by grepping the imports, not run on a clean machine). |

## Bring it up

    export UPTERM_ROM=<path to your Kickstart 3.1 A1200 ROM>   # once per shell
    python3 tools/rig/rig.py setup          # copies the system disk once, writes build/rig/vtcon-rig.fs-uae and the boot drawer
    python3 tools/rig/rig.py start --max    # boots it in the background, waits for wasabid (two retries)
    python3 tools/rig/rig.py status         # is wasabid answering on 127.0.0.1:1234 (rig 1)
    python3 tools/rig/ami.py exec "Version" # a command on the emulated Amiga; the first thing to try
    python3 tools/rig/rig.py stop           # kills this rig only (matched by its config path)

`start` prints the boot log (`build/rig/boot/boot.log`) when wasabid never
answers. Keep the machine flags the same between `setup` and `start`.

Machine flags (on `start`, and `setup` when the config alone changes):

| Flag | Machine |
|---|---|
| none | the owner's machine: `--max` (below). Every test rig runs as this, so no script falls back to the 020 |
| `--a1200` | A1200, 68020 + 68882, 8 MB fast + 64 MB Zorro III, RTG card, JIT, real CPU speed (timing runs only) |
| `--fast` | the `--a1200` machine at the host's speed |
| `--060` | 68060 (no MMU) + FPU at the host's speed, 128 MB Zorro III |
| `--max` | `--060` with 1 GB Zorro III. The default machine, so the flag is optional. |
| `--exact` | cycle-exact, no JIT (slow) |
| `--stock` | an A1200 as it left the factory: 2 MB chip, no fast RAM, no card (for benchmark comparisons) |
| `--os32` | AmigaOS 3.2 tree and ROM (see the table above) |
| `--kick <file>` | another Kickstart ROM for this boot |
| `--serial <path>` | serial port to a host pty (`getty_rig.py` does this itself) |
| `--ro` | DH0: read-only (stalls on "write protected" requesters; avoid) |

`dvmatrix.sh <kickstart file> <label>` boots the rig on another ROM and runs the console.device checks on it.

`rig.py install` copies `build/amiga/vtcon-handler` into the VTC: drawer
(host folder `build/rig/vtc`); a reboot is needed for DOS to load a new
handler. `rig.py aga` and `rig.py rtg` choose the Workbench screen mode from the
next boot.

Layout of `build/rig/` (gitignored): `sys.hdf` (DH0:), `boot/` (BOOTX:, boots
first: assigns, mounts XCON:, starts wasabid with key `rigwasabi`), `vtc/`
(VTC:, the binaries under test), `shots/` (screenshots), `serial.log`.

## A second rig, to run two jobs in parallel

`UPTERM_RIG=2` selects rig 2: the same FS-UAE backend and the same config
generator (`rig.py`), so results are comparable, but a separate machine:

| | rig 1 (default) | rig 2 (`UPTERM_RIG=2`) |
|---|---|---|
| directory | `build/rig` | `build/rig2` (own `sys.hdf`, `vtc/`, `boot/`, `shots/`, config) |
| wasabid | port 1234 | port 1235 (`go` starts `wasabid 1235 name rig2`) |
| FS-UAE process | matched by `build/rig/vtcon-rig.fs-uae` | matched by `build/rig2/vtcon-rig.fs-uae` |

```
UPTERM_RIG=2 python3 tools/rig/rig.py setup    # once: copies UPTERM_SYSTEM_HDF (1.5 GB) and rig 1's vtc/ (about 400 MB, read only)
UPTERM_RIG=2 python3 tools/rig/rig.py start    # then status / install / stop, as for rig 1
UPTERM_RIG=2 python3 tools/rig/ami.py ping
UPTERM_RIG=2 python3 tools/rig/<script>_rig.py
```

`paths.py` is the one reader of `UPTERM_RIG` (`paths.RIG`, `paths.WASABI_PORT, paths.WASABI_KEY, paths.wasabi_port()`);
`ami.py` takes its port from it (`AMI_PORT` still wins) and the scripts take
their `build/rig` directories from `paths.RIG`. `stop` kills only the FS-UAE
whose config path is that rig's. The two machines share nothing but the
read-only ROM and the source disk image; rig 2's `vtc/` is a copy taken at
setup, so run `UPTERM_RIG=2 ... rig.py install` after a rebuild. Two scripts
may run at once only if they run on different rigs. Rig 2 needs CPU for its own
JIT: timing benchmarks run while another rig works are not comparable.

Not yet on rig 2: `install_rig.py` still reads `build/rig/vtc` (an other
agent's file, left alone), and `dvmatrix.sh` still logs to `build/rig/shots`.
Amiberry (`/Applications/Amiberry.app`) could become a rig 3 with another
config writer; it was not needed because a second FS-UAE runs beside the first.

## MuForce: crash feedback on a rig

`rig.py muforce on|off|log` runs MuForce (Thomas Richter's Enforcer for the MMU; NDK 3.2R4 MuTools) on the
rig. It reports an access to address 0 or to missing memory, and a write to ROM, with the PC, the registers
and the task, and lets the program go on. Its `FILE` option (documented in `MuForce.guide`) writes the report
to `VTC:muforce/hits.log`, which is the host file `build/<rig>/vtc/muforce/hits.log`: the Mac and an agent read
it with no wasabid call.

    python3 tools/rig/rig.py start --max --mmu    # once: a 68060 with an emulated MMU (slower, no JIT)
    python3 tools/rig/rig.py muforce on           # copies MuForce + libraries to VTC:muforce, starts MuForce
    python3 tools/rig/ami.py exec "VTC:muhit"     # make build/amiga/muhit: one read of address 0 (muhit rom: a ROM write)
    python3 tools/rig/rig.py muforce log          # one line per hit: access, address, PC, task, CLI command
    python3 tools/rig/rig.py muforce log --raw    # the report itself
    python3 tools/rig/rig.py muforce off          # MuForce QUIT

- The default rig is 68060-NOMMU (the JIT needs that) and cannot run MuForce: `muforce on` stops with the
  `--mmu` hint. Restart without `--mmu` for the normal machine. With the JIT on the emulated MMU is not seen
  (`MuForce requires a working MMU`), so `--mmu` also turns the JIT off.
- Inputs, from the Mac (`paths.py`): `UPTERM_MUTOOLS` (default `~/Downloads/NDK3.2R4/MuTools`: MuForce,
  disassembler.library) and `UPTERM_MMULIB` (default `~/Library/CloudStorage/Dropbox/WB/Libs/mmu.library`, 42.8).
  The NDK does not ship mmu.library (only its MuManual); it is in Thomas Richter's MMULib archive.
- Licence: MuForce and mmu.library are under the THOR-Software Licence (v3 for MuForce, `MuForce.guide`):
  freely distributable, no payment, no modified redistribution. Nothing of it is in this repository or the
  kit; the rig copies it from the Mac. SegTracker is not installed (it is a separate Aminet tool): hits then
  carry the task and the CLI command name, which `muhit` shows as `Background CLI` / `VTC:muhit`.
- `make test ONLY=rig_muforce` tests the report parser and the arguments.

## The client, and a real Amiga

`ami.py` is the rig's client of wasabid (wasabi's own Python client, build/wasabi, behind the API the rig scripts use): `ping | info | exec "cmd" [secs] | shot out.png | get | put | uitree | ui | key | type | rexx | menus | screens | gclick`
(its docstring has the exact forms). It talks to the emulator by default. For
a real Amiga running wasabid:

    AMI_HOST=<the Amiga's address> AMI_PORT=<its port> AMI_TOKEN=<its wasabi key> python3 tools/rig/ami.py ping

(`AMI_PORT`, default 1234 + rig - 1.) Gaps are named errors (`ami.py gap: ...`): BREAK of the running command and the pointer position have no wasabi command; raw key codes, region grabs, gadgets by id and tick-timed input are mapped (the docstring says how). wasabid runs whatever it is sent: use the key and a trusted network (see tools/nas/README.md).


## Mac: amiga MCP for Claude Code

Claude Code in this repo gets an `amiga` MCP server (upstream amimcp, Apache 2.0,
pinned to 3145dee, Python standard library only, no venv) that drives a rig's
amiagent: run commands, read and write files, screenshots, input.

- Install: `make amimcp` (fetches the pinned commit into build/amimcp, checks the
  wrapper starts, prints the next step; a second run changes nothing).
  Remove: `make amimcp-clean`. Nothing outside the repo is touched.
- Next step: start Claude Code in the repo and approve the `amiga` project MCP
  server from `.mcp.json`.
- Target: rig 1 (127.0.0.1:7846, token `rigtoken`) by default. `UPTERM_RIG=2` or
  `3` selects that rig's port (the rule is paths.py's). A real Amiga: `AMI_HOST`,
  `AMI_PORT`, `AMI_TOKEN`, the same variables as ami.py. Set them in the shell
  that starts Claude Code.
- Token safety: the amiagent link is unencrypted and the token travels in clear.
  Use it on a LAN you trust only; never commit a real Amiga's token.
- Wrapper: `tools/rig/amimcp.py` (`--print` shows the chosen host and port);
  test: `make test ONLY=rig_amimcp`.

## Debugging on a rig: which tool answers which question

| Question | Tool | Example (rig 3; drop `UPTERM_RIG=3` for rig 1) |
|---|---|---|
| Which files, libraries and DOS calls does a program make, and which fail | wasabi snoop | `rig.py wasabi on; rig.py wasabi snoop start; ami.py exec "List SYS:"; rig.py wasabi snoop stop; rig.py wasabi snoop log 40` (`--task 'List*'` on start filters by task name; a `= fail (Error 205: Object not found)` line is the failing call) |
| What does the program print with KPrintF / RawPutChar | wasabi debug | `rig.py wasabi debug start; ami.py exec "VTC:myprog"; rig.py wasabi debug log` |
| The program died with a system alert: which alert, which task, which PC | wasabi guru | `rig.py wasabi guru` (the last alert, `T:lastguru`, and alert lines in the two logs). Only alerts that reach `Alert()`: a program that faults on a CPU trap is not caught here, MuForce is the tool for that |
| Did a task run out of stack, which tasks are alive | wasabi ps | `rig.py wasabi ps` (column FREE = stack headroom; `rig.py wasabi ps 'vsh*'` filters) |
| An illegal memory access (address 0, ROM write, non-existent memory) with registers and PC | MuForce | `rig.py start --mmu` (slower, no JIT), `rig.py muforce on`, run the program, `rig.py muforce log` |

Steps: `rig.py wasabi on` once, start the streams before the program, stop them after, read the logs from the host
(`build/rig<N>/wasabi/snoop.log`, `debug.log`); `rig.py wasabi off` when done.

The rest of the wasabi client works on a rig too, with the flags `rig.py wasabi` itself uses:

    python3 build/wasabi/wasabi --host 127.0.0.1 --port $((1233+N)) --key rigwasabi <command>

| Client command | What it does on a rig | State |
|---|---|---|
| `ping`, `info` | daemon alive; versions, memory, volumes | tested (rig 3) |
| `ls`, `put`, `get`, `del`, `mkdir` | files by the network, `get X -` prints to stdout | tested |
| `run "cmd"` | a command with live output and its exit code | tested (`Echo hi`) |
| `screen --list`, `grab file.png` | screens; the front screen as a PNG (native path) | tested; the PNG was not looked at, the corrupted-column finding of the 2026-10-03 research is not re-checked |
| `speedtest 1MB --pings 5` | latency and throughput | tested |
| `discover` | finds daemons by broadcast (each rig answers with `name rig<N>`) | tested |
| `snoop`, `debug` streams | as above | snoop tested; debug: the stream opens, no KPrintF output was produced to see |
| `mouse move|click|doubleclick|down|up`, `kill target [--force]` | pointer and menus; stop a task (Ctrl-C, or RemTask) | untested |
| `deploy` (put + run/reboot/restart), `reboot`, `restart`, `update`, `quit` | `quit` is `rig.py wasabi off` (tested); the others are untested. `reboot` would reboot the rig: use `rig.py stop` / `start` | untested |
| Guru report | no alert was provoked | untested |

## wasabi (call trace, debug output, Guru, task list)

wasabi (MIT, https://github.com/creep-ltx/wasabi) is a daemon on the Amiga (`wasabid`) that patches ~30 dos/exec/icon/diskfont
calls (SnoopDOS style), RawPutChar and `Alert()`, and a one-file Python client. `make wasabi` clones it into
`build/wasabi`, pinned at ee23c09 (0.9b2), builds `wasabid` with Bebbo's gcc (`~/opt/amiga`, `AGCC=`) against the
AmiSSL 5.27 SDK (`AMISSL_SDK`, the one `make amiga` uses; the CyberGraphX headers are upstream's own since 0.9b2), and keeps the client, `wasabi_mcp.py` and the helper modules beside it;
`make wasabi-clean` removes it. TLS is optional at run time since 0.8: an emulator on loopback and an Amiga without
AmiSSL or keys run plain. Research: thoughts/shared/research/2026-10-14_wasabi-0.8-eval.md.

    rig.py wasabi on                              wasabid into VTC:wasabi, key set, started, waits for a ping
    rig.py wasabi off                             stops the daemon (its patches come off with it)
    rig.py wasabi snoop|debug start|stop|log [N]  streams into build/rig<N>/wasabi/{snoop,debug}.log; log = last N lines (snoop: with a count per call)
    rig.py wasabi guru | ps [PATTERN]

Rig N listens on port 1234 + N - 1 (TCP, and UDP for discovery): FS-UAE's bsdsocket gives the Amiga the Mac's
sockets, so two rigs on one port clash (`paths.wasabi_port`, the one place). The key is the rig key
`rigwasabi` (set with `SetEnv wasabi.key`, i.e. RAM only), never a real one.

`rig.py start` boots wasabid in every rig (it is the only daemon: no amiagent). Its patches are off until `snoop`/`debug`
start a stream, so rig control and timing runs are unaffected; `rig.py wasabi on` only restarts it after `off`.

## Which script proves what

One reachability test per feature; the rest test where the behaviour lives.
Each script's docstring has its full steps and its pass condition.

The reachability tests (the product's top-level entry, with a sentinel):

| Command | Proves |
|---|---|
| `make test-rig` (`reach.py`) | `XCON:` is served by the freshly built handler: installs it, reboots the rig if it differs, runs the probe through DOS, requires PASS |
| `concon_rig.py` | the system's CON:/RAW: served by UP-Term through `C:UPConsole` |
| `condev_rig.py` | UP-Term's console.device under the ROM con-handler |
| `install_rig.py` | the kit: unpack `build/dist/UP-Term`, run `Files/install.dos`, check what it did, Uninstall (needs `make dist` first) |
| `standalone_rig.py` | what each add-on (Claude, amiga-pi, Neovim, Python 3) does on a stock Workbench 3.2 with no UP-Term file, in a ROM CON: and a KingCON window (K2-1; the screen text is read from the pixels, not OCR) |
| `installer_rig.py` | the real Installer on the kit's Install icon, as a user starts it |
| `ptytest_rig.py`, `ixpty_rig.py` | `PTY:` and BSD ptys through the patched ixemul |
| `tmux_rig.py`, `screen_rig.py` | tmux and GNU screen through the XCON: window (screen: 256 colours) |

Other groups (one line each; docstrings say more):

| Area | Scripts |
|---|---|
| Colours, fonts, screens | `cube_rig`, `colour_check`, `aspect_rig`, `outline_rig`, `ownscreen_rig`, `fullscreen_rig`, `vttest_rig` (vttest screens against the engine) |
| Window behaviour | `resize_rig`, `winch_rig`, `closecrash_rig`, `autoprobe_rig`, `tabs_rig`, `menus_rig`, `slash_rig`, `pf1_rig`, `prefs_rig`, `mouse_rig`, `snip_rig` |
| Input, signals | `intr_rig`, `stackext_rig` (gcc 16 library A/B), `ixpipe_rig`, `umask_rig`, `vshpath_rig` |
| Shell and completion | `kingcon_rig`, `h8_rig`, `userland_rig` (every ported Unix tool against the Mac's output; resumable, verdicts in `build/rig/userland/`) |
| console.device | `chainprobe_rig`, `cdprobe_rig`, `cudump_rig`, `dosnode_rig`, `mediumprobe_rig`, `devctl_rig`, `devverify_rig`, `devspeed_rig`, `rkc_rig`, `soak_rig` |
| Serial login | `getty_rig` (a host pty is the terminal on the cable) |
| C:Claude against recorded answers (no key, no Anthropic; start `tools/claude_fixture.py` first) | `claude_rig`, `claude_rig2`, `claude_rig3`, `claude_rig4` |
| Speed | `conbench_rig`, `phase_rig`, `race_rig`, `tablat` |
| Support | `paths.py` (the one reader of `UPTERM_ROOT`, `UPTERM_ROM`, `UPTERM_SYSTEM_HDF`, `UPTERM_OS32_TREE`, `UPTERM_OS32_ROM`), `ami.py`, `fakeircd.py` (a scripted IRC server), `dvmatrix.sh` |

The exact command and prerequisites for each (which `make` target builds its
probe, whether it needs a fresh boot) are in the table in `RULES.md`,
section "Commands".
