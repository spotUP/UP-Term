#!/usr/bin/env python3
"""uptelnet_rig.py -- C:uptelnet and vshrc's telnet() on the rig against tools/uptelnetd.py on this Mac (5.2 telnet,
the rig half of A1.4; self-checking).

The Mac end runs here for the run only: loopback, a throwaway password and clients file in a temp drawer, the
command `echo READY; stty size; while read l; do echo "got:$l"; stty size; done`. FS-UAE's bsdsocket emulation
connects from the Mac itself, so 127.0.0.1 on the Amiga is this Mac.

Two sessions, each one pass over the same checks:
  amigados   an AmigaShell window: `C:uptelnet 127.0.0.1 <port> NOAGENT KEY RAM:upt-a.key`
  vsh        a vsh window with the kit's dist/vshrc as ENV:vsh/vshrc: `telnet 127.0.0.1 <port> NOAGENT KEY RAM:upt-v.key`
             (telnet() adds `TERM xterm-256color` itself)
Checks per session: the password is asked for and is not echoed (the text never holds it); READY and a stty size
equal to the window's own (ixwinch); a typed line comes back as got:...; after the window is dragged larger the
next line reports the larger size (resize follows); Ctrl-] ends the session and the shell prompt is back.

Usage: UPTERM_RIG=<n> python3 -I tools/rig/uptelnet_rig.py [amigados|vsh]
Rig up (RTG screen, rig.py install done, `make amiga build/amiga/ixwinch build/amiga/uptelnet build/amiga/snipprobe`).
build/amiga/uptelnet is copied over C:uptelnet of the rig for the run (the kit's copy may be older)."""
import os, re, secrets, shutil, subprocess, sys, tempfile, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami, paths, typewin, vshwin, winch_rig
import condev_rig as c

ROOT = paths.VTCON
PORT = 23000 + os.getpid() % 900
PW = secrets.token_hex(6)
failed = 0
CMD = 'echo READY; stty size; while read l; do echo "got:$l"; stty size; done'


def check(ok, what, seen=''):
    global failed
    failed += not ok
    print('%s %s%s' % ('ok  ' if ok else 'FAIL', what, '' if ok else ': ' + str(seen).strip()[-300:]), flush=True)
    return ok


def until(cond, secs):
    end = time.time() + secs
    while time.time() < end:
        v = cond()
        if v:
            return v
        time.sleep(2)
    return cond()


def start_daemon(tmp):
    pwf = os.path.join(tmp, 'password')
    fd = os.open(pwf, os.O_WRONLY | os.O_CREAT, 0o600)
    os.write(fd, (PW + '\n').encode())
    os.close(fd)
    log = open(os.path.join(tmp, 'daemon.log'), 'w')
    p = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools/uptelnetd.py'), '--bind', '127.0.0.1', '--port', str(PORT),
                          '--password-file', pwf, '--clients-file', os.path.join(tmp, 'clients'), '--command', CMD],
                         stdout=log, stderr=subprocess.STDOUT)
    time.sleep(2)
    return p


def window_size(title):
    """(columns, rows) of the window as ixwinch reads it, typed in the window itself."""
    typewin.typ('VTC:ixwinch 1 >RAM:upt-size')
    typewin.key(typewin.RET, wait=4)
    out = c.run('Type RAM:upt-size')[1]
    m = re.search(r'ready (\d+)x(\d+)', out)
    return (int(m.group(1)), int(m.group(2))) if m else None


def remote_sizes(text):
    return [(int(r), int(col)) for r, col in re.findall(r'^(\d+) (\d+)\s*$', text, re.M)]   # stty size: rows columns


