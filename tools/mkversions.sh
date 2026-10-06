#!/bin/sh
# Writes the kit's VERSIONS file to stdout: what the kit was built from.
#   line 1:  UP-Term <vtcon commit> <date>  (upterm <meta-repo commit>)
#   then one line per consumed part:  <name> <branch> <commit>[+dirty]
# A part with uncommitted changes gets "+dirty" and one warning on stderr.
# Usage: tools/mkversions.sh [UPTERM_ROOT]   (default: the parent of vtcon)
here=$(cd "$(dirname "$0")/.." && pwd)
root=${1:-${UPTERM_ROOT:-$(dirname "$here")}}
parts="vtcon ixemul-vtcon upterm-ports neovim-amiga tmux-amiga cpython-amiga screen-amiga/src"

rev() {  # rev <dir>: "<branch> <commit>[+dirty]" or "none unknown"
    d=$1
    if ! c=$(git -C "$d" rev-parse --short HEAD 2>/dev/null); then
        echo "none unknown"; return
    fi
    b=$(git -C "$d" rev-parse --abbrev-ref HEAD 2>/dev/null)
    if [ -n "$(git -C "$d" status --short 2>/dev/null)" ]; then
        c="$c+dirty"
        echo "mkversions: warning: $d has uncommitted changes" >&2
    fi
    echo "$b $c"
}

set -- $(rev "$here" 2>/dev/null)
vt=$2
meta=$(rev "$root/upterm" | cut -d' ' -f2)
echo "UP-Term $vt $(date +%Y-%m-%d) (upterm $meta)"
for p in $parts; do
    if [ "$p" = vtcon ]; then d=$here; else d=$root/$p; fi
    echo "$p $(rev "$d")"
done
