#!/bin/sh
# Probe: an install holding up-hexedit 1.0; feed now has up-hexed $2 (Replaces/Conflicts/Provides up-hexedit).
# What do `upkg upgrade` and `upkg install up-hexed` do?
#   sh rename_probe.sh UPKG NEWVERSION CHECKDIR
set -u
upkg=$1; nv=$2; here=$3
w=$(mktemp -d "${TMPDIR:-/tmp}/renprobe.XXXXXX")
trap 'rm -rf "$w"' EXIT
mkfeed() { # $1 = old|new
  rm -rf "$w/feed"; mkdir "$w/feed"
  python3 -I - "$w" "$here" "$1" "$nv" <<'PY'
import sys
w, here, which, nv = sys.argv[1:5]; sys.path.insert(0, here)
from ipk import ipk, stanza, write_index
if which == 'old':
    name = 'up-hexedit_1.0_m68k-amigaos.ipk'
    ctl = b'Package: up-hexedit\nVersion: 1.0\nArchitecture: m68k-amigaos\nMaintainer: t\nDescription: old\n'
    data = [('./old-marker', b'old\n')]
else:
    name = 'up-hexed_%s_m68k-amigaos.ipk' % nv
    ctl = ('Package: up-hexed\nVersion: %s\nReplaces: up-hexedit\nConflicts: up-hexedit\nProvides: up-hexedit\n'
           'Architecture: m68k-amigaos\nMaintainer: t\nDescription: new\n' % nv).encode()
    data = [('./new-marker', b'new\n')]
pkg = ipk([('./control', ctl)], data)
open('%s/feed/%s' % (w, name), 'wb').write(pkg)
write_index('%s/feed/Packages.gz' % w, stanza(ctl, name, pkg))
PY
}
{ echo "arch m68k-amigaos 10"; echo "src/gz test file://$w/feed"; echo "dest root /"
  for o in lists_dir lock_file info_dir status_file cache_dir; do echo "option $o $w/c.$o"; done; } > "$w/c.conf"
U() { "$upkg" -f "$w/c.conf" -o "$w/root" "$@"; }
mkfeed old; U update >/dev/null 2>&1; U install up-hexedit >/dev/null 2>&1 || echo "old install failed"
echo "== installed before: $(U list-installed 2>/dev/null | tr '\n' ';')"
mkfeed new; U update >/dev/null 2>&1
echo "== list-upgradable: $(U list-upgradable 2>&1 | tr '\n' ';')"
U upgrade > "$w/up.out" 2>&1; echo "== upgrade rc=$?: $(tr '\n' ';' < "$w/up.out" | cut -c1-300)"
echo "== installed after upgrade: $(U list-installed 2>/dev/null | tr '\n' ';')"
U install up-hexed > "$w/in.out" 2>&1; echo "== install up-hexed rc=$?: $(tr '\n' ';' < "$w/in.out" | cut -c1-300)"
echo "== installed after install: $(U list-installed 2>/dev/null | tr '\n' ';')"
echo "== files: $(cd "$w/root" && ls *marker 2>/dev/null | tr '\n' ' ')"
