Claude - Claude for the Amiga
=============================

Claude, Anthropic's AI model: C:Claude (with C:uptelnet, for Claude Code on a server). It stands alone; UP-Term.lha is optional.

WHAT YOU NEED
  AmigaOS 3.0 or newer, a 68020 or better, a TCP/IP stack (Roadshow, Miami, AmiTCP) and,
  for https, AmiSSL 5 (Aminet util/libs/AmiSSL-v5-OS3.lha). UP-Term.lha is not needed.

ON A PLAIN WORKBENCH
  Without UP-Term the program runs in the Shell window in its line mode (the console's own
  line editing: in the ROM CON: window Up does not recall a line; KingCON's does) and the
  commands it runs go through the AmigaShell, which it tells the model. In an UP-Term window
  it gets the full screen, the keys and vsh, and keeps its data in UP-Term:var.

INSTALL
  Unpack the archive and double-click Install in its drawer (needs C:Installer or
  SYS:Utilities/Installer). From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files
  It needs no UP-Term and looks for none. Its record (the list of files, for Update and
  Uninstall) is ENVARC:upkg/up-claude. Install the archive over an earlier one and its first
  page offers Update, which copies only what changed. An UP-Term installed from the single
  kit (before the kit split) that already has this program keeps it: Install recognises it
  and offers Update from "installed by the single UP-Term kit".

USE
  The sections below.

UNINSTALL
  Double-click Uninstall (or Execute Uninstall). It removes C:Claude (and an old C:ClaudeCode,
  which Claude.lha 1.0 had), the C:wasabid this archive put in (not one that was there before), and C:uptelnet unless UP-Term is installed; your settings in ENVARC:Claude stay. With UP-Term
  installed it asks whether to delete your data in UP-Term:var/Claude (default No).
  Uninstall of UP-Term.lha leaves this program alone.

CLAUDE FROM THE AMIGA
  One app, one icon. Install asked where Claude Code runs (the NAS) and
  wrote it to ENVARC:Claude/remote ("host port"; Claude SETUP writes it too;
  edit or delete it). Then:
    Claude            with no API key set, Claude Code on the NAS in this
                      window; with an API key too, a question first: Claude
                      on the Amiga (API key) or Claude Code on the NAS
                      (subscription). The last answer is the one highlighted
                      (ENVARC:Claude/last-mode). Up, Down, Enter, or 1 and 2.
    Claude LOCAL      Claude on this Amiga with the API key, no question
    Claude REMOTE     Claude Code on the NAS, no question
  Print mode (Claude -p ...), PING, URL= and a prompt on the command line
  always run on this Amiga and never ask. Double-click the Claude icon in
  UP-Term: for the same.
  The NAS first asks the password; then it pairs: it hands this Amiga a key
  (ENVARC:Claude/nas-key) and from then on no password is typed or sent: the
  Amiga and the NAS prove the key to each other (HMAC-SHA256 challenge-response,
  net/tn.h). Revoke an Amiga on the NAS with: pairs revoke ID. The NAS starts
  Claude Code in tmux. (An old ClaudeCode icon and C:ClaudeCode are removed by
  Install: the one Claude does both.)
  Claude Code runs on your Mac; the Amiga is its terminal over the LAN.
  To set up the NAS end (a Synology container that keeps Claude Code in
  tmux), see tools/nas/README.md in the vtcon source tree; to let Claude
  there run commands on this Amiga, wasabi (wasabid on the Amiga) is described
  in the same file (never share its key).
  Install puts wasabid 0.11b2 (MIT, by Tobias Karlsson, Files/wasabid.LICENSE) in
  C: only when no C:wasabid is there; a wasabid of UP-Term.lha or of yours stays.
  It is not started at boot: after a key login uptelnet starts a copy for that
  login, with a key of its own, and ends it when the login ends.

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
  First run: with no key, no ENVARC:Claude/remote and no
  ENVARC:Claude/setup-done, Claude starts a short setup: it asks whether
  Claude Code runs on another computer, whether you use an API key here,
  or later; checks the TCP/IP stack, AmiSSL 5 (key) or C:uptelnet (other
  computer) and that the computer answers; offers amimcp/amiagent; writes
  the settings. `Claude SETUP` (or /setup) runs it again.
  The key: SetEnv ANTHROPIC_API_KEY yourkey, then Copy ENV:ANTHROPIC_API_KEY
  ENVARC: to keep it (OS 3.2 and later: SetEnv SAVE ANTHROPIC_API_KEY
  yourkey does both), or put it alone in the
  file ENVARC:Claude/key (Install makes the drawer, never a key; Uninstall
  leaves the drawer and your key). Claude never shows the key and never
  writes it to a log; DEBUG logs to T:Claude.log with the key blanked. The
  key only goes over https.
  The screen is Claude Code's: what was said scrolls above, an input box
  and a status line stay at the bottom (model, effort, the start
  directory, how much of the context is left, the permission mode).
    Enter               sends; a new line: Shift+Enter, \ Enter, or Ctrl+J
    Up / Down           earlier prompts, kept across sessions per start
                        directory (UP-Term:var/Claude/history); Ctrl+R searches
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
  conversation saved after each answer (UP-Term:var/Claude/session.json).
  Without UP-Term: (the kit not installed) the history and the session stay
  in ENVARC:Claude. Uninstall asks whether to delete UP-Term:var (default
  No: everything else goes, var stays); Install and Update never touch it.
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

