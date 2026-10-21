---
date: 2026-10-21
topic: upkg browse, a keyboard list of add-ons (arrows, Enter = info, i = install, r = remove, u = upgrade)
status: plan
research: thoughts/shared/research/2026-10-21_upkg-browse.md
ledger: thoughts/spot/todos.md, section "2026-10-20 UPKG-BROWSE"
repos: vtcon (script, kit, host tests, rig driver), upterm-ports (upkg-ui option, opkg patch 0016)
---

# Plan: `upkg browse`

## Goal

`upkg browse` typed in the UP-Term terminal shows every add-on of the feed in one full-screen
list, in the upkg-ui chrome. Each row tells whether the add-on is installed or has an upgrade.
Keys: Up/Down/Home/End/PgUp/PgDn move, Enter shows `upkg info`, `i` installs, `r` removes
(after a yes/no question), `u` upgrades, `q` or Escape quits. After each action the list comes
back with the cursor on the same add-on and the markers refreshed.

## Design decisions (made; do not re-open)

D1. Three layers, each reused, none new:
- **upkg-ui** (upterm-ports `pkgs/upkg-ui/src/upkg-ui.c`) draws the list. It gets one new menu
  option, `--hotkeys KEYS`. No second curses program.
- **dist/upkg-browse** (vtcon, POSIX sh, sibling of `dist/upkg-ask`) is the loop: it runs
  `upkg list`, `list-installed` and `list-upgradable`, calls upkg-ui once per screen, runs the
  chosen upkg command, and repeats.
- **opkg patch 0016** (upterm-ports `pkgs/opkg/patches`) adds the `browse` row to the command
  table, so the user types `upkg browse`. It runs the script and returns its exit code.

D2. Curses is never nested. upkg-ui exits (endwin) before the script runs `upkg install`. So a
package's postinst can open its own upkg-ask / upkg-ui screen without conflict, and install
output goes to the plain terminal as it does today (research section 4).

D3. The `browse` row has `privileged = false`: the parent upkg holds no lock, so the child
`upkg install/remove/upgrade` takes the lock as usual.

D4. The patch runs the script the way postinst runs upkg-ask (`tools/pkgs.py:164`, known to work
on the A1200): `/bin/sh -c "vsh UP-Term:bin/upkg-browse ARGS"` through `xsystem`
(`libopkg/xsystem.c:39`). The environment variable `UPKG_BROWSE` replaces the
`vsh UP-Term:bin/upkg-browse` part (used by the host check; not documented to users).

D5. The parsing is new code, because no parser of opkg list output exists in either repo
(research section 3; searched vtcon and upterm-ports, Python and C). The script splits each
line at the FIRST ` - ` for the name, and at the second ` - ` for the version (a description may
itself contain ` - `). Output to a pipe is not wrapped: patch 0009 wraps only when `isatty`
(`patches/0009-...patch:189-191`), so the lines arrive whole.

D6. Row text (tag = package name, item = what follows), full words, ASCII only:
- `installed 1.3   Vim, the text editor`
- `upgrade 1.2 to 1.3   Vim, the text editor`
- `available 1.3   Vim, the text editor`
The prompt line above the list carries the key help:
`Enter info   i install   r remove   u upgrade   q quit`.

D7. `--hotkeys KEYS` (menu only). Pressing one of the KEYS ends the menu with exit 0 and writes
`KEY TAG` to stderr. With `--hotkeys`, Enter writes `enter TAG`. Escape stays 255. Without
`--hotkeys` the output contract is unchanged (bare TAG), so upkg-ask is not affected.
Hotkeys take priority over the first-letter jump. Every add-on name starts with `up-`, so the
jump does not help in the browse list anyway. `--hotkeys` without `--menu` is a usage error (exit 2).

D8. A key that does not fit the row prints one line and waits for Return. No upkg command runs.
- `i` on an installed add-on: `up-vim is already installed.`
- `r` on an add-on that is not installed: `up-vim is not installed.`
- `u` with no upgrade: `up-vim has no newer version.`
`r` asks first with `upkg-ui --defaultno --yesno "Remove up-vim?"`. Only Yes runs `upkg remove`.

D9. After a command, the script prints `[OK] upkg install up-vim` or
`[ERROR] upkg install up-vim returned N`. Then it prints `Press Return to go back to the list.`
and reads one line. Enter (info) runs `upkg info TAG`, then the same pause.

