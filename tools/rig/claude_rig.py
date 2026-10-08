#!/usr/bin/env python3
"""claude_rig.py -- C:Claude on the rig against tools/claude_fixture.py (no key,
no Anthropic): the checks the owner's A3/A4 steps describe, run the same way
every time, with screenshots in build/rig/shots/claude-*.png for the eye.

Asserted (from the fixture's log and the Amiga's files, not from pixels):
  1. hello: one request, the text answer streamed (fixture log: text.sse)
  2. "show me the startup" + 2 (allow reads for the session): the tool round
     goes back with both results (tool_results=2) and no second question
  3. "please edit it" in ROOT=RAM: + Return at the menu: RAM:claude-test.txt
     changed on disk ("hello" -> "hello from the Amiga")
  4. Esc during a slow answer: the turn is dropped whole (the next request
     carries the same history as the interrupted one)
  5. Ctrl+C twice: back in the Shell (an Echo after it writes its file)
  0. first start in a folder: the workspace trust question, Yes stored in
     ENVARC:Claude/claude.json (as Claude Code keeps it in ~/.claude.json)
By eye (screenshots): the input box and status line, the slash menu,
Shift+Tab's mode text, the diff and todo list, the spinner.

The rig must be up (rig.py start) with the handler and C:Claude installed:
  make amiga && python3 tools/rig/rig.py install && cp build/amiga/Claude build/rig/vtc/
  python3 tools/rig/claude_rig.py
"""
import os, re, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths
import ami
import condev_rig as c

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SHOTS = os.path.join(paths.RIG, 'shots')
LOG = os.path.join(paths.RIG, 'claude_fixture.log')
URL = 'http://127.0.0.1:8080/v1/messages'
RET, ESC, TAB = 0x44, 0x45, 0x42
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, '' if ok else detail), flush=True)
    if not ok:
        fails.append(name)


def typ(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.6)


def key(k, q=0, wait=0.7):
    ami.key(k, q)
    time.sleep(wait)


def shot(name):
    os.makedirs(SHOTS, exist_ok=True)
    ami.main(['shot', os.path.join(SHOTS, 'claude-%s.png' % name)])


def fixture(delay):
    subprocess.run(['pkill', '-f', 'claude_fixture.py'])
    time.sleep(0.5)
    f = open(LOG, 'w')
    p = subprocess.Popen([sys.executable, os.path.join(ROOT, 'tools/claude_fixture.py'),
                          '--delay', str(delay)], stdout=f, stderr=subprocess.STDOUT)
    time.sleep(1)
    return p


def requests():
    try:
        return [l for l in open(LOG) if '/v1/messages' in l]
    except OSError:
        return []


def window(title, root):
    c.run('Run >NIL: NewShell "XCON:0/12/760/440/%s/CLOSE"' % title)
    time.sleep(5)
    w = ami.window(title)
    if not w:
        raise SystemExit('window %s never appeared' % title)
    kx, ky = ami.pointer_scale()
    x, y, ww, hh = w['box']
    ami.script(('move', int((x + 300) * kx), int((y + 150) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    typ('Stack 32768')
    key(RET, wait=1)
    typ('C:Claude URL=%s ROOT=%s' % (URL, root) if os.environ.get('CLAUDE_IN_C') else
        'VTC:Claude URL=%s ROOT=%s' % (URL, root))
    key(RET, wait=8)


def main():
    c.run('Echo >RAM:claude-test.txt hello')
    c.run('Delete >NIL: ENVARC:Claude/claude.json QUIET')  # forget RAM:'s trust: the question comes
    proc = fixture(0.05)
    try:
        window('claude1', 'RAM:')
        shot('trust')
        key(RET, wait=3)  # 1. Yes, proceed
        got = c.run('Type ENVARC:Claude/claude.json')[1]
        check('workspace trust: Yes stored for the folder', '"Ram Disk:":{"hasTrustDialogAccepted":true}' in got, got)
        shot('idle')
        n0 = len(requests())
        typ('hello')
        key(RET, wait=10)
        r = requests()[n0:]
        check('hello: one request, a text answer', len(r) == 1 and 'text.sse' in r[0], ''.join(r))
        typ('show me the startup')
        key(RET, wait=10)
        shot('permission')
        typ('2')
        time.sleep(12)
        r = requests()
        check('read tools allowed for the session: both results in one message',
              any('tool_results=2' in l for l in r[-2:]), ''.join(r[-3:]))
        key(TAB, 0x0001, wait=1)
        shot('shift-tab')
        typ('/')
        time.sleep(2)
        shot('slash-menu')
        key(ESC)
        key(0x41, wait=0.5)  # Backspace: the '/' the menu leaves
        typ('please edit it')
        key(RET, wait=14)
        shot('edit-menu')
        key(RET, wait=10)
        shot('edit-done')
        got = c.run('Type RAM:claude-test.txt')[1].strip()
        check('the edit reached the file', got == 'hello from the Amiga', repr(got))
        proc.terminate()
        proc = fixture(0.6)  # slow: Esc lands mid-answer
        n1 = len(requests())
        typ('hello')
        key(RET, wait=2.5)
        shot('spinner')
        key(ESC, wait=4)
        shot('esc')
        typ('hello')
        key(RET, wait=25)
        r = requests()[n1:]
        m = [int(x) for x in re.findall(r'messages=(\d+)', ''.join(r))]
        check('Esc drops the unfinished turn (the next request carries the same history)',
              len(m) == 2 and m[1] == m[0] + 0, str(m))
        key(0x33, 0x0008, wait=1)
        key(0x33, 0x0008, wait=3)
        c.run('Delete >NIL: RAM:claude-shell.txt QUIET')
        typ('Echo >RAM:claude-shell.txt back')
        key(RET, wait=2)
        got = c.run('Type RAM:claude-shell.txt')[1].strip()
        check('Ctrl+C twice: the Shell runs commands again', got == 'back', repr(got))
        shot('shell')
    finally:
        proc.terminate()
        c.run('Delete >NIL: RAM:claude-test.txt RAM:claude-shell.txt QUIET')
    print('%d failed; screenshots in build/rig/shots/claude-*.png' % len(fails))
    sys.exit(1 if fails else 0)


main()
