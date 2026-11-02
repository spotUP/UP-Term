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
