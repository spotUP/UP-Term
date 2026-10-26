up-lua: Lua, the scripting language, for UP-Term

Contents: bin/lua, bin/luac, share/man/man1/lua.1, share/man/man1/luac.1, licenses/lua.
Licence: MIT; see LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer); these files were part of it before the
slim base. It installs them at the same paths in UP-Term: (bin, share, licenses): no new drawer, no
change of the path. In an UP-Term window, in vsh:
  lua -v

Install: run Install (Installer) or, with upkg, install up-lua. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.
