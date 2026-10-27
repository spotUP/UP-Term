#!/usr/bin/env python3
"""Every Amiga-native command of the kit answers a lone `?` with its template
or usage and ends with return code 0 (owner rule 2026-10-14, ledger W-A3): no
ReadArgs prompt waiting for the arguments. The one helper is tty/qmark.h.

The command set comes from the Makefile, not from a grep of the sources: the
prerequisites of the `amiga:` target plus every build/amiga/<name> the dist
targets copy into an archive. Each name must be a command (its source includes
qmark.h and calls the helper, before its first ReadArgs) or be listed below as
excluded with its reason; a new program in the Makefile fails here until it is
classified, so a command cannot slip by.

Source-level on purpose: a link-level check would need the 68k binaries (and
the helper is static, inlined), and the call's behaviour is the host suite
`make test ONLY=qmark`; this pins that each command's main reaches the call."""
import os, re, sys

ROOT = os.path.dirname(os.path.dirname(os.path.abspath(__file__)))
fails = 0

def check(ok, what):
    global fails
    print('%s %s' % ('[OK]' if ok else '[FAIL]', what))
    if not ok:
        fails += 1

# command -> (source holding its main, how it is called)
COMMANDS = {
    'UPConsole': 'device/upconsole.c',
    'UPDemo': 'demo/updemo_amiga.c',
    'Reboot': 'install/reboot.c',
    'UPBigVol': 'install/bigvol.c',
    'UPTerm': 'handler/upterm.c',
    'upprefs': 'prefs/upprefs.c',
    'upicon': 'install/upicon.c',
    'upupdate': 'install/upupdate.c',
    'upsame': 'install/upsame.c',
    'upgetty': 'device/upgetty.c',
    'uptelnet': 'net/uptelnet.c',
    'sz': 'zm/zm_amiga.c',
    'rz': 'zm/zm_amiga.c',
    'vsh': 'shell/vsh.c',
    'ixkill': 'shell/ixkill.c',
    'hl': 'view/hl_main.c',
    'mdv': 'view/md_main.c',
    'ps': 'proc/ps_amiga.c',
    'top': 'proc/top_amiga.c',
    'Claude': 'claude/main_amiga.c',
    'Amarchy': 'amarchy/amarchy.c',
    'amarchy-menu': 'amarchy/menu.c',
    'amarchy-gui': 'amarchy/gui.c',
    'amarchy-index': 'amarchy/index.c',
}
# not commands: handlers, devices, objects, and probes / test tools
EXCLUDED = {
    'vtengine-$(CPU).o': 'object file, not a program',
    'keymap.o': 'object file (the dist-check alloc probe)',
    'vtcon-handler': 'handler (mounted, takes no command line)',
    'pty-handler': 'handler',
    'ixpipe-handler': 'handler',
    'up-console.device': 'device',
    'reach': 'probe (rig reachability)',
    'vtshow': 'probe / test tool',
    'winbox': 'probe',
    'sizewatch': 'probe',
    'breakport': 'probe',
    'ttyprobe': 'probe',
    'dsrtime': 'probe',
    'dripens': 'probe',
    'tcpprobe': 'install probe (not a user command)',
    'wasabikey': 'test tool for the Wasabi board (not a user command)',
    'cellbench': 'benchmark',
    'wprobe': 'probe',
    'phaseprobe': 'probe',
    'engbench': 'benchmark',
    'ptytest': 'rig test tool',
}

mk = open(os.path.join(ROOT, 'Makefile')).read()
NAME = r'\$\(BUILD\)/amiga/([A-Za-z0-9_.\-$()]+)'

m = re.search(r'^amiga:(.*)$', mk, re.M)
check(m is not None, 'Makefile has the amiga: target')
names = re.findall(NAME, m.group(1))
check(len(names) > 20, 'amiga: prerequisites read (%d names)' % len(names))

# what the dist targets copy into an archive
for blk in re.finditer(r'^dist[A-Za-z0-9_\-]*:.*\n((?:\t.*\n|#.*\n)*)', mk, re.M):
    for n in re.findall(NAME, blk.group(1).replace('\\\n', ' ')):
        if n not in names:
            names.append(n)

for n in names:
    if n in COMMANDS:
        continue
    check(n in EXCLUDED, '%s is classified (command or excluded)' % n)
for n in EXCLUDED:
    check(n in names, 'excluded %s is still a Makefile program' % n)

for n in sorted(COMMANDS):
    check(n in names, '%s is built by the Makefile' % n)
    src = open(os.path.join(ROOT, COMMANDS[n])).read()
    check('#include "../tty/qmark.h"' in src, '%s: %s includes tty/qmark.h' % (n, COMMANDS[n]))
    call = re.search(r'\bqmark_(line|argv)\(', src[src.find('main('):] if 'main(' in src else '')
    check(call is not None, '%s: its main calls the helper' % n)
    if 'ReadArgs(' in src and call:
        main_at = src.find('int main(')
        check(main_at >= 0 and src.find('qmark_', main_at) < src.find('ReadArgs(', main_at),
              '%s: the helper runs before ReadArgs (which would prompt)' % n)

print('%d commands, %d excluded, %d failures' % (len(COMMANDS), len(EXCLUDED), fails))
sys.exit(1 if fails else 0)
