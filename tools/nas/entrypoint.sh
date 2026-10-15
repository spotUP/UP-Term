#!/bin/sh
# Runs as root: the volume Container Manager made is root's; give the claude
# user its home, then run uptelnetd as that user. Without a login (a password, or
# a file of paired Amigas) it waits (no restart loop) and says how to set one.
set -e
mkdir -p /home/claude/.config/uptelnetd /home/claude/work
chown -R claude:claude /home/claude
# replaces the shell with "$@" run as the claude user (in a subshell: returns)
as_claude() { exec as-claude "$@"; }
# the image's skill and MCP config into the home, as the claude user: root would
# follow symlinks the user can plant in it (~/work/.mcp.json, the seeded records)
(as_claude seed-claude-files)
# where the amiga MCP server finds its Amiga: the relay below, with a key made once
(as_claude relay-config)
# Claude Code on the volume (its auto-updater writes there): installed once
if [ ! -x "$NPM_CONFIG_PREFIX/bin/claude" ]; then
    echo "claude-amiga: installing Claude Code into $NPM_CONFIG_PREFIX"
    (as_claude npm install -g @anthropic-ai/claude-code)
fi
PW=/home/claude/.config/uptelnetd/password
CLIENTS=/home/claude/.config/uptelnetd/clients
while [ ! -s "$PW" ] && [ ! -s "$CLIENTS" ] && [ -z "$UPTELNETD_PASSWORD" ]; do
    echo "claude-amiga: no login yet. In Container Manager > claude-amiga > Terminal, run: setpw (a password), or: pairs add (a key for one Amiga)"
    sleep 30
done
# NAS_IP and ALLOW may each hold several, space-separated: the LAN address
# and 127.0.0.1, where Synology's Tailscale (userspace networking) hands over
# the connections that reach the NAS's tailnet address
case "$NAS_IP $ALLOW" in
*EDIT-ME*)
    echo "claude-amiga: NAS_IP and ALLOW in compose.yaml are still the EDIT-ME placeholders."
    echo "claude-amiga: set NAS_IP to the NAS's LAN address and ALLOW to your LAN (like 192.168.1.0/24), then start again."
    exit 1 ;;
esac
set --
for b in $NAS_IP; do set -- "$@" --bind "$b"; done
for n in $ALLOW; do set -- "$@" --allow "$n"; done
# the relay: wasabi_mcp.py (Claude Code's amiga tools) talks to 127.0.0.1:1234, uptelnetd passes it
# to the Amiga that logged in by key (its session wasabid); the key is the config's key line
as_claude python3 /usr/local/bin/uptelnetd.py "$@" --port "${PORT:-2323}" \
    --agent-relay 127.0.0.1:1234 --relay-key-file /home/claude/.config/wasabi/config \
    --wasabi-client /opt/wasabi/wasabi \
    --command 'cd ~/work && tmux new -A -s claude claude'
