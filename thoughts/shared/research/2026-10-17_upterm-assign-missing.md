---
date: 2026-10-17
topic: UP-Term: assign missing after kit install / reboot
status: NOT reproduced, no root cause found, no code change, no commit
---

# Result

The kit's installer does not lose the UP-Term: assign on rig 2 (68060, --max), neither right after install nor after a reboot.

## Evidence (rig 2)
- `install_rig.py --only base-dest --force`: PASS (checks 5, 9, 12-14: UP-Term: assigned to VTC:KitTest/UP-Term, marked block in S:User-Startup, assign recreated from ENVARC:up-term/Dir). Log: scratchpad assign-base-dest.log.
- Install (runinstall), `Assign UP-Term: EXISTS` rc 0, then rig stop + start --max, again rc 0; S:User-Startup after the reboot holds `;BEGIN UP-Term assign / Assign UP-Term: "VTC:KitTest/UP-Term" / ;END UP-Term assign`. Logs assign-rb1.log, assign-rb2.log.
- Uninstall afterwards: assign gone (rc 5), rig 2 stopped. installer_rig.py (real Installer GUI) already checks the same and was not re-run.

## Static review (dist/install.dos, Install.installer, procs.installer, Uninstall, unstartup.sh, reassign.sh)
- No bare `Assign UP-Term:` outside Uninstall (dist/Uninstall:125, intended, after the Dir file is deleted). `makeassign "UPTKIT"` (procs.installer:31) removes only UPTKIT:.
- Live assign: dist/install.dos:128. Boot assign: marked block dist/install.dos:148-158 (appended with Echo >>, skipped when the exact line already exists).

## Open hypotheses (unverified, need the owner's A1200 S:User-Startup)
1. The A1200 run (2026-10-17_a1200-kit-tests.md) got only the WASABI, ixemul, ixnet blocks and no assign block: consistent with the `drawer` part never running (parts run by STEP by hand, or the Search at install.dos:148 matching an older line), so no live Assign. A STEP run of a part after `drawer` does not check that UP-Term: exists (add-ons do: dist/common/check.dos:5).
2. The block is appended at the END of S:User-Startup (install.dos:155), though the comments say "before the blocks that use it". A user's own earlier line that ends the script (EndCLI/Quit/hang) keeps it from running at boot.
3. Same session lost LIBS: through a bare `Assign LIBS:` (a1200 note, run 2); a bare `Assign UP-Term:` from a test command would look identical. Not proven either way.

Need from owner: `Type S:User-Startup` and `Assign LIST` from the A1200 (read only). Commit: none.

## A1200 state, read 2026-10-17 (after the git install, before any reinstall)

- `Assign UP-Term: EXISTS` prints `UP-Term        Rushhours:UP-Term-inst` (rc 0).
- S:User-Startup (124 lines) holds, near the end and in this order: `;BEGIN UP-Term wasabi`, `;BEGIN UP-Term assign` with `Assign UP-Term: "Rushhours:UP-Term-inst"`, `;BEGIN UP-Term` (GG:), `;BEGIN UP-Term tmp`. No line before them ends the script (no EndCLI/Quit), so candidate 2 is ruled out for this file.
- So the block exists now. The file alone cannot tell whether an Install wrote it or the earlier manual repair did; the original missing-assign state was not captured before it was repaired.
- Also in the file: `SYS:C/UnixDirs3 WILDSTAR UNIXROOT QUIET`. UnixDirs3 rewrites `..` and `/` path handling in dos.library; it is a candidate for the vsh `cd ..` failure seen in the A1200 git test (49016bda), unverified.
