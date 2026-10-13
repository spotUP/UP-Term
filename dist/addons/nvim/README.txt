UP-Term-nvim - Neovim for an UP-Term window
===========================================

Neovim 0.12 with its runtime (about 2100 files, 32 MB), built for AmigaOS 3.x (68020+).
It goes into UP-Term:nvim, beside the base's Unix commands.

WHAT YOU NEED
  UP-Term.lha 1.0 or newer, installed first: Neovim runs on UP-Term's patched
  ixemul.library 80 and in an UP-Term window (XCON:, vsh). Install checks both.
  AmigaOS 3.0 or newer, a 68020 or better, and room for 32 MB.

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
    nvim file.txt
  vsh's path already names /UP-Term/nvim/bin. Neovim follows a resized window.

UNINSTALL
  Double-click Uninstall (or Execute Uninstall), or Uninstall of UP-Term.lha, which
  runs it first. Your own configuration is not touched.
