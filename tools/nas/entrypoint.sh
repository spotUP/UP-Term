#!/bin/sh
# Runs as root: the volume Container Manager made is root's; give the claude
# user its home, then run uptelnetd as that user. Without a password it waits
# (no restart loop) and says how to set one.
set -e
mkdir -p /home/claude/.config/uptelnetd /home/claude/work
chown -R claude:claude /home/claude
PW=/home/claude/.config/uptelnetd/password
while [ ! -s "$PW" ] && [ -z "$UPTELNETD_PASSWORD" ]; do
    echo "claude-amiga: no login password yet. In Container Manager > claude-amiga > Terminal, run: setpw"
    sleep 30
done
exec setpriv --reuid=claude --regid=claude --init-groups --reset-env \
    env HOME=/home/claude USER=claude LOGNAME=claude SHELL=/bin/bash LANG=C.UTF-8 PATH="$PATH" \
    python3 /usr/local/bin/uptelnetd.py --bind "$NAS_IP" --allow "$ALLOW" --port "${PORT:-2323}" \
    --command 'cd ~/work && tmux new -A -s claude claude'
