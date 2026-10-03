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
  Unpack the archive and double-click Install in the UP-Term drawer
  (needs C:Installer or SYS:Utilities/Installer, AmigaOS 3.0 and up).
  It copies the handlers, DOSDrivers entries, vsh, ixkill, the terminal
  entries to ENVARC:up-term, sets TERMINFO, puts in the patched
  ixemul.library and mounts XCON: and PTY:. A page of check boxes picks
  the optional parts: ssh and scp, bebboget, curl (see NETWORK), the
  serial login (SERIAL LOGIN), the UP-Term icon in SYS:System and the
  Shell icon. On 3.0/3.1 a second page asks whether UP-Term should also
  serve CON: and RAW: (see CON: AND RAW: below) and console.device (see
  CONSOLE.DEVICE below).
  From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files [CONSOLE|NOCONSOLE] [DEVICE|NODEVICE]
      [SERIAL] [SYSICON] [SHELLICON] [SSH] [BEBBOGET] [CURL] [WASABI] [CPU040|CPU060]
  (each optional part only when named).
  Remove everything again: double-click Uninstall (or Execute Uninstall).

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
     font-fallback = SymbolsNerdFontMono
                               an installed outline font for what the font
                               cannot show (see OUTLINE FONTS below)
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

   Themes: the kit installs 112 colour themes in ENVARC:up-term/themes.
   In UP-Term Prefs, Colors page, press Theme... and pick one: its colours
   go into the profile you are editing; Save (or Use) applies them to the
   windows opened after.

   Themes from another terminal convert to a profile section with
   tools/theme_import.py in the source tree; it reads the Terminal.app,
   iTerm2, Alacritty, Warp and Ghostty formats and prints the fg, bg,
   cursor-color and palette lines to paste here.

