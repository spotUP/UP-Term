# SHELLICON root cause (265bcefb) -- 2026-10-17

Question: what caused the SHELLICON problem that 265bcefb fixed or worked around; which commit
introduced it; root-cause fix or workaround?

## Verdict
- 265bcefb (2026-10-02) is neither. It is the commit that ADDED the SHELLICON option (feature:
  "the Shell icon can open UP-Term", plus SYSICON, plus two incidental fixes: UPConsole DEVICE ON
  after a switched-off device, and a ';'-less comment line in install.dos since 9df88d6).
- If the "problem" is "the SHELLICON option broke the shell on real hardware", the repo holds no
  record of that failure: no commit, research doc or handoff says what broke, on which kit, with
  which symptom. The only statement is the ledger/handoff line (finish-run-session-8.md:47) asking
  for "that kit's VERSIONS file from the owner". Owner now has no old kit, so the cause cannot be
  established from history. No rig check was run.

## Evidence
- `git log -S SHELLICON`: introduced in 265bcefb (via 19f6c290 kit layout); later only option
  plumbing (4da1fc44 check boxes, d074f3ee drops NOSHELLICON, kit-split K2-4).
- Mechanism today: dist/install.dos:576-586 copies SYS:System/Shell.info to
  ENVARC:up-term-orig/Shell.info (first time only), then `upicon SYS:System/Shell WINDOWDEV=XCON`.
  install/iconspec.c swaps only the device part of the WINDOW tooltype. dist/Uninstall:272 puts it
  back. install_rig.py:600-606 checks the tooltype swap on a rig (Workbench start opens a window).
- Known risk (hypothesis, unverified on hardware): the icon then depends on XCON: being mounted.
  install.dos:748-751 notes XCON: (DOSDrivers) only appeared after a reboot at one time; a Shell
  icon double-clicked while XCON: is unmounted, or after Uninstall removed the handler without
  restoring the icon, would fail to open a window. Uninstall does restore the icon (line 272).
- Also: the option is only offered when SYS:System/Shell.info exists (Install.installer); an
  icon with an unusual WINDOW value (no device part) is handled by iconspec (prefixes XCON:).

## Not done
No fix: no reproducible defect identified. To close: either owner gives the symptom (error text,
what the Shell icon did) or test on the A1200 with: Install SHELLICON, double-click Shell, reboot,
double-click Shell, Uninstall, double-click Shell.
