# The test rig

An emulated Amiga (FS-UAE) that the host drives through amiagent. Every
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
| amiagent | `build/rig/boot/amiagent` | Aminet `comm/net/amiagent` (the agent of https://github.com/thomas-luebker/amimcp); copy the 68k binary there once. The owner's copy came from the Up Rough demo system; that it is byte-identical to the Aminet one is not verified. `--stock` patches two stack constants at fixed offsets and refuses another build of amiagent. |
| ncurses 5.5, less, nano, BitchX (what the userland and tmux/screen rigs and the tmux and screen builds use) | `build/rig/vtc/pkgs/ncurses-5.5-1-p-bin-m68k` and siblings | Aminet `dev/gg/ncurses-5.5-1-bin-m68k` (Geek Gadgets) unpacked there. The `-p-` in the directory name is the owner's; whether it marks a modified unpack is not verified. tmux-amiga and screen-amiga read the same directory (`NCURSES=`), so build the rig tree before them. |
| Python 3.14 | the host | `python3 --version` must be 3.14 or newer (the upterm doctor checks). The rig scripts use the standard library plus Pillow (`import PIL`, for screenshots: `python3 -m pip install pillow`; found by grepping the imports, not run on a clean machine). |

## Bring it up

    export UPTERM_ROM=<path to your Kickstart 3.1 A1200 ROM>   # once per shell
    python3 tools/rig/rig.py setup          # copies the system disk once, writes build/rig/vtcon-rig.fs-uae and the boot drawer
    python3 tools/rig/rig.py start --max    # boots it in the background, waits for amiagent (two retries)
    python3 tools/rig/rig.py status         # is amiagent answering on 127.0.0.1:7846
    python3 tools/rig/ami.py exec "Version" # a command on the emulated Amiga; the first thing to try
    python3 tools/rig/rig.py stop           # kills this rig only (matched by its config path)

`start` prints the boot log (`build/rig/boot/boot.log`) when amiagent never
answers. Keep the machine flags the same between `setup` and `start`.

Machine flags (on `start`, and `setup` when the config alone changes):

| Flag | Machine |
|---|---|
| none | A1200, 68020 + 68882, 8 MB fast + 64 MB Zorro III, RTG card, JIT, real CPU speed |
| `--fast` | the same at the host's speed |
| `--060` | 68060 (no MMU) + FPU at the host's speed, 128 MB Zorro III |
| `--max` | `--060` with 1 GB Zorro III. The machine the owner runs everything on by default. |
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
first: assigns, mounts XCON:, starts amiagent with token `rigtoken`), `vtc/`
(VTC:, the binaries under test), `shots/` (screenshots), `serial.log`.

## A second rig, to run two jobs in parallel

`UPTERM_RIG=2` selects rig 2: the same FS-UAE backend and the same config
generator (`rig.py`), so results are comparable, but a separate machine:

| | rig 1 (default) | rig 2 (`UPTERM_RIG=2`) |
|---|---|---|
| directory | `build/rig` | `build/rig2` (own `sys.hdf`, `vtc/`, `boot/`, `shots/`, config) |
| amiagent | port 7846 | port 7847 (`go` starts `amiagent PORT=7847`) |
| FS-UAE process | matched by `build/rig/vtcon-rig.fs-uae` | matched by `build/rig2/vtcon-rig.fs-uae` |

```
UPTERM_RIG=2 python3 tools/rig/rig.py setup    # once: copies UPTERM_SYSTEM_HDF (1.5 GB) and rig 1's vtc/ (about 400 MB, read only) and amiagent
UPTERM_RIG=2 python3 tools/rig/rig.py start    # then status / install / stop, as for rig 1
UPTERM_RIG=2 python3 tools/rig/ami.py ping
UPTERM_RIG=2 python3 tools/rig/<script>_rig.py
```

`paths.py` is the one reader of `UPTERM_RIG` (`paths.RIG`, `paths.AGENT_PORT`);
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

## The client, and a real Amiga

`ami.py` is the client for amiagent: `ping | info | exec "cmd" [secs] | shot out.png | get | put | uitree | ui | key | rexx | menus | screens | gclick`
(its docstring has the exact forms). It talks to the emulator by default. For
a real Amiga running amiagent:

    AMI_HOST=<the Amiga's address> AMI_TOKEN=<its amiagent token> python3 tools/rig/ami.py ping

(`AMI_PORT`, default 7846.) amiagent runs whatever it is sent and the link is
unencrypted: use a token and a trusted network (see tools/nas/README.md).

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
