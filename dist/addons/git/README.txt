up-git: git 2.56.0, the version control system for UP-Term

Contents: bin/git, its dashed programs (git-upload-pack and others) and the templates in share/git-core.
Licence: GPL v2 only; see LICENSES.txt.

Needs UP-Term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share, licenses): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  git --version

Install: run Install (Installer) or, with upkg, install up-git. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
