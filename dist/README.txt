vtcon - a console window for AmigaOS 3.x (68020+)
================================================

XCON: is a console window like CON:, with a modern terminal inside:

  - xterm dialect (default): what Unix ports expect - colours (16, 256,
    RGB), scroll regions, alternate screen, mouse, bracketed paste,
    sixel images (img2sixel, lsix, gnuplot's sixel terminal), and
    lines that re-wrap when the window is resized.
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
  CONSOLE.DEVICE below). Install first asks where UP-Term's own drawer
  goes (see WHERE IT IS INSTALLED).
  From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files [DEST <drawer>] [CONSOLE|NOCONSOLE] [DEVICE|NODEVICE]
      [SERIAL] [SHELLICON] [SSH] [BEBBOGET] [CURL] [WASABI] [CPU040|CPU060]
  (each optional part only when named).
  Remove everything again: double-click Uninstall (or Execute Uninstall).

WHERE IT IS INSTALLED
  UP-Term's own files (bin with the Unix commands and sh, unifont, emoji,
  Python3, nvim, VERSIONS) live in one drawer: SYS:UP-Term unless you
  pick another in the first question of Install (DEST <drawer> from a
  Shell; the drawer's parent must exist). Everything finds it through the
  assign UP-Term:, which Install makes at once and at every boot (a block
  in S:User-Startup between ;BEGIN UP-Term assign and ;END UP-Term assign).
  To move the drawer later, copy it and change that one line. Uninstall
  looks the drawer up through UP-Term: and removes the drawer, the block
  and the assign.

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
   holds named profiles; the Prefs app (SYS:Prefs/UP-Term-Prefs, its
   tool is "C:UP-Term Prefs") writes it. Install lays down a fully
   commented sample; without the file every window behaves as before.

   Prefs: three pages, General, Colors and Advanced (the screen, the
   Backspace key, what programs may do). Type the profile name in the
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
     reflow = on               on | off (a resize re-wraps the lines and the scrollback;
                               off cuts or pads the rows, as xterm does)
     program-clipboard = write write | read-write | off: programs set the
                               clipboard (OSC 52: tmux, neovim over ssh);
                               read-write also lets them read it, which a
                               remote host can then do too -- off by default
     link-open = OpenURL %s    the command a Ctrl + click on a link runs
                               (%s the URL, quoted)
     backspace = del           del | bs: the Backspace key sends ^? or ^H
  A program's notification (OSC 9, OSC 777: a build finished...)
  shows in the title bar for 5 seconds.
     scrollbar = show          show | hide (the scroll bar in a sizable window's border)
     palette = 1,0x00CD00,4,0x5C5CFF
                               remap ANSI colours: index,RRGGBB pairs

   A save (Prefs, or Save settings to profile) changes only the keys
   Prefs shows; any other key and every comment line stays as you wrote
   it. Blank lines are not kept.

   Example: a dim, silent editor window
     NewShell "XCON:0/20/640/300/vim/PROFILE vim/CLOSE"
   with  [profile vim]  bell = none  in the file.

   Themes: the kit installs 112 colour themes in ENVARC:up-term/themes.
   In UP-Term Prefs, Colors page, press Theme... and pick one: its colours
   go into the profile you are editing; Save (or Use) applies them to the
   windows opened after. In a window, Settings > Theme... puts one on
   that window; /theme with no name lists them under the line: Up and
   Down move through them, and the one the bar stops on (a quarter of a
   second) shows on the window; Enter keeps it, Escape puts the window's
   own colours back. Every theme requester opens in the drawer of the
   theme you chose last, else ENVARC:up-term/themes, else its copy in
   ENV:, else a themes drawer beside the program.

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
   What neither font has comes from GNU Unifont, when Install put it in
   (the "Unifont" part: UP-Term:unifont, 1.8 MB, SIL Open Font License
   1.1, see OFL-1.1.txt and SOURCE.txt there): every character up to
   U+FFFF and the emoji (U+1F000-U+1FAFF, in one colour, two cells wide as
   in any terminal), drawn 1:1 with a 16-pixel font (TopazPro 16, IBM 16)
   and at half height, each pair of rows merged, with an 8-pixel one
   (topaz 8). A character with a Latin-1 stand-in keeps it ('>' for the
   prompt arrow U+276F, '-' for a dash). Other font sizes keep the '?' (a
   box for an emoji). A window reads a page of 256 characters the first
   time it needs one and keeps the last 8 (at most 66 KB).
   Colour emoji: on an RTG screen of 15 bits or more (16, 24 or 32 bit
   modes of Picasso96 or CyberGraphX, which offer cybergraphics.library)
   the emoji are drawn in colour over their two cells, from Twemoji, when
   Install put them in (the "Colour emoji" part: UP-Term:emoji, 0.5 MB,
   CC-BY 4.0 by Twitter and contributors, see CC-BY-4.0.txt and SOURCE.txt
   there). 16x16 pixels with a 16-pixel font, 16x8 with an 8-pixel one,
   centred in the two cells for other sizes, over the cell's background
   (also when it is reversed or selected). Sequences (skin tones, flags,
   joined emoji) show their first emoji. Planar screens (OCS, ECS, AGA) and
   8-bit RTG modes keep the one-colour Unifont glyph. A window keeps the
   last 4 pages it read (at most about 0.4 MB).

COMMANDS (/cursor bar)
   Every setting of the menus and of Prefs is also a command you type at
   the prompt: a line that starts with "/" and one of these names is
   UP-Term's, it answers in the window and the shell shows a new prompt.
   /help lists them; Tab completes the names and their values.
     /cursor bar        /bell visual      /scrollback 5000   /font topaz 11
     /fg C0C0C0         /theme dracula-default /profile vim       /tab new
     /completion kingcon                  /font-fallback SymbolsNerdFontMono
     /backspace bs      /program-clipboard read-write
     /link-open Run >NIL: OpenURL %s
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
              bright, Meta key, Copy on select, Wheel scrolls, Reflow on
              resize, Scroll bar, Backspace key sends, Programs may (the
              clipboard), Tab completion, KingCON style, Profile, Save
              settings to profile
    Help      Demo tour (a tour of what the terminal does, in a new tab:
              see DEMO; /demo does the same)
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
  Right Amiga Shift + Up / Down
                        to the previous / next shell prompt (vsh marks
                        its prompts; so do fish, and bash or zsh with
                        OSC 133 in their prompt)
  Ctrl + click          open the link under the pointer (ls --hyperlink,
                        gcc, delta print them): the profile's
                        link-open = OpenURL %s  runs (%s the URL), so the
                        OpenURL package must be installed, or name your
                        browser's command there
  Right Amiga T         new tab (a shell of its own, the same profile,
                        in the directory the shell of this tab is in)
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
  Workbench: double-click SYS:System/UP-Term. It opens an XCON: window
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
  /UP-Term/bin:/gg/bin:/c by default), then the Shell's path (Path).
  vsh's  type  and  which  builtins shadow the AmigaDOS C:Type command (as in
  bash); C:Type stays reachable as  C:Type  or by its full path. The  umask
  builtin sets the local variable UMASK, which started programs inherit.
  Startup: ENVARC:vsh/vshrc, then $HOME/.vshrc (HOME defaults to SYS:).
  Differences from bash
  printf %b and echo -e drop a NUL byte (\0, \x00): strings are C strings
  (probe builtins/printf_bnul).
  Stack: vsh needs none set (it takes 64 KB itself). The commands it runs
  get the stack the builtin  stack [bytes]  sets (at least 16000), or more
  when their file asks for it with a $STACK: cookie (as on AmigaOS 3.2).
  Prompt: PS1 takes bash (\w \u \h) and zsh (%~ %n %m %? %F{red}...%f)
  escapes; default %F{cyan}%~%f %#.
  Unix commands: GNU coreutils 5.2.1 (ls cp mv rm mkdir cat sort head tail
  wc tr cut uniq seq du stat dd tee date and the rest, 86 in all) in
  UP-Term:bin, first in vsh's $PATH. They are not in C: (AmigaDOS
  names ignore case: GNU sort would be C:Sort), so the AmigaDOS Shell keeps
  its own commands. .. is the parent directory, / the list of volumes.
  /tmp is TMP:; without one Install assigns it to T: at every boot.
  coreutils is GPL v2: COPYING and the complete source are in the kit's
  Files/coreutils drawer.
  Differences from bash
    vsh is being brought to bash 5 behaviour. A difference that stays on
    purpose (AmigaOS cannot do it, or it was decided) gets one line here,
    named by its test probe id (tests/bash/divergences.txt). None yet.

