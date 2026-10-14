---
date: 2026-10-14
topic: Claude Code on the NAS feels local on the Amiga (one Claude, key login, NAS Claude controls the connected Amiga, one-key start)
tags: [claude, uptelnet, uptelnetd, nas, amiagent, amimcp, security, CL1]
status: draft
---

# NAS Claude Code feels local

Owner goal (2026-10-14): the real Claude Code runs on the NAS (Docker, tmux, the
owner's Claude subscription) and the Amiga is its terminal through C:Claude and
C:uptelnet. It should feel like a local program: one icon, no password every time,
Claude Code on the NAS can act on the Amiga it is talking to, one key to start.

The subscription stays legitimate: Claude Code itself, logged in with `/login` on the
NAS, does all model traffic; the Amiga only carries a terminal (ledger lines 1203 and 1227:
no Claude.ai login in third-party apps; this design adds none).

Done = NL1..NL22 ticked, each with its test; the manual items (NL21, NL22) collected
for the owner.

## Decisions for the owner

Each has a recommendation. Nothing below NL5 starts before D1-D3 are answered.

- **D1 How the Amiga proves itself to the NAS.**
  (a) Challenge-response with a per-Amiga key over the existing uptelnet link
  (telnet AUTHENTICATION option, HMAC-SHA256; the key never crosses the wire after
  pairing). (b) ssh: BebboSSH from the kit (`dist/net/bebbossh-1.45`) to an sshd in the
  container with a public key: encrypts the whole session, but the 68020 cost is high
  (memory note: SSL too heavy for 68020), and BebboSSH's fit to an UP-Term window
  (termios raw mode, window-size polling, the goodbye reset that `net/tn.c` does) is
  untested; uptelnet's console work would be lost. (c) store the password in ENVARC and
  have uptelnet type it: the reusable secret still crosses the wire in clear at every
  start. **Recommended: (a)** because it removes the reusable secret from the wire for
  the cost of one small option in code the repo already host-tests, keeps uptelnet, and
  leaves (b) open later for anyone who needs the session itself encrypted.
- **D2 How the key reaches both sides.**
  (a) Pairing: the first connection logs in with the password as today; the NAS then
  generates the key and hands it to the Amiga inside that session, once. Same exposure
  as one password typed today, then never again. (b) Manual: the owner runs `pairs add`
  on the NAS and types the printed key into a file on the Amiga. No wire exposure, but
  64 hex characters typed by hand. **Recommended: (a)** because it is zero setup and its
  one exposure equals one of today's logins; (b) stays available as `pairs add` for the
  careful case.
- **D3 Which amiagent Claude Code on the NAS talks to, and when.**
  (a) A session agent: uptelnet starts its own amiagent on port 7847 right after a key
  login, with a token both ends derive from the key and the login's nonces (never typed,
  never stored, new every session), and stops it when the session ends. A boot-time
  amiagent on 7846 (the Mac's ami.py, the rig) keeps running untouched beside it.
  (b) Today's way: the owner's boot-time amiagent, its fixed token and the Amiga's IP
  copied to the NAS by hand (`tools/nas/README.md` steps 1-3). **Recommended: (a)**
  because it needs no setup, the Amiga is only controllable while you are connected,
  and a sniffed token dies with the session.
- **D4 What Claude Code on the NAS may do without asking.**
  (a) Ask on every amiga tool call (Claude Code's default). (b) Read-only tools
  (list, get, info, screens, shot, uitree) allowed; exec, put, input, rexx ask.
  (c) Everything allowed. **Recommended: (b)** because reading is what makes it feel
  local and cannot damage the Amiga, while every write still shows you the command
  first. Tool names depend on amimcp (unverified U2); the setting lists them by name.
- **D5 Where Claude.lha gets amiagent from** (Claude.lha stands alone: nothing in it
  may need the base kit, `dist/addons/claude/install.dos:4`). (a) Claude.lha carries
  amiagent.020 (Apache 2.0) and installs it as `C:amiagent`; uptelnet looks for
  `UP-Term:bin/amiagent` first, then `C:amiagent`. (b) Only the base kit's optional
  amiagent (`dist/Install.installer:178-191`); without it, no NAS control.
  **Recommended: (a)** because "no manual setup" fails for every Claude.lha-only user
  under (b); the binary only runs while a key-logged-in session asks for it.
- **D6 One-key start inside UP-Term.** (a) A menu item "Claude" in the UP-Term menu with
  a Right Amiga key, opening a new tab that runs C:Claude (the Help > Demo tour path),
  plus `/claude`. (b) (a) and a Workbench-wide hotkey (a commodity). **Recommended:
  (a)** with Right Amiga L (free: the menus use T . , C V A F G K + -,
  `handler/vtcon_handler.c:1298-1326`), because from Workbench the Claude icon already
  is the one action, and a commodity is a new resident program for little gain. Say if
  you want another letter.

Implementation defaults (not decisions, stated so they are not re-litigated): the mode
question highlights the last choice (ENVARC:Claude/last-mode); the relay serves the most
recent key-logged-in Amiga only; the session agent's port is 7847.

## Research findings

### Today's flow (W49)

- `C:Claude` with no key and an `ENVARC:Claude/remote` runs `uptelnet "host" port` in
  the same window: `claude/main_amiga.c:102-136` (`remote()`), called at
  `claude/main_amiga.c:252` only when there is no key (`load_key`,
  `claude/main_amiga.c:82-100`). With a key it never offers the NAS: CL1's gap.
- `dist/ClaudeCode` is an AmigaDOS script that gets "host port" from
  `C:Claude REMOTE-ADDRESS` and runs uptelnet. The Installer also uses
  REMOTE-ADDRESS (`dist/addons/claude/Install.installer:14,23`), so that switch stays.
- ClaudeCode appears in: `dist/packages.txt:23`, `dist/addons/claude/parts.txt:11-14`,
  `dist/addons/claude/install.dos:4,29-36,62,68`, `dist/addons/claude/Install.installer:8,40,77,82`,
  `dist/addons/claude/README.txt:4,31,39-42`, `dist/addons/claude/LICENSES.txt:6,9`,
  `dist/aminet/claude.readme.in:11,18`, `dist/README.txt:677,794`, `tools/mkicon.py:151`,
  `tools/mkinstaller.py:45`; removal of old copies already exists in `dist/Uninstall:67`
  and `dist/addons/claude/uninstall-body.dos:2,12`.
- The quick-launch `launch/Claude` runs `UP-Term:bin/vsh -c C:Claude` in an IconX window
  (`dist/addons/claude/launch/Claude`).
- A keyboard selector exists: `tui_ext_menu` (`claude/tui.c:4337`, `claude/tui.h:345`).

### uptelnet authentication today

- Server: `tools/uptelnetd.py` (653 lines; Python stdlib). After telnet negotiation it
  writes `Password: ` (`tools/uptelnetd.py:444`), reads a line, compares in constant time
  (`:177-178`, `:464`). Safety already there: private binds only, peer allowlist, 2 s per
  wrong password, 3 tries per connection, 5 failures in 10 minutes lock the address,
  4 pending logins at most, 60 s to log in (`:24-41`). The password is in
  `~/.config/uptelnetd/password`, 0600, checked by `load_password` (`:151-175`).
- Telnet options the server handles: ECHO, SGA, BINARY on its side; BINARY, SGA, NAWS,
  TTYPE on the client's (`:215-220`). Subnegotiation buffer 64 bytes (`:344,351`).
- Client: `net/uptelnet.c` (template `HOST/A,PORT/N,TERM/K`, `:215`) does socket and
  console; the protocol is `net/tn.c`/`net/tn.h`, portable C89, host-tested in
  `tests/test_telnet.c`, and driven against the real server by `build/tn_host`
  (`Makefile:232-235`, `tools/test_uptelnetd.py:348`). Options above 40 are refused
  (`net/tn.h:30`); subnegotiation buffer 64 bytes (`net/tn.h:51`).
- On the NAS: the same `uptelnetd.py` copied into the image (`tools/nas/Dockerfile`),
  run by `tools/nas/entrypoint.sh:37` with `--command 'cd ~/work && tmux new -A -s claude claude'`;
  the entrypoint waits until a password exists (`:20`); `setpw` writes it
  (`tools/nas/setpw`). Host networking (`tools/nas/compose.yaml:9`), so 127.0.0.1 in the
  container is the NAS's loopback and the container reaches the Amiga's LAN address
  directly. Tailscale connections arrive from 127.0.0.1 (`compose.yaml:12-13`).
- SHA-256 and HMAC-SHA256 in portable C89 already exist: `llm/sha256.h`/`llm/sha256.c`
  (used by SigV4 and amiga-pi), same code on the 68k and the host. No new crypto code.

### The NAS's `amiga` MCP entry

- `tools/nas/mcp.json`: server `amiga` = `python3 $HOME/amimcp/server/amimcp.py` with
  `AMIGA_HOST` and `AMIGA_TOKEN` read from `~/.config/amimcp/host` and `.../token` when
  the MCP server starts. amimcp is upstream https://github.com/thomas-luebker/amimcp,
  cloned by hand into the volume (`tools/nas/README.md`, "Give Claude control of the
  Amiga" step 2); no version is recorded anywhere in this repo (U2).
- The Amiga address is whatever the owner typed into `~/.config/amimcp/host`
  (README step 3); nothing discovers it. Port: amiagent's default 7846.
- amiagent 0.13.2 (`dist/net/amiagent-0.13.2/`): one request per TCP connection, 12-byte
  frames "AMI0"; with TOKEN set, an `AUTH` (0x10) frame with the byte-identical token
  must come first on each connection, a wrong one closes it; the token and payloads are
  cleartext (`PROTOCOL.md:35-39,60,95-106`). Options: PORT/N, TOKEN/K, VERBOSE/S, QUIET/S
  (`amiagent.readme:344-348`); no bind address, no peer allowlist (so it listens on every
  interface). A second instance runs without the AMIAGENT ARexx port (`amiagent.readme:115-117`),
  so two agents on two ports can coexist.
- Why the address cannot simply be written at login: Claude Code runs inside one tmux
  session that outlives connections (`entrypoint.sh:37`), and its MCP server process is
  started once with the host and token in its environment; a later login from another
  address (DHCP, another Amiga) would not reach it. Hence the relay (NL15): the MCP
  server always talks to 127.0.0.1, and uptelnetd, which knows who is logged in,
  forwards each request.

### One-key start

- UP-Term has no global hotkey. Its menu (`handler/vtcon_handler.c:1298-1352`) has New tab
  (Right Amiga T) and Help > Demo tour, which opens a tab running a script
  (`tab_demo_script`, `:5382-5402`; `host_command`, `:5515-5527`; slash `demo`,
  `handler/slash.c:82`). A Claude tab is the same path with another command.

## Threat model and what the design protects

Home LAN. Assets: (1) the uptelnetd login, which is a shell as the `claude` user in the
container with the owner's Claude subscription and Claude Code's own Bash tool;
(2) amiagent, which is full control of the Amiga.

| Attacker | Today | After this plan |
|---|---|---|
| Internet | Nothing listens publicly: uptelnetd refuses public binds; amiagent sits behind the router's NAT. Never forward 2323/7846/7847. | Same. |
| Passive sniffer on the LAN (a compromised IoT device, a Wi-Fi guest on the same network) | Sees the uptelnetd password at every start and reuses it; sees a fixed amiagent token at every MCP call and reuses it any time the agent runs. | Sees no reusable login secret (the key never crosses the wire after pairing; one-time nonces). Sees the session agent's token in amiagent's AUTH frames, but that token is valid only while that session lasts, on the session agent's port. Still sees the session's content (Claude Code's screen) in clear. |
| Active attacker on the LAN (ARP spoofing) | Can take over any session. | Still can take over a session after login (plain TCP): only encryption stops that (D1 (b)). A fake NAS cannot pass the client's check of the server's proof, so it learns nothing usable. |
| Someone at the Amiga | Owns the Amiga anyway. | The key is in ENVARC (AmigaOS has no file permissions); revoke it on the NAS with `pairs revoke`. |
| Another process on the NAS | n/a | The relay listens on loopback and needs the relay token from a 0600 file of the `claude` user. |

amiagent access is limited to: running only while a key-logged-in session asked for it
(D3 a), a 128-bit token new every session, reachable from the NAS only through the relay
to the address that just proved the key, and D4's permission prompts in Claude Code.
Over Tailscale the peer is 127.0.0.1, so the relay serves no Amiga there (stated in the
README; a later item if wanted).

## Protocol (NL6 writes it into net/tn.h and the uptelnetd docstring)

RFC 2941 framing, option AUTHENTICATION (37), one private auth-type `0x75` ("UPT1"),
all binary fields IAC-doubled inside SB. K = 32-byte key, ID = up to 16 ASCII characters.

1. Server opens with `IAC DO AUTH` beside its existing offers.
   A client without KEY= answers `WONT` (old clients: refuse, as `tn.c` does for any
   unknown option) and gets `Password:` exactly as today.
2. Client with KEY= answers `IAC WILL AUTH`.
3. Server: `SB AUTH SEND 0x75 sn[16]` (sn from `os.urandom`).
4. Client: `SB AUTH IS 0x75 ID 0x00 cn[16] mac[32]`,
   `mac = HMAC(K, "upt1-client" | sn | cn | ID)`. cn: EClock + DateStamp + task address
   hashed (uniqueness, not secrecy). A client with an empty key file sends ID empty and
   no mac: "pair me after the password".
5. Server, mac good: `SB AUTH REPLY 0x75 ACCEPT smac[32]`,
   `smac = HMAC(K, "upt1-server" | sn | cn)`; no Password prompt. Client checks smac; a
   bad one closes the line ("this is not the NAS that paired with this Amiga").
   mac bad or ID unknown: `REPLY 0x75 REJECT`, the failure counts in the rate limiter,
   then `Password:` as today (and the client says the NAS no longer knows this key).
6. Pairing (D2 a): after a good password from a client that asked in step 4, the server
   makes ID and K, stores them, and sends `SB AUTH REPLY 0x75 PAIR ID K`; the client writes
   its key file. Next start goes through step 5.
7. Session agent (D3 a): after ACCEPT, `T = hex(HMAC(K, "upt1-agent" | sn | cn))[0:32]`.
   The client starts amiagent with `TOKEN=T PORT=7847` and sends
   `SB AUTH IS 0x75 AGENT port` (port 0: no agent). The server records
   (peer, port, T) for the relay.

Key files: Amiga `ENVARC:Claude/nas-key` (one line `ID hexkey`, copied to ENV:);
NAS `~/.config/uptelnetd/clients` (0600, lines `ID hexkey date first-peer`).

## Checklist

Size: S under 2 hours, M half a day, L a day or more. "Host" tests run in `make test`
(the one suite named). Rig checks need the rig up (`rig.py start --max`).

### Phase A: one Claude (CL1)

- [ ] **NL1** Launch choice as a pure function `cl_launch_mode` (key?, remote file?,
  LOCAL, REMOTE, PRINT, PING, SETUP, URL, PROMPT) -> LOCAL | REMOTE | ASK | SETUP.
  Files: `claude/cli.c`, `claude/cli.h`. Test: host `ONLY=claude_cli` (every row of the
  table, incl. print mode never asks, both switches refused). S.
- [ ] **NL2** `LOCAL/S`, `REMOTE/S` in the template (`claude/cli.h:46`) and `--local`,
  `--remote`; `Claude ?` and `--help` updated (W-A3). REMOTE with no remote file:
  the existing "no host" message. Files: `claude/cli.c`, `claude/cli.h`. Test: host
  `ONLY=claude_cli` (both syntaxes). S.
- [ ] **NL3** The question at start, wired at `claude/main_amiga.c:252`: on an UP-Term
  console `tui_ext_menu` with "Claude on the Amiga (API key)" and "Claude Code on the NAS
  (subscription)", last choice highlighted (ENVARC:Claude/last-mode); a plain console
  gets a numbered line question. The question logic lives in a host-testable file, the
  Amiga file only calls it. Files: `claude/main_amiga.c`, `claude/launch.c` (new, or
  `cli.c` if small), `claude/repl.h`. Test: host `ONLY=claude_tui` golden screen of the
  question + keys; reachability: rig, Claude with both a key and a remote file shows the
  question and REMOTE goes to uptelnet (counter: uptelnet's connect line). M.
- [ ] **NL4** ClaudeCode leaves the kit: delete `dist/ClaudeCode`, its rows in
  `dist/packages.txt:23` and `dist/addons/claude/parts.txt:11-14`, copy/protect/launch
  lines in `dist/addons/claude/install.dos`, the Installer texts, `tools/mkicon.py:151`;
  install.dos and the Installer delete an old `C:ClaudeCode` and
  `UP-Term:ClaudeCode(.info)` on reinstall (Uninstall already does). Test: host
  `ONLY=packages`, `make dist-check`; rig `install_rig.py --only` the Claude cases (old
  ClaudeCode gone after a reinstall over a 1.0 install). M.
- [ ] **NL5** Docs: `dist/addons/claude/README.txt`, `dist/README.txt` (CLAUDE FROM THE
  AMIGA, 677, 794), `dist/aminet/claude.readme.in`, `dist/addons/claude/LICENSES.txt`,
  `tools/nas/README.md` steps 4-5; bump Claude's minor in `dist/packages.txt`.
  Test: `python3 tools/mkreadme.py claude`, host `ONLY=packages`. S.

### Phase B: key login (D1 a, D2 a)

- [ ] **NL6** Protocol written down: `net/tn.h` header comment and the uptelnetd
  docstring (the section above). Files: `net/tn.h`, `tools/uptelnetd.py`. Test: none
  (text), reviewed with NL7/NL8. S.
- [ ] **NL7** Client half in `net/tn.c`: KEY given -> WILL AUTH; build IS with
  `llm_hmac_sha256`; check smac; PAIR -> a `save_key` callback; AGENT message; grow
  `sb` 64 -> 128. Files: `net/tn.c`, `net/tn.h`, Makefile (link `llm/sha256.c` into
  uptelnet and tn_host). Test: host `ONLY=telnet`: key login bytes against a fixed vector,
  bad smac closes, PAIR stores, no key -> WONT and today's bytes unchanged, IAC (255) in a
  nonce doubled and split bytewise. M.
- [ ] **NL8** Server half in `tools/uptelnetd.py`: clients file (same 0600/owner/no-link
  checks as `load_password`), DO AUTH, nonce, constant-time mac check, failures through
  `RateLimiter`, fallback to Password, PAIR after a password login, log "key login ID".
  `--clients-file`. Start allowed with a password OR a clients file. Files:
  `tools/uptelnetd.py`, `tools/test_uptelnetd.py`. Test: host `ONLY=uptelnetd` units, and
  the ONE reachability test of key login: `build/tn_host ... KEY=<file>` (the real
  `net/tn.c`) through `main()` logs in with no `Password:` on the line, server log has
  "key login" (sentinel); a fresh tn_host with an empty key file pairs after the password
  and the second run logs in without it; a revoked ID gets `Password:`. L.
- [ ] **NL9** `tools/tn_host.c`: KEY= argument and key-file writing, so NL8 drives the real
  protocol code. Files: `tools/tn_host.c`, Makefile. Test: part of NL8. S.
- [ ] **NL10** uptelnet on the Amiga: `KEY/K` (file; PAIR writes it and its ENV: copy),
  cn from ReadEClock + DateStamp + FindTask, messages "paired with this NAS", "the NAS
  does not know this Amiga's key any more: type the password", "not the NAS this Amiga
  paired with". Template and `uptelnet ?` updated. Files: `net/uptelnet.c`. Test: rig,
  uptelnet to `tools/uptelnetd.py --bind 127.0.0.1` on the Mac (FS-UAE's bsdsocket uses
  host sockets): first run asks the password and writes the key, second run reaches the
  session with no prompt (screen read back). M.
- [ ] **NL11** C:Claude's remote path passes `KEY=ENVARC:Claude/nas-key` (`remote()`,
  `claude/main_amiga.c:102-136`). Files: `claude/main_amiga.c`. Test: covered by NL10's rig
  run started through `Claude REMOTE`. S.
- [ ] **NL12** NAS side: `entrypoint.sh` starts with a password or a clients file;
  new `tools/nas/pairs` (list, add, revoke; add prints a key for D2 b); Dockerfile COPY;
  `compose.yaml` image tag 4 -> 5; README section rewritten. Files: `tools/nas/*`.
  Test: host `ONLY=uptelnetd` runs `pairs` against a temp home (list/add/revoke file
  contents and modes); NAS check in NL21. S.

### Phase C: Claude Code on the NAS works on that Amiga (D3 a, D4, D5)

- [ ] **NL13** Token derivation shared by both ends, one vector pinned in C and Python.
  Files: `net/tn.c`, `tools/uptelnetd.py`, both test files. Test: host `ONLY=telnet` and
  `ONLY=uptelnetd` assert the same hex for the same K, sn, cn. S.
- [ ] **NL14** uptelnet starts the session agent: `AGENT/K` (path; default
  `UP-Term:bin/amiagent`, then `C:amiagent`), `AGENTPORT/N` (7847), after ACCEPT:
  LoadSeg + CreateNewProc `TOKEN=T PORT=7847` (process kept, NP_FreeSeglist), sends
  AGENT port (0 when the agent is missing or exits at once), Ctrl-C to it when the
  session ends by any path (Ctrl-], Break, line dropped). `NOAGENT/S` to opt out.
  C:Claude passes AGENT when an agent file exists. Files: `net/uptelnet.c`,
  `claude/main_amiga.c`. Test: rig, after a key login `Status` lists the agent and
  `ami.py --port 7847 --token <T from the test's key> ping` answers; after Ctrl-] it is
  gone and the boot agent on 7846 still answers. M.
- [ ] **NL15** The relay in uptelnetd: `--agent-relay 127.0.0.1:7846` and
  `--relay-token-file`; per connection: first frame must be AUTH with the relay token
  (constant time) else close; no live key session with an agent -> ST_ERR "no Amiga is
  connected to Claude Code"; else connect to (peer, port), send AUTH(T), read its status,
  then pipe the client's request and the answer. Registry: the most recent key-logged-in
  session with port > 0, removed when it ends; a loopback peer is never a target unless
  `--relay-loopback` (rig/tests only). Files: `tools/uptelnetd.py`,
  `tools/test_uptelnetd.py`. Test: the ONE reachability test of NAS control: a fake
  amiagent (Python, frame parser, counts requests) on 127.0.0.1:x; tn_host key-logs in
  and announces port x; a relay client sends AUTH(relay token) + PING; the fake agent saw
  AUTH(T derived) then PING (call count 1); after tn_host leaves, the same request gets
  ST_ERR; a wrong relay token gets a closed socket. L.
- [ ] **NL16** NAS wiring: `mcp.json` -> `AMIGA_HOST=127.0.0.1`, token from
  `~/.config/amimcp/token` (the relay token; entrypoint creates it 0600 when missing);
  entrypoint passes `--agent-relay`/`--relay-token-file`; `~/work/.claude/settings.json`
  with `enabledMcpjsonServers: ["amiga"]` (no approval prompt) and D4's allow list;
  deferred tool schemas so the amiga tools cost no context until used (Claude Code's
  tool search; env name to confirm against the installed version, U4). Files:
  `tools/nas/mcp.json`, `tools/nas/entrypoint.sh`, `tools/nas/settings.json` (new),
  `tools/nas/compose.yaml`, `tools/nas/README.md`. Test: host `ONLY=uptelnetd` checks the
  entrypoint's argument building (shell run with a fake python); NAS check in NL21. S-M.
  Depends on U2 (amimcp's env names and whether it sends AUTH on every connection).
- [ ] **NL17** Skill `tools/nas/skills/amiga-upterm/SKILL.md`: the amiga tools reach the
  Amiga you are typing from; "no Amiga is connected" means the session has no agent. Also
  the copy in vtcon-demo-perf (cross-repo rule). Test: none (text). S.
- [ ] **NL18** Claude.lha carries amiagent (D5 a): `dist/addons/claude/parts.txt`,
  `install.dos` (C:amiagent), `LICENSES.txt` (Apache 2.0, SOURCE.txt hashes), Uninstall
  removes C:amiagent with the last of Claude; base kit Installer help text
  (`dist/Install.installer:178-191`) mentions the session agent. Test: host
  `ONLY=packages`, `make dist-check`; rig `install_rig.py --only` the Claude cases. M.

### Phase D: one-key start (D6)

- [ ] **NL19** UP-Term menu "Claude" (Right Amiga L) and `/claude`: a new tab running
  C:Claude, by generalising `tab_demo_script` into one tab-script helper used by both;
  a requester when C:Claude is missing. Files: `handler/vtcon_handler.c`,
  `handler/slash.c`. Test: host `ONLY=slash` (or the suite that pins the slash table)
  for `/claude`; rig: menu pick through `ami.py menus`/`ui` opens a tab whose screen
  shows Claude's question or prompt. M.
- [ ] **NL20** Workbench: the Claude icon in the app drawer stays the one icon; with NL3 it
  asks the question. Files: none beyond NL4 (icon list). Test: rig install case
  double-clicks it (`wbrun`) and reads the question. S.

### Phase E: real hardware (manual, owner)

- [ ] **NL21** NAS deploy: copy `tools/nas/*` and `tools/uptelnetd.py`, Build image 5,
  start; `pairs list` empty; container log shows the relay listening on 127.0.0.1:7846.
- [ ] **NL22** Real Amiga: Claude icon -> question -> NAS; first time the password once,
  then "paired"; quit, start again: no password; ask Claude Code "what volumes does my
  Amiga have?": it answers from that Amiga; Ctrl-]: `Status` shows no session agent;
  Claude Code's amiga tools then say no Amiga is connected; Right Amiga L in an UP-Term
  window opens a Claude tab.

## Verification

Automated (host, CI): `make test ONLY=claude_cli`, `ONLY=claude_tui`, `ONLY=telnet`,
`ONLY=uptelnetd`, `ONLY=packages`, `ONLY=slash`; `make dist-check`. Reachability tests:
NL8 (key login through uptelnetd's main with the real `net/tn.c`) and NL15 (relay to a
fake amiagent), each with a sentinel; NL3's rig check for the launch path.

Rig (one case each, never the whole set): NL3, NL4, NL10, NL14, NL18, NL19, NL20.

Manual (owner only, never ticked by an agent): NL21, NL22.

## Not verified (read-only commands for the owner)

- **U1** The address and token setup on the NAS today (is the Amiga's IP static?):
  `docker exec -u claude claude-amiga sh -c 'cat ~/.config/amimcp/host; ls -l ~/.config/amimcp ~/work/.mcp.json'`
  (do not print the token file). On the Amiga: `Type DEVS:NetInterfaces/#?` (ADDRESS=dhcp
  or a fixed address) and `Search S:User-Startup amiagent`.
- **U2** amimcp: version, environment names (host, token, port), whether it sends AUTH
  on every connection, its tool names (for D4):
  `docker exec -u claude claude-amiga sh -c 'cd ~/amimcp && (git log -1 --format="%H %cd" || true); ls server; grep -n -i "version\|environ\|AMIGA_\|AUTH\|def tool\|\"name\"" server/amimcp.py | head -80'`
- **U3** The NAS runs the repo's uptelnetd: `docker exec claude-amiga sha256sum /usr/local/bin/uptelnetd.py`
  against `shasum -a 256 tools/uptelnetd.py` on the Mac; recent logins and their peer
  addresses: `docker logs --tail 30 claude-amiga`.
- **U4** Claude Code version and its deferred-tool setting:
  `docker exec -u claude claude-amiga sh -lc 'claude --version'`; the env name is then
  checked against that version's docs before NL16.
- **U5** That BebboSSH works in an UP-Term window (only relevant if D1 (b) is chosen):
  not tried anywhere.

Queue: Phase A is ledger CL1 (Q3-14, after U3 part 2); the caller adds NL6-NL22 to
`thoughts/shared/plans/2026-10-07-amiga-pi.progress.md` once D1-D6 are answered.