def session(kind):
    title = 'upt-' + kind
    if kind == 'amigados':
        typewin.open_xcon(title, '0/12/700/400')
        launch = 'C:uptelnet 127.0.0.1 %d NOAGENT KEY RAM:upt-a.key' % PORT
    else:
        vshwin.open_vsh(title, '0/12/700/400', wait=7)
        typewin._last_title = title
        typewin._last_clip = typewin._clip('RAM:clip-pre.txt')
        typewin.click_in(title)
        time.sleep(1)
        launch = 'telnet 127.0.0.1 %d NOAGENT KEY RAM:upt-v.key' % PORT
    win = window_size(title)
    check(win is not None, '%s: the window\'s own size is read (ixwinch)' % kind, win)
    typewin.typ(launch)
    typewin.key(typewin.RET, wait=1)
    ok = until(lambda: 'assword' in typewin.screen_text(), 40)
    check(ok, '%s: the password is asked for' % kind)
    typewin.typ(PW)
    after_typing = typewin.screen_text()
    check(PW not in after_typing, '%s: the password is not echoed' % kind, after_typing[-200:])
    typewin.key(typewin.RET, wait=1)
    seen = []
    ok = until(lambda: 'READY' in (seen.append(typewin.screen_text()) or seen[-1]), 40)
    text = seen[-1]
    check(ok, '%s: logged in, the command ran (READY)' % kind, text[-300:])
    first = (remote_sizes(text) or [None])[-1]
    check(first is not None and win is not None and first == (win[1], win[0]),
          '%s: stty size on the Mac equals the window (rows, columns)' % kind, 'window %s remote %s' % (win, first))
    typewin.typ('hello-%s' % kind)
    typewin.key(typewin.RET, wait=3)
    text = typewin.screen_text()
    check('got:hello-%s' % kind in text, '%s: a typed line comes back through the Mac' % kind, text[-200:])
    x, y, ww, hh = ami.window(title)['sys:size']
    winch_rig.drag((x + ww // 2, y + hh // 2), 60, 40)
    time.sleep(3)
    typewin.typ('again-%s' % kind)
    typewin.key(typewin.RET, wait=3)
    text = typewin.screen_text()
    sizes = remote_sizes(text)
    check(len(sizes) >= 2 and sizes[-1][0] >= sizes[0][0] and sizes[-1][1] > sizes[0][1] and 'got:again-%s' % kind in text,
          '%s: after the window grew the Mac end reports the larger size' % kind, sizes)
    typewin.key(0x1B, typewin.CTRL, wait=3)      # Ctrl-] (raw key 0x1B is the right bracket)
    text = typewin.screen_text()
    check(until(lambda: 'got:again-%s' % kind in typewin.screen_text() and not c.run('Status FULL')[1].count('uptelnet'), 20),
          '%s: Ctrl-] ends uptelnet (no uptelnet process left)' % kind, c.run('Status FULL')[1])
    typewin.typ('exit' if kind == 'vsh' else 'EndCLI')
    typewin.key(typewin.RET, wait=2)
    typewin._last_title = None


def main(which):
    tmp = tempfile.mkdtemp(prefix='uptrig')
    daemon = start_daemon(tmp)
    try:
        shutil.copyfile(ROOT / 'build/amiga/ixwinch', paths.RIG / 'vtc/ixwinch')
        ami.put(str(ROOT / 'build/amiga/uptelnet'), 'C:uptelnet')
        c.run('Makedir ENV:vsh')
        ami.put(str(ROOT / 'dist/vshrc'), 'ENV:vsh/vshrc')
        c.run('Delete RAM:upt-a.key RAM:upt-v.key QUIET')
        for kind in ('amigados', 'vsh'):
            if which in (None, kind):
                session(kind)
    finally:
        daemon.terminate()
        c.run('Delete ENV:vsh/vshrc QUIET')
        print('     daemon log tail: %s' % open(os.path.join(tmp, 'daemon.log')).read()[-300:].replace('\n', ' | '))
        shutil.rmtree(tmp, ignore_errors=True)
    print('uptelnet_rig: %s' % ('FAIL' if failed else 'PASS'))
    sys.exit(1 if failed else 0)


if __name__ == '__main__':
    main(sys.argv[1] if len(sys.argv) > 1 else None)
