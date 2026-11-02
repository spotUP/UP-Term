# Kit ports print nothing through vsh (rig 64, 2026-11-02)

Status: cause not named; ixemul mismatch hypothesis REJECTED.

- Rig 64, `install_rig.py --only base` reproduced: 80 of 92, PORTS checks 51-63 fail (every `$(...)` value empty, `man -w` and `command -v` fine).
- LIBS:ixemul.library after install: 201256 bytes, 02 Nov 12:54:30 = the kit's file. Kit md5 a8a7bf4c = ixemul-vtcon/build295/library/68020/68881/amigaos/ixemul.library (30 Oct 18:52); Files/libs/ixemul.library.nofpu = build295/.../soft-float (c842e747). Makefile 1728 IXEMUL_LIB, copies at 1788-1789. Stock ixemul on the rig before install was 80.1; kit's is 48.2. ENV:IXEMUL unset.
- `UP-Term:bin/sed --version` directly: rc 0, "GNU sed 4.10". Same via `C:vsh -c`: rc 0, same text. `echo abc | UP-Term:bin/sed s/b/X/` in a vsh script prints aXc.
- So ixemul loads and the ports run. What is empty is only command substitution of an external program in a vsh script (builtins in $() like command -v and man -w work).
- Suspect (unproven): vsh.c commits fe76c6a7 and 010faab9 (1 Nov, shell file handle for jobs) changed the capture handle; kit built 2 Nov 12:54 contains them. Next probe: kit installed, script `x=$(sed --version | head -1); echo $x` under vsh, then revert fe76c6a7/010faab9 locally in a worktree.
- My ad-hoc probes were corrupted twice: host shell expanded $(...) inside double quotes; a vsh run without UP-Term: assigned hangs (after Uninstall).
