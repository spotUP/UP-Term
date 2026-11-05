# Rig 96 rebuild 3 (s58): owner eye-check rig, AmigaOS 3.2, start hang fix

- Build: worktree ../vtcon-s58-rig96 (branch s58-rig96) at main 92b29ff8. make worktree-ready, wasabi, amiga rc 0; make dist rc 0 (needs an absolute UPTERM_PORTS=/Users/spot/Code/UP-Term/upterm-ports/.claude/worktrees/kit-main; the relative form in the brief fails with "make kit-stage first").
- Old rig 96 (s57 build) stopped, logwatch pid 11272 killed.
- Install: installer_rig.py ONLY fresh-all, INSTALLER_KEEP=1, UPTERM_RIG_FLAGS="--max --os 32 --idle-minutes 0 --keep": [PASS] fresh-all, 35 checks ok, Amarchy menu window after reboot True.
- start.args: --max --os 32 --idle-minutes 0 --keep --purpose "vtcon-ee s58: owner eye check rig 96 (start hang fix, UPDemo 15, drag)".

## Boot series (rig.py reboot x10; ok = Amarchy ARexx STATUS answers within 90 s)

- boot 1: workbench_up=yes (19s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 2: workbench_up=yes (4s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 3: workbench_up=yes (16s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 4: workbench_up=yes (5s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 5: workbench_up=yes (2s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 6: workbench_up=yes (1s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 7: workbench_up=no (nones after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
  HANG diagnostics boot 7:
  host load:
      421  51.6 /System/Library/PrivateFrameworks/SkyLight.framework/Resources/WindowServer -daemon
     3147  33.6 /Users/spot/Code/Emulators/fs-uae/fs-uae /Users/spot/Code/UP-Term/vtcon/build/rig96/vtcon-rig.fs-uae
    14140  32.7 /Users/spot/Code/Emulators/fs-uae/fs-uae /Users/spot/Code/UP-Term/vtcon/build/rig143/vtcon-rig.fs-uae
    16287  24.6 /Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Framework.framework/Versions/156.0.8078.80/Helpers/Google Chro
    17407  21.4 /opt/homebrew/Cellar/python@3.14/3.14.7/Frameworks/Python.framework/Versions/3.14/Resources/Python.app/Contents/MacOS/Python too
    72772  21.1 /Applications/Google Chrome.app/Contents/MacOS/Google Chrome --origin-trial-disabled-features=CanvasTextNg|WebAssemblyCustomDesc
    17462  20.2 /Applications/Xcode.app/Contents/Developer/usr/bin/git -c core.askPass= -c protocol.ext.allow=never -c core.alternateRefsCommand
    59535  18.1 /Library/SystemExtensions/4386F88B-9CA1-4ED9-ACF9-CF07D302DDE8/com.microsoft.wdav.netext.systemextension/Contents/MacOS/netext
  wasabi debug.log tail:
    2026-11-05 11:54:51.762 client | debug: reconnected after 58 s - wasabid 0.11b2
    2026-11-05 11:55:16.949 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 11:58:10.526 debug |   ... 109 more identical
    2026-11-05 11:58:10.527 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 11:59:08.797 client | debug: reconnected after 58 s - wasabid 0.11b2
    2026-11-05 11:59:26.290 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:02:20.455 debug |   ... 121 more identical
    2026-11-05 12:02:20.456 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:03:14.596 client | debug: reconnected after 54 s - wasabid 0.11b2
    2026-11-05 12:03:39.675 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:06:34.253 debug |   ... 121 more identical
    2026-11-05 12:06:34.261 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:07:10.476 client | debug: reconnected after 36 s - wasabid 0.11b2
    2026-11-05 12:07:31.469 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:10:01.453 debug |   ... 121 more identical
    2026-11-05 12:10:01.454 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:10:35.576 client | debug: reconnected after 34 s - wasabid 0.11b2
    2026-11-05 12:10:53.994 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:11:26.521 debug |   ... 2 more identical
    2026-11-05 12:11:26.522 client | debug: nothing heard for 20 s - the machine is frozen, rebooted, or busy with something long
  task states (ami.py exec ps):
- boot 8: workbench_up=yes (10s after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
- boot 9: workbench_up=no (nones after reboot returned); fs-uae.log 'CPU reset PC=' lines now: 0 []; '[WARN] Guru before reset' lines: 0
  HANG diagnostics boot 9:
  host load:
      421  56.3 /System/Library/PrivateFrameworks/SkyLight.framework/Resources/WindowServer -daemon
      354  53.3 /System/Library/Frameworks/CoreServices.framework/Versions/A/Frameworks/FSEvents.framework/Versions/A/Support/fseventsd
    39705  47.7 /Users/spot/Code/Emulators/fs-uae/fs-uae /Users/spot/Code/UP-Term/vtcon/build/rig96/vtcon-rig.fs-uae
    46271  44.1 /Users/spot/Code/Emulators/fs-uae-debug/fs-uae /Users/spot/Code/UP-Term/vtcon/build/rig142/vtcon-rig.fs-uae
    60947  31.8 /Library/SystemExtensions/71FC95A8-F26A-46D6-9700-AADB610A2861/com.microsoft.wdav.epsext.systemextension/Contents/MacOS/epsext
    50729  30.4 /Applications/Xcode.app/Contents/Developer/Toolchains/XcodeDefault.xctoolchain/usr/bin/clang -cc1 -triple arm64-apple-macosx15.0
    30224  30.1 node /Users/spot/Code/DEViLBOX/node_modules/.bin/tsc -b --force
    73672  18.7 /Applications/Google Chrome.app/Contents/Frameworks/Google Chrome Framework.framework/Versions/156.0.8078.80/Helpers/Google Chro
  wasabi debug.log tail:
    2026-11-05 12:03:39.675 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:06:34.253 debug |   ... 121 more identical
    2026-11-05 12:06:34.261 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:07:10.476 client | debug: reconnected after 36 s - wasabid 0.11b2
    2026-11-05 12:07:31.469 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:10:01.453 debug |   ... 121 more identical
    2026-11-05 12:10:01.454 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:10:35.576 client | debug: reconnected after 34 s - wasabid 0.11b2
    2026-11-05 12:10:53.994 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:11:26.521 debug |   ... 2 more identical
    2026-11-05 12:11:26.522 client | debug: nothing heard for 20 s - the machine is frozen, rebooted, or busy with something long
    2026-11-05 12:15:38.771 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:18:39.279 client | debug: reconnected after 180 s - wasabid 0.11b2
    2026-11-05 12:19:24.620 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:22:26.835 debug |   ... 92 more identical
    2026-11-05 12:22:26.836 client | debug: connection lost (connection closed by the Amiga) - reconnecting until the Amiga returns; Ctrl-C to stop
    2026-11-05 12:23:17.066 client | debug: reconnected after 50 s - wasabid 0.11b2
    2026-11-05 12:23:41.832 debug | [wasabi: 127.0.0.1 connected]
    2026-11-05 12:24:24.889 debug |   ... 2 more identical
    2026-11-05 12:24:24.890 client | debug: nothing heard for 20 s - the machine is frozen, rebooted, or busy with something long
  task states (ami.py exec ps):

## HOLD (coordinator order, 2026-11-05 about 12:31)

- Step reached: boot series, boot 10 of 10 in progress. Boots 1-9 recorded above. Boot 10 has no verdict line.
- Tally boots 1-9: 7 clean (1-6, 8), 2 hang (7, 9), 0 Guru before reset, 0 new 'CPU reset PC=' lines.
- Caveat: the brief's log path build/rig96/fs-uae.log.txt does not exist; the log is build/rig96/logs/fs-uae.log.txt. A planned reset may not write 'CPU reset PC=' there, so the 0 count is unverified as a reset detector.
- Last commands sent to rig 96 by this agent: the boot 10 `rig.py reboot` followed by `ami.py rexx` STATUS polls every 3 s (boot 10 loop, roughly 12:27 to 12:31). The driver script (s58boot.sh) was killed at 12:31:27. Nothing was sent after that. Child processes already in flight were left to finish.
- Boot 7 and 9 hangs: host load at boot 7 showed rig96 fs-uae 34% and rig143 fs-uae 33% (another session) plus WindowServer 51%; wasabi debug.log ended "nothing heard for 20 s"; task states empty because wasabid did not answer.
- Rig 96 left as is (up, frozen, read-only reads by vtcon-c1). Series incomplete; a new agent continues. No "SERIES DONE" line written.
