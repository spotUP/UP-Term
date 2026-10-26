up-nc: nc, the OpenBSD netcat, for UP-Term

Contents: bin/nc, share/man/man1/nc.1.
Licence: ISC; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  nc -h

Install: run Install (Installer) or, with upkg, install up-nc. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
