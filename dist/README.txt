vtcon - a console window for AmigaOS 3.x (68020+)
================================================

XCON: is a console window like CON:, with a modern terminal inside:

  - xterm dialect (default): what Unix ports expect - colours (16, 256,
    RGB), scroll regions, alternate screen, mouse, bracketed paste,
    sixel images (img2sixel, lsix, gnuplot's sixel terminal), kitty
    graphics pictures (RGB, RGBA, zlib-packed, and PNG through the
    system's PNG datatype; not the layer below the text, placeholders
    or animation), and lines that re-wrap when the window is resized.
    ixemul programs run with TERM=vtcon (less, nano, BitchX ...).
  - Amiga dialect (option AMIGA): the ROM console.device sequences.
    Amiga programs also work in the xterm dialect: the 8-bit CSI ($9B)
    they send keeps its Amiga meaning there.
  - PC-ANSI dialect (option PCANSI): ANSI.SYS / BBS art, CP437.

WHAT YOU NEED
  An Amiga (or FPGA/emulated Amiga) with a 68020 or better and AmigaOS 3.0
  or newer; Install refuses a 68000. 3.2 is what it is tested on most.
  Installer (C:Installer or SYS:Utilities/Installer; AmigaOS 3.2 has it) to
  use the Install icon; without it, the Shell route below works.
  Only for the network parts, all optional:
    a TCP/IP stack running: Roadshow (part of AmigaOS 3.2), Miami or AmiTCP
    AmiSSL 5: Aminet util/libs/AmiSSL-v5-OS3.lha; run its own installer
      (Install ticks curl only when LIBS:amisslmaster.library is found)
  Get up-term.lha onto the Amiga any way you like (a memory card, a shared
  drive, ftp over the stack) and unpack it with lha. Reboot after Install
  (its last page offers to): XCON: and PTY: are mounted from then on.

INSTALL
  Unpack the archive and double-click Install in the UP-Term drawer
  (needs C:Installer or SYS:Utilities/Installer, AmigaOS 3.0 and up).
  It copies the handlers, DOSDrivers entries, vsh, ixkill, the terminal
  entries to ENVARC:up-term, sets TERMINFO, puts in the patched
  ixemul.library and mounts XCON: and PTY:. A page of check boxes picks
  the optional parts: ssh and scp, bebboget, curl (see NETWORK), wasabi
  (see WASABI), the serial login (SERIAL LOGIN), the UP-Term icon in SYS:System and the
  Shell icon (it then starts vsh in an UP-Term window; scripts and NewShell
  keep the AmigaDOS Shell). On 3.0/3.1 a second page asks whether UP-Term should also
  serve CON: and RAW: (see CON: AND RAW: below) and console.device (see
  CONSOLE.DEVICE below). Install first asks where UP-Term's own drawer
  goes (see WHERE IT IS INSTALLED).
  From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files [DEST <drawer>] [CONSOLE|NOCONSOLE] [DEVICE|NODEVICE]
      [SERIAL] [SHELLICON] [SSH] [BEBBOGET] [CURL] [WASABI] [CPU040|CPU060]
  (each optional part only when named).
  Remove everything again: double-click Uninstall (or Execute Uninstall).
  When it has finished, Install looks in the drawer that holds the UP-Term
  drawer for the add-on archives (see ADD-ONS): a drawer with its own
  Install in it, or an .lha file (unpacked there with C:lha first). It
  lists what it found, all ticked, and runs the ticked ones' own Install
  one after the other, each with its own pages and record.

UPDATE
  Install a newer kit over one installed with Install: its first page
  names the installed version and the kit's and offers Update (the
  default) or Full install. Update asks nothing: it compares the file
  list the last install wrote (UP-Term:MANIFEST: every file of that kit,
  its size and checksum) with this kit's, copies only the files that
  changed, deletes the ones this kit no longer has, and runs again only
  the parts of the install whose files or script changed. The drawer,
  the assigns, the blocks in S:User-Startup and every choice of the last
  install (kept in ENVARC:up-term/opts) stay as they are; the bar counts
  only what Update copies. An install from a Shell keeps no choices and
  a kit older than Update no list: Install then installs in full.

PACKAGES (upkg)
  UP-Term:bin/upkg is opkg, the package manager, set up by
  UP-Term:etc/upkg.conf (the feed is the latest release on GitHub; its lists
  and records live in UP-Term:pkg/upkg). From an UP-Term window:
    upkg update              fetch the feed's package list
    upkg list                the packages the feed offers
    upkg install <package>   install an add-on (needs the base installed)
    upkg upgrade             bring the installed add-ons up to the feed
    upkg remove <package>    remove an add-on
    upkg browse              pick add-ons from a full-screen list
  upkg browse shows every add-on of the feed with its state (installed,
  upgrade, available). Up and Down move, Enter shows the add-on's info,
  i installs, r removes (after a Yes/No question), u upgrades, q or Escape
  quits. After each action the list comes back on the same add-on. Run
  upkg update first so the list is current.
  Uninstall removes UP-Term:pkg/upkg and UP-Term:etc/upkg.conf with the drawer.

