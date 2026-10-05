#!/usr/bin/env python3
"""claude_rig4.py -- C:Claude's A4 gaps-3 TUI rows on the rig against the
recorded fixture (no key), asserted from the request bodies (--dump) and
files on the Amiga; screenshots in build/rig/shots/claude4-*.png.

  1. keybindings.json is read: "ctrl+e": "chat:submit" sends the prompt
  2. @agent-NAME adds Claude Code's agent note to the prompt's message
  3. --allow-dangerously-skip-permissions: Shift+Tab reaches bypass, and an
     edit then runs with no question
  By eye: /color red on the box frame, the bypass mode text.

Prompt suggestions and the away recap are switched off (settings.json):
they have their own check once the input bug they trigger is fixed.

  make amiga && python3 tools/rig/rig.py install && cp build/amiga/Claude build/rig/vtc/
  python3 tools/rig/claude_rig4.py
"""
import json, os, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami
import condev_rig as c
import claude_rig2 as r2

RET, TAB = 0x44, 0x42
E_KEY, CTRL, SHIFT = 0x12, 0x0008, 0x0001


def main():
    import shutil, subprocess
    shutil.rmtree(r2.DUMP, ignore_errors=True)
    subprocess.run(['pkill', '-f', 'claude_fixture.py'])
    time.sleep(0.5)
    fx = subprocess.Popen([sys.executable, '-u', os.path.join(r2.ROOT, 'tools/claude_fixture.py'), '--dump', r2.DUMP],
                          stdout=open(r2.LOG, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    c.run('Echo >RAM:claude-test.txt hello')
    c.run('MakeDir >NIL: RAM:.claude RAM:.claude/agents ENVARC:Claude')
    r2.trust_ram()
    r2.put('RAM:.claude/settings.json', json.dumps({"promptSuggestionEnabled": False, "awaySummaryEnabled": False}))
    r2.put('RAM:.claude/agents/code-reviewer.md',
           '---\nname: code-reviewer\ndescription: Reviews code for problems\n---\nYou review code.\n')
    r2.put('ENVARC:Claude/keybindings.json',
           json.dumps({"bindings": [{"context": "Chat", "bindings": {"ctrl+e": "chat:submit"}}]}))
    try:
        r2.start_claude('c4', '--allow-dangerously-skip-permissions')
        r2.shot('4-start')
        # 1. the keybindings file: Ctrl+E submits
        n = len(r2.bodies())
        r2.typ('hello from ctrl e')
        r2.key(E_KEY, CTRL, wait=8)
        b = r2.bodies()
        r2.check('keybindings.json: ctrl+e submits the prompt',
                 len(b) > n and 'hello from ctrl e' in b[-1], len(b) - n)
        # 2. @agent-NAME
        n = len(r2.bodies())
        r2.line('ask @agent-code-reviewer to look', 8)
        b = r2.bodies()
        r2.check('@agent-code-reviewer: the agent note goes with the prompt',
                 len(b) > n and 'invoke the agent \\"code-reviewer\\"' in b[-1],
                 b[-1][-300:] if b else 'no request')
        # 3. bypass through Shift+Tab, then an edit with no question
        for _ in range(3):
            r2.key(TAB, SHIFT, wait=1)
        r2.shot('4-bypass')
        r2.line('please edit it', 14)
        got = r2.amiga_text('RAM:claude-test.txt')
        r2.check('bypass permissions: the edit ran with no question', got == 'hello from the Amiga', got)
        # by eye: /color
        r2.line('/color red', 2)
        r2.shot('4-color-red')
        r2.line('/exit', 3)
    finally:
        fx.terminate()
        c.run('Delete >NIL: RAM:claude-test.txt RAM:.claude ENVARC:Claude/keybindings.json ALL QUIET')
    print('%d failed; screenshots in build/rig/shots/claude2-4-*.png' % len(r2.fails))
    sys.exit(1 if r2.fails else 0)


if __name__ == "__main__":
    main()
