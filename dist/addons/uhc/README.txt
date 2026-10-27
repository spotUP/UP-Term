up-uhc: the UHC tools (Ultimate Hunting Companion) for AmigaOS 3, for UP-Term

Contents: this package carries no UHC files. Install downloads uhctools_os3.lha from uhc.megaburken.net (aget,
UHCSearch, the Aminet, WHDLoad and other get/search scripts), checks it is the UHC tools (the version strings in
UHCFixENV and UHC-Startup), unpacks it into SYS:UHC and puts two small scripts of ours over S/uhcupdate and
S/uhccheck (see below). Needs a network connection and UP-Term:bin/curl or UP-Term:bin/bebboget, and lha or
UP-Term:bin/tar to unpack. A failed download ends the install with an error and return code 20.
Licence notes: LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer). Install adds UHC: (assign), the path UHC:C and
UHC:S, and the environment variable UHCBIN to S:User-Startup, and sets them for the current session.

UHC lives in SYS:UHC, one tree. A UHC install that is there already (SYS:UHC/UHC-Startup, or UHCBIN naming a drawer) is used
as it is: nothing is downloaded, replaced or added to S:User-Startup, and Uninstall leaves it alone. No UHC variable names
UP-Term:, because S:User-Startup runs UHC's startup before UP-Term: is assigned. Upgrading from 1.3 moves its UP-Term:uhc
settings and packages to SYS:UHC.
In a Shell, for example:  aget

Install: run Install (Installer) or, with upkg, install up-uhc. Uninstall takes the package out
again; the UP-Term Uninstall runs it first.

Changes to the author's files (made on your machine after the download): S/uhcupdate and S/uhccheck replace the whole tool set from the author's server,
so here they are two small scripts that print "uhcupdate is disabled in UP-Term: use upkg upgrade" and end with
return code 5 (WARN). A new UHC catalogue arrives as a new version of up-uhc. Install also fills ENV:UHC from
ENVARC:UHC (and the User-Startup block does at every boot), and adds EXTBIN and EXTDOC (drawers ExtBin and ExtDoc under UHCBIN, UHC's own default)
when they are missing, which the UHC scripts need. Your settings in ENVARC:UHC are kept over upgrade and remove.
