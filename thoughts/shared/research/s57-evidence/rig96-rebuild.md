# Rig 96 rebuilt from 31ca8e69 (s57, vtcon-16)

Build: worktree ../vtcon-s57-rig96 detached at 31ca8e69 (origin/main was dabb4317, 6 commits ahead; rig.py needed `--allow-stale-base pinned-by-caller-31ca8e69`, the caller pinned the hash). `make worktree-ready`, `make wasabi`, `make amiga`, `UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main make dist` (the relative path form in the brief fails: UPTERM_PORTS is resolved against the worktree, so use the absolute path).
Old rig 96 (7e51272c, started by the main checkout): no runner script named it (ps), idle; plain `rig.py stop` from the main checkout removed its build/rig96.
Install: `UPTERM_RIG=96 UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0 --allow-stale-base ..." INSTALLER_KEEP=1 python3 -I tools/rig/installer_rig.py ONLY fresh-all` -> [PASS], 35 checks, post-install reboot, Amarchy menu opens. Install dir is SYS:UP-Term (drawer), UPDemo goes to C:UPDemo.

| Item | Rig path | md5 | Build file | Match |
|---|---|---|---|---|
| handler | L:vtcon-handler | dce2fa28523554b6da90cbb472681c57 | dist Files/vtcon-handler = build/amiga | yes |
| UPDemo | C:UPDemo | 93e2cdb7539dd63e24fb2c8ca6e17ab8 | dist Files/UPDemo = build/amiga | yes |
| earth.anim | SYS:UP-Term/share/updemo/earth.anim | 0ff3fabef997b62e580b702ea5147605 | dist Files/share/updemo/earth.anim | yes |
| Amarchy | SYS:UP-Term/bin/Amarchy | b55a9312feae585b324974731916a678 | build/amiga/Amarchy | yes |
| ixemul | LIBS:ixemul.library | a8a7bf4c4df7f53395fc320500fd25ed | dist libs/ixemul.library | yes |
| ixnet | LIBS:ixnet.library | 9247e6d423a976f8d99bd100fe768eec | dist libs/ixnet.library | yes |
| cryptossh | LIBS:libcryptossh.library | 45e5f923962e5825056fb73a6fb3c7dc | build/bebbossh/out/libcryptossh.library060 | yes (060 variant) |

Handler: `Version L:vtcon-handler` = vtcon-handler 1.0. Amarchy: process UP-Term:bin/Amarchy and amarchy-flat running; ENVARC:Amarchy/prefs header-only (default flat-system-look on, flat-window-gadgets on); ARexx STATUS after reboot: enabled=1 rows=71 bar=1.
Warnings from the installer run: "UP-Term:share/updemo/* is missing in the rig's UP-Term: tree" (the check looks at UP-Term: where the Installer put the files under SYS:UP-Term/share/updemo; files present and md5-equal); "the Workbench stayed PAL (W43)".
Rig 96 left UP (fs-uae, --idle-minutes 0), WAITING on owner eye check. Scratch deleted: build/dist, build/ipk, the three build/*.lha, build/host-obj, build/vttest_host, build/phosphor in the worktree (about 480 MB). Kept: build/amiga (rig 96 itself lives in /Users/spot/Code/UP-Term/vtcon/build/rig96, the main checkout's build tree).
