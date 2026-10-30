"""Ctrl-C stops a Copy running in an UP-Term tab (CTRLC-COPY, owner A1200 2026-10-23).
Run (UPTERM_RIG=<n>) with the rig up and the kit installed (UP-Term:bin/vsh, L:vtcon-handler).
A titled XCON window, UP-Term > New tab (vsh, the tab's shell), `Copy C: TO RAM:ccx`, Ctrl-C 0.2 s after
Return (the whole of C: copies in under a second at rig speed; `#` typed through ami.req comes out as the
keymap's pound sign, so the pattern form is not typed). PASS = the tab shows "***Break" and RAM:ccx holds far fewer files than C: (a stock
Shell's Copy checks CTRL_C between files). RAM:ccx is deleted afterwards. Exit status 1 on FAIL."""
import os, subprocess, sys, time
RIG = os.path.join(os.path.dirname(os.path.abspath(__file__)), "..")
sys.path.insert(0, RIG)
import ami, typewin
import condev_rig as c

WASABI = os.path.join(RIG, "..", "..", "build", "wasabi", "wasabi")
TITLE = 'ctrlc-copy'
bad = 0


def check(ok, what, seen=''):
    global bad
    print(("ok   " if ok else "FAIL ") + what + ('' if ok else ': ' + str(seen).strip()[-300:]), flush=True)
    bad += 0 if ok else 1


def menu(path):
    n = int(os.environ.get('UPTERM_RIG', '1') or 1)
    subprocess.run([sys.executable, WASABI, '--host', '127.0.0.1', '--port', str(1233 + n), '--key', 'rigwasabi',
                    'ui', 'menu', path], check=True, capture_output=True)


def files(d):
    rc, out = c.run('List %s FILES QUICK NOHEAD' % d)
    return len([l for l in out.splitlines() if l.strip() and 'file' not in l]) if rc == 0 else 0


c.run('Delete >NIL: RAM:ccx ALL QUIET')
total = files('C:')
typewin.open_xcon(TITLE)
menu('UP-Term/New tab')
time.sleep(5)
typewin._last_title = None          # the tab retitles the window; it stays the active one
typewin.typ('Copy C: TO RAM:ccx')
typewin.key(typewin.RET, wait=0)
time.sleep(0.2)
typewin.key(0x33, typewin.CTRL, wait=0)      # Ctrl-C
time.sleep(8)
copied = files('RAM:ccx')
typewin.typ('UPTerm screen FILE RAM:ccx.txt')   # typed in the tab: ACTIVE from outside reads the host tab
typewin.key(typewin.RET, wait=3)
text = c.run('Type RAM:ccx.txt')[1]
print('C: %d files, RAM:ccx %d files' % (total, copied))
print('tab text (tail): %r' % text.strip()[-240:])
check('***Break' in text, 'the tab shows ***Break', text[-240:])
check(0 < copied < total // 2, 'Copy stopped early (%d of %d files)' % (copied, total))
typewin.typ('exit')
typewin.key(typewin.RET, wait=2)
typewin.typ('EndCLI')
typewin.key(typewin.RET, wait=2)
c.run('Delete >NIL: RAM:ccx RAM:ccx.txt ALL QUIET')
print('ctrlc_copy: %s' % ('FAIL' if bad else 'PASS'))
sys.exit(1 if bad else 0)
