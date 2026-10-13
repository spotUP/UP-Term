UP-Term-pi - amiga-pi, the pi coding agent
==========================================

Needs UP-Term.lha 1.0 or newer installed first (K2-1 may decide that C:amiga-pi
stands alone; then this line changes). The section moved here from the base
README by the kit split; the archive's Install, Uninstall and WHAT YOU NEED are
written with its Installer (kit-split plan K2-6).

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

