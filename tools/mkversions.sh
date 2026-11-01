#!/bin/sh
# Writes the kit's VERSIONS file to stdout: what the kit was built from.
#   line 1:  <archive> [<version>] <vtcon commit> <date>  (upterm <meta-repo commit>)
#            (UP-Term <vtcon commit> <date> without --package)
#   then one line per consumed part:  <name> <branch> <commit>[+dirty]
# A part with uncommitted changes to tracked files gets "+dirty" and one
# warning on stderr; untracked files do not count (same as the Makefile GITREV).
# Usage: tools/mkversions.sh [--package <archive> <version>] [UPTERM_ROOT [part ...]]
#   (UPTERM_ROOT default: the parent of vtcon; parts default: the single kit's list.
#   make dist passes the archive's repositories from dist/packages.txt, column consumes.)
here=$(cd "$(dirname "$0")/.." && pwd)
head=UP-Term
if [ "$1" = --package ]; then
    head="$2 $3"
    shift 3
fi
root=${1:-${UPTERM_ROOT:-$(dirname "$here")}}
[ $# -gt 0 ] && shift
parts="$*"
[ -n "$parts" ] || parts="vtcon amiga-pi ixemul-vtcon upterm-ports neovim-amiga tmux-amiga cpython-amiga screen-amiga/src"

rev() {  # rev <dir>: "<branch> <commit>[+dirty]" or "none unknown"
    d=$1
    if ! c=$(git -C "$d" rev-parse --short HEAD 2>/dev/null); then
        echo "none unknown"; return
    fi
    b=$(git -C "$d" rev-parse --abbrev-ref HEAD 2>/dev/null)
    if [ -n "$(git -C "$d" status --short --untracked-files=no 2>/dev/null)" ]; then
        c="$c+dirty"
        echo "mkversions: warning: $d has uncommitted changes" >&2
    fi
    echo "$b $c"
}

set -- $(rev "$here" 2>/dev/null)
vt=$2
meta=$(rev "$root/upterm" | cut -d' ' -f2)
echo "$head $vt $(date +%Y-%m-%d) (upterm $meta)"
for p in $parts; do
    if [ "$p" = vtcon ]; then d=$here; else d=$root/$p; fi
    echo "$p $(rev "$d")"
done
