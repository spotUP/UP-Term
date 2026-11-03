# SHELL-PATH-RC10

Cause: tools/rig/rig.py SHELL_STARTUP_PATH (was line 198) wrote into S:Shell-Startup
  "Assign >NIL: VTC: EXISTS / If NOT WARN / Path >NIL: VTC:gg/bin ADD / EndIf"
which tested the VTC: assign, not the VTC:gg/bin drawer. Rig 91 (strings of build/rig91/sys.hdf, read only)
holds exactly that block in S:Shell-Startup, and build/rig91/vtc has no gg directory, so every new Shell ran
Path on a missing drawer. Kit files are not involved: dist/install.dos writes only an Alias block into
S:Shell-Startup; the kit's Path lines (AmiSSL:, VBCC:bin, UHC:ExtBin) are in S:User-Startup, boot only, and
the AmiSSL line is the only unguarded one (guarded by the installer creating AmiSSL: first).
Fix: Path line nested in If EXISTS VTC:gg/bin; guard_shell_startup rewrites the earlier blocks
(upgrade_shell_path). Test: tests/test_rig_shell_path.py (fails on main, passes on branch).
Unverified: rc 10 of Path on a missing drawer was not reproduced on a rig (no rig 92 run; disk 2.2 GB free).
