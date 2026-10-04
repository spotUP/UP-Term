# Claude Code for the Amiga, on a Synology NAS (or any Linux box with Docker):
# Claude Code in tmux, reached from UP-Term's uptelnet through uptelnetd
# (LAN only, password, UNENCRYPTED). See README.md beside this file.
FROM node:22-bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 tmux git ca-certificates less \
 && rm -rf /var/lib/apt/lists/* \
 && npm install -g @anthropic-ai/claude-code \
 && useradd -m -s /bin/bash claude

COPY uptelnetd.py /usr/local/bin/uptelnetd.py

USER claude
WORKDIR /home/claude/work
ENV LANG=C.UTF-8 SHELL=/bin/bash

# NAS_IP: the NAS's LAN address (host networking: the container listens on it).
# ALLOW: the LAN that may connect, e.g. 192.168.0.0/22.
# The password: /home/claude/.config/uptelnetd/password (0600), on the volume.
CMD ["sh", "-c", "exec python3 /usr/local/bin/uptelnetd.py --bind \"$NAS_IP\" --allow \"$ALLOW\" --port \"${PORT:-2323}\" --command 'tmux new -A -s claude claude'"]
