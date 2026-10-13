UP-Term-pi - amiga-pi, the pi coding agent
==========================================

pi, the coding agent, for an UP-Term window: C:amiga-pi.

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
  first. It removes C:amiga-pi; your settings in ENVARC:amiga-pi stay. It asks
  whether to delete your data in UP-Term:var/amiga-pi (default No).

AMIGA-PI
  C:amiga-pi is pi, the coding agent (pi.dev), for an UP-Term window: it
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

