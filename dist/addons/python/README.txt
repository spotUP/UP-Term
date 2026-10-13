UP-Term-python - Python for an UP-Term window
=============================================

Python 3.14 (15 MB), built for AmigaOS 3.x (68020+). It goes into UP-Term:Python3,
and Python3: is assigned to it at every boot (a block in S:User-Startup).

WHAT YOU NEED
  UP-Term.lha 1.0 or newer, installed first: Python runs on UP-Term's patched
  ixemul.library 80. Install checks it.
  AmigaOS 3.0 or newer, a 68020 or better, and room for 15 MB.

INSTALL
  Unpack the archive and double-click Install in its drawer (needs C:Installer or
  SYS:Utilities/Installer). From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files
  The first part checks that UP-Term.lha is installed and new enough; when it is not,
  Install says so and stops before it copies anything. Install the archive over an
  earlier one and its first page offers Update, which copies only what changed.
  An UP-Term installed from the single kit (before the kit split) that already has this
  program keeps it: Install recognises it and offers Update from "installed by the
  single UP-Term kit".

USE
  In an UP-Term window, in vsh:
    python3
    python3 -c "print(6*7)"
  vsh's path already names /UP-Term/Python3/bin.

UNINSTALL
  Double-click Uninstall (or Execute Uninstall), or Uninstall of UP-Term.lha, which runs
  it first. It removes the Python3: assign and only this archive's block in
  S:User-Startup; your own files are not touched.
