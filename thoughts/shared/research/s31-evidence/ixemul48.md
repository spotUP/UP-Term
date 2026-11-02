# ixemul v48 requester (RIG51-IXEMUL48-REQ) - partial, rig proof NOT done

Static findings (s31):
- The text comes from ixemul-vtcon libsrc/crt0.c:225 (lib_Version < IX_VERSION) and :352 (OpenLibrary of ixemul.library failed).
  IX_VERSION is 48 (library/parse_version.c:45 generates it).
- The kit ships build/dist/up-term/Files/libs/ixemul.library and .nofpu: both resident rt_Version 80 ("ixemul 80.1 [vtcon-4]").
  So an installed kit library can never fail the :225 test; the requester is the :352 case, OpenLibrary failing
  at the moment some ixemul-linked program starts at boot (library missing from LIBS:, or not yet there / wrong CPU variant).
- Source program still unidentified; needs a boot-time wasabi snoop of OpenLibrary ixemul.library.

Blocked: rig 57 is used by other sessions at the same time (Amiberry rig 57 with "purpose not given", a second
rig.py start --max --os 32 for "AmiSSL install stall fix", plus installer-check10 per ledger). My start was killed by
timeout; no install was run. Needs a free rig number.

## Rig 62 run (2026-11-02, OS 3.2 rig, installer_rig fresh-all, kit build 12:54)

Result: the v48 requester was NOT reproduced. No fix made.

- Full-tick install (fresh-all) on OS 3.2: SYS:Libs/ixemul.library = 80.1 (201256 bytes, kit file), SYS:Libs/ixnet.library present.
  ENVARC:up-term-orig holds ixemul.none and ixnet.none (no original library existed; kit copy went to LIBS:).
- The installer's own reboot check passed: BOOTX:boot.log ran to "wasabid started", no requester logged.
- S:User-Startup order after install: BOOTX:go, UP-Term assign, shadows-up, Amarchy, amarchy-index, amarchy-flat BOOT,
  upgetty LOOP, wasabid, SolidWin, UPConsole, then the AmiSSL block (Assign LIBS: AmiSSL:Libs ADD, last).
  Work:AmiSSL/Libs holds no ixemul.library, so the AmiSSL assign cannot supply a wrong copy.
- A second, manual reboot (ami.py reboot) ended in an Exec alert, not the ixemul requester:
  "UAE clipboard sharing - Program failed (error #80000006). Wait for disk activity to finish." (Suspend / Reboot).
  That is the rig's clipboard task failing on a warm reboot; wasabid never came back. Rig artifact, not the reported bug.
- Not tested: --nofpu variant, a machine where an original ixemul.library already exists elsewhere in the LIBS: chain,
  and the real A1200. These stay the open leads for crt0.c:352 (OpenLibrary failed) versus :225 (version too low).
- Tooling note: the amiga MCP server talks to port 1234 (rig 1); for another rig use UPTERM_RIG=N python3 tools/rig/ami.py.
  A rig whose wasabid is down can be read by OCR of the FS-UAE window (see tools/rig/standalone_rig.py emulator_text).
