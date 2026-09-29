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
  (copies the handlers, DOSDrivers entries, vsh, ixkill, the terminal
  entries to ENVARC:up-term, sets TERMINFO, puts in the patched
  ixemul.library and mounts XCON: and PTY:)

USE
  NewShell "XCON:0/20/640/300/My Shell/CLOSE"

  Options after the title, one per /: CLOSE WAIT BACKDROP NODRAG NOBORDER
  NOSIZE NODEPTH INACTIVE  SCREEN name  FONT name size
  DARK (light grey on black: the default except with AMIGA)  LIGHT (the screen's
  own text and background pens)  FG rrggbb  BG rrggbb (default colours, hex)
  XTERM AMIGA PCANSI (dialect)  LATIN1 or CP437 (xterm byte set; default
  UTF-8)

  Terminal type: vsh sets TERM=vtcon and TERMCAP for the programs it runs
  when it is in an XCON: window. From the AmigaDOS Shell in XCON:, or in
  tcsh, set them for that shell:
                         SetEnv TERM vtcon
                         SetEnv TERMCAP /ENV/up-term/termcap.vtcon
  (not globally: a ROM CON: window cannot show vtcon's sequences).
  TERMINFO (set by Install) is /ENV/up-term/terminfo.

KEYS
  Mouse drag            select (Shift+drag when a program uses the mouse)
  Right Amiga C / V     copy / paste (clipboard, IFF FTXT)
  Shift+PgUp / PgDn     scroll back / forward
  Right Amiga Up / Down scroll back / forward one line
  Left Amiga + key      Meta (ESC prefix) for Unix programs; Alt stays
                        with your keymap
  In the Shell line:
    Left/Right, Shift+Left/Right, Ctrl-A/E     move; Ctrl+Left/Right words
    Ctrl-K/U/W/X, Meta-D, Meta-Backspace       kill; Ctrl-_ undo
    Up/Down, Shift+Up/Down                     history; history by prefix
    Ctrl-R                                     search history as you type
    grey text after the cursor                 a suggestion from history:
                                               Right or End takes it
    Tab                                        complete (commands for the
                                               first word, else files);
                                               Tab again lists, then cycles
    Ctrl-L                                     clear, keep prompt and line
    Ctrl-\                                     end of file
  The command word shows green when it is a command, red when not.
  History is kept in ENVARC:vtcon.history (the last 100 lines).

VSH (the shell)
  Type  vsh  in an XCON: window. A POSIX shell with zsh touches: pipes,
  redirections (2> 2>&1 &> <<EOF), $( ), $(( )), globs, if/while/for/case,
  functions, aliases, jobs (& jobs fg bg wait, "[1] Done" notices), Ctrl-C to
  the running command, Ctrl-Z to suspend it (ixemul programs: a native
  Amiga command cannot be stopped; vsh says so). A stopped job gets its
  terminal settings back with fg; its console reads wait while it is
  stopped or in the background. ixkill (C:ixkill) sends Unix signals to
  ixemul programs:  ixkill -TERM 0x<process> Subshells, $( ) and all but the last stage of a
  pipeline run as processes of their own (there is no fork on the Amiga).
  Amiga commands run as usual; the command path is the Shell's (Path).
  Startup: ENVARC:vsh/vshrc, then $HOME/.vshrc (HOME defaults to SYS:).
  Prompt: PS1 takes bash (\w \u \h) and zsh (%~ %n %m %? %F{red}...%f)
  escapes; default %F{cyan}%~%f %#.
  The vshrc gives Unix names: ls (-l) mkdir (-p) rm (-r -f) cp (-r) mv cat
  touch clear ll; ../x is passed on as /x.

PTY: (pseudo-terminals)
  For terminal multiplexers and remote shells: PTY:<id>/m is the master,
  PTY:<id>/s the slave, with the same Unix line discipline XCON: runs
  between them (termios, Ctrl-C, Ctrl-\, Ctrl-Z, window size). A master
  is open once per id; a program looks for a free one by trying ids.

IXEMUL
  Install puts in ixemul.library 48.2 with the UP-Term patches: termios,
  window size and signal keys go to the terminal (XCON:, PTY:), Ctrl-Z and
  fg/bg job control work, /dev/ptyXY are PTY: pairs. It runs the existing
  ixemul programs (it is 48.2, built with the compiler 48.2 was). The
  library that was there is kept as LIBS:ixemul.library.orig; to go back:
    Copy LIBS:ixemul.library.orig LIBS:ixemul.library
  To tell which one is in:  Search LIBS:ixemul.library UP-Term

STATUS
  Test build. CON:/RAW: are not replaced; XCON: runs beside them.