OUTLINE FONTS (icons, other scripts)
   Amiga bitmap fonts hold 256 characters. A character outside them (a
   Nerd Font icon in a prompt or a Neovim status line, CJK, symbols) shows
   as '?', or as a near Latin-1 stand-in ('-' for a dash). With
   font-fallback naming an installed outline font, UP-Term draws those
   characters from it, at the size of your font; everything else keeps
   your bitmap font. Characters up to U+FFFF.
   You need a font engine for TrueType fonts: ttf.library 0.8.5 by Richard
   Griffith, Aminet util/libs/ttflib68020 (also 68000, 68030, 68040 and
   68060 builds), freely distributable, AmigaOS 3.0 or newer. Copy
   ttf.library to LIBS:, then install a font with its ttfinstall, e.g. the
   icons-only Nerd Font (github.com/ryanoasis/nerd-fonts, NerdFontsSymbolsOnly,
   MIT licence):
     ttfinstall SymbolsNerdFontMono-Regular.ttf FONTS:
   and put  font-fallback = SymbolsNerdFontMono  (the .otag's name) in a
   profile, or type it in UP-Term Prefs, General page, Fallback font.
   Open windows take it at once.

COMMANDS (/cursor bar)
   Every setting of the menus and of Prefs is also a command you type at
   the prompt: a line that starts with "/" and one of these names is
   UP-Term's, it answers in the window and the shell shows a new prompt.
   /help lists them; Tab completes the names and their values.
     /cursor bar        /bell visual      /scrollback 5000   /font topaz 11
     /fg C0C0C0         /theme dracula-default /profile vim       /tab new
     /completion kingcon                  /font-fallback SymbolsNerdFontMono
   They change the window you type in; /save writes them to its profile.
   "/" alone, "//", "/Work" and every other path are the shell's as
   before, and a line that starts with a blank goes to the shell as typed
   (" /cursor" runs a program called cursor in the parent directory).
   From a script, or in a window whose shell does its own line editing,
   C:UPTerm does the same: UPTerm cursor bar (return code 10 when refused).

MENU
  An XCON: window has a menu (right mouse button, on the screen's title
  bar):
    UP-Term   New tab, Next tab, Previous tab, Close tab, Preferences...
              (opens UP-Term Prefs), About UP-Term..., Close window
    Edit      Copy, Paste, Select all, Find..., Find next, Clear screen,
              Clear scrollback, Reset terminal
    View      Bigger font, Smaller font (the font's next size on disk),
              80 x 24, 132 x 43 (the window sized to that grid)
    Settings  Font..., Theme..., Cursor, Bell, Scrollback, Bold is
              bright, Meta key, Copy on select, Wheel scrolls, Tab
              completion, KingCON style, Profile, Save settings to profile
  Settings are the same as UP-Term Prefs, for this window and at once
  (Prefs keeps them for the profile). With KingCON completion a Complete
  menu follows. Every item is also a command (see COMMANDS). The right
  button therefore opens the menu rather than reaching a program that
  asked for mouse reports.

KEYS
  Mouse drag            select (Shift+drag when a program uses the mouse)
  Right Amiga C / V     copy / paste (clipboard, IFF FTXT)
  Shift+PgUp / PgDn     scroll back / forward
  Right Amiga Up / Down scroll back / forward one line
  Right Amiga T         new tab (a shell of its own, the same profile)
  Right Amiga 1-9       that tab; Right Amiga . and , the next / previous
                        (or click the tab bar); the close gadget closes
                        the tab you see
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
  Tab completion has two styles, chosen per profile in UP-Term Prefs
  ("Tab completion"), or with  completion = unix | kingcon  in the file:
    Unix (the default)   as above
    KingCON              as KingCON (David Larsson) does it:
      Tab           file names, anywhere on the line (none: device names)
      Shift-Tab     devices, volumes and assigns
      Alt-Tab       commands (also Left Amiga-Tab)
      Ctrl-D        on a line with text: list the word's directory
      One match goes in with "/" after a directory, a space after a file,
      quoted when the name has a space. Several open a "Select ..." window:
      Tab / Shift-Tab / cursor keys move, Return or a double-click takes the
      name, Escape or Cancel leaves the line as it was. Tab on an empty
      word opens a file requester.
    KingCON style (Prefs, or  kingcon-mode = letters ) picks KingCON's
    FNCMODE: W the window (the default), L print the list, B cycle through
    the names on the line (Tab next, Shift-Tab back, Ctrl-S the window), C
    complete the part all names share first (then W or B on the next Tab),
    S no beeps. ".info in lists" (kingcon-info = show) lists .info files.
    A KingCON window has KingCON's Complete menu: Filename, Command and
    Device (as the keys), Enable cache (the command directories' names are
    kept between Tabs; kingcon-cache = off, or "Directory cache" in Prefs,
    starts with it off), Reset cache (read every directory again), Purge
    cache (free the kept names) and Show .info. The menu's switches are for
    that window; Prefs sets the profile.

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
  Commands are looked for in $PATH (Unix form, as ixemul programs read it:
  /SYS/UP-Term/bin:/gg/bin:/c by default), then the Shell's path (Path).
  Startup: ENVARC:vsh/vshrc, then $HOME/.vshrc (HOME defaults to SYS:).
  Stack: vsh needs none set (it takes 64 KB itself). The commands it runs
  get the stack the builtin  stack [bytes]  sets (at least 16000), or more
  when their file asks for it with a $STACK: cookie (as on AmigaOS 3.2).
  Prompt: PS1 takes bash (\w \u \h) and zsh (%~ %n %m %? %F{red}...%f)
  escapes; default %F{cyan}%~%f %#.
  Unix commands: GNU coreutils 5.2.1 (ls cp mv rm mkdir cat sort head tail
  wc tr cut uniq seq du stat dd tee date and the rest, 86 in all) in
  SYS:UP-Term/bin, first in vsh's $PATH. They are not in C: (AmigaDOS
  names ignore case: GNU sort would be C:Sort), so the AmigaDOS Shell keeps
  its own commands. .. is the parent directory, / the list of volumes.
  /tmp is TMP:; without one Install assigns it to T: at every boot.
  coreutils is GPL v2: COPYING and the complete source are in the kit's
  Files/coreutils drawer.

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

SERIAL LOGIN
  A Unix-style login on the serial port: connect a null-modem cable (or a
  USB serial adapter) to another computer, open a terminal program there
  (screen /dev/ttyUSB0 19200, minicom, PuTTY), and on the Amiga run
    upgetty LOOP
  The far end then has vsh with a real tty: line editing, Ctrl-C/Z as
  signals, the window size (stty rows/cols), vim, less, tmux.
  Options: BAUD n (default 19200), RTSCTS (hardware flow control; needed
  for 115200 and a cable that carries RTS/CTS), UNIT n, DEVICE name,
  ROWS n COLS n, TERM name, SHELL command, LOOP (a new shell when one ends).
  Files over the same line, ZMODEM: in the login,  sz file ...  sends
  (the far end's  rz  or terminal program receives), and  rz  receives
  into the current directory (OVERWRITE replaces files there). Unlike
  NewShell AUX:, every byte arrives as it was sent.

THE SHELL FOR UNIX PROGRAMS
  ixemul programs run their shell commands (system(), popen(), tmux's
  run-shell and #() status jobs) with /gg/bin/sh, which is GG:bin/sh, where
  Geek Gadgets keeps its sh. Without a GG: Install puts vsh there: the
  drawer SYS:UP-Term, assigned as GG: by a block in S:User-Startup between
  ;BEGIN UP-Term and ;END UP-Term (the file as it was is kept as
  S:User-Startup.before-UP-Term). Uninstall takes the block out again. An
  existing GG: (an ADE or Geek Gadgets install) is left alone.
  In vsh, a name that starts with "/" means what it means on AmigaDOS
  (the parent directory) when something is there, and otherwise what it
  means on Unix: /RAM/notes is RAM:notes, as ixemul programs name it.

A SCREEN OF ITS OWN, FULL SCREEN
  An UP-Term window can have a screen to itself: Settings > Screen >
  Own screen or Full screen (or /screen own, /screen fullscreen; back
  with Workbench). The text, the scrollback and the line you are typing
  go along. Full screen is one borderless window over the whole screen;
  the menus still open on the right mouse button, the title shows in the
  screen's title bar.
  The screen has the Workbench's mode and size, 32 colours on an AGA
  screen (16 on ECS) or 256 on a graphics card: the terminal's 16 ANSI
  colours exactly, and the window frames, title bar and menus in your
  Workbench's colours (on ECS the nearest of the 16). It is a public screen ("UP-Term", "UP-Term.2" ...): other
  programs can open on it. It closes with the last UP-Term window on it,
  or, if another program's window is still there, when that one closes.
  From the start, in the window's name or in a profile:
    XCON:0/0/640/256/UP-Term/FULLSCREEN
    XCON:...../OWNSCREEN    PUBSCREEN name    SCREENMODE 0x29000    DEPTH 4
    screen = fullscreen     screen-mode = 0x29000     screen-depth = 4
  (0x29000 is PAL hires; ScreenMode prefs show the numbers. Not with tabs:
  move the window before opening a second tab.)

FONTS BY PIXEL SHAPE
  topaz 8 is drawn for tall pixels. On screens with square pixels
  (graphics cards, AGA hires interlaced) UP-Term draws it as TopazPro 16,
  topaz redrawn for square pixels, and IBM 8 as IBM 16; on PAL / NTSC
  hires the 8-pixel fonts again. A window moved between screens changes
  with them. Other fonts, and topaz 9 or 11, stay as chosen. Install puts
  TopazPro and IBM in FONTS: (only the sizes not there). To keep the font
  as asked everywhere: font-aspect = off in the profile.

NETWORK
  Install puts network tools in SYS:UP-Term/bin when you say yes (the
  Installer asks); they need a TCP/IP stack (Roadshow, AmiTCP):
    ssh, scp        BebboSSH 1.45 (bebbossh, bebboscp), with bebbosshkeygen
                    and the server bebbosshd; libcryptossh.library in LIBS:
    bebboget        https downloads (installcerts adds root certificates)
    curl            curl 8.22.0; it also needs AmiSSL 5 (Aminet
                    util/libs/AmiSSL-v5-OS3.lha)
  In vsh, ssh runs bebbossh with TERM=xterm-256color, which is what an
  UP-Term window is to a Unix machine. Keys: bebbosshkeygen makes one
  (ENVARC:.ssh/id_ed25519); bebbossh -i names another. The first connection
  to a host asks whether to trust its key.
  bebbosshd, the server, answers a login with its own simple shell, not a
  terminal: full-screen programs do not run through it.
  Licences: BebboSSH and bebboget by Stefan Franke, GPL v3 or later; curl
  under the curl licence. Their COPYING files and sources (or where the
  source is) are in the kit's Files/net drawer.

DEMO
  UPDemo, typed in an UP-Term window (76 x 20 characters or more), shows
  what the terminal draws: text styles, double-size lines, 256 and 24-bit
  colours, box drawing, scroll regions, and text-mode demo effects --
  copper bars, plasma, fire, a rotozoomer, vector cubes, a sine scroller,
  palette cycling. Space: the next scene, B: back, Q: quit; UPDemo 5
  starts at scene 5. Your shell comes back as it was.
  UPDemo BENCH runs every scene for three seconds and prints the frames
  a second each reached: a benchmark of the terminal on your machine.
  The full-screen effects need a fast processor (a 68060, a PiStorm, an
  emulator at full speed); a stock A1200 draws them at about a frame a
  second.

WASABI (REMOTE DEVELOPMENT)
  For working on this Amiga from a Mac or Linux machine on the same
  network: copy files to it, run commands with their output on the other
  machine, grab its screen, read its debug output and a SnoopDOS-style
  trace. wasabi by Tobias Karlsson, MIT licence (Files/wasabi).
  Install puts wasabid in C: when you tick it, starts it, and starts it at
  every boot. It makes a key (ENVARC:wasabi.key; one you set before stays)
  and shows it at the end. On the other machine, with Python 3:
    WASABI_KEY=yourkey ./wasabi discover
    WASABI_KEY=yourkey ./wasabi run "Dir RAM:"
  (the client is Files/wasabi/wasabi in the kit; ~/.config/wasabi/config
  with "key = yourkey" saves typing it). Anyone with the key can run any
  command on this Amiga: keep it to yourself. wasabid answers only private
  network addresses. Uninstall stops and removes it.

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
