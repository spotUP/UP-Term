# Claude Code on the NAS, for the Amiga

Claude Code runs in a container on the NAS, inside tmux, so a session survives the
Amiga disconnecting. The Amiga reaches it with UP-Term's `uptelnet`:

    uptelnet <NAS LAN address> 2323

UNENCRYPTED telnet: LAN only, never forward the port on the router.

## What you need

- A NAS (Synology DSM 7 with Container Manager, x86-64 or ARM64) or any Linux
  machine with Docker, on the same LAN as the Amiga, with internet access: the
  first start installs Claude Code from npm into the volume.
- A Claude subscription (Pro or Max) to log in with (step 4).
- On the Amiga: UP-Term installed (its `uptelnet` does the connecting), a
  TCP/IP stack running (Roadshow, Miami or AmiTCP). The Amiga's side is in
  dist/README.txt, section CLAUDE FROM THE AMIGA: the Installer asks where
  Claude Code runs and writes `ENVARC:Claude/remote` ("host port"); the first
  run of `C:Claude` asks the same (`Claude SETUP` asks again), and the
  `ClaudeCode` icon reads that file (or takes a host and port as arguments).

## Set up (Synology DSM 7, Container Manager)

1. Copy this drawer (Dockerfile, compose.yaml, entrypoint.sh, setpw, README.md) and
   `tools/uptelnetd.py` into one folder on the NAS, e.g. `/volume1/docker/claude-amiga`,
   and create `home` inside it (the container's /home/claude volume).
2. Container Manager > Project > Create, source: the existing compose.yaml; edit
   `NAS_IP` and `ALLOW` first; build and start. The container log then repeats
   "no login password yet ... run: setpw" until step 3.
3. Container Manager > Container > claude-amiga > Action > Open terminal > Create >
   Launch with command: `setpw`. Type the password the Amiga will log in with
   (8+ characters, not echoed). The server starts within 30 s.
4. Log Claude Code in once WITH YOUR CLAUDE SUBSCRIPTION (Pro/Max), not an API key.
   Same terminal window, Launch with command:
   `su - claude -c 'cd ~/work && tmux new -A -s claude claude'`
   (as the `claude` user: a root shell would keep the login in /root, off the volume,
   where the Amiga's sessions never see it). Run `/login`, choose the Claude account
   (subscription) option, open the printed URL in a browser, sign in, paste the code
   back. Never set ANTHROPIC_API_KEY in the container: Claude Code would use it
   instead of the subscription. `/status` shows which one is in use.
5. On the Amiga: `uptelnet <NAS_IP> 2323`, the password, and you are in Claude Code.
   Ctrl+] leaves; Claude keeps running in tmux for the next connection.

Without Synology: put the same files in one folder, set `NAS_IP` (the address the
Amiga reaches, space-separated with any others to listen on) and `ALLOW` (the
LAN's CIDR) in `compose.yaml`, create `home/`, run `docker compose up -d`, then
`docker exec -it claude-amiga setpw` for step 3 and
`docker exec -it -u claude claude-amiga sh -c 'cd ~/work && tmux new -A -s claude claude'`
for step 4. (Not tried on a machine other than the owner's Synology.)

Claude Code itself is installed on the volume (`home/.npm-global`, owned by the
`claude` user) on the first start, so its auto-updater can write there and updates
survive a rebuild; delete that folder to force a fresh install.

Changed Dockerfile or entrypoint.sh: raise the `image:` tag in compose.yaml
(claude-amiga:4 -> 5), then Project > Action > Build. With the same tag, Build
reuses the old image.

If a Build leaves DSM showing a container named `<id>_claude-amiga` that it cannot
stop or clean ("No such container"), remove it as root and recreate it: Control
Panel > Task Scheduler > Create > Triggered Task > User-defined script (user root,
not enabled), script `cd /volume1/docker/claude-amiga && for c in $(docker ps -a
--format '{{.Names}}' | grep claude-amiga); do docker rm -f "$c"; done && docker
compose up -d`, Run it once, then delete the task. The login on the volume stays.

Tailscale (optional, away from home): the Synology Tailscale package hands what
reaches the NAS's tailnet address to 127.0.0.1, which is why NAS_IP also lists
127.0.0.1. Its own page does not open over QuickConnect.

Model, settings and CLAUDE.md live in `/volume1/docker/claude-amiga/home/.claude`.
Work files: `/home/claude/work` in the container (add a volume for a NAS share if
Claude should edit files there).

Status: built and running on the owner's DS218+ (DSM 7.2.2) 2026-10-05. Needs an
x86-64 or ARM64 Synology with Container Manager.

## Give Claude control of the Amiga (amimcp)

With this, the Claude Code in the container can act on the Amiga you talk to
it from: run AmigaDOS commands, read and write files, list drawers, see the
screen, click and type. It uses amimcp (https://github.com/thomas-luebker/amimcp):
amiagent on the Amiga, an MCP server (pure Python 3 standard library) beside
Claude Code. amiagent runs whatever it is sent and the link is NOT encrypted:
always set a TOKEN, keep it on a LAN you trust, never forward the port (7846).

1. On the Amiga: install amiagent (Aminet comm/net/amiagent) and start it with
   a token, at every boot from S:User-Startup:

       Run >NIL: amiagent TOKEN=pick-a-secret QUIET

   Note the Amiga's IP address.

2. On the NAS (container volume = /volume1/docker/claude-amiga/home, which is
   /home/claude inside the container), put three things beside Claude Code:

   - amimcp's server: clone https://github.com/thomas-luebker/amimcp and copy its
     `server/` drawer to `home/amimcp/server/`.
   - the skill: copy `skills/amiga-upterm/` (beside this README) to
     `home/.claude/skills/amiga-upterm/`. It teaches Claude AmigaDOS, the
     UP-Term layout and the safety rules (requesters block amiagent, never
     kill an install, read before writing).
   - the MCP config: copy `mcp.json` (beside this README) to
     `home/work/.mcp.json`. Claude Code starts in ~/work, so it finds it; the
     server reads the Amiga's address and token from two files at start, so
     the token is in neither the config nor any chat.

   Copying from a Mac: Synology's scp/SFTP may be off; tar over ssh works:

       tar -C staging -cf - amimcp .claude/skills/amiga-upterm work/.mcp.json \
         | ssh -p <port> <user>@<nas> "tar -C /volume1/docker/claude-amiga/home -xf -"

3. The address and token, written as the container's claude user (Container
   Manager > Container > claude-amiga > Terminal > Create > bash):

       su -s /bin/sh claude -c 'umask 077; mkdir -p ~/.config/amimcp; printf "%s" "AMIGA_IP" > ~/.config/amimcp/host; read -rs T; printf "%s" "$T" > ~/.config/amimcp/token'

   It waits for the token (not echoed), then Return.

4. Start `claude` from UP-Term (C:Claude or ClaudeCode). Claude asks once to
   approve the project's `amiga` MCP server: approve it. Check: ask "what
   volumes does my Amiga have?" -- it runs a command on the Amiga and answers.

Not covered here: amiagent on the Amiga is the only thing that makes the link
that Claude controls the machine; without step 1 above the container's Claude
Code is just a terminal session. The rig (tools/rig/README.md) uses the same
amiagent, with token `rigtoken`, on an emulated Amiga: never reuse that token
on a real one.

## ssh to the NAS (for setting this up from another computer)

DSM: Control Panel > Terminal & SNMP > Enable SSH service (any port); Control
Panel > User & Group > Advanced > Enable user home service (key login needs a
home); on DSM 7 only members of `administrators` may log in by ssh. Then, in a
normal terminal (it asks for the password, so not from a tool without a TTY):

    ssh-copy-id -p <port> -i ~/.ssh/id_ed25519.pub <user>@<nas>

Synology refuses keys unless the home is not group-writable:
`chmod 755 ~; chmod 700 ~/.ssh; chmod 600 ~/.ssh/authorized_keys`.