PTY: (pseudo-terminals)
  For terminal multiplexers and remote shells: PTY:<id>/m is the master,
  PTY:<id>/s the slave, with the same Unix line discipline XCON: runs
  between them (termios, Ctrl-C, Ctrl-\, Ctrl-Z, window size). A master
  is open once per id; a program looks for a free one by trying ids.

IXEMUL
  Install puts in ixemul.library 80.1 with the UP-Term patches, and the
  ixnet.library of the same build (ixnet refuses an ixemul of another
  version): termios, window size and signal keys go to the terminal
  (XCON:, PTY:), Ctrl-Z and fg/bg job control work, /dev/ptyXY are PTY:
  pairs, local sockets pass descriptors (tmux, screen). It runs the
  existing ixemul programs (80.x keeps every 48.x vector where it was).
  The libraries that were there are kept as LIBS:ixemul.library.orig and
  LIBS:ixnet.library.orig; to go back:
    Copy LIBS:ixemul.library.orig LIBS:ixemul.library
    Copy LIBS:ixnet.library.orig LIBS:ixnet.library
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

VIEWING FILES (hl, mdv)
  hl shows a source file in colour, with line numbers, the language found
  from the name, the #! line or -l:
    hl main.c              hl -l asm intro.i      hl -n S:Startup-Sequence
    hl -p file             (colours, no numbers)  hl --list (the languages)
  C/C++, 68k assembler (vasm, Devpac), AmigaE, Python, shell and vsh
  scripts, AmigaDOS scripts, ARexx, Lua, JavaScript/TypeScript, JSON,
  YAML, TOML, INI and up-term.conf, Makefile, Markdown, HTML, XML, diff,
  CSS, Rust, Go, Java. Into a pipe or a file hl is cat: the bytes as they
  are (--color=always and -n keep the colours and numbers there).
  mdv shows Markdown (a README.md) formatted for the window: headings,
  bold and italic, lists, quotes, tables fitted to the width, code
  blocks in colour, links with their address after them (and as OSC 8
  hyperlinks for terminals that follow them), images as their text:
    mdv README.md          mdv -w 60 notes.md     mdv -U (no addresses)
  Through a pager (vshrc): hlp file, mdp README.md -- less -R, or the
  pager $PAGER names; less is not part of UP-Term (Geek Gadgets, Aminet).
  Colours: the 16 of the window's profile (theme ansi), so a profile
  theme changes them too; --theme mono (bold and underline only),
  --theme rich (24-bit colours, brought down to 256 or 16 where TERM and
  COLORTERM say the terminal has fewer), or a theme file (also the
  variable HL_THEME), one line a class:
    comment = grey italic
    keyword = bright-blue bold
    string  = #98c379
    h1      = magenta bold underline
  Words: bold dim italic underline reverse strike, a colour (black red
  green yellow blue magenta cyan white, bright-<name>, grey, 0-255,
  #rrggbb), "on <colour>" for the background. The classes: plain comment
  keyword type builtin string escape number preproc function label
  variable key section tag attr heading emphasis link code meta added
  removed lineno, and for mdv h1-h6 quote bullet rule codespan codeblock
  url image table th task. Text in Latin-1 (the Amiga's own) is shown
  right in a UTF-8 window; --latin1 / --utf8 say what the terminal reads
  when TERM does not (a ROM CON: window: Latin-1, lines and boxes drawn
  with + - |). hl --help and mdv --help list the options; Ctrl-C stops hl.

SERIAL LOGIN
  A Unix-style login on the serial port: connect a null-modem cable (or a
  USB serial adapter) to another computer, open a terminal program there
  (screen /dev/ttyUSB0 19200, minicom, PuTTY), and on the Amiga run
    upgetty LOOP
  The far end then has vsh with a real tty: line editing, the window size
  (stty rows/cols), vim, less, tmux. Ctrl-C, Ctrl-Z and Ctrl-\ work as on a
  Unix tty: no signal crosses the cable, only the bytes 0x03, 0x1A and 0x1C;
  the Amiga end turns them into signals for the running program.
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
  drawer UP-Term: (see WHERE IT IS INSTALLED), assigned as GG: by a block in S:User-Startup between
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
  Install puts network tools in UP-Term:bin when you say yes (the
  Installer asks); they need a TCP/IP stack (Roadshow, AmiTCP):
    ssh, scp        BebboSSH 1.45 (bebbossh, bebboscp), with bebbosshkeygen
                    and the server bebbosshd; libcryptossh.library in LIBS:
    bebboget        https downloads (installcerts adds root certificates)
    curl            curl 8.22.0; it also needs AmiSSL 5 (Aminet
                    util/libs/AmiSSL-v5-OS3.lha)
  In vsh, ssh (bebbossh), telnet and rlogin run with TERM=xterm-256color,
  which is what an UP-Term window is to a Unix machine (no remote host has
  a vtcon entry). UP_REMOTE_TERM=name in $HOME/.vshrc sends another name;
  a TERM that is not vtcon (inside screen) goes as it is. Keys: bebbosshkeygen makes one
  (ENVARC:.ssh/id_ed25519); bebbossh -i names another. The first connection
  to a host asks whether to trust its key.
  bebbosshd, the server, answers a login with its own simple shell, not a
  terminal: full-screen programs do not run through it.
  Licences: BebboSSH and bebboget by Stefan Franke, GPL v3 or later; curl
  under the curl licence. Their COPYING files and sources (or where the
  source is) are in the kit's Files/net drawer.

CLAUDE FROM THE AMIGA
  Quickest: Claude. Install asked where Claude Code runs (the NAS) and
  wrote it to ENVARC:Claude/remote ("host port"; edit or delete it): with
  no API key set, Claude connects there in the window. Or ClaudeCode
  (or double-click ClaudeCode in the UP-Term drawer)
  connects to the NAS at 192.168.0.198 port 2323 -- ClaudeCode HOST PORT
  for another -- asks its password and starts Claude Code in tmux.
  Claude Code runs on your Mac; the Amiga is its terminal over the LAN.
  On the Mac, from an UP-Term source checkout (nothing is installed; it
  runs until Ctrl-C):
    (umask 077; mkdir -p ~/.config/uptelnetd; read -rs p; printf '%s\n' "$p" > ~/.config/uptelnetd/password)
    python3 tools/uptelnetd.py
  It prints the address it listens on, e.g. 192.168.0.58 port 2323. On the
  Amiga (TCP/IP stack running), in an UP-Term window:
    uptelnet 192.168.0.58 2323
  (vsh: telnet 192.168.0.58 2323). Type the password; you get your Mac
  shell, where claude runs. uptelnetd --command 'tmux new -A -s claude
  claude' goes straight into Claude Code in a tmux session that survives a
  dropped line. uptelnet tells the Mac TERM=xterm-256color and the window's
  size, and follows a resize. Ctrl-] ends it.
  UNENCRYPTED: telnet carries everything in clear, the password too. The
  Mac end listens on its LAN address only and lets in only its own subnet,
  locks out an address after 5 wrong passwords, and never on 0.0.0.0;
  still, use it only on a network you trust and never forward the port.
  The encrypted alternative is ssh (BebboSSH, NETWORK above) to the Mac's
  Remote Login (System Settings > General > Sharing); not yet tried
  against macOS's sshd.

DEMO
  UPDemo, typed in an UP-Term window (76 x 20 characters or more), shows
  what the terminal draws: text styles, double-size lines, 256 and 24-bit
  colours, box drawing, scroll regions, and text-mode demo effects --
  copper bars, plasma, fire, a rotozoomer, vector cubes, a sine scroller,
  palette cycling. Space: the next scene, B: back, Q: quit; UPDemo 5
  starts at scene 5. Your shell comes back as it was.
  Help > Demo tour (or /demo, or UPDemo TOUR typed in a window) plays
  a tour of what UP-Term does, about three minutes, a caption on each
  scene: text styles, 256 and 24-bit colours, line graphics, Latin-1 and
  UTF-8, wide characters and accents, scroll regions, a tmux split, vsh,
  Tab completion (Unix and KingCON), themes switched live, mouse reports
  (click in the window), links, a sixel image, synchronized output, a
  program resizing the window (from 80 columns), reflow (drag the size
  gadget when it asks), then some of the effects. From the menu it runs
  in a tab of its own, which closes when the tour ends; any key ends it.
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

CLAUDE
  C:Claude talks to Claude, Anthropic's AI model, from an UP-Term window,
  and can read, search and edit files and run commands for you. It needs
  a TCP/IP stack (Roadshow, Miami, AmiTCP), AmiSSL 5 (Aminet
  util/libs/AmiSSL-v5-OS3.lha), a 68020 or better, and an API key of your
  own from console.anthropic.com. What it uses is billed to that key:
  /cost shows the tokens and the price so far.
    Claude                    a conversation (/help lists the commands)
    Claude list the files in S:   one request, then back to the Shell
    Claude PING               checks the connection: HTTP status, times
    Claude MODEL=name EFFORT=low|medium|high|xhigh|max ROOT=dir DEBUG
    Claude PLAIN              the line mode at the window's own prompt
  The key: SetEnv SAVE ANTHROPIC_API_KEY yourkey, or put it alone in the
  file ENVARC:Claude/key (Install makes the drawer, never a key; Uninstall
  leaves the drawer and your key). Claude never shows the key and never
  writes it to a log; DEBUG logs to T:Claude.log with the key blanked. The
  key only goes over https.
  The screen is Claude Code's: what was said scrolls above, an input box
  and a status line stay at the bottom (model, effort, the start
  directory, how much of the context is left, the permission mode).
    Enter               sends; a new line: Shift+Enter, \ Enter, or Ctrl+J
    Up / Down           earlier prompts, kept across sessions per start
                        directory (ENVARC:Claude/history); Ctrl+R searches
                        them all (again: older; Tab edits, Enter sends)
    Ctrl+A/E/K/U/W/Y    start, end, cut to the end / start / white space,
                        put back; Ctrl+_ undoes; a paste stays one block
    /                   the commands, as a menu: Up/Down, Tab completes
    Shift+Tab           the permission mode: default, accept edits (writes
                        and edits in the start directory run unasked), plan
                        (only reading tools run; Claude presents a plan)
    Esc                 stops Claude (the unfinished answer is not kept)
    Esc Esc             clears the box (Up brings it back); on an empty box
                        the rewind menu: back to before an earlier prompt
    Enter while Claude works   queues the prompt; it goes in after the tool
                        calls, or when Claude is done; Up takes it back
    ! command           runs it in the shell; Claude sees the output
    # note              saves the note to a memory file (CLAUDE.md, yours
                        or the project's: a menu asks which)
    @path               attaches the file (Tab completes the path)
    Ctrl+O              the whole transcript: results in full, thinking;
                        Up/Down/PgUp/PgDn, / or Ctrl+R searches, q leaves
    Ctrl+T              Claude's todo list under the box, on or off
    Ctrl+G              the prompt in your editor (ENV:EDITOR, else Ed)
    Ctrl+L              draws the screen again
    Ctrl+C              clears the line; twice on an empty line: leave
                        (Ctrl+D twice too)
  /theme picks the colours (dark, light, colour-blind friendly, monochrome
  for 2- and 4-colour screens); /vim turns on vim keys in the box. When a
  long answer ends or Claude asks for permission, the window rings and its
  title says so.
  /compact summarises the conversation and goes on from the summary;
  /context shows how full the context is; /init writes AMIGA.md, notes on
  the start directory that every later session reads; /resume loads the
  conversation saved after each answer (ENVARC:Claude/session.json).
  Tools: every call is shown. Reading, listing and searching ask once
  (answer 2 allows all three for the session); writing, editing and
  running a command ask every time unless you answer 2 for that tool; an
  edit shows its change in red and green before you answer. 3 (or Esc)
  says no and lets you tell Claude what to do instead. Anything outside
  the start directory (the current one, or ROOT=) asks every time.
  Commands run through vsh with no input, up to 60 seconds. The text is
  UTF-8: the screen needs an UP-Term window in the xterm dialect (the
  default); elsewhere Claude falls back to the line mode, where Ctrl+C
  stops an answer and the questions are answered y, a or n.
  Model claude-opus-5-5 and effort medium unless you choose others.

STATUS
  Test build. CON:/RAW: and console.device stay the system's unless you
  switch them (above); XCON: runs beside them.
