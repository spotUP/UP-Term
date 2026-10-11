#!/usr/bin/env python3
"""amigapi_rig.py -- amiga-pi on the rig, one numbered step per plan phase
(thoughts/shared/plans/2026-10-07-amiga-pi.md), asserted from the fixture's
log and the files on the Amiga, not from pixels.

Step 1 (B12): build/amiga/pi-llmprobe sends one prompt through llm_request
(openai-completions) to tools/claude_fixture.py over plain http, with a key
given that must stay home:
  1a. the probe printed the recorded answer's text and "[DONE] stop=stop"
  1b. the fixture's log shows exactly one /v1/chat/completions request and
      no Authorization header on it
  1c. the probe said "key for fixture not sent: plain http"
The run is in a visible window titled "amiga-pi step 1".

Step 2 (C10, C12, C15): amiga-pi itself (~/Code/amiga-pi build/amiga/amiga-pi)
in print mode against the same fixture, with ENV:PI_MEM set:
  2a. "amiga-pi -p hello" printed the recorded text and returned 0
  2b. one more /chat/completions request, no Authorization header
  2c. its memory line: free memory at the start and the least while it ran
  2d. C:Claude (build/amiga/Claude, console io moved to claude/io_amiga.c)
      still answers PRINT against the fixture and returns 0
Each in a visible window titled "amiga-pi step 2".

Step 3 (D8): the tools. "amiga-pi -p please edit it" against the fixture,
whose openai_edit.sse calls edit on RAM:pi-test.txt ("hello" -> "hello from
the Amiga") and bash "List RAM:", then answers openai_edit_final.sse:
  3a. RAM:pi-test.txt says "hello from the Amiga"
  3b. the fixture's log shows edit's result text and List's (pi-test.txt in it)
  3c. the final text printed, exit code 0
In a visible window titled "amiga-pi step 3".

  make build/amiga/pi-llmprobe build/amiga/Claude; make -C ~/Code/amiga-pi amiga
  UPTERM_RIG=2 python3 -I tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 -I tools/rig/amigapi_rig.py [--port 8092] [--step 1|2|3]
"""
import argparse
import os
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402
import condev_rig as c  # noqa: E402

ROOT = paths.VTCON
VTC = paths.RIG / 'vtc'
fails = []


def check(name, ok, detail=''):
    print('[%s] %s %s' % ('PASS' if ok else 'FAIL', name, '' if ok else detail), flush=True)
    if not ok:
        fails.append(name)


def there(path):
    rc, _ = c.run('List "%s" QUICK' % path)
    return rc == 0


def step1(port):
    probe = ROOT / 'build' / 'amiga' / 'pi-llmprobe'
    if not probe.exists():
        sys.exit('[ERROR] build/amiga/pi-llmprobe missing: make build/amiga/pi-llmprobe')
    shutil.copy(probe, VTC / 'pi-llmprobe')
    log = paths.RIG / 'amigapi_fixture.log'
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01'], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    out = VTC / 'pi1.out'
    try:
        if out.exists():
            out.unlink()
        c.run('Delete RAM:pi1-done QUIET')
        with open(VTC / 'pi1.dos', 'w') as f:
            f.write('Echo "amiga-pi step 1: one prompt through llm_request to the fixture"\n'
                    'VTC:pi-llmprobe http://127.0.0.1:%d/v1 qwen2.5-coder:7b sk-must-not-be-sent hello >VTC:pi1.out\n'
                    'Type VTC:pi1.out\n'
                    'Echo >RAM:pi1-done "done"\n'
                    'Wait 3\n'
                    'EndCLI\n' % port)
        c.run('Run >NIL: NewShell "CON:0/12/640/200/amiga-pi step 1/CLOSE" FROM VTC:pi1.dos')
        for _ in range(120):
            if there('RAM:pi1-done'):
                break
            time.sleep(0.5)
        check('1 the probe ended', there('RAM:pi1-done'))
        time.sleep(4)
        text = out.read_text(errors='replace') if out.exists() else ''
        check('1a the answer streamed', 'Hello from the Amiga!' in text, repr(text[:200]))
        check('1a it completed', '[DONE] stop=stop attempts=1 key_sent=0' in text, repr(text[-200:]))
        check('1c the key was kept home', 'key for fixture not sent: plain http' in text, repr(text[:200]))
        reqs = [l for l in open(log) if '/chat/completions' in l]
        check('1b one chat/completions request', len(reqs) == 1, repr(reqs))
        check('1b no Authorization header', all('key_header=none' in l for l in reqs) and reqs, repr(reqs))
    finally:
        fx.terminate()
        c.run('Delete RAM:pi1-done VTC:pi1.dos QUIET')


AMIGA_PI = paths.repo('amiga-pi') / 'build' / 'amiga' / 'amiga-pi'


def run_window(name, title, lines, done):
    """lines as a script in a visible, titled window; True when it ended"""
    c.run('Delete RAM:%s QUIET' % done)
    with open(VTC / (name + '.dos'), 'w') as f:
        f.write(''.join(l + '\n' for l in lines) + 'Echo >RAM:%s "done"\nWait 3\nEndCLI\n' % done)
    c.run('Run >NIL: NewShell "CON:0/12/640/200/%s/CLOSE" FROM VTC:%s.dos' % (title, name))
    for _ in range(240):
        if there('RAM:' + done):
            return True
        time.sleep(0.5)
    return False