D10. Refusals, each with exit 2 and one line on stderr:
- stdin is not a terminal: `upkg browse needs a terminal. Use upkg list.`
- no upkg-ui on the PATH: `upkg browse needs UP-Term:bin/upkg-ui.`
- `upkg list` prints nothing: `No package lists. Run upkg update first.` (exit 1, not 2)

D11. Help forms (memory rule: `--help`, `-h`, `help`, `?`): `upkg browse --help|-h|help|?`
prints `usage: upkg browse` plus one line of key help, then exits 0. Any other argument prints
the usage on stderr and exits 2. The script handles this. The patch passes the arguments
through, each one quoted.

D12. Global upkg options given before `browse` (`-f`, `-o`, ...) are NOT forwarded to the
child commands. The child commands use the default config, the same as a bare `upkg`. The
script calls `${UPKG:-upkg}`; `UPKG` is for the host tests only.

D13. upkg's own usage text (patch 0008 layout, 76 columns) gets one line under Informational Commands:
`  browse                      Pick add-ons from a list (install, remove, upgrade)`.
`check/help_width.sh` must still pass.

D15. 2026-10-21 implementation: the D13 text is 81 columns, over the 76-column help_width check. The usage row reads `Pick add-ons from a list to install or remove` (upterm-ports 2e9bdef).

D14. Owner 2026-10-21: keep D1 (script + upkg-ui + patch row), not a C browser inside upkg. Reasons: no ncursesw in the static upkg, no copy of the upkg-ui menu, opkg 0.9 runs one command per process. Every action is a child `upkg install/remove/upgrade/info NAME`.

## Checklist

Phase A: upkg-ui `--hotkeys` (upterm-ports)
- [x] A1 `parse()` (`upkg-ui.c:462`): accept `--hotkeys KEYS`; store it in `opt.hotkeys`. Usage line (`:456`) and header comment (`:5`) name it.
- [x] A2 `parse()`: `--hotkeys` without `--menu` returns `usage("hotkeys need a menu")`.
- [x] A3 `key_menu()` (`:385`): before the first-letter jump, `c < 128 && strchr(opt.hotkeys, c)` sets `hotkey = c` and returns 0. Enter with hotkeys set leaves `hotkey = 0`.
- [x] A4 `main()` (`:571-580`): for K_MENU with `opt.hotkeys`, write `"%c %s"` (hotkey) or `"enter %s"` to stderr. Without hotkeys, keep the bare tag.
- [x] A5 check: `check/cases` row `badhotkeys  upkg-ui --hotkeys ir --yesno x 0 0 2>&1 | sed -n 1p`, plus its expected line.
- [x] A6 check: `check/tty` row `hotkey`: a menu with `--hotkeys iru`, keys file `check/keys/hotkey` (`600 \eOB`, `300 r`), expected `ans.txt` = `r b`. Also row `hotenter` with Enter only: expected `enter a`.
- [x] A7 confirm A5/A6 fail with A3/A4 reverted, then pass (`make host-upkg-ui` check run).

Phase B: dist/upkg-browse (vtcon)
- [x] B1 `dist/upkg-browse`: header comment in upkg-ask style (contract, keys, exits, env `UPKG`), D10 refusals, D11 help forms.
- [x] B2 build rows: `list`, `list-installed`, `list-upgradable` into shell variables; read with here-documents in the current shell (no pipe subshell); `set --` tag/item pairs per D5/D6.
- [x] B3 loop: `upkg-ui --title "UP-TERM ADD-ONS" --backtitle "N add-ons" --hotkeys iruq --default-item LAST --menu "<D6 prompt>" 0 0 0 "$@" 2>answer`; dispatch per D7-D9; `q` / exit 255 quits with 0.
- [x] B4 `tests/test_upkg_browse.py` (unittest, fake `upkg` and fake `upkg-ui` on the PATH, as `tests/test_upkg_ask.py:46-60` does): rows and markers from fixed list outputs (description containing ` - `); i/r/u/Enter run the right command and the cursor stays (`--default-item`); D8 messages run no command; r + No runs nothing; q and Escape exit 0; D10 three refusals; D11 four help forms exit 0, bad argument exit 2.
- [x] B5 `tests/test_upkg_browse.py` real-upkg-ui case on a pty, skipped when the host upkg-ui is missing (as `test_upkg_ask.py:158`).
- [x] B6 wire the test into `make test` next to test_upkg_ask (`Makefile:115`).
- [x] B7 kit: `Makefile:1082-1083` gains `cp dist/upkg-browse $(KIT)/Files/userland/bin/upkg-browse`; `tests/test_dist_ipk.py` (or the kit-content test that names upkg-ask) asserts it is in up-term.lha.

