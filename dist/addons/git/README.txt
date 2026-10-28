up-git: git 2.56.0 for UP-Term

Contents: git 2.56.0 in UP-Term:bin and UP-Term:share/git-core.
Licence: GPL-2.0-only; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer). git needs a volume with long file names: FFS in long-name mode (DOS7), PFS3 or SFS. OFS and plain FFS stop at 30 characters, too short for git's object files (38 hex characters). In an
UP-Term window, in vsh:
  git

Install: run Install (Installer) or, with upkg, install up-git. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
