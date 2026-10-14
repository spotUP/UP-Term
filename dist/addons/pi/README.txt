amiga-pi - the pi coding agent
==============================

pi, the coding agent, for the Amiga: C:amiga-pi. It stands alone; UP-Term.lha is optional.

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
  Uninstall) is ENVARC:uppkg/amiga-pi. Install the archive over an earlier one and its first
  page offers Update, which copies only what changed. An UP-Term installed from the single
  kit (before the kit split) that already has this program keeps it: Install recognises it
  and offers Update from "installed by the single UP-Term kit".

USE
  The sections below.

UNINSTALL
  Double-click Uninstall (or Execute Uninstall). It removes C:amiga-pi; your settings in
  ENVARC:amiga-pi stay. With UP-Term installed it asks whether to delete your data in
  UP-Term:var/amiga-pi (default No). Uninstall of UP-Term.lha leaves this program alone.

AMIGA-PI
  C:amiga-pi is pi, the coding agent (pi.dev), for the Amiga: it
  reads, searches and edits files and runs commands for you, with the
  model of your choice (Anthropic, OpenAI and compatible servers, Google,
  Azure). It has the same needs as Claude: a TCP/IP stack, AmiSSL 5 for
  https, a 68020 or better, and an API key of your own for the provider.
    amiga-pi                  a conversation (/help lists the commands)
    amiga-pi --version        the version (1.1.0, the pi it follows)
  The key: SetEnv SAVE ANTHROPIC_API_KEY yourkey (OPENAI_API_KEY and the
  provider's other variables the same way), or in
  ENVARC:amiga-pi/auth.json. What it uses is billed to that key.
  Its settings are small files in ENVARC:amiga-pi: settings.json,
  models.json (more models and servers), auth.json, keybindings.json,
  SYSTEM.md, APPEND_SYSTEM.md, AGENTS.md. Install makes the drawer and
  never writes a file in it; Install, Update and Uninstall leave your
  files there. Its sessions, skills, prompts, themes and extensions (Lua)
  are in UP-Term:var/amiga-pi, which Uninstall keeps unless you say yes
  (above). Without UP-Term: everything stays in ENVARC:amiga-pi.
  "Connection error." and nothing more: SetEnv PI_DEBUG_LOG T:amiga-pi.log
  and try again; that file says why (no answer from the address, AmiSSL
  not in LIBS:, the certificate). It also holds each request, the key
  left out, and each answer; UnSetEnv PI_DEBUG_LOG turns it off.

AMIGA SKILLS
  Install puts four Agent Skills (one drawer each with a SKILL.md) beside your own: amigados
  (Shell commands, scripts, assigns, protection bits, redirection pitfalls), arexx (rx, ports,
  RexxMast, ADDRESS), amiga-c-build (vbcc, vasm and vlink flags with a worked example, gcc) and
  amiga-autodocs (how to look a call up). They go to UP-Term:var/amiga-pi/skills when UP-Term is
  installed, else to ENVARC:amiga-pi/skills. Install replaces only these four drawers; a skill of
  another name in that drawer is yours and stays, and Uninstall removes only these four. Keep a
  changed copy under another name: Install overwrites these.
  The Amiga autodocs are not in this archive (Hyperion's licence). The amiga-autodocs skill reads
  them from your own NDK when you have one: make an assign NDK: to the NDK drawer (the one holding
  Autodocs, Include_H and FD), e.g.  Assign NDK: Work:NDK3.2R4  and put the line in S:User-Startup.
  amiga-pi then greps NDK:Autodocs/<library>.doc for the one function it needs and reads those
  lines only; without the NDK it says so.
