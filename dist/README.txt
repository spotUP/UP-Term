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
  It asks whether UP-Term should also serve CON: and RAW: (see CON: AND
  RAW: below); answer in advance with  Execute Install CONSOLE  or
  Execute Install NOCONSOLE. The same for console.device (see
  CONSOLE.DEVICE below): DEVICE or NODEVICE.

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

CONFIGURATION (profiles)
   XCON: windows are user-configurable like iTerm2's: /ENV/up-term/up-term
   holds named profiles; the Prefs app (SYS:Utilities/UP-Term-Prefs, its
   tool is "C:UP-Term Prefs") writes it. Install lays down a fully
   commented sample; without the file every window behaves as before.

   Prefs: two pages, General and Colors. Type the profile name in the
   Profile field, press Load to edit an existing profile or New to start
   one, change the values, press Save. Save writes ENVARC:up-term/up-term
   and keeps the file that was there as up-term.orig; Cancel closes without
   writing. Open windows keep the settings they started with; a new window
   takes the file. Preferences apply to XCON:, PTY: and RAW: windows (the
   console.device ones, CON: under console.device, are not touched).

   A window takes the profile named by the spec option  PROFILE <name>
   (otherwise the "default" profile). Precedence, low to high: the built-in
   defaults < the profile < the window spec options (DARK, FG, BG, FONT ...)
   < the running program (OSC colours, DECSCUSR). A profile only changes
   what it names; it does not inherit from another profile.

   The keys (all optional, case-insensitive):
     font = TOPAZ:8.8.font     font name : size
     fg = C0C0C0  bg = 000000  default foreground / background (RRGGBB hex)
     scrollback = 2000         lines to keep (0 = the built-in 500, -1 = none)
     cursor = block            block | underline | bar
     cursor-blink = off        on | off
     cursor-color = inverse    inverse (the flipped cell) or RRGGBB
     selection-bg = 87AFD7     the selected cell's background and text
     selection-fg = 262626     colour; blank either keeps the flip
     bell = beep               none | beep | visual (a one-frame screen flash)
     bold-bright = on          xterm only: SGR 1 takes the bright colours 8-15
     meta = amiga              amiga (Left Amiga = Meta) | alt (the Alt keys)
     copy-on-select = off      on: a drag ends with the selection on the clipboard
     wheel = scroll            scroll | ignore (the mouse wheel moves the scrollback)
     palette = 1,0x00CD00,4,0x5C5CFF
                               remap ANSI colours: index,RRGGBB pairs

   Example: a dim, silent editor window
     NewShell "XCON:0/20/640/300/vim/PROFILE vim/CLOSE"
   with  [profile vim]  bell = none  in the file.

   Themes from another terminal convert to a profile section with
   tools/theme_import.py in the source tree; it reads the Terminal.app,
   iTerm2, Alacritty, Warp and Ghostty formats and prints the fg, bg,
   cursor-color and palette lines to paste here.

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

STARTING IT
  Workbench: double-click SYS:Utilities/UP-Term. It opens an XCON: window
  with vsh, starting in $HOME (SYS:) with your Workbench path. Change the
  window in the icon's WINDOW tooltype (any XCON: spec).
  (Directory Opus: Shift + double-click runs a project through its icon;
  a plain double-click opens it by file type.)
  Shell: NewShell "XCON:0/20/640/300/UP-Term/CLOSE", then vsh.

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
  Stack: vsh needs none set (it takes 64 KB itself). The commands it runs
  get the stack the builtin  stack [bytes]  sets (at least 16000), or more
  when their file asks for it with a $STACK: cookie (as on AmigaOS 3.2).
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

SCREEN
  GNU screen 4.9.1: C-a c new window, C-a n / C-a p next / previous,
  C-a " list, C-a S / C-a | split, C-a Tab next region, C-a d detach,
  screen -r to come back. vsh runs in its windows (ENV:screenrc: shell,
  256 colours). Each window costs about 1 MB (its process and shell).

TMUX
  tmux 3.6a: C-b c new window, C-b % / C-b " split, C-b arrows move
  between panes, C-b d detach, tmux attach to come back. vsh runs in its
  panes (ENV:tmux.conf: default-shell, 256 colours). Your own settings go
  in ~/.tmux.conf.

THE SHELL FOR UNIX PROGRAMS
  ixemul programs run their shell commands (system(), popen(), tmux's
  run-shell and #() status jobs) with /gg/bin/sh, which is GG:bin/sh, where
  Geek Gadgets keeps its sh. Without a GG: Install puts vsh there: the
  drawer SYS:UP-Term, assigned as GG: by a block in S:User-Startup between
  ;BEGIN UP-Term and ;END UP-Term (the file as it was is kept as
  S:User-Startup.before-UP-Term). Uninstall takes the block out again. An
  existing GG: (an ADE or Geek Gadgets install) is left alone.

CON: AND RAW:
  UP-Term can serve the system's CON: and RAW: too, so every new Shell
  window is an UP-Term window (the Amiga personality: programs see the
  console they know). C:UPConsole switches it:
    UPConsole CON ON     new CON:/RAW: windows are UP-Term
    UPConsole CON OFF    new windows are the system's again
    UPConsole STATUS     who serves CON: and RAW: now
  Windows already open keep whoever opened them. When Install switched it
  on, a block in S:User-Startup between ;BEGIN UP-Term console and
  ;END UP-Term console does it at every boot; Uninstall switches it off and
  takes the block out. It refuses to switch when another console
  replacement (KingCON, ViNCEd, ...) serves CON:, and on AmigaOS 3.2, whose
  Shell needs a console mode UP-Term does not have yet.

CONSOLE.DEVICE
  UP-Term can also be console.device itself: every console window any
  program opens (Ed, More, a Shell's CON: window from the system's
  con-handler) then draws with UP-Term's engine, the Amiga personality,
  with the ROM's public behaviour. C:UPConsole switches it at runtime:
    UPConsole DEVICE ON      console windows opened from now on are UP-Term's
    UPConsole DEVICE OFF     the ROM's again (open windows keep UP-Term's
                             until they close)
    UPConsole EXCLUDE name   the program with that task name gets the ROM's
                             units (for one that needs the ROM's internals)
    UPConsole EXCLUDE CLEAR  nobody excluded
  Install's block ;BEGIN UP-Term device / ;END UP-Term device in
  S:User-Startup switches it at every boot; Uninstall switches it off and
  takes the block out. It refuses to switch when console.device has been
  patched by another program (SetFunction). Fields of struct ConUnit that
  programs write are overwritten at the next output, except the keymap
  (CD_SETKEYMAP). Not offered on AmigaOS 3.2 yet.

STATUS
  Test build. CON:/RAW: and console.device stay the system's unless you
  switch them (above); XCON: runs beside them.
