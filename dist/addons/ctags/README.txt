up-ctags: ctags, the source tag generator, for UP-Term

Contents: bin/ctags, share/man/man1/ctags.1, licenses/ctags.
Licence: GPL v2 or later; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share, licenses): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  ctags --version

Install: run Install (Installer) or, with upkg, install up-ctags. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
