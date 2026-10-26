up-mandoc: mandoc, the man page viewer, for UP-Term

Contents: bin/mandoc, bin/man, share/man/man1/mandoc.1, share/man/man1/man.1, share/man/man7/man.7, share/man/man7/mdoc.7, share/man/man7/roff.7, licenses/mandoc.
Licence: ISC; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share, licenses): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  mandoc -V

Install: run Install (Installer) or, with upkg, install up-mandoc. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
