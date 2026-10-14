# Claude Code for the Amiga, on a Synology NAS (or any Linux box with Docker):
# Claude Code in tmux, reached from UP-Term's uptelnet through uptelnetd
# (LAN only, password or a key per Amiga, UNENCRYPTED). See README.md beside this file.
FROM node:22-bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 tmux git ca-certificates less \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -m -s /bin/bash claude \
 && printf 'export NPM_CONFIG_PREFIX=/home/claude/.npm-global\nexport PATH=/home/claude/.npm-global/bin:$PATH\n' \
    > /etc/profile.d/claude-code.sh \
 && printf 'set -g mouse on\nset -g focus-events on\n' > /etc/tmux.conf

# tmux as Claude Code asks for it: mouse on (the wheel scrolls; in UP-Term
# Shift+drag still selects text) and focus events passed through.

# Claude Code itself lives on the volume, owned by the claude user, so its
# auto-updater can write there and an update survives a rebuild. The
# entrypoint installs it on the first start.
ENV NPM_CONFIG_PREFIX=/home/claude/.npm-global PATH=/home/claude/.npm-global/bin:$PATH

COPY uptelnetd.py /usr/local/bin/uptelnetd.py
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY as-claude /usr/local/bin/as-claude
COPY setpw /usr/local/bin/setpw
COPY pairs /usr/local/bin/pairs
COPY amiga-target /usr/local/bin/amiga-target
COPY seed-claude-files /usr/local/bin/seed-claude-files
RUN chmod 755 /usr/local/bin/entrypoint.sh /usr/local/bin/as-claude /usr/local/bin/setpw /usr/local/bin/pairs \
    /usr/local/bin/amiga-target /usr/local/bin/seed-claude-files

# wasabi (MIT, Python standard library only), at the one commit in wasabi.rev (the Makefile's
# wasabi target reads the same file). Its client `wasabi`, the MCP server wasabi_mcp.py and the
# LICENSE live in /opt/wasabi; the skill and mcp.json are seeded into the claude user's home
# by the entrypoint (seed-claude-files). Host, port and key come from ~/.config/wasabi/config
# (amiga-target writes them).
COPY wasabi.rev /opt/wasabi/wasabi.rev
RUN git clone -q https://github.com/creep-ltx/wasabi /tmp/wasabi \
 && git -C /tmp/wasabi checkout -q "$(cat /opt/wasabi/wasabi.rev)" \
 && cp /tmp/wasabi/wasabi /tmp/wasabi/wasabi_mcp.py /tmp/wasabi/LICENSE /opt/wasabi/ \
 && rm -rf /tmp/wasabi
COPY skills /opt/wasabi/seed/skills
COPY mcp.json /opt/wasabi/seed/mcp.json

WORKDIR /home/claude/work
ENV LANG=C.UTF-8 SHELL=/bin/bash

# NAS_IP: the NAS's LAN address (host networking: the container listens on it).
# ALLOW: the LAN that may connect. The password: `setpw` in the container's
# terminal (kept 0600 on the volume); `pairs` lists, adds and revokes the Amigas
# that log in with a key instead (the Amiga pairs itself over its first password login). Claude Code logs in with /login (the
# Claude subscription); no API key anywhere.
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
