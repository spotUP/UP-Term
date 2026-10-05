# Claude Code on the NAS, for the Amiga

Claude Code runs in a container on the NAS, inside tmux, so a session survives the
Amiga disconnecting. The Amiga reaches it with UP-Term's `uptelnet`:

    uptelnet <NAS LAN address> 2323

UNENCRYPTED telnet: LAN only, never forward the port on the router.

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

Claude Code itself is installed on the volume (`home/.npm-global`, owned by the
`claude` user) on the first start, so its auto-updater can write there and updates
survive a rebuild; delete that folder to force a fresh install.

Model, settings and CLAUDE.md live in `/volume1/docker/claude-amiga/home/.claude`.
Work files: `/home/claude/work` in the container (add a volume for a NAS share if
Claude should edit files there).

Status: built and running on the owner's DS218+ (DSM 7.2.2) 2026-10-05. Needs an
x86-64 or ARM64 Synology with Container Manager.
