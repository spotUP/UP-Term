#!/usr/bin/env python3
"""wget_rig.py -- wget() and tldr() of dist/vshrc on the rig (unix-tool-ports 5.3 and 5.9; self-checking).

The two functions are cut out of dist/vshrc and run by `VTC:vsh` with the kit's curl (the upterm-ports build, kit/userland/bin/curl, 060
build) and build/amiga/mdv against a Python http.server on this Mac: FS-UAE's bsdsocket emulation connects from the
Mac itself, so 127.0.0.1 on the Amiga is this Mac. No terminal window: each case is a vsh script writing to
VTC:out/wget/<case>.txt, compared with what the case must print.

Usage: UPTERM_RIG=<n> python3 -I tools/rig/wget_rig.py
Rig up (rig.py start --max, VTC: is build/rig/vtc); `make build/amiga/vsh build/amiga/mdv` first."""
import http.server, os, pathlib, re, shutil, sys, tempfile, threading
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, paths, userland_rig as u

ROOT = paths.VTCON
VTC = u.VTC
PORTS_CURL = paths.ports() / 'build/kit/userland/bin/curl'   # make kit-stage in upterm-ports
failed = 0


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok  ' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()[-300:]), flush=True)


def functions():
    src = (ROOT / 'dist/vshrc').read_text()
    return ''.join(re.search(r'^%s\(\) \{\n.*?^\}\n' % n, src, re.S | re.M).group(0) for n in ('wget', 'tldr'))


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


def main():
    www = pathlib.Path(tempfile.mkdtemp(prefix='wgetrig'))
    (www / 'pages/common').mkdir(parents=True)
    (www / 'hello.txt').write_text('hello net\n')
    (www / 'pages/common/tar.md').write_text('# tar\n\n> Archiving utility.\n\n- Create an archive:\n\n`tar cf {{target.tar}} {{file1}}`\n')
    srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), lambda *a, **k: Quiet(*a, directory=str(www), **k))
    url = 'http://127.0.0.1:%d' % srv.server_address[1]
    threading.Thread(target=srv.serve_forever, daemon=True).start()
    try:
        u.amiga_delete('wget', 'out/wget')
        (VTC / 'userland/bin').mkdir(parents=True, exist_ok=True)
        shutil.copyfile(PORTS_CURL, VTC / 'userland/bin/curl')
        shutil.copyfile(ROOT / 'build/amiga/mdv', VTC / 'userland/bin/mdv')
        (VTC / 'out/wget').mkdir(parents=True)
        (VTC / 'wget/work').mkdir(parents=True)
        u.kit_assigns()
        cases = {
            'save': ('wget -q %s/hello.txt && cat hello.txt' % url, 'hello net\n[exit 0]'),
            'named': ('wget -q -O named.txt %s/hello.txt && cat named.txt' % url, 'hello net\n[exit 0]'),
            'stdout': ('wget -O - %s/hello.txt' % url, 'hello net\n[exit 0]'),
            'missing': ('wget -q %s/nothing.txt; echo rc=$?' % url, 'rc=22\n[exit 0]'),
            'unknown': ('wget --mirror %s/hello.txt 2>&1; echo rc=$?' % url, 'wget: option --mirror is not supported (curl does the work)\nrc=2\n[exit 0]'),
            'tldr': ('TLDR_URL=%s/pages tldr tar' % url, None),
            'tldr-none': ('TLDR_URL=%s/pages tldr nosuchtool 2>&1; echo rc=$?' % url, 'tldr: no page for nosuchtool\nrc=1\n[exit 0]'),
        }
        for name, (cmd, _) in cases.items():
            (VTC / 'wget' / (name + '.sh')).write_text(
                'cd VTC:wget/work\nexport PATH=/VTC/userland/bin:$PATH\nexport TERM=xterm-256color\n'
                'unset SHLVL\n' + functions() + '{ ' + cmd + '\n}\necho "[exit $?]"\n', encoding='latin-1')
        for name, (cmd, want) in cases.items():
            rc, _ = u.run('VTC:vsh VTC:wget/%s.sh >VTC:out/wget/%s.txt' % (name, name), 90)
            got = (VTC / 'out/wget' / (name + '.txt')).read_text(encoding='latin-1', errors='replace') if (VTC / 'out/wget' / (name + '.txt')).exists() else ''
            got = re.sub(r'\x1b\[[0-9;]*m', '', got).replace('\r', '').strip()
            if want is None:
                check('Create an archive' in got and 'target.tar' in got and got.endswith('[exit 0]'), name, got)
            else:
                check(got == want.strip(), name, got)
    finally:
        srv.shutdown()
        shutil.rmtree(www, ignore_errors=True)
    print('wget_rig: %s' % ('FAIL' if failed else 'PASS'))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main()
