up-uhc: the UHC tools (Ultimate Hunting Companion) for AmigaOS 3, for UP-Term

Contents: this package carries no UHC files. Install runs UHC's own installer: it downloads uhcinstall from
uhc.megaburken.net and runs it (ARCH=os3 DEST=SYS:UHC SILENT). That installer downloads and unpacks the UHC tools
(aget, UHCSearch, the Aminet, WHDLoad and other get/search scripts), sets UHCBIN and ENVARC:UHC and adds its
own ";BEGIN UHC" block to S:User-Startup. Nothing of UHC is modified. Needs a network connection, UP-Term:bin/curl or
UP-Term:bin/bebboget, and LhA (UHC's installer unpacks with it). Without LhA Install stops with an error and
return code 20 before UHC's installer runs. A failed download ends the install with an error and return code 20.
Licence notes: LICENSES.txt.

Needs up-term.lha installed first (version 1.0 or newer).

UHC lives in SYS:UHC, one tree. A UHC install that is there already (SYS:UHC/UHC-Startup, or UHCBIN naming an existing
drawer) is used as it is: UHC's installer does not run over it (a second run would add a second UHC block to
S:User-Startup and reset ENVARC:UHC), and nothing of it is replaced. No UHC variable names UP-Term:, because
S:User-Startup runs UHC's startup before UP-Term: is assigned. Upgrading from 1.0 to 1.3 (UHC in UP-Term:uhc) takes
the old ";BEGIN uhc" block out of S:User-Startup, drops the UHC variables that named UP-Term:, installs UHC as above
and copies your UHC packages (ExtBin, ExtDoc, Packages/Installed) from UP-Term:uhc into SYS:UHC.
In a Shell, for example:  aget

Install: run Install (Installer) or, with upkg, install up-uhc. Uninstall takes only up-uhc's own leftovers out
(the old UP-Term:uhc tree of 1.0 to 1.3); UHC stays: SYS:UHC, its ";BEGIN UHC" block, ENVARC:UHC and your packages.
UHC updates itself (uhcupdate).
