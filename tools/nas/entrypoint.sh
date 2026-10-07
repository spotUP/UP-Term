#!/bin/sh
# Runs as root: the volume Container Manager made is root's; give the claude
# user its home, then run uptelnetd as that user. Without a password it waits
# (no restart loop) and says how to set one.
set -e
mkdir -p /home/claude/.config/uptelnetd /home/claude/work
chown -R claude:claude /home/claude
# replaces the shell with "$@" run as the claude user (in a subshell: returns)
as_claude() {
    exec setpriv --reuid=claude --regid=claude --init-groups --reset-env \
        env HOME=/home/claude USER=claude LOGNAME=claude SHELL=/bin/bash LANG=C.UTF-8 PATH="$PATH" \
        NPM_CONFIG_PREFIX="$NPM_CONFIG_PREFIX" "$@"
}
# Claude Code on the volume (its auto-updater writes there): installed once
if [ ! -x "$NPM_CONFIG_PREFIX/bin/claude" ]; then
    echo "claude-amiga: installing Claude Code into $NPM_CONFIG_PREFIX"
    (as_claude npm install -g @anthropic-ai/claude-code)
fi
PW=/home/claude/.config/uptelnetd/password
while [ ! -s "$PW" ] && [ -z "$UPTELNETD_PASSWORD" ]; do
    echo "claude-amiga: no login password yet. In Container Manager > claude-amiga > Terminal, run: setpw"
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
as_claude python3 /usr/local/bin/uptelnetd.py "$@" --port "${PORT:-2323}" \
    --command 'cd ~/work && tmux new -A -s claude claude'
