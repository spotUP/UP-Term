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

  make build/amiga/pi-llmprobe
  UPTERM_RIG=2 python3 -I tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 -I tools/rig/amigapi_rig.py [--port 8092]
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


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8092, help="the fixture's port (8080 may be another rig's)")
    a = ap.parse_args()
    step1(a.port)
    print('amigapi_rig: %s' % ('PASS' if not fails else 'FAIL: ' + ', '.join(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
