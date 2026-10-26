up-jq: jq, the JSON processor, for UP-Term

Contents: bin/jq, share/man/man1/jq.1, licenses/jq.
Licence: MIT; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share, licenses): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  jq --version

Install: run Install (Installer) or, with upkg, install up-jq. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