Phase C: opkg patch 0016 `browse` (upterm-ports)
- [x] C1 `libopkg/opkg_cmd.c`: `opkg_browse_cmd(int argc, char **argv)` builds `"${UPKG_BROWSE:-vsh UP-Term:bin/upkg-browse} 'arg'..."` (single-quote each argument; an argument containing `'` is refused with exit 2), runs `xsystem({"/bin/sh","-c",cmd,NULL})`, returns its exit code. Row `{"browse", 0, opkg_browse_cmd, 0, false}` near `{"list", ...}` (`opkg_cmd.c:1243`).
- [x] C2 `src/opkg.c` usage: D13 line.
- [x] C3 make sure the exit code of `upkg browse` is the script's code (opkg main returns the cmd result; confirm through check C4, not by reading).
- [x] C4 `pkgs/opkg/check/browse_dispatch.sh UPKG`: `UPKG_BROWSE` = a stub that writes its arguments to a file and exits 7. Assert: `upkg browse --help` reaches the stub with `--help`, rc 7. `upkg browse 'a b'` reaches it as one argument. `upkg --help` lists `browse`. It must fail without the patch (`unknown sub-command browse`), and the commit message says so.
- [x] C5 `check/help_width.sh` still passes with the new usage line.
- [x] C6 `make opkg kit-stage` in upterm-ports; kit upkg size and sha256 into the ledger.

Phase D: reachability (product top-level entry point = `upkg browse` typed in UP-Term)
- [x] D1 `tools/rig/upkg_browse_rig.py` (modelled on `upkg_ask_rig.py`): copies `dist/upkg-browse`, the upkg-ui and upkg binaries into UP-Term:bin, serves build/ipk, runs `upkg update`, types `upkg browse` into the UP-Term window. Sentinel: the rig's `UP-Term:pkg/up-vim/Uninstall` appears after Down-to-up-vim + `i`. Then `r` + Yes removes it (the record is gone). Asserts `list-installed` before and after. Screenshots go to the out dir; a haiku subagent checks them.
- [x] D2 step 0 of D1: `upkg update` must return rc 0 on the rig. Rig 2 had no working AmiSSL (ledger, v1.2-test3). If it fails, D1 runs on the A1200 instead. In that run, the keys are typed into the UP-Term window with `ami.py key` (hex codes: Return = 44), never through `ami.py long`. The sentinel is the same.
- [x] D3 `rig.py start --max`; stop the rig at the end.

Phase E: land
- [x] E1 vtcon `make test` rc 0; upterm-ports checks rc 0 for upkg-ui and opkg.
- [x] E2 commits: upterm-ports (A, C) push; upterm repos.lock re-pin; vtcon (B, D) push. No release in this plan: shipping in a release (1.4) is the owner's call.
- [x] E3 ledger entry with commits, kit sha256, rig/A1200 result.

## Automated verification
- upterm-ports: `make host-upkg-ui` checks (A5, A6), opkg host checks incl. `browse_dispatch.sh`, `help_width.sh`.
- vtcon: `python3 -m unittest tests.test_upkg_browse`, `make test`, `make dist dist-ipk dist-index dist-check` (B7).
- Reachability: `tools/rig/upkg_browse_rig.py` (D1) or its A1200 form (D2).

## Manual verification (owner, A1200, LIVE after a release or LOCAL with the kit copied by hand)
1. In the UP-Term window, type `upkg browse`. PASS: a full-screen list in the add-on chrome. Each add-on has an `installed`, `upgrade ... to ...` or `available` marker.
2. Move to an add-on that is not installed and press `i`. PASS: the install runs in the plain terminal (any question screen appears). Then `[OK] upkg install ...`. After Return, the list shows `installed`.
3. Press `r` on the same add-on, answer No, then `r` and Yes. PASS: No does nothing. Yes removes the add-on, and the marker goes back to `available`.
4. Press Enter on an add-on. PASS: the info lines appear, and Return goes back to the list.
5. Press `q`. PASS: you are back at the shell prompt with the screen clean.

## Success criteria
All checklist IDs ticked (A1-A7, B1-B7, C1-C6, D1-D3, E1-E3: 26 items). The A5, A6, B4 and C4
checks each fail with their fix reverted. The D1 sentinel is observed through the real
`upkg browse` on a rig or the A1200.
