up-vbcc: the vbcc C compiler for AmigaOS 68k, for UP-Term

Contents: vbcc 0.9h patch2 (vc, vbccm68k, vasm, vlink, the m68k-amigaos target), unmodified from
phoenix.owl.de, with UP-Term's configuration config/upterm68k. It installs in UP-Term:vbcc.
Licence: redistributable unmodified, non-commercial use; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer). Install adds VBCC: (assign), the path
VBCC:bin and the environment variable VBCC to S:User-Startup, and sets them for the current session.
The Amiga NDK headers are not part of this package (no licence to redistribute them): point NDKINC:
at your Include_H drawer, for example  Assign NDKINC: Work:NDK/Include_H  and then, in a Shell:
  vc +upterm68k hello.c -o hello

Install: run Install (Installer) or, with upkg, install up-vbcc. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
