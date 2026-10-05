# Claude Code for the Amiga, on a Synology NAS (or any Linux box with Docker):
# Claude Code in tmux, reached from UP-Term's uptelnet through uptelnetd
# (LAN only, password, UNENCRYPTED). See README.md beside this file.
FROM node:22-bookworm-slim

RUN apt-get update \
 && apt-get install -y --no-install-recommends python3 tmux git ca-certificates less \
 && rm -rf /var/lib/apt/lists/* \
 && useradd -m -s /bin/bash claude \
 && printf 'export NPM_CONFIG_PREFIX=/home/claude/.npm-global\nexport PATH=/home/claude/.npm-global/bin:$PATH\n' \
    > /etc/profile.d/claude-code.sh

# Claude Code itself lives on the volume, owned by the claude user, so its
# auto-updater can write there and an update survives a rebuild. The
# entrypoint installs it on the first start.
ENV NPM_CONFIG_PREFIX=/home/claude/.npm-global PATH=/home/claude/.npm-global/bin:$PATH

COPY uptelnetd.py /usr/local/bin/uptelnetd.py
COPY entrypoint.sh /usr/local/bin/entrypoint.sh
COPY setpw /usr/local/bin/setpw
RUN chmod 755 /usr/local/bin/entrypoint.sh /usr/local/bin/setpw

WORKDIR /home/claude/work
ENV LANG=C.UTF-8 SHELL=/bin/bash

# NAS_IP: the NAS's LAN address (host networking: the container listens on it).
# ALLOW: the LAN that may connect. The password: `setpw` in the container's
# terminal (kept 0600 on the volume). Claude Code logs in with /login (the
# Claude subscription); no API key anywhere.
ENTRYPOINT ["/usr/local/bin/entrypoint.sh"]
