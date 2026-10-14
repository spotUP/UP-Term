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
  run of `C:Claude` asks the same (`Claude SETUP` asks again). There is one
  Claude app: with an API key and that file it asks "Claude on the Amiga" or "Claude Code
  on the NAS" at every start (`Claude LOCAL` and `Claude REMOTE` answer in advance).

## Set up (Synology DSM 7, Container Manager)

1. Copy this drawer (Dockerfile, compose.yaml, entrypoint.sh, as-claude, setpw, pairs, amiga-target, seed-claude-files,
   wasabi.rev, mcp.json, skills/, README.md) and
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
5. On the Amiga: `Claude REMOTE` (or `Claude` and pick the NAS; `uptelnet <NAS_IP> 2323`
   does the same without C:Claude). The first time it asks for the password and then says
   "paired with this NAS": the NAS handed this Amiga a key, kept in `ENVARC:Claude/nas-key`.
   From then on no password is typed. Ctrl+] leaves; Claude keeps running in tmux for the
   next connection.

### Key login instead of the password

The password crossed the LAN in clear at every start. An Amiga with a key does not send it
again: the first connection uses the password (step 3) and the NAS hands the Amiga a key over
that one session; from then on uptelnetd and the Amiga prove the key to each other with an
HMAC-SHA256 challenge-response (protocol: `net/tn.h` and the docstring of `uptelnetd.py`).
A sniffer on the LAN sees one-time values, and a fake NAS cannot pass the Amiga's check.
The session itself is still plain telnet. The keys are in `home/.config/uptelnetd/clients`
(0600); manage them in the container's terminal:

    pairs list              the paired Amigas (ID, date, first address)
    pairs revoke <ID>       that Amiga is asked for the password again
    pairs add               a key for an Amiga you pair by hand: prints the line to put in
                            ENVARC:Claude/nas-key (no wire exposure at all; the careful case)

The server needs a new `uptelnetd.py` for this: copy tools/uptelnetd.py again with the other
files, raise the image tag and Build. A plain password login still works for an Amiga that is
not paired (older uptelnet, no key file, or `Claude` without KEY).

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
(claude-amiga:5 -> 6), then Project > Action > Build. With the same tag, Build
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

## Give Claude control of the Amiga (wasabi)

With this, the Claude Code in the container can act on the Amiga you talk to
it from: run AmigaDOS commands, read and write files, list drawers, see the
screen, click and type. It uses wasabi (https://github.com/creep-ltx/wasabi, MIT):
wasabid on the Amiga and an MCP server (`wasabi_mcp.py`, pure Python 3 standard
library) beside Claude Code. wasabid runs whatever it is sent, as you. It answers only
clients with the key and only from private addresses; the link is encrypted only when
wasabid has AmiSSL and keys, otherwise it is plain. Keep the key to yourself, keep
the Amiga on a LAN you trust, never forward the port (1234).

1. On the Amiga: tick wasabid in the UP-Term kit's Install (it puts wasabid in
   C:, makes a key in ENVARC:wasabi.key, shows it, and starts it at every boot).
   Note the Amiga's IP address and the key.

2. On the NAS: rebuild the image (Build section below: copy this whole drawer,
   including `skills/`, `mcp.json`, `wasabi.rev`, `amiga-target` and `seed-claude-files`,
   raise the `image:` tag, Project > Action > Build). The image then holds wasabi's
   client and MCP server in `/opt/wasabi` (cloned at the commit in `wasabi.rev`, its LICENSE
   beside it) and the entrypoint seeds the skill (`~/.claude/skills/amiga-upterm`,
   which teaches Claude AmigaDOS, the UP-Term layout and the safety rules) and the MCP
   config (`~/work/.mcp.json`) into the volume.

   Then, in Container Manager > Container > claude-amiga > Action > Open terminal >
   Create > Launch with command `amiga-target`, give the Amiga's IP address, the
   wasabid port (Return = 1234) and the KEY (not echoed). They go into
   `home/.config/wasabi/config` (0600), the file wasabi reads itself, in neither the
   compose file nor any chat; other lines in it are kept. `amiga-target show`
   prints the address and port (never the key); `amiga-target clear` removes them.
   Coming from the amimcp image: run `amiga-target` again (the old amiagent token does not
   open wasabid); the old `home/.config/amimcp/` can be deleted.

   Seeding rule: a missing file is copied. A file that is still the copy we seeded
   (its sha256 matches `~/.config/wasabi/seeded/<name>`) is replaced when a newer
   image brings a new one. A file you edited is never touched (the log says "kept");
   delete it to get the image's copy back at the next start. The records of the amimcp
   image (`~/.config/amimcp/seeded`) are carried over, so its `.mcp.json` is replaced too.

3. Start `claude` from UP-Term (C:Claude, and pick the NAS). Claude asks once to
   approve the project's `amiga` MCP server: approve it. Check: ask "what
   volumes does my Amiga have?" -- it runs a command on the Amiga and answers.

The Mac's `amiga` MCP server (the rig or a real Amiga) is `.mcp.json` with
`tools/rig/wasabimcp.py`, after `make wasabi`; see tools/rig/README.md.

Not covered here: wasabid on the Amiga is the only thing that makes the link
that Claude controls the machine; without step 1 above the container's Claude
Code is just a terminal session. The rig (tools/rig/README.md) uses the same
wasabid, with key `rigwasabi`, on an emulated Amiga: never reuse that key
on a real one.

## ssh to the NAS (for setting this up from another computer)

DSM: Control Panel > Terminal & SNMP > Enable SSH service (any port); Control
Panel > User & Group > Advanced > Enable user home service (key login needs a
home); on DSM 7 only members of `administrators` may log in by ssh. Then, in a
normal terminal (it asks for the password, so not from a tool without a TTY):

    ssh-copy-id -p <port> -i ~/.ssh/id_ed25519.pub <user>@<nas>

Synology refuses keys unless the home is not group-writable:
`chmod 755 ~; chmod 700 ~/.ssh; chmod 600 ~/.ssh/authorized_keys`.
