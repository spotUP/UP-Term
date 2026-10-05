# Claude Code on the NAS, for the Amiga

Claude Code runs in a container on the NAS, inside tmux, so a session survives the
Amiga disconnecting. The Amiga reaches it with UP-Term's `uptelnet`:

    uptelnet <NAS LAN address> 2323

UNENCRYPTED telnet: LAN only, never forward the port on the router.

## Set up (Synology DSM 7, Container Manager)

1. Copy this drawer (Dockerfile, compose.yaml, README.md) and `tools/uptelnetd.py`
   into one folder on the NAS, e.g. `/volume1/docker/claude-amiga/build`.
2. Create `/volume1/docker/claude-amiga/home` and inside it
   `.config/uptelnetd/password` holding the login password (8+ characters), mode 0600,
   owned by uid 1000 (the container's `claude` user): over SSH on the NAS
   `sudo chown -R 1000:1000 /volume1/docker/claude-amiga/home`.
3. Container Manager > Project > Create, source: the build folder; edit `NAS_IP` and
   `ALLOW` in compose.yaml; build and start.
4. Log Claude Code in once WITH YOUR CLAUDE SUBSCRIPTION (Pro/Max), not an API key:
   Container Manager > the container > Terminal (or
   `sudo docker exec -it claude-amiga tmux new -A -s claude claude`), run `/login`,
   choose the Claude account (subscription) option, open the printed URL in a browser,
   sign in, paste the code back. The login is kept on the volume. Never set
   ANTHROPIC_API_KEY in the container: Claude Code would use it instead of the
   subscription. `/status` shows which one is in use.
5. On the Amiga: `uptelnet <NAS_IP> 2323`, the password, and you are in Claude Code.
   Ctrl+] leaves; Claude keeps running in tmux for the next connection.

Model, settings and CLAUDE.md live in `/volume1/docker/claude-amiga/home/.claude`.
Work files: `/home/claude/work` in the container (add a volume for a NAS share if
Claude should edit files there).

Status: written 2026-10-05, not yet built on the NAS (the owner logs in with the
session on 2026-10-06). Needs an x86-64 or ARM64 Synology with Container Manager.