def step2(port):
    claude = ROOT / 'build' / 'amiga' / 'Claude'
    for b, hint in ((AMIGA_PI, 'make -C ~/Code/amiga-pi amiga'), (claude, 'make build/amiga/Claude')):
        if not b.exists():
            sys.exit('[ERROR] %s missing: %s' % (b, hint))
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    shutil.copy(claude, VTC / 'Claude')
    log = paths.RIG / 'amigapi_fixture2.log'
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01'], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    try:
        for f in ('pi2.out', 'pi2.rc', 'cl2.out', 'cl2.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ok = run_window('pi2', 'amiga-pi step 2', [
            'Echo "amiga-pi step 2: amiga-pi -p hello against the fixture"',
            'SetEnv PI_MEM 1',
            'SetEnv FIXTURE_API_KEY sk-must-not-be-sent',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 -p hello >VTC:pi2.out' % port,
            'Echo "rc=$RC" >VTC:pi2.rc',
            'UnSetEnv PI_MEM',
            'UnSetEnv FIXTURE_API_KEY',
            'Type VTC:pi2.out',
            'Echo "C:Claude PRINT against the fixture"',
            'VTC:Claude PRINT URL=http://127.0.0.1:%d/v1/messages hello >VTC:cl2.out' % port,
            'Echo "rc=$RC" >VTC:cl2.rc',
            'Type VTC:cl2.out'], 'pi2-done')
        check('2 the window ended', ok)
        time.sleep(4)
        text = (VTC / 'pi2.out').read_bytes().decode('utf-8', 'replace') if (VTC / 'pi2.out').exists() else ''
        rc = (VTC / 'pi2.rc').read_text(errors='replace').strip() if (VTC / 'pi2.rc').exists() else ''
        check('2a the recorded text', 'Hello from the Amiga! Gr\u00fc\u00dfe \u2014 ok' in text, repr(text[:300]))
        check('2a exit code 0', rc == 'rc=0', repr(rc))
        check('2b the key was kept home', 'key for fixture not sent: plain http' in text, repr(text[:300]))
        reqs = [l for l in open(log) if '/chat/completions' in l]
        check('2b one chat/completions request, no Authorization',
              len(reqs) == 1 and 'key_header=none' in reqs[0], repr(reqs))
        mem = [l for l in text.splitlines() if l.startswith('[INFO] memory:')]
        check('2c the memory line', len(mem) == 1, repr(text[-300:]))
        if mem:
            print('[INFO] ' + mem[0][7:], flush=True)
        ctext = (VTC / 'cl2.out').read_text(errors='replace') if (VTC / 'cl2.out').exists() else ''
        crc = (VTC / 'cl2.rc').read_text(errors='replace').strip() if (VTC / 'cl2.rc').exists() else ''
        check('2d C:Claude PRINT answered', 'Hello from the Amiga!' in ctext and crc == 'rc=0',
              repr((crc, ctext[:300])))
    finally:
        fx.terminate()
        c.run('Delete RAM:pi2-done VTC:pi2.dos QUIET')


def step3(port):
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture3.log'
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01'], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    try:
        for f in ('pi3.out', 'pi3.rc', 'pi3.file'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ok = run_window('pi3', 'amiga-pi step 3', [
            'Echo "amiga-pi step 3: edit RAM:pi-test.txt and List RAM: through the tools"',
            'Echo hello >RAM:pi-test.txt',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 -p "please edit it" '
            '>VTC:pi3.out' % port,
            'Echo "rc=$RC" >VTC:pi3.rc',
            'Type RAM:pi-test.txt >VTC:pi3.file',
            'Type VTC:pi3.out',
            'Type RAM:pi-test.txt'], 'pi3-done')
        check('3 the window ended', ok)
        time.sleep(4)
        read = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        text, rc, body = read('pi3.out'), read('pi3.rc').strip(), read('pi3.file')
        check('3a the file was edited', body.strip() == 'hello from the Amiga', repr(body))
        lines = open(log).read().splitlines()
        reqs = [l for l in lines if '/chat/completions' in l and '.sse' in l]
        check('3b two chat/completions requests', len(reqs) == 2, repr(reqs))
        edit = [l for l in lines if 'tool_result call_PiEditTest:' in l]
        lst = [l for l in lines if 'tool_result call_PiListRam:' in l]
        check('3b edit\'s result reached the fixture',
              len(edit) == 1 and 'Successfully replaced 1 block(s) in RAM:pi-test.txt.' in edit[0], repr(edit))
        check('3b List RAM:\'s output reached the fixture', len(lst) == 1 and 'pi-test.txt' in lst[0], repr(lst))
        for l in edit + lst:
            print('[INFO] ' + l.strip(), flush=True)
        check('3c the final text', 'Done: RAM:pi-test.txt now says hello from the Amiga.' in text, repr(text[:300]))
        check('3c exit code 0', rc == 'rc=0', repr(rc))
    finally:
        fx.terminate()
        c.run('Delete RAM:pi3-done VTC:pi3.dos RAM:pi-test.txt QUIET')


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8092, help="the fixture's port (8080 may be another rig's)")
    ap.add_argument('--step', type=int, choices=(1, 2, 3), help='one step only (default: all)')
    a = ap.parse_args()
    if a.step in (None, 1):
        step1(a.port)
    if a.step in (None, 2):
        step2(a.port + 1)
    if a.step in (None, 3):
        step3(a.port + 2)
    print('amigapi_rig: %s' % ('PASS' if not fails else 'FAIL: ' + ', '.join(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
