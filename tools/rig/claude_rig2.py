#!/usr/bin/env python3
"""claude_rig2.py -- C:Claude's A4 features on the rig against the recorded
fixture (no key): memory, settings, custom commands, hooks, rewind, the
prompt's prefixes, the new tools, --continue. Asserted from the request
bodies the fixture saves (--dump) and from files on the Amiga; screenshots
in build/rig/shots/claude2-*.png for the rest.

  make amiga && python3 tools/rig/rig.py install && cp build/amiga/Claude build/rig/vtc/
  python3 tools/rig/claude_rig2.py
"""
import glob, json, os, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import condev_rig as c

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..', '..'))
SHOTS = os.path.join(ROOT, 'build/rig/shots')
DUMP = os.path.join(ROOT, 'build/rig/claude_dump')
LOG = os.path.join(ROOT, 'build/rig/claude_fixture.log')
URL = 'http://127.0.0.1:8080/v1/messages'
RET, ESC, UP = 0x44, 0x45, 0x4C
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, '' if ok else str(detail)[:300]), flush=True)
    if not ok:
        fails.append(name)


def typ(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1'))
    time.sleep(0.6)


def key(k, q=0, wait=0.7):
    ami.key(k, q)
    time.sleep(wait)


def line(s, wait=8):
    typ(s)
    key(RET, wait=wait)


def shot(name):
    os.makedirs(SHOTS, exist_ok=True)
    ami.main(['shot', os.path.join(SHOTS, 'claude2-%s.png' % name)])


def bodies():
    out = []
    for p in sorted(glob.glob(os.path.join(DUMP, '*.json'))):
        try:
            out.append(open(p, encoding='utf-8', errors='replace').read())
        except OSError:
            pass
    return out


def put(path, text):
    tmp = os.path.join(ROOT, 'build/rig/_c2_put')
    open(tmp, 'w').write(text)
    ami.main(['put', tmp, path])
    os.unlink(tmp)


def amiga_text(path):
    rc, out = c.run('Type %s' % path)
    return out.strip() if rc == 0 else None


def trust_ram():
    """RAM: trusted as a Yes at the trust question leaves it (claude_rig.py checks the question)"""
    put('ENVARC:Claude/claude.json', json.dumps({"projects": {"Ram Disk:": {"hasTrustDialogAccepted": True}}}))


def start_claude(title, extra=''):
    c.run('Run >NIL: NewShell "XCON:0/12/760/440/%s/CLOSE"' % title)
    time.sleep(5)
    w = ami.window(title)
    kx, ky = ami.pointer_scale()
    x, y, ww, hh = w['box']
    ami.script(('move', int((x + 300) * kx), int((y + 150) * ky)), ('wait', 2), ('button', 0, 1),
               ('wait', 2), ('button', 0, 0), ('wait', 5))
    time.sleep(1)
    line('Stack 32768', 1)
    line('VTC:Claude URL=%s ROOT=RAM: %s' % (URL, extra), 8)


def main():
    shutil.rmtree(DUMP, ignore_errors=True)
    subprocess.run(['pkill', '-f', 'claude_fixture.py'])
    time.sleep(0.5)
    fx = subprocess.Popen([sys.executable, '-u', os.path.join(ROOT, 'tools/claude_fixture.py'), '--dump', DUMP],
                          stdout=open(LOG, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    c.run('Echo >RAM:claude-test.txt hello')
    c.run('MakeDir >NIL: RAM:.claude RAM:.claude/commands ENVARC:Claude')
    trust_ram()
    put('RAM:CLAUDE.md', 'PROJECT-MEMORY-SENTINEL\n')
    put('ENVARC:Claude/CLAUDE.md', 'USER-MEMORY-SENTINEL\n')
    put('RAM:.claude/commands/greet.md', '---\ndescription: Greet someone\n---\nSay hello to $ARGUMENTS.\n')
    put('RAM:hook.dos', 'Echo "listing is not allowed here"\nQuit 2\n')
    put('RAM:.claude/settings.json', json.dumps({
        "permissions": {"allow": ["Read"]},
        "hooks": {"PreToolUse": [{"matcher": "Glob", "hooks": [{"type": "command", "command": "Execute RAM:hook.dos"}]}]}}))
    try:
        start_claude('c2a')
        shot('start')
        # memory + a custom command
        line('/greet Amiga', 10)
        b = bodies()
        check('memory: user and project CLAUDE.md in the system prompt',
              b and 'USER-MEMORY-SENTINEL' in b[-1] and 'PROJECT-MEMORY-SENTINEL' in b[-1], b[-1][:200] if b else 'no request')
        check('custom command /greet expanded', b and 'Say hello to Amiga.' in b[-1])
        # permission rule + hook
        n = len(bodies())
        line('show me the startup', 14)
        shot('rule-hook')
        b = bodies()
        check('allow rule: Read ran with no question, the hook blocked Glob',
              len(b) > n + 1 and 'listing is not allowed here' in b[-1], len(b) - n)
        # edit, then /rewind
        line('please edit it', 14)
        key(RET, wait=10)  # Yes in the edit menu (Edit is not in the allow rule)
        check('edit reached the file', amiga_text('RAM:claude-test.txt') == 'hello from the Amiga',
              amiga_text('RAM:claude-test.txt'))
        line('/rewind 1 code', 4)
        shot('rewind')
        check('/rewind put the file back', amiga_text('RAM:claude-test.txt') == 'hello',
              amiga_text('RAM:claude-test.txt'))
        # the prompt's prefixes
        c.run('Delete >NIL: RAM:bang.txt QUIET')
        line('!Echo >RAM:bang.txt bang', 5)
        check('! runs a shell command', amiga_text('RAM:bang.txt') == 'bang', amiga_text('RAM:bang.txt'))
        typ('#remember the milk')
        key(RET, wait=2)
        key(RET, wait=3)  # the first file in the "where" menu (the project's)
        got = amiga_text('RAM:CLAUDE.md') or ''
        check('# writes a memory note', 'remember the milk' in got, got)
        n = len(bodies())
        line('explain @claude-test.txt', 10)
        b = bodies()
        check('@ attaches the file', len(b) > n and 'claude-test.txt' in b[-1] and 'hello' in b[-1])
        # new tools, by eye and by request
        line('search the web', 12)
        shot('websearch-ask')
        key(RET, wait=12)  # WebSearch asks first, as Claude Code does: Yes (unanswered, the next prompt's Enter did)
        shot('websearch')
        line('fetch the page', 15)
        shot('webfetch-ask')
        key(RET, wait=5)  # WebFetch asks first, as Claude Code does: Yes
        shot('webfetch')
        for _ in range(60):  # the fetch, then the small model's answer: give it time
            if 'GET /page' in open(LOG).read() and any('"claude-haiku' in x for x in bodies()):
                break
            time.sleep(1)
        check('WebFetch: the page fetched and a small-model call made',
              'GET /page' in open(LOG).read() and any('"claude-haiku' in x for x in bodies()))
        line('a question', 10)
        shot('ask')
        key(RET, wait=8)
        line('/exit', 3)
        # --continue
        n = len(bodies())
        start_claude('c2b', 'CONTINUE')
        line('hello', 10)
        b = bodies()
        try:
            msgs = json.loads(b[-1]).get('messages', [])
        except ValueError:
            msgs = []
        check('CONTINUE carries the last session', len(b) > n and len(msgs) > 3, len(msgs))
        key(UP, wait=1)
        shot('history-up')
        key(0x16, 0x0008)  # Ctrl+U: the recalled prompt out of the box, or /exit is sent as "hello/exit"
        line('/exit', 3)
    finally:
        fx.terminate()
        c.run('Delete >NIL: RAM:claude-test.txt RAM:bang.txt RAM:hook.dos RAM:CLAUDE.md RAM:.claude ENVARC:Claude/CLAUDE.md ALL QUIET')
    print('%d failed; screenshots in build/rig/shots/claude2-*.png' % len(fails))
    sys.exit(1 if fails else 0)


if __name__ == "__main__":
    main()
