vtcon - a console window for AmigaOS 3.x (68020+)
================================================

XCON: is a console window like CON:, with a modern terminal inside:

  - xterm dialect (default): what Unix ports expect - colours (16, 256,
    RGB), scroll regions, alternate screen, mouse, bracketed paste.
    ixemul programs run with TERM=vtcon (less, nano, BitchX ...).
  - Amiga dialect (option AMIGA): the ROM console.device sequences.
    Amiga programs also work in the xterm dialect: the 8-bit CSI ($9B)
    they send keeps its Amiga meaning there.
  - PC-ANSI dialect (option PCANSI): ANSI.SYS / BBS art, CP437.

INSTALL
  Unpack, cd into the drawer, then:   Execute Install
  (copies L:vtcon-handler, DEVS:DOSDrivers/XCON, the terminfo entry to
  ENVARC:terminfo and mounts XCON:)

USE
  NewShell "XCON:0/20/640/300/My Shell/CLOSE"

  Options after the title, one per /: CLOSE WAIT BACKDROP NODRAG NOBORDER
  NOSIZE NODEPTH INACTIVE  SCREEN name  FONT name size
  XTERM AMIGA PCANSI (dialect)  LATIN1 or CP437 (xterm byte set; default
  UTF-8)

  For ixemul programs:   SetEnv TERM vtcon
                         SetEnv TERMINFO /ENVARC/terminfo
  (or copy termcap.vtcon into your ixemul termcap)

KEYS
  Mouse drag            select (Shift+drag when a program uses the mouse)
  Right Amiga C / V     copy / paste (clipboard, IFF FTXT)
  Shift+PgUp / PgDn     scroll back / forward
  Right Amiga Up / Down scroll back / forward one line
  In the Shell line: Left/Right, Shift+Left/Right, Ctrl-A/E/K/U/W/X,
  Up/Down history, Shift+Up/Down history search, Ctrl-\ end of file.

STATUS
  Test build. CON:/RAW: are not replaced; XCON: runs beside them.
