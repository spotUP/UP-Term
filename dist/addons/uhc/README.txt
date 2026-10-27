up-uhc: the UHC tools (Ultimate Hunting Companion) for AmigaOS 3, for UP-Term

Contents: uhctools_os3.lha from uhc.megaburken.net, unmodified except S/uhcupdate and S/uhccheck (see below) (aget, UHCSearch, the Aminet, WHDLoad and
other get/search scripts). It installs in UP-Term:uhc. Permission to redistribute: its author, Patrik.
Licence notes: LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer). Install adds UHC: (assign), the path UHC:C and
UHC:S, and the environment variable UHCBIN to S:User-Startup, and sets them for the current session.
In a Shell, for example:  aget

Install: run Install (Installer) or, with upkg, install up-uhc. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.

Changes to the author's files: S/uhcupdate and S/uhccheck replace the whole tool set from the author's server,
so here they are two small scripts that print "uhcupdate is disabled in UP-Term: use upkg upgrade" and end with
return code 5 (WARN). A new UHC catalogue arrives as a new version of up-uhc. Install also fills ENV:UHC from
ENVARC:UHC (and the User-Startup block does at every boot), which the UHC scripts need; Uninstall removes both.