WHERE IT IS INSTALLED
  UP-Term's own files (bin with the Unix commands and sh, unifont, emoji,
  VERSIONS, and the add-on archives' files: Python3, nvim, pkg) live in one drawer: UP-Term on the local disk with the most free
  room (SYS: when none qualifies; an earlier install's drawer wins), unless you
  pick another in the first question of Install (DEST <drawer> from a
  Shell; the drawer's parent must exist). Everything finds it through the
  assign UP-Term:, which Install makes at once and at every boot (a block
  in S:User-Startup between ;BEGIN UP-Term assign and ;END UP-Term assign).
  XCON:, PTY: and the console device start before that block runs, so the
  drawer is also written in ENVARC:up-term/Dir: whatever of UP-Term reads
  UP-Term: first makes the assign from it when it is missing, and nothing
  of UP-Term ever asks you to "insert volume UP-Term:" (a missing assign
  is "not installed" to it). To move the drawer later, copy it and change
  that one line and ENVARC:up-term/Dir (or run Install again with DEST).
  Uninstall looks the drawer up through UP-Term: and removes the drawer,
  the block and the assign. It asks whether to delete your data in
  UP-Term:var too (the sessions and history of Claude and amiga-pi;
  default No): on No the drawer stays with only var in it, and Uninstall
  says so.

USE
  NewShell "XCON:0/20/640/300/My Shell/CLOSE"

  The spec is left/top/width/height/title. A width of 0 is the screen's
  width and a height of 0 its height, counted from the left or top edge
  (the UP-Term icon uses XCON:0/20/0/400/UP-Term/CLOSE); an empty field
  keeps the default.

  Options after the title, one per /: CLOSE WAIT BACKDROP NODRAG NOBORDER
  NOSIZE NODEPTH INACTIVE  SCREEN name  FONT name size
  DARK (light grey on black: the default except with AMIGA)  LIGHT (the screen's
  own text and background pens)  FG rrggbb  BG rrggbb (default colours, hex)
  XTERM AMIGA PCANSI (dialect)  LATIN1 or CP437 (xterm byte set; default
  UTF-8)

  Encoding: in a UTF-8 window vsh sets LC_CTYPE=C.UTF-8, so the programs
  it runs (upkg-ui, nano, less) write UTF-8. LATIN1 and CP437 windows do
  not set it, and those programs keep to ISO 8859-1.

  Terminal type: vsh sets TERM=vtcon and TERMCAP for the programs it runs
  when it is in an XCON: window. From the AmigaDOS Shell in XCON:, or in
  tcsh, set them for that shell:
                         Set TERM vtcon
                         Set TERMCAP /ENV/up-term/termcap.vtcon
   (Set, not SetEnv: SetEnv is global, and a ROM CON: window cannot show
   vtcon's sequences. Install and Uninstall delete a global TERM that says vtcon.)
   An XCON: window in the AMIGA personality gets TERM=vtcon-amiga instead:
   the same entry without bce, since console.device fills what a clear or
   scroll leaves with the window's one background (the entry is in
   TERMINFO beside vtcon). A /term in the window beats both.
   TERMINFO (set by Install) is /ENV/up-term/terminfo.

CONFIGURATION (profiles)
   XCON: windows are user-configurable like iTerm2's: /ENV/up-term/up-term
   holds named profiles; the Prefs app (SYS:Prefs/UP-Term-Prefs, its
   tool is "C:UP-Term Prefs") writes it. Install lays down a fully
   commented sample; without the file every window behaves as before.

   Prefs: five pages, General (profile, font, fallback font, scrollback,
   cursor, bell), Colors (text, background, selection, Theme...), Palette
   (the 16 colours by name), Keyboard (Meta key, AmigaShell keys, Backspace,
   Tab completion and KingCON's style, Copy on select, the wheel) and
   Advanced (the screen, its mode and depth, Programs may, Highlight files
   shown with cat, the link command). Each value has a control that fits
   it: Font, Fallback font and Screen mode open the system requesters, a
   colour swatch opens a colour window (colour wheel, brightness, Red,
   Green, Blue and the hex digits RRGGBB, all following each other), KingCON
   style is a cycle and two boxes. The Profile gadget lists the profiles of
   the file; New..., Rename... and Delete (asks first) change the list.
   Hold the pointer over a control for its help line; the underlined letter
   of a label is its key. Save writes ENVARC:up-term/up-term and ENV:, keeps
   the file that was there as up-term.orig, applies the settings and ends
   Prefs; Use does the same for ENV: only (until the next reboot); Cancel
   ends Prefs with nothing written or applied. The close gadget asks
   "Discard changes?" when there are unsaved edits. Windows already open
   take the new settings at once: colours, palette, font, scrollback, scroll
   bar, cursor, bell, keys and completion. These need a new window: the
   screen, its mode and depth (Screen, Screen mode), and TERM / colors /
   Highlight files shown with cat for programs already started. Preferences
   apply to XCON:, PTY: and RAW: windows (the console.device ones, CON:
   under console.device, are not touched).

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
     scrollback = 2000         lines to keep (no key = the built-in 500; 0 or less = none)
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
     amiga-keys = on           on | off: Shift+Left and Shift+Right send Home and
                               End (the line's start and end, as in the
                               AmigaShell; CSI H / F, SS3 H / F in a program's
                               application cursor mode) instead of xterm's
                               CSI 1;2D / 1;2C. Off for nvim or tmux bindings
                               that use the xterm codes. The console line
                               editor and C:Claude take both either way.
     dither = on               on | off: on a screen of 16 colours or fewer, a colour no
                               pen shows closely is drawn with two pens in a fixed
                               pattern; off draws the nearest single pen
     outline-antialias = on    on | off: outline-font glyphs are drawn smoothed (off: hard edges)
     padding-color = background  background | extend: the strips beside the character
                               grid (the part of the window that is not a whole cell)
                               are the background, or take the colour of the cell next
                               to them
     highlight-cat = on        on | off: vsh's cat shows files through hl -p
                               (see VIEWING FILES). The save also writes it
                               to ENV:up-term/highlight-cat (and ENVARC:'s),
                               which the vshrc reads; it holds for every
                               profile.
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
   your bitmap font. UP-Term reads TrueType fonts with its own engine; it
   needs no font library. Install puts the icons-only Nerd Font in FONTS:
   (the "fonts" part, always installed: FONTS:SymbolsNerdFontMono.ttf,
   Symbols Nerd Font 3.5.1, github.com/ryanoasis/nerd-fonts, licences in
   UP-Term:fonts/SymbolsNerdFont: MIT.txt, ICON-SETS.md, SOURCE.txt). Any other TrueType font works the same:
   copy the .ttf to FONTS:, or install it with a tool that writes a .otag
   for a TrueType file (ttfinstall). Characters above U+FFFF draw too (the
   Nerd Fonts 3 icons at U+F0001 and up, the Material Design set).
   Put  font-fallback = SymbolsNerdFontMono  (the name of the .ttf or .otag,
   without the extension) in a profile, or type it in UP-Term Prefs,
   General page, Fallback font.
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
     /amiga-keys off    /dither off
     /outline-antialias off
     /padding-color extend
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
              resize, Scroll bar, Backspace key sends, AmigaShell keys, Programs may (the
              clipboard), Tab completion, KingCON style, Profile, Save
              settings to profile
    Help      Demo tour (a tour of what the terminal does, in a new tab:
              see DEMO; /demo does the same), Slash commands (the list of
              commands, as /help prints it), Shell tutorial (see NEW TO A
              UNIX SHELL; /tutorial does the same)
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
  Programmable completion: complete (-W -F -C -a -b -c -d -f -v, -A function,
  -o default dirnames filenames nosort nospace plusdirs ...), compgen and
  compopt as in bash; Tab in an XCON: window asks vsh for the words.
  COMP_WORDS splits at blanks, " and = only (bash also at :, which would cut
  an Amiga path in two); -G -X -P -S -D -E -I are not there.
  Keys: bind and an .inputrc ($INPUTRC, else $HOME/.inputrc) set the
  window's line editor: set editing-mode vi|emacs, and "\C-x" / "\M-x" /
  Control-x / Meta-x bound to one of the 19 functions bind -l lists (no
  macros, no key sequences longer than one key; $if blocks are skipped).
  Differences from bash
  printf %b and echo -e drop a NUL byte (\0, \x00): strings are C strings
  (probe builtins/printf_bnul).
  Associative arrays list their keys sorted, bash in hash order: scripts must
  not depend on the order (probes sort).
  CDPATH is a list of directories in Unix form (/RAM/work:/Work/src), as PATH
  is: a volume name (RAM:work) holds a colon, which splits the entry, so it
  cannot be listed. cd and pushd themselves take either form.
  Process substitution goes through a file in T: (T:vsh-ps-...; RAM: without
  T:), which native commands and ixemul programs both open: <(cmd) runs cmd
  to its end before the command starts, so the two do not run at the same
  time and a cmd that never ends hangs the line; >(cmd) runs after the
  command that writes to it, so its output comes after the writer's.
  Stack: vsh needs none set (it takes 256 KB itself). The commands it runs
  get the stack the builtin  stack [bytes]  sets (at least 16000), or more
  when their file asks for it with a $STACK: cookie (as on AmigaOS 3.2).
  Prompt: PS1 takes bash (\w \u \h) and zsh (%~ %n %m %? %F{red}...%f)
  escapes; default %F{cyan}%/%f %#, the full path (SYS:Work %);
  %~ shows HOME as ~.
  Unix commands: GNU coreutils 5.2.1 (ls cp mv rm mkdir cat sort head tail
  wc tr cut uniq seq du stat dd tee date and the rest, 86 in all) in
  UP-Term:bin, first in vsh's $PATH. They are not in C: (AmigaDOS
  names ignore case: GNU sort would be C:Sort), so the AmigaDOS Shell keeps
  its own commands; it gets only upkg, through an Alias in S:Shell-Startup
  (Install writes it, Uninstall removes it), so its C: commands stay unshadowed.
  .. is the parent directory, / the list of volumes.
  /tmp is TMP:; without one Install assigns it to T: at every boot.
  coreutils is GPL v2: COPYING and the complete source are in the kit's
  Files/coreutils drawer.
  Beside them, the Unix tool ports: grep, sed, awk (the One True Awk),
  less, nano, find, xargs, diff, cmp, diff3, sdiff, patch, man (mandoc),
  gzip, gunzip, zcat, bzip2, xz, tar, cpio, bsdunzip, bsdcat, zip and
  ncurses' tput, tset, reset, clear, tabs, infocmp, tic, toe. Their manual
  pages, and vsh's, UPTerm's, upgetty's, sz's and rz's, are in
  UP-Term:share/man:  man grep  pages it through less;  man -T utf8 ...
  writes UTF-8. nano colours code with the syntax files in
  UP-Term:share/nano (UP-Term:etc/nanorc includes them). Each package's
  licence is in UP-Term:licenses, with SOURCES.txt (version, upstream
  archive, sha256); the complete source with UP-Term's patches and build
  recipes is up-term-src.lha, published next to up-term.lha.
  Differences from bash
    vsh is being brought to bash 5 behaviour. A difference that stays on
    purpose (AmigaOS cannot do it, or it was decided) gets one line here,
    named by its test probe id (tests/bash/divergences.txt).
    Brace expansion stops at 1,000,000 words: {1..100000000} fails with
    "brace expansion: out of memory", status 1 (probe expand/brace_oom).
    A stream closed with  n>&-  or  n<&-  is the null device (NIL:) for a
    command that is not a builtin: its write or read does not fail as in
    bash (probe redir/close_out).
    ~user is left as typed: there is no user database to look the home up in
    (probe expand/tilde_user); ~  ~+  ~-  are expanded.
    $PPID, $UID and $EUID are 0: AmigaOS has no parent process id and no
    user ids (probe vars/ids).
    Function calls nest as deep as the stack lets them: vsh runs on 256 KB
    (more when the Shell's Stack is larger), a subshell, pipeline stage or
    $( ) on 128 KB. A level of a small function takes about 0.7 to 1.1 KB:
    about 230 levels in the shell, 110 in a subshell. Deeper, the call
    stops with "nested too deeply (N function levels)", status 2, and no
    crash (probe grammar/deep goes 200 levels).
    $PWD is an AmigaDOS name (Work:dir); its colon would split PATH, so a
    directory goes into PATH as /Work/dir (probe invocation/source_path).
    Descriptors 3 and up (exec 3>file, {fd}<file, n>&m, read -u N, mapfile -u N)
    live in the shell: builtins and  >&N  targets use them, but a native
    command gets only 0, 1 and 2 (probe fd/ext_fd3). /dev/fd/N, /dev/stdin,
    /dev/stdout and /dev/stderr are redirection targets, source files and
    test -e/-a/-r/-w operands; a native command that is given such a name as
    an argument cannot open it (probe fd/ext_devfd).
    coproc: a coproc whose body is shell code answers line by line as in
    bash; an external program as the coproc (coproc cat) gets its input only
    when PIPE: has filled the read it asked for or NAME[1] is closed, so a
    line-by-line exchange with it waits (probe builtins/coproc).
    Traps: ERR (set -E), DEBUG (set -T) and RETURN work as in bash; the
    DEBUG text of a command with a here-document leaves the document out.
    A signal from another process reaches vsh's traps only as a break bit:
    INT is Ctrl-C (break C), QUIT is break E (kill -s QUIT sends it, as
    does ^\). TERM and HUP sent by vsh's kill arrive as INT (vsh is no
    ixemul program, so kill falls back to Ctrl-C); USR1, USR2, TSTP and the
    other signals cannot be sent to vsh, break D and F stand for no signal,
    and vsh sees no window size change (WINCH) and no closed console (HUP:
    the input ends). kill -s SIG $$ inside the shell runs any trap
    (tools/rig/trapsig_rig.py).
    shopt knows all of bash 5.3's names. extglob, globstar, nullglob,
    failglob, dotglob, nocaseglob, nocasematch, lastpipe, expand_aliases,
    inherit_errexit, xpg_echo and sourcepath act; the others are remembered
    and listed but change nothing here (globasciiranges and checkwinsize
    included). Aliases expand in an interactive shell, in a script only after
    shopt -s expand_aliases, as in bash. extglob is one switch for the whole
    shell, as in bash: a line after shopt -s extglob may use @( ) ?( ) *( )
    +( ) !( ). globstar does not follow links to directories (on the Amiga
    soft links are not told apart, so ** may enter them); the pattern  */**
    lists directories with a slash. nocaseglob is on at the Amiga prompt and
    off elsewhere.
    time prints the real time as bash does; the user and system times are 0
    on the Amiga, as are those of  times (see below).
    ulimit prints "unlimited" for every limit and refuses to set one. help
    lists the builtin names only; enable -a lists vsh's builtins, not bash's.
    The hash table is filled by the commands run in the shell itself; the
    commands of a pipeline are hashed in their own subshells, so the table
    does not change (probe builtins/hash_pipeline). The work directory has
    no logical name on the Amiga: AmigaDOS names it by its lock, so after
    cd V2A: (an assign) or cd link (a soft link) the name is the volume's and
    the target's (Ram Disk:v2/sub), with cd -L and cd -P alike, and pwd -L
    and pwd -P print the same; in bash, -L keeps the link's name (probe
    builtins/cd_physical, tools/rig/vshpath_rig.py). DIRSTACK[n]=dir sets the
    n-th directory of the stack as in bash; element 0 and indexes past the
    stack are dropped, and after unset DIRSTACK it is an ordinary array
    (probe builtins/dirstack_write). coproc [NAME] command works with two
    pipes (NAME[0] reads, NAME[1] writes, NAME_PID; probe builtins/coproc);
    the Amiga half runs on the rig only. The user and system times of time
    and times stay 0: AmigaOS keeps no CPU time per task (reading it would
    need a task-switch hook for every task), so only the real time is shown.
    exec of a command that is not found ends a shell that is not interactive
    with status 127, as in bash (shopt -s execfail and an interactive shell
    keep running; exec_missing).
    Posix mode (set -o posix, --posix, POSIXLY_CORRECT assigned or in the
    environment; leaving it by set +o posix or unset POSIXLY_CORRECT) does
    these, each with a probe in tests/bash/probes/posix: the assignments
    before a special builtin stay (special_prefix); a shell that is not
    interactive ends on the error of a special builtin or of an assignment,
    on a redirection error of a special builtin,
    on  .  of a missing file, on eval of a syntax error, and on  return
    outside a function (special_errors); a function cannot take the name of
    a special builtin (function_name); export -p and readonly -p print
    "export NAME=..." and "readonly NAME=..." (export_readonly_p); type and
    command -V say "special shell builtin" (type_special); a command
    substitution inherits set -e (subst_errexit); POSIXLY_CORRECT follows the
    mode (posixly_correct); an interactive posix shell reads $ENV and no
    posix shell reads BASH_ENV (env_login). Nothing else of posix mode is
    done. A login shell (--login, -l) reads ENV:vsh/profile, then
    $HOME/.vsh_profile, before ENV:vsh/vshrc; logout leaves it. ~/.bashrc and
    ~/.bash_profile are not read.

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
  A machine without an FPU gets the soft-float build of ixemul.library (no FPU
  opcodes); ixnet.library has one build for both.
  The libraries that were there are kept as LIBS:ixemul.library.orig and
  LIBS:ixnet.library.orig; to go back:
    Copy LIBS:ixemul.library.orig LIBS:ixemul.library
    Copy LIBS:ixnet.library.orig LIBS:ixnet.library
  To tell which one is in:  Search LIBS:ixemul.library vtcon-2

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

TASKS (ps, top)
  ps lists the tasks and processes: CLI number, priority, state, type and
  name (a CLI process shows the command it runs). top shows them by CPU
  use, refreshed every 2 seconds (-d SECONDS); q or Ctrl-C ends it, -b
  prints plain lines (-n COUNT refreshes). AmigaOS counts no time per
  task: top samples the running task 50 times a second at the vertical
  blank, so a share is statistical, and idle is the time no task ran.

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
  In vsh, cat is  hl -p  (the vshrc's alias): a file on the console comes
  in colour, a .md or .markdown file formatted as mdv shows it, and into a
  pipe or a file cat is still the bytes. UP-Term Prefs, Advanced page:
  "Highlight files shown with cat" (on by default; highlight-cat in the
  profile) switches it off for the next vsh. AmigaDOS Type is never
  aliased. hl does not take cat's options: for cat -n type
  command cat -n, or switch it off.
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

AMARCHY
  Amarchy is a small commodity that gives the whole machine Super-key
  shortcuts, the way a tiling desktop does. Super is the Left Amiga key. It
  runs from boot (a block in S:User-Startup starts UP-Term:bin/Amarchy), so
  the keys work with no UP-Term window open: Left Amiga + T opens a window,
  Left Amiga + Space shows the quick menu, Left Amiga + K lists every key.
  Inside an UP-Term window the window itself answers first (Left Amiga + T is
  a new tab there); everywhere else Amarchy does. Only the keys listed below
  are taken; every other Left Amiga key reaches the program as before, and
  Left Amiga + M, Left Amiga + N, Left Amiga + the mouse and the two-Amiga
  reset keys are never touched.
  The keys it ships (generated from dist/keymap):
  [keys]
  Left Amiga + Space          Menu
  Left Amiga + Shift + Space  Apps
  Left Amiga + K              Key bindings
  Left Amiga + Alt + Space    Other menu
  Left Amiga + Alt + K        Other key bindings
  Left Amiga + H              Help
  Left Amiga + Shift + H      Tutorial
  Left Amiga + T              New tab
  Left Amiga + W              Close tab
  Left Amiga + Backspace      Close tab
  Left Amiga + Q              Close window
  Left Amiga + 1 to 9         Switch to tab N
  Left Amiga + F1             Switch to tab 1
  Left Amiga + F2             Switch to tab 2
  Left Amiga + F3             Switch to tab 3
  Left Amiga + F4             Switch to tab 4
  Left Amiga + .              Next tab
  Left Amiga + ,              Previous tab
  Left Amiga + Right          Next tab
  Left Amiga + Left           Previous tab
  Left Amiga + C              Copy
  Left Amiga + V              Paste
  Left Amiga + A              Select all
  Left Amiga + F              Find
  Left Amiga + G              Find next
  Left Amiga + +              Bigger font
  Left Amiga + -              Smaller font
  Left Amiga + Shift + T      Theme
  Left Amiga + Shift + B      Background
  Left Amiga + Shift + F      Font
  Left Amiga + Shift + S      Own screen
  Left Amiga + Shift + W      Workbench screen
  Left Amiga + Shift + D      Wide window
  Left Amiga + Shift + R      Standard window
  Left Amiga + Shift + L      Demo tour
  Left Amiga + P              Preferences
  Left Amiga + Return         Tmux
  Left Amiga + L              Claude
  Left Amiga + E              Editor
  Left Amiga + U              Install and update
  Left Amiga + Shift + U      Upgrade all
  Left Amiga + I              Install add-ons
  [end keys]
  The quick menu (Left Amiga + Space) is a short list you filter by typing;
  Left Amiga + K is the key viewer. Two programs draw them. amarchy-gui is
  the drawn menu: its own borderless window with an icon in front of each row.
  amarchy-menu is the text menu in a console window. The setting
    menu-style = graphical
  in ENVARC:up-term/up-term (before the first section; graphical or text,
  default graphical) picks the one Left Amiga + Space and Left Amiga + K
  start. Left Amiga + Alt + Space and Left Amiga + Alt + K start the other
  one. You can also run them by hand:
    amarchy-gui menu [node]      the menu (node: a menu id, default root)
    amarchy-gui viewer           the list of every key
  and the same two words for amarchy-menu. The icons come from a table made
  when the kit is built (the file dist/menu-icons in the source); a row in
  keymap.user cannot name an icon, and a row without an entry in the table
  shows an empty icon column. The groups of the index have glyphs too:
  games, demos and music each have their own, and every other drawer of the
  Apps level (a drawer found by the scan) shows the drawer glyph, a folder.
  Change a key: edit ENVARC:up-term/keymap (the shipped file, written by
  Install; an upgrade never overwrites it, the new one is saved beside it as
  keymap.new) or put your own rows in ENVARC:up-term/keymap.user: a row there
  replaces the row for the same key, and  super+t = none  unbinds one. Then
  tell Amarchy and every window to read the file again:
    rx "address AMARCHY RELOAD"
  ARexx port AMARCHY: STATUS (hits, rows, enabled, active), RELOAD, RUN n
  (do row n of the table), SHOW (the key viewer) and QUIT.
  Switch it off for a while in Exchange (the commodity AMARCHY, Disable), or
  stop it with  rx "address AMARCHY QUIT" . Uninstall takes the startup block
  out. man amarchy has the file format.

  App index: the menu lists the programs found on your disks. The scanner
  amarchy-index looks at the hard-disk volumes (not floppies, RAM: or other
  virtual volumes) and walks their drawers down to depth 6. An icon of type
  Tool with a program beside it makes an app; a project icon whose default
  tool is WHDLoad makes a game, listed under Games. The system
  drawers Devs, Libs, L, Fonts, Locale, Env, T and Trashcan are skipped, and
  so is Prefs/Env-Archive. The result is the cache ENVARC:Amarchy/apps,
  written to a temporary file and renamed, so a scan cut short never leaves
  a broken cache. Amarchy only reads the cache; it never scans itself.
  The scan runs in the background at every boot (a line in S:User-Startup,
  taken out by Uninstall) and on demand with the Rescan apps row. The
  scan runs in the background and the menu stays open: its first row shows
  "Scanning..." with the drawer being read, then "Scan done: N apps, M
  games" (or why it stopped), and the Apps level lists the new rows.
  The Apps level of the menu lists the indexed apps after the menu's own
  rows, a Games level lists the games, and the search at the root finds
  both. When a menu row has the same label as an indexed app, the menu row
  wins and the index row is left out. An indexed row starts like a
  double-click in Workbench (OpenWorkbenchObject); if Workbench cannot
  start it, Amarchy starts it through WBStartup itself, never through a
  shell. To keep drawers out of the scan, list path prefixes, comma
  separated, in ENVARC:up-term/up-term (before the first section):
    amarchy-index-skip = Work:Old,DH1:Backup
  A path matches when it begins with an entry, so Work:Old also skips
  Work:Old/Tools. Press Rescan apps afterwards to rebuild the cache.
  Network volumes (an SMB or NFS share, found by its handler) are left out
  of the scan, since walking one over the network takes hours. To scan a
  share, list its volume name, comma separated, in the same file:
    amarchy-index-volumes = spotUP,Music
  Ctrl-C stops a running scan: it writes no cache and returns WARN.
  The scan keeps its progress in memory (a record named Amarchy.index, no
  disk writes): the stage and the drawer or icon it is on. After a scan
  that failed or hung, run  amarchy-index --where  (or -w): it prints the
  stage, the volume and path last handled, the drawers and icons counted,
  and whether the scanner task still exists, is moving, is stuck (a
  Software Failure requester keeps it alive) or has died. After a normal
  finish it shows done with the counts. A failed cache write shows stage
  "write failed" (and the scan returns ERROR). The record lasts until a
  reboot; one left by an older amarchy-index build is not read or reused,
  --where then says "record from another amarchy-index build, reboot to
  clear".

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
    ssh, scp        BebboSSH 1.46.1 (bebbossh, bebboscp), with bebbosshkeygen
                    and the server bebbosshd; libcryptossh.library in LIBS:
    bebboget        https downloads (installcerts adds root certificates)
    curl            curl 8.22.0; it also needs AmiSSL 5 (Aminet
                    util/libs/AmiSSL-v5-OS3.lha)
  In vsh, ssh (bebbossh), telnet and rlogin run with TERM=xterm-256color,
  which is what an UP-Term window is to a Unix machine (no remote host has
  a vtcon entry). UP_REMOTE_TERM=name in $HOME/.vshrc sends another name;
  a TERM that is not vtcon (inside screen) goes as it is. Keys: bebbosshkeygen makes one
  (ENVARC:ssh/id_ed25519; an older ENVARC:.ssh is still used when ENVARC:ssh
  does not exist); bebbossh -i names another. The first connection
  to a host asks whether to trust its key; a host key that later differs is
  refused (delete its line in ENVARC:ssh/known_hosts if the change is real).
  This is BebboSSH 1.45 with the fixes of Matthias Muench (Aminet bebbossh_1.46.1), built by UP-Term from source.
  bebbosshd, the server, answers a login with its own simple shell, not a
  terminal: full-screen programs do not run through it.
  Licences: BebboSSH and bebboget by Stefan Franke, GPL v3 or later; curl
  under the curl licence. Their COPYING files and sources (or where the
  source is) are in the kit's Files/net drawer.

UPTELNET
  uptelnet is a telnet client for the window (no ixemul; needs a TCP/IP
  stack). up-claude.lha's C:Claude uses it (that archive carries its own copy) to reach Claude Code on the NAS
  or a Mac, with a key instead of the password once the NAS has paired this Amiga (KEY file).
 On the Mac, from an UP-Term source checkout (nothing is installed; it
  runs until Ctrl-C):
    (umask 077; mkdir -p ~/.config/uptelnetd; read -rs p; printf '%s\n' "$p" > ~/.config/uptelnetd/password)
    python3 tools/uptelnetd.py
  It prints the address it listens on, e.g. <Mac address> port 2323. On the
  Amiga (TCP/IP stack running), in an UP-Term window:
    uptelnet <Mac address> 2323
  (vsh: telnet <Mac address> 2323). Type the password; you get your Mac
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

NEW TO A UNIX SHELL
  tutorial teaches the shell by doing: eleven short lessons (where am I,
  looking at files, making and moving, wildcards and quotes, pipes and
  redirection, getting help, keys that save typing, variables and PATH,
  editing a file, packages). Each step gives a task; type the command
  and the tutorial checks the result, shows a hint after a wrong try and
  the answer after the third. Type tutorial at a vsh prompt, or pick
  Help > Shell tutorial (or /tutorial) to run it in a tab of its own,
  which closes when you quit. tutorial N starts lesson N, tutorial next
  continues where you stopped (the finished lessons are listed in
  $HOME/.tutorial), tutorial reset forgets the progress. The lessons
  practise in a drawer of their own (T:tutorial), but a command you type
  runs for real: rm -r on one of your drawers deletes it. Until you have
  finished lesson 1, a new vsh window prints two lines pointing here;
  creating $HOME/.tutorial-hint-off hides it.

DEMO
  UPDemo, typed in an UP-Term window (76 x 20 characters or more), plays
  a tour of what UP-Term does, about three minutes, a caption on each
  scene: text styles, double-size lines, box drawing, 256 and 24-bit
  colours, line graphics, Latin-1 and UTF-8, wide characters and accents,
  scroll regions, a tmux split, vsh, Tab completion (Unix and KingCON),
  themes switched live, mouse reports (click in the window), links, a
  sixel image, synchronized output, a program resizing the window (from
  80 columns), reflow (drag the size gadget when it asks), then the
  text-mode demo effects: copper bars, plasma, fire, a rotozoomer, vector
  cubes, a sine scroller, palette cycling. Space, Return or the Right
  arrow key goes to the next scene, B or the Left arrow key back, Q, Esc
  or Ctrl-C quits (no other key does anything); UPDemo 17 starts at
  scene 17 (the effects). Your shell comes back as it was.
  Help > Demo tour (or /demo) plays the same in a tab of its own, which
  closes when the demo ends.
  On a screen of 16 colours (an AGA screen of 4 planes) UPDemo plays a
  version made for the 16 ANSI colours by itself: the same 24 scenes,
  only SGR 30-37, 90-97, 40-47 and 100-107, gradients from the shade
  blocks and the full block as in classic ANSI art, and colour cycling
  through the 16 colours instead of palette writes. It asks the window
  how many colours it has (XTGETTCAP colors; UP-Term answers with the
  screen's colours). UPDemo ANSI plays that version anywhere, UPDemo
  FULL the 256 and 24-bit colour one (dithered on a 16-colour screen);
  both go with a scene number or BENCH: UPDemo FULL 3, UPDemo BENCH ANSI 17.
  UPDemo SOLID is the 16-colour version in solid colours only: no shade
  blocks anywhere, every gradient step drawn as the nearest of the 16
  colours (half blocks of two solid colours keep the half-cell detail);
  it implies ANSI, also on a 256-colour window. Arguments on the Amiga
  (template SCENE/N,ANSI/S,FULL/S,SOLID/S,BENCH/S, any order): UPDemo ?
  prints the template and waits for them like any AmigaDOS command.
  UPDemo BENCH runs the effect scenes (text, colours, scroll regions and
  the effects) for three seconds each and prints the frames a second
  each reached: a benchmark of the terminal on your machine. UPDemo BENCH
  17 measures scene 17 alone.
  The full-screen effects need a fast processor (a 68060, a PiStorm, an
  emulator at full speed); a stock A1200 draws them at about a frame a
  second.

WASABI (REMOTE DEVELOPMENT, CLAUDE CODE ON ANOTHER COMPUTER)
  For working on this Amiga from a Mac or Linux machine on the same
  network: copy files to it, run commands with their output on the other
  machine, grab its screen, read its debug output and a SnoopDOS-style
  trace. wasabi 0.11b2 by Tobias Karlsson, MIT licence (Files/wasabi).
  It replaces the wasabid 0.2 and the amiagent of earlier kits: Install
  stops and replaces our older wasabid, and removes our amiagent.
  Install puts wasabid in C: when you tick it, starts it, and starts it at
  every boot. It makes a key (ENVARC:wasabi.key; one you set before stays)
  and shows it at the end. On the other machine, with Python 3:
    WASABI_KEY=yourkey ./wasabi discover
    WASABI_KEY=yourkey ./wasabi run "Dir RAM:"
  (the client is Files/wasabi/wasabi in the kit; ~/.config/wasabi/config
  with "key = yourkey" saves typing it). Claude Code on that machine
  drives this Amiga through Files/wasabi/wasabi_mcp.py, which sits beside
  the client:  claude mcp add wasabi -- python3 /path/to/wasabi_mcp.py
  (tools/nas/README.md in the vtcon source tree: the NAS image). Anyone
  with the key can run any command on this Amiga: keep it to yourself.
  wasabid answers only private network addresses; the link is encrypted
  only when the Amiga has AmiSSL 5 and keys (wasabi's README). Uninstall
  stops and removes it.
  up-claude.lha also carries this wasabid (as C:wasabid, only when none is
  there): uptelnet starts a session daemon from it for each login to the
  NAS, with a key of its own, and ends it with the login. A wasabid put
  in by this kit is used as it is; the one up-claude.lha put in is not
  started at boot.

CON: AND RAW:
  UP-Term can serve the system's CON: and RAW: too, so every new Shell
  window is an UP-Term window. Programs see the console they know (the
  Amiga escape sequences, the Shell inside), the window looks like any
  UP-Term window: your default profile's font, colours and scroll bar
  (a spec's DARK, LIGHT, FG, BG, FONT or PROFILE still win). Pen 0 is the
  window's background colour and pen 1 its text colour; the pens 2 to 7 a
  program asks for stay the screen's pens. C:UPConsole switches it:
    UPConsole CON ON     new CON:/RAW: windows are UP-Term
    UPConsole CON OFF    new windows are the system's again
    UPConsole STATUS     who serves CON: and RAW: now
  Windows already open keep whoever opened them. When Install switched it
  on, a block in S:User-Startup between ;BEGIN UP-Term console and
  ;END UP-Term console does it at every boot; Uninstall switches it off and
  takes the block out. It refuses to switch when another console
  replacement (KingCON, ViNCEd, ...) serves CON:. It works on AmigaOS 3.2
  (the Shell's medium mode is supported).

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
  (CD_SETKEYMAP). It works on AmigaOS 3.2 too.

ADD-ONS
  Neovim, Python, Claude and amiga-pi are archives of their own, next to
  up-term.lha. Each has its own Install, Uninstall and README.txt:
    up-nvim.lha     Neovim 0.12 and its runtime (UP-Term:nvim). Needs this
                    one installed first: it runs on its patched
                    ixemul.library; its Install says so and stops when this
                    one is missing or too old.
    up-python.lha   Python 3.14 (SYS:Python3, Python3: assigned at boot).
                    Stands alone: brings an ixemul.library when LIBS: has
                    none of version 47 or newer, and uses this kit's.
    up-claude.lha   C:Claude and C:uptelnet (Claude, Anthropic's AI model).
                    Stands alone.
    up-pi.lha       C:amiga-pi (pi, the coding agent). Stands alone.
  Git, Perl and Vim are packages for upkg, not archives: they were in this
  archive before the slim base, and they install at the same places
  (UP-Term:bin, UP-Term:share). Type  upkg install git  (or perl, vim) in an
  UP-Term window; upkg remove takes them out. BitchX, the IRC client, is an
  upkg package too (upkg install bitchx): C:BitchX in UP-Term:bin and its
  scripts and help in UP-Term:lib/bx. Uninstall of UP-Term takes
  them out too, before it takes UP-Term out.
  The stand-alone ones install on a Workbench with no UP-Term and record
  themselves in ENVARC:upkg; in an UP-Term window they use its screen, keys
  and vsh. Uninstall of UP-Term takes up-nvim out first, by its own
  Uninstall (and asks once about UP-Term:var); the stand-alone ones stay.
  An UP-Term installed from the single kit (before the split) that had Neovim,
  Python, Claude or amiga-pi keeps them when up-term.lha is installed over it
  (Update says so); install the archive of each to keep it updated. The source
  of the GNU commands is up-term-src.lha, next to up-term.lha, with the Unix
  tools' source.

STATUS
  Test build. CON:/RAW: and console.device stay the system's unless you
  switch them (above); XCON: runs beside them.
