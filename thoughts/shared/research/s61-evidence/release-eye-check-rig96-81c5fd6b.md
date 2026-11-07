# Release eye-check, fresh-all on rig 96 (up-term-49, build 81c5fd6b)

Command: installer_rig.py ONLY fresh-all, UPTERM_RIG=96, INSTALLER_KEEP=1, flags --max --os 32 --idle-minutes 0 --keep. Log: /private/tmp/claude-501/-Users-spot-Code-UP-Term/59d7869f-00a7-443e-824b-0e3cb1d5d3aa/scratchpad/rig96-fresh-all.log

Result: PASS. Decisive lines: "ok 35 after the reboot: Left Amiga + Space opens the Amarchy menu window", "menu window after reboot: True", "[PASS] fresh-all", "installer_rig: 0 of 1 cases failed".

Verdict JSON: /Users/spot/Code/UP-Term/vtcon/build/rig-reports/rig96/installer/fresh-all.json (the shared rig-reports dir, not build/rig/installer): verdict "pass", detail "all 35 checks (kit kept)".

Counts: 35 ok lines, no FAIL or ERROR lines.

Warnings (all known or environmental): Directory Opus 5.104 not installed in the rig's VTC: drawer; UP-Term:share/updemo files missing (earth.anim, fjord.png, kanim-stream.bin, photo.png); Workbench stayed PAL (W43).

Rig state: rig 96 left running (fs-uae pid 77764, build/rig96 of the main checkout), kit kept installed. No screenshot taken; the text check was unambiguous.
