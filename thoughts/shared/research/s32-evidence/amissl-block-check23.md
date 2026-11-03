# install_rig wasabi check 23: who leaves ;BEGIN AmiSSL in S:User-Startup

Verdict: mixed (a)-by-design, and the failure is a harness bug: the wasabi case never passes the AmiSSL leave.
It is not (b); Uninstall has no condition for the block, it never removes it on any path.

Steps
1. dist/install.dos:1167-1177: Shell install without STEP runs Execute install-amissl.dos when LIBS:amisslmaster.library < v5
   (the rig image has none on 3.2 or the wasabi case path hits it).
2. dist/install-amissl.dos:22-25,45-54: when no AmiSSL: assign existed, upamnew=1 and the script appends
   ";BEGIN AmiSSL ... ;END AmiSSL" to S:User-Startup.
3. dist/Uninstall:4 (header): "AmiSSL ... stays: other programs use it" - it never touches the block (no AmiSSL in its body).
4. tools/rig/install_rig.py:1242-1244 defines BASE_STARTUP_LEAVES = ('AmiSSL',) for exactly this; tools/rig/installer_rig.py:1290
   (installer case) passes it to check_startup_after_uninstall.
5. tools/rig/install_rig.py:1857 (wasabi case) calls base_uninstall(ck, before) with default leaves=() and installed_text='',
   so check_startup_after_uninstall (1272-1286) compares the whole file and flags the block (line 15). Same call shape at
   1596,1654,1689,1701,1751,1782 (other cases; they may also be hit if they install AmiSSL).

Minimal fix: tools/rig/install_rig.py:1857 -> base_uninstall(ck, before, leaves=BASE_STARTUP_LEAVES,
installed_text=<Type S:User-Startup captured before Uninstall>), or make base_uninstall default leaves to
BASE_STARTUP_LEAVES and capture installed_text itself (fixes every case at once). No dist change needed.
Not run on a rig (host-side reading only).
