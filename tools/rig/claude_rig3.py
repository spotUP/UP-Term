#!/usr/bin/env python3
"""claude_rig3.py -- C:Claude's audit-round features on the rig (fixture, no key):
checkpoints across a restart (/rewind after CONTINUE), the statusLine row, /help's
"Not on the Amiga" list and /mcp's reason. Asserted from the Amiga's files;
screenshots in build/rig/shots/claude2-c3-*.png for the rows drawn on screen.

  python3 tools/rig/claude_rig3.py
"""
import json, os, shutil, subprocess, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import condev_rig as c
import claude_rig2 as r2


def main():
    shutil.rmtree(r2.DUMP, ignore_errors=True)
    subprocess.run(['pkill', '-f', 'claude_fixture.py'])
    time.sleep(0.5)
    fx = subprocess.Popen([sys.executable, '-u', os.path.join(r2.ROOT, 'tools/claude_fixture.py'), '--dump', r2.DUMP],
                          stdout=open(r2.LOG, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    c.run('Echo >RAM:claude-test.txt hello')
    c.run('MakeDir >NIL: ENVARC:Claude')
    r2.trust_ram()
    r2.put('ENVARC:Claude/settings.json', json.dumps({"statusLine": {"type": "command", "command": "Echo STATUS-ROW-OK"}}))
    try:
        r2.start_claude('c3a')
        r2.line('please edit it', 14)
        r2.key(r2.RET, wait=10)
        r2.check('edit reached the file', r2.amiga_text('RAM:claude-test.txt') == 'hello from the Amiga',
                 r2.amiga_text('RAM:claude-test.txt'))
        r2.shot('c3-statusline')
        r2.line('/help', 4)
        r2.shot('c3-help')
        r2.line('/mcp', 3)
        r2.shot('c3-mcp')
        r2.line('/exit', 3)
        r2.start_claude('c3b', 'CONTINUE')
        r2.line('/rewind 1 code', 5)
        r2.shot('c3-rewind')
        r2.check('/rewind after a restart put the file back', r2.amiga_text('RAM:claude-test.txt') == 'hello',
                 r2.amiga_text('RAM:claude-test.txt'))
        r2.line('/exit', 3)
    finally:
        fx.terminate()
        c.run('Delete >NIL: RAM:claude-test.txt ENVARC:Claude/settings.json QUIET')
    print('%d failed; screenshots in build/rig/shots/claude2-c3-*.png' % len(r2.fails))
    sys.exit(1 if r2.fails else 0)


main()
