#!/usr/bin/env python3
"""bebbossh follows the vtcon window (FP4.5, patches 0020..0022). A throwaway sshd on the Mac (port 2223, loopback, own keys
in a temp dir) gives a session whose pty size and received text are written to files by the remote. Asserts:
  1. `stty size` at the start equals the rows and columns of the window (box fitted to 8x16 cells minus the frame),
  2. after the window is dragged larger, `stty size` on the server shows the new, larger size,
  3. printf of UTF-8 e-acute arrives as the two bytes C3 A9 on screen, not as a replacement character.
Run: UPTERM_RIG=3 python3 tools/rig/bebbossh_winsize_rig.py   (BB=<bebbossh binary> to override build/bebbossh/out/bebbossh)."""
import os, pathlib, shutil, subprocess, sys, tempfile, time
HERE = pathlib.Path(__file__).resolve().parent
ROOT = HERE.parent.parent
sys.path.insert(0, str(HERE))
import paths, ami, vshwin, winch_rig, screen_rig, typewin, ixpty_rig  # noqa: E402
from install_rig import run  # noqa: E402

PORT = 2223
VTC = paths.RIG / 'vtc'
NET = ROOT / 'build/dist/up-term/Files/net/bebbossh-1.46.1'
BIN = os.environ.get('BB', str(ROOT / 'build/bebbossh/out/bebbossh'))
fails = []


def ck(ok, what, detail=''):
    print(('[OK] ' if ok else '[FAIL] ') + what + ('' if ok else ' :: %s' % (detail,)))
    if not ok:
        fails.append(what)


def wait_file(f, secs):
    for _ in range(secs):
        if f.exists() and f.stat().st_size:
            return f.read_text().strip()
        time.sleep(1)
    return None


def main():
    d = pathlib.Path(tempfile.mkdtemp(prefix='fp45rig-'))
    try:
        for k in ('hk', 'ck'):
            subprocess.run(['ssh-keygen', '-q', '-t', 'ed25519', '-N', '', '-f', str(d / k)], check=True)
        (d / 'ak').write_text((d / 'ck.pub').read_text())
        (d / 'sshd.conf').write_text('Port %d\nListenAddress 127.0.0.1\nHostKey %s\nAuthorizedKeysFile %s\n'
                                     'PasswordAuthentication no\nStrictModes no\nUsePAM no\nPidFile %s\n'
                                     % (PORT, d / 'hk', d / 'ak', d / 'pid'))
        (d / 'probe.sh').write_text("stty size > %s/s1\nprintf 'caf\\303\\251\\n'\nsleep 40\nstty size > %s/s2\n" % (d, d))
        subprocess.run(['/usr/sbin/sshd', '-f', str(d / 'sshd.conf'), '-E', str(d / 'sshd.log')], check=True)
        shutil.copyfile(BIN, VTC / 'bebbossh')
        shutil.copyfile(NET / 'libcryptossh.library020', VTC / 'libcryptossh.library')
        shutil.copyfile(d / 'ck', VTC / 'ck.key')
        run('Copy VTC:libcryptossh.library LIBS:libcryptossh.library')
        title = 'fp45rig'
        vshwin.open_vsh(title, '0/20/600/200', wait=7)
        typewin._last_title = title
        shutil.copyfile(ROOT / 'build/amiga/snipprobe', VTC / 'snipprobe')
        ixpty_rig.use_ixemul()
        t = screen_rig.typeline
        run('Delete ENVARC:ssh/known_hosts ENVARC:.ssh/known_hosts QUIET')  # the host key is new every run
        t('export TERM=xterm-256color', 2)
        w = ami.window(title)
        t('VTC:bebbossh -i VTC:ck.key -p %d %s@127.0.0.1 "sh %s/probe.sh"' % (PORT, os.environ.get('USER', 'spot'), d), 8)
        for _ in range(90):
            if 'do you trust host' in typewin.screen_text():
                t('yes', 2)
                break
            time.sleep(1)
        s1 = wait_file(d / 's1', 200)
        ck(s1 is not None, 'a session started and wrote its size')
        if s1:
            box = ami.window(title)['box']
            rows, cols = (int(x) for x in s1.split())
            ck(abs(cols * 8 - box[2]) <= 40 and abs(rows * 16 - box[3]) <= 56,
               'size at start %s matches the window box %s (8x16 cells, frame allowance)' % (s1, box), (s1, box))
            x, y, ww, hh = ami.window(title)['sys:size']
            winch_rig.drag((x + ww // 2, y + hh // 2), 200, 150)
            s2 = wait_file(d / 's2', 80)
            r2, c2 = (int(x) for x in s2.split()) if s2 else (0, 0)
            ck(c2 > cols and r2 > rows, 'after the drag the server sees a larger size: %s -> %s' % (s1, s2), s2)
            ck('cafÃ©' in typewin.screen_text() or 'café' in typewin.screen_text(),
               'UTF-8 e-acute reaches the screen as C3 A9 (no replacement character)', typewin.screen_text()[:300])
        t('exit', 3)
    finally:
        pidf = d / 'pid'
        if pidf.exists():
            subprocess.run(['kill', pidf.read_text().strip()])
        shutil.rmtree(d, ignore_errors=True)
    print('FAILED: %s' % fails if fails else '[OK] bebbossh follows the vtcon window')
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
