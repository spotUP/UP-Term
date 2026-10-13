UP-Term-Claude - Claude for an UP-Term window
=============================================

Claude, Anthropic's AI model, for an UP-Term window: C:Claude, and C:ClaudeCode for
Claude Code on a server.

WHAT YOU NEED
  UP-Term.lha 1.0 or newer, installed first (the screen, the keys and vsh come from it;
  Install checks the version). AmigaOS 3.0 or newer, a 68020 or better, a TCP/IP stack
  (Roadshow, Miami, AmiTCP) and, for https, AmiSSL 5 (Aminet util/libs/AmiSSL-v5-OS3.lha).

INSTALL
  Unpack the archive and double-click Install in its drawer (needs C:Installer or
  SYS:Utilities/Installer). From a Shell, without Installer, cd into the drawer, then:
    Execute Files/install.dos Files
  The first part checks that UP-Term.lha is installed and new enough; when it is not,
  Install says so and stops before it copies anything. Install the archive over an
  earlier one and its first page offers Update, which copies only what changed.
  An UP-Term installed from the single kit (before the kit split) that already has this
  program keeps it: Install recognises it and offers Update from "installed by the
  single UP-Term kit".

USE
  The sections below.

UNINSTALL
  Double-click Uninstall (or Execute Uninstall), or Uninstall of UP-Term.lha, which runs it
  first. It removes C:Claude and C:ClaudeCode; your settings in ENVARC:Claude stay. It asks
  whether to delete your data in UP-Term:var/Claude (default No).

CLAUDE FROM THE AMIGA
  Quickest: Claude. Install asked where Claude Code runs (the NAS) and
  wrote it to ENVARC:Claude/remote ("host port"; edit or delete it): with
  no API key set, Claude connects there in the window. Or ClaudeCode
  (or double-click ClaudeCode in the UP-Term drawer)
  connects to the host and port in ENVARC:Claude/remote (Claude SETUP
  writes it) -- ClaudeCode HOST PORT names another -- asks its password
  and starts Claude Code in tmux.
  Claude Code runs on your Mac; the Amiga is its terminal over the LAN.
  To set up the NAS end (a Synology container that keeps Claude Code in
  tmux), see tools/nas/README.md in the vtcon source tree; to let Claude
  there run commands on this Amiga, amimcp and amiagent are described in
  the same file (never run amiagent without a TOKEN).

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

