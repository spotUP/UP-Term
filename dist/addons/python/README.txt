Python3 - Python 3.14 for AmigaOS
=================================

Python 3.14 (15 MB), built for AmigaOS 3.x (68020+) on ixemul. It stands alone: it installs
on a plain Workbench, with the ixemul.library it needs. UP-Term.lha is optional.

WHAT YOU NEED
  AmigaOS 3.0 or newer, a 68020 or better, and room for 15 MB. ixemul.library 47 or newer:
  see THE IXEMUL.LIBRARY RULE.

THE IXEMUL.LIBRARY RULE
  Python runs on any ixemul.library of version 47 or newer (tested with 48.3 and with
  UP-Term's 80.1). Install looks at LIBS:
    - a library of 47 or newer is there: it stays as it is. Nothing is copied: never a
      downgrade, and never over UP-Term's patched ixemul.library (version 80), which Python
      then uses;
    - none, or an older one: this archive's ixemul.library 80.1 and its ixnet.library (the
      same build; ixnet refuses another ixemul revision) go into LIBS:, and an older pair is
      kept there as <name>.before-Python3.
  Either way ENVARC:upkg/ixemul/needs/Python3 says that Python needs the library; the file
  installed-by-Python3 beside it says that this archive put it there. Uninstall removes the
  library only if this archive installed it, UP-Term is not installed and no other package
  recorded a need in ENVARC:upkg/ixemul/needs; it then puts the older pair back.

INSTALL
  Unpack the archive and double-click Install in its drawer (needs C:Installer or
  SYS:Utilities/Installer). From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files
  It needs no UP-Term and looks for none. Python goes into SYS:Python3 and Python3: is
  assigned to it now and at every boot (a block in S:User-Startup, ;BEGIN Python3 to
  ;END Python3, that assigns only while SYS:Python3 exists). Its record (the list of files,
  for Update and Uninstall) is ENVARC:upkg/up-python. Install the archive over an earlier one
  and its first page offers Update, which copies only what changed.
  An UP-Term installed from the single kit (before the kit split) keeps its own copy in
  UP-Term:Python3 (it goes with the UP-Term drawer); this archive installs a second one.

USE
  In a Shell:      Python3:bin/python3
                   Python3:bin/python3 -c "print(6*7)"
  In an UP-Term window (vsh): python3     (vsh's path names /SYS/Python3/bin)
  The REPL's line editing is the console's: Up recalls a line in KingCON and in UP-Term, not
  in the ROM CON: window. Do not set TERM=vtcon outside an UP-Term window: the REPL hangs
  on Up there (ledger Y1).

UNINSTALL
  Double-click Uninstall (or Execute Uninstall). It removes SYS:Python3, the Python3: assign,
  this archive's block in S:User-Startup, and the ixemul.library as THE IXEMUL.LIBRARY RULE
  says. S:User-Startup is put back as Install found it when it is still as long as that plus
  the block; otherwise the block stays (it does nothing without SYS:Python3) and Uninstall
  says so. Uninstall of UP-Term.lha leaves this program alone.
