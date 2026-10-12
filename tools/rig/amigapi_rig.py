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

Step 4 (parity Q9-Q11): the bash tool's shell and read's images on the Amiga.
"amiga-pi -p ask the shell" against the fixture, whose openai_shell.sse calls
bash with a command only vsh answers (echo $((1+2)), quoting, a here-document read by a loop,
$0) and read on RAM:pi-wide.bmp (2400x10):
  4a without UP-Term: (no assign, no ENVARC:up-term/Dir, no C:vsh): the run
     ends, no requester window appears (amiagent's UI tree, polled), the
     prompt says vsh is not installed, the AmigaShell ran the command; read
     sent the BMP as PNG at 2000x8 with pi's notes
  4b UP-Term: the kit's way: VTC:pi-kit/bin/vsh and ENVARC:up-term/Dir naming
     VTC:pi-kit (upassign_ensure makes the assign, as for an installed kit):
     the prompt names vsh and the command's output is vsh's ("3", the quoted
     words, the here-document)
  4c step 3 again with UP-Term: there (bash's List RAM: through vsh)
  4d vsh -c against vsh <script> (what sys_amiga runs): $0, exit status,
     standard input, the directory, quoting, two lines; the outputs side by
     side in the log; only $0 differs
  4e a 600-character command: the script runs it, as vsh -c the Shell
     refuses the line (too long), so the bash tool keeps the script
Everything 4b made is removed again. Visible windows titled "amiga-pi step 4".

Step 5 (E12): sessions. UP-Term: made as step 4b makes it (VTC:pi-kit), so the data
directory is UP-Term:var/amiga-pi and the host can read what amiga-pi wrote.
  5a "amiga-pi -p hello" writes one session file under var/amiga-pi/sessions/<dir>/:
     every name at most 30 characters, pi's header (version 3, timestamp without Z),
     model_change, thinking_level_change, the system message, the user message, the answer
  5b "amiga-pi -c -p again" continues that file (still one file, two user messages, one
     system message) and the request carries the first exchange
  5c the host opens the file the Amiga wrote with pi's own SessionManager
     (tools/pi_oracle/gen.ts --dump in the tree the binary was built in; skipped when the oracle is not there)
  5d --no-session leaves the directory as it was
Visible windows titled "amiga-pi step 5".

Step 7 (G5): project instructions and trust. RAM:AGENTS.md (the volume root) and RAM:proj/AGENTS.md,
and RAM:proj/.pi/SYSTEM.md; "amiga-pi --no-session -p hello" with RAM:proj as the current directory:
  7a exit code 0, the request's system prompt holds both AGENTS.md texts, RAM:AGENTS.md first, each in a
     <project_instructions path="..."> naming its file
  7b the project is not trusted in print mode (nothing decided): the default prompt's preamble stays and
     SYSTEM.md's text is not sent; no requester appeared
  7c the same with -a: SYSTEM.md replaces the preamble (its text is sent, the default's is not), the
     AGENTS.md texts are still there
  7d nothing was written to ENVARC:amiga-pi/trust.json (one that was there is put aside first and restored after)
Everything made is removed again. Visible windows titled "amiga-pi step 7".

Step 10 (K5, K8): MCP. ENVARC:amiga-pi/mcp.json names two servers: "fake",
tools/fake_mcp.py over streamable HTTP on the Mac at its LAN address (the LAN
rule's plain http), and "amiga", build/amiga/mcp-echo (a native stdio server)
on the Amiga, started by spawn2 on PTY: pipe pairs. "amiga-pi -p use the mcp
tools" against the fixture, whose openai_mcp.sse calls mcp__fake__echo and
mcp__amiga__echo, then answers openai_final.sse:
  10a exit code 0, the final text printed
  10b the Mac's server: one initialize (no session id) and one tools/call with
      the model's arguments and Mcp-Session-Id, from the Mac's LAN address;
      its result reached the model (the fixture's log)
  10c the Amiga's server answered over PTY: ("echo: over PTY: (3 requests)":
      initialize, tools/list, tools/call reached it) and its result reached
      the model
  10d the server ended with amiga-pi (no more mcp-echo in Status than before)
ENVARC:amiga-pi/mcp.json is removed again. A visible window titled
"amiga-pi step 10". The tree the binary was built in gives mcp-echo and
fake_mcp.py (make -C <amiga-pi> amiga build/amiga/mcp-echo).

  make build/amiga/pi-llmprobe build/amiga/Claude; make -C ~/Code/amiga-pi amiga
  UPTERM_RIG=2 python3 -I tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 -I tools/rig/amigapi_rig.py [--port 8092] [--step 1|2|3|4|5|7|10]
"""
import argparse
import json
import os
import pathlib
import struct
import shutil
import subprocess
import sys
import time

sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import paths  # noqa: E402
import condev_rig as c  # noqa: E402
import ami  # noqa: E402

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


AMIGA_PI = pathlib.Path(os.environ.get('AMIGA_PI_BIN') or
                        paths.repo('amiga-pi') / 'build' / 'amiga' / 'amiga-pi')   # AMIGA_PI_BIN: another build (a worktree's)


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


def assigns():
    """the assign, device and volume names the Amiga has (Assign LIST never asks for a volume)"""
    out = ami.req(0x02, struct.pack('>H', 30) + b'Assign LIST', 60)[4:].decode('latin-1')
    return {l.split()[0].rstrip(':').upper() for l in out.splitlines() if l.strip() and not l[0].isspace()}


def run_watched(name, title, lines, done):
    """run_window, polling amiagent's UI tree for a requester the while: (ended, requester titles seen)"""
    c.run('Delete RAM:%s QUIET' % done)
    with open(VTC / (name + '.dos'), 'w') as f:
        f.write(''.join(l + '\n' for l in lines) + 'Echo >RAM:%s "done"\nWait 3\nEndCLI\n' % done)
    c.run('Run >NIL: NewShell "CON:0/12/640/200/%s/CLOSE" FROM VTC:%s.dos' % (title, name))
    seen = set()
    for _ in range(240):
        tree = ami.req(0x0D).decode('latin-1')
        for l in tree.splitlines():
            if 'System Request' in l or 'insert volume' in l.lower() or 'Please insert' in l:
                seen.add(l.strip()[:120])
        if there('RAM:' + done):
            return True, sorted(seen)
        time.sleep(0.5)
    return False, sorted(seen)


def bmp(path, w, h):
    row = (w * 3 + 3) & ~3
    data = bytearray()
    for y in range(h):
        r = bytearray()
        for x in range(w):
            r += bytes((255, 0, 0)) if x < w // 2 else bytes((0, 0, 255))
        data += r + b'\0' * (row - len(r))
    hdr = b'BM' + struct.pack('<IHHI', 54 + len(data), 0, 0, 54) + \
        struct.pack('<IiiHHIIiiII', 40, w, h, 1, 24, 0, len(data), 2835, 2835, 0, 0)
    open(path, 'wb').write(hdr + data)


def body_of(dump, k):
    import json
    p = dump / ('%03d.json' % k)
    return json.loads(p.read_text()) if p.exists() else {}


def shell_run(port, tag, log, dump):
    """amiga-pi -p ask the shell in a watched window: (ended, requesters, system prompt, results by call id)"""
    for f in ('pi4%s.out' % tag, 'pi4%s.rc' % tag):
        if (VTC / f).exists():
            (VTC / f).unlink()
    ended, req = run_watched('pi4' + tag, 'amiga-pi step 4%s' % tag, [
        'Echo "amiga-pi step 4%s: bash and read through the tools"' % tag,
        'Copy VTC:pi-wide.bmp RAM:pi-wide.bmp',
        'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 -p "ask the shell" '
        '>VTC:pi4%s.out' % (port, tag),
        'Echo "rc=$RC" >VTC:pi4%s.rc' % tag,
        'Type VTC:pi4%s.out' % tag], 'pi4%s-done' % tag)
    time.sleep(3)
    bodies = sorted(dump.glob('*.json'))
    first = body_of(dump, len(bodies) - 1) if len(bodies) >= 2 else {}
    last = body_of(dump, len(bodies)) if bodies else {}
    system = ''
    for m in first.get('messages', []):
        if m.get('role') == 'system':
            system = m.get('content') if isinstance(m.get('content'), str) else str(m.get('content'))
    res = {}
    for m in last.get('messages', []):
        if m.get('role') == 'tool':
            cn = m.get('content')
            res[m.get('tool_call_id')] = cn if isinstance(cn, str) else ' '.join(
                b.get('text', '') for b in cn if isinstance(b, dict))
    return ended, req, system, res


IMG_NOTE = ('Read image file [image/png]\n[Image converted from image/bmp to image/png.]\n'
            '[Image: original 2400x10, displayed at 2000x8. Multiply coordinates by 1.20 to map to original image.]')


def q9_probes():
    """vsh -c against vsh <script>: (name, command) pairs"""
    return [('$0', 'echo "$0"'), ('exit status', 'exit 7'), ('standard input', 'cat; echo end'),
            ('directory', 'pwd'), ('quoting', 'printf \'%s|\' "a b" \'c"d\' "e*f"; echo'),
            ('two lines', 'echo one\necho two')]


def amigados_quote(cmd):
    """cmd as one quoted AmigaDOS argument: * as **, " as *", a newline as *N"""
    return '"' + cmd.replace('*', '**').replace('"', '*"').replace('\n', '*N') + '"'


def step4(port):
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    if not (VTC / 'vsh').exists():
        sys.exit('[ERROR] VTC:vsh missing')
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    bmp(VTC / 'pi-wide.bmp', 2400, 10)
    had = assigns()
    saved_dir = ami.read_file('ENVARC:up-term/Dir')
    check('4 no UP-Term: on this rig to begin with', 'UP-TERM' not in had and saved_dir is None,
          repr(('UP-TERM' in had, saved_dir)))
    rc, out = c.run('List C:vsh QUICK')
    check('4 no C:vsh', rc != 0, out)
    log = paths.RIG / 'amigapi_fixture4.log'
    dump = paths.RIG / 'amigapi_dump4'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    made_assign = False
    try:
        # 4a: nothing of the kit
        ended, req, system, res = shell_run(port, 'a', log, dump)
        check('4a the run ended', ended)
        check('4a no requester', not req, repr(req))
        check('4a the prompt: vsh is not installed', 'vsh is not installed' in system, repr(system[-400:]))
        probe = res.get('call_VshProbe', '')
        print('[INFO] 4a AmigaShell ran: %r' % probe[:200], flush=True)
        check('4a the AmigaShell ran it (not vsh)', probe and not probe.startswith('3\n'), repr(probe[:200]))
        check('4a read sent the BMP as PNG at 2000x8', res.get('call_ImgRead', '') == IMG_NOTE,
              repr(res.get('call_ImgRead', '')))
        rcs = (VTC / 'pi4a.rc').read_text(errors='replace').strip() if (VTC / 'pi4a.rc').exists() else ''
        check('4a exit code 0', rcs == 'rc=0', repr(rcs))
        # 4b: UP-Term: as an installed kit makes it, from ENVARC:up-term/Dir
        c.run('MakeDir VTC:pi-kit VTC:pi-kit/bin ENVARC:up-term')
        c.run('Copy VTC:vsh VTC:pi-kit/bin/vsh')
        with open(VTC / 'pi-kit-dir', 'w') as f:
            f.write('VTC:pi-kit\n')
        c.run('Copy VTC:pi-kit-dir ENVARC:up-term/Dir')
        c.run('Copy VTC:pi-kit-dir ENV:up-term/Dir')
        made_assign = True
        ended, req, system, res = shell_run(port, 'b', log, dump)
        check('4b the run ended', ended)
        check('4b no requester', not req, repr(req))
        check('4b UP-Term: made from the Dir file', 'UP-TERM' in assigns())
        check('4b the prompt names vsh', 'vsh, a bash-compatible shell' in system, repr(system[-400:]))
        probe = res.get('call_VshProbe', '')
        print('[INFO] 4b vsh ran: %r' % probe[:200], flush=True)
        check('4b vsh answered (3, the quoted words, the here-document)',
              probe.startswith('3\na b|c"d|\ngot heredoc line\n'), repr(probe[:200]))
        check('4b read sent the BMP as PNG at 2000x8', res.get('call_ImgRead', '') == IMG_NOTE,
              repr(res.get('call_ImgRead', '')))
        # 4c: step 3 under vsh
        fx.terminate()
        fx = None
        step3(port + 10)
        # 4d: vsh -c against vsh <script>
        lines = ['Echo "amiga-pi step 4d: vsh -c against vsh <script>"', 'CD RAM:']
        for i, (name, cmd) in enumerate(q9_probes()):
            with open(VTC / ('q9s%d.sh' % i), 'w') as f:
                f.write(cmd + '\n')
            lines += ['VTC:vsh -c %s >VTC:q9c%d.out <NIL:' % (amigados_quote(cmd), i),
                      'Echo "$RC" >VTC:q9c%d.rc' % i,
                      'VTC:vsh VTC:q9s%d.sh >VTC:q9s%d.out <NIL:' % (i, i),
                      'Echo "$RC" >VTC:q9s%d.rc' % i]
        ended, req = run_watched('pi4d', 'amiga-pi step 4d', lines, 'pi4d-done')
        check('4d the probes ended', ended)
        rd = lambda f: (VTC / f).read_bytes().decode('latin-1') if (VTC / f).exists() else '(none)'
        same = []
        for i, (name, cmd) in enumerate(q9_probes()):
            a, b = rd('q9c%d.out' % i) + ' rc=' + rd('q9c%d.rc' % i).strip(), \
                rd('q9s%d.out' % i) + ' rc=' + rd('q9s%d.rc' % i).strip()
            print('[INFO] 4d %-14s -c: %r | script: %r' % (name, a, b), flush=True)
            same.append((name, a == b))
        print('[INFO] 4d the same both ways: %s' % ', '.join('%s=%s' % x for x in same), flush=True)
        check('4d only $0 differs', [n for n, ok in same if not ok] == ['$0'], repr(same))
        # 4e: a 600-character command: the script carries it; as a -c argument the AmigaDOS
        # line is too long for the Shell (the script's last line: the window is left at the
        # error and closed here)
        long_cmd = 'echo ' + 'x' * 600
        with open(VTC / 'q9long.sh', 'w') as f:
            f.write(long_cmd + '\n')
        for f in ('q9long.out', 'q9longc.out'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        c.run('Delete RAM:pi4e-done QUIET')
        with open(VTC / 'pi4e.dos', 'w') as f:
            f.write('Echo "amiga-pi step 4e: a 600-character command, as a script and as vsh -c"\n'
                    'VTC:vsh VTC:q9long.sh >VTC:q9long.out <NIL:\n'
                    'Echo >RAM:pi4e-done "done"\n'
                    'VTC:vsh -c %s >VTC:q9longc.out <NIL:\n'
                    'Wait 3\nEndCLI\n' % amigados_quote(long_cmd))
        c.run('Run >NIL: NewShell "CON:0/12/640/200/amiga-pi step 4e/CLOSE" FROM VTC:pi4e.dos')
        for _ in range(40):
            if there('RAM:pi4e-done'):
                break
            time.sleep(0.5)
        time.sleep(6)
        script_out = rd('q9long.out')
        c_out = rd('q9longc.out')
        print('[INFO] 4e script: %d x; -c: %r' % (script_out.count('x'), c_out[:80]), flush=True)
        check('4e the script ran the 600-character command', script_out.strip() == 'x' * 600, repr(script_out[:80]))
        tree = ami.req(0x0D).decode('latin-1')
        if 'amiga-pi step 4e' in tree:
            ami.req(0x08, bytes([4]) + b'endcli\r')
            time.sleep(2)
        check('4e as vsh -c it did not run (the AmigaDOS line is too long)', 'x' * 600 not in c_out, repr(c_out[:80]))
    finally:
        if fx:
            fx.terminate()
        if made_assign:
            c.run('Delete ENVARC:up-term/Dir ENV:up-term/Dir QUIET')
            if 'UP-TERM' not in had:
                c.run('Assign UP-Term: REMOVE')
            c.run('Delete VTC:pi-kit ALL QUIET')
        c.run('Delete RAM:pi4a-done RAM:pi4b-done RAM:pi4d-done RAM:pi4e-done RAM:pi-wide.bmp QUIET')


def step5(port):
    import json
    import re
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    had = assigns()
    saved_dir = ami.read_file('ENVARC:up-term/Dir')
    check('5 no UP-Term: on this rig to begin with', 'UP-TERM' not in had and saved_dir is None,
          repr(('UP-TERM' in had, saved_dir)))
    log = paths.RIG / 'amigapi_fixture5.log'
    dump = paths.RIG / 'amigapi_dump5'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    kit = VTC / 'pi-kit'
    sess = kit / 'var' / 'amiga-pi' / 'sessions'
    made_assign = False

    def pi_run(tag, args):
        for f in ('pi5%s.out' % tag, 'pi5%s.rc' % tag):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ended = run_window('pi5' + tag, 'amiga-pi step 5%s' % tag, [
            'Echo "amiga-pi step 5%s: %s"' % (tag, args),
            'CD RAM:',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 %s >VTC:pi5%s.out' % (
                port, args, tag),
            'Echo "rc=$RC" >VTC:pi5%s.rc' % tag,
            'Type VTC:pi5%s.out' % tag], 'pi5%s-done' % tag)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        return ended, rd('pi5%s.out' % tag), rd('pi5%s.rc' % tag).strip()

    def session_files():
        return sorted(sess.glob('*/*.jsonl')) if sess.exists() else []

    try:
        c.run('MakeDir VTC:pi-kit VTC:pi-kit/bin ENVARC:up-term')
        with open(VTC / 'pi-kit-dir', 'w') as f:
            f.write('VTC:pi-kit\n')
        c.run('Copy VTC:pi-kit-dir ENVARC:up-term/Dir')
        c.run('Copy VTC:pi-kit-dir ENV:up-term/Dir')
        made_assign = True
        # 5a: a new session
        ended, text, rc = pi_run('a', '-p hello')
        check('5a the run ended', ended)
        check('5a exit code 0', rc == 'rc=0', repr(rc))
        check('5a UP-Term: made from the Dir file', 'UP-TERM' in assigns())
        check('5a the answer printed', 'Hello from the Amiga!' in text, repr(text[:200]))
        files = session_files()
        check('5a one session file under UP-Term:var/amiga-pi/sessions', len(files) == 1, repr(files))
        # FS-UAE keeps protection bits and comments in <name>.uaem beside a file: not an Amiga name
        names = [p.name for p in sess.rglob('*') if not p.name.endswith('.uaem')] if sess.exists() else []
        check('5a every name at most 30 characters', names and max(len(n) for n in names) <= 30, repr(names))
        check('5a the file name is <YYYYMMDDTHHMMSS>_<8 hex>.jsonl',
              files and re.fullmatch(r'\d{8}T\d{6}_[0-9a-f]{8}\.jsonl', files[0].name) is not None,
              repr([f.name for f in files]))
        entries = []
        if files:
            entries = [json.loads(l) for l in files[0].read_text(encoding='utf-8').splitlines() if l.strip()]
        head = entries[0] if entries else {}
        shape = [(e.get('type'), (e.get('message') or {}).get('role')) for e in entries[1:]]
        check('5a the header is pi\'s v3', head.get('type') == 'session' and head.get('version') == 3 and
              re.fullmatch(r'[0-9a-f-]{36}', str(head.get('id'))) is not None, repr(head))
        check('5a timestamps are the Amiga\'s local time, no Z',
              re.fullmatch(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}', str(head.get('timestamp'))) is not None and
              all(re.fullmatch(r'\d{4}-\d\d-\d\dT\d\d:\d\d:\d\d\.\d{3}', e.get('timestamp', '')) for e in entries[1:]),
              repr(head.get('timestamp')))
        check('5a entries: model, thinking level, system, user, answer',
              shape == [('model_change', None), ('thinking_level_change', None), ('message', 'system'),
                        ('message', 'user'), ('message', 'assistant')], repr(shape))
        check('5a the answer is in the file', 'Hello from the Amiga!' in (files[0].read_text(encoding='utf-8')
                                                                           if files else ''))
        # 5b: continue it
        ended, text, rc = pi_run('b', '-c -p again')
        check('5b the run ended', ended)
        check('5b exit code 0', rc == 'rc=0', repr(rc))
        files = session_files()
        check('5b still one session file', len(files) == 1, repr(files))
        entries = [json.loads(l) for l in files[0].read_text(encoding='utf-8').splitlines() if l.strip()] if files else []
        roles = [(e.get('message') or {}).get('role') for e in entries[1:] if e.get('type') == 'message']
        check('5b one system message, two user messages, two answers',
              roles == ['system', 'user', 'assistant', 'user', 'assistant'], repr(roles))
        bodies = sorted(dump.glob('*.json'))
        last = body_of(dump, len(bodies)) if bodies else {}
        said = json.dumps(last)
        check('5b the second request carries the first exchange', 'hello' in said and 'Hello from the Amiga!' in said
              and 'again' in said, repr(said[-300:]))
        # 5c: pi's own code opens the file
        oracle = AMIGA_PI.parents[2] / 'tools' / 'pi_oracle' / 'gen.ts'     # the tree the binary was built in
        pi_dir = os.environ.get('PI_ORACLE') or str(pathlib_home() / 'Code/amiga-pi/build/pi-oracle/oracle')
        if files and oracle.exists() and os.path.isdir(pi_dir):
            r = subprocess.run(['node', str(oracle), '--dump', str(files[0])], capture_output=True, text=True,
                               env=dict(os.environ, PI_ORACLE=pi_dir))
            head5 = ' '.join(r.stdout.split('\n')[:2])
            check('5c pi\'s SessionManager opened the Amiga\'s file', r.returncode == 0 and r.stdout.startswith('open ok'),
                  repr((r.returncode, r.stdout[:200], r.stderr[-200:])))
            print('[INFO] 5c pi says: %s' % head5, flush=True)
        else:
            print('[INFO] 5c skipped: the pi oracle (%s) is not here' % pi_dir, flush=True)
        # 5d: --no-session
        before = [(f.name, f.stat().st_size) for f in session_files()]
        ended, text, rc = pi_run('d', '--no-session -p ephemeral')
        check('5d the run ended, exit code 0', ended and rc == 'rc=0', repr((ended, rc)))
        check('5d nothing written', [(f.name, f.stat().st_size) for f in session_files()] == before)
    finally:
        fx.terminate()
        if made_assign:
            c.run('Delete ENVARC:up-term/Dir ENV:up-term/Dir QUIET')
            if 'UP-TERM' not in had:
                c.run('Assign UP-Term: REMOVE')
            c.run('Delete VTC:pi-kit ALL QUIET')
        c.run('Delete RAM:pi5a-done RAM:pi5b-done RAM:pi5d-done QUIET')


def mac_lan_ip():
    """the Mac's own LAN address (en0, en1), 0 none"""
    for ifc in ('en0', 'en1'):
        r = subprocess.run(['ipconfig', 'getifaddr', ifc], capture_output=True, text=True)
        if r.returncode == 0 and r.stdout.strip():
            return r.stdout.strip()
    return None


def step10(port):
    tree = AMIGA_PI.parents[2]
    echo = tree / 'build' / 'amiga' / 'mcp-echo'
    fake = tree / 'tools' / 'fake_mcp.py'
    for b, hint in ((AMIGA_PI, 'make -C %s amiga' % tree), (echo, 'make -C %s build/amiga/mcp-echo' % tree),
                    (fake, 'the amiga-pi tree with phase K')):
        if not b.exists():
            sys.exit('[ERROR] %s missing: %s' % (b, hint))
    ip = mac_lan_ip()
    check('10 the Mac has a LAN address', ip is not None and not ip.startswith('127.'), repr(ip))
    if not ip:
        return
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    shutil.copy(echo, VTC / 'mcp-echo')
    mlog = paths.RIG / 'amigapi_mcp.log'
    if mlog.exists():
        mlog.unlink()
    srv = subprocess.Popen([sys.executable, '-I', str(fake), '--http', '0', '--bind', '0.0.0.0', '--log', str(mlog)],
                           stdin=subprocess.PIPE, stdout=subprocess.PIPE, text=True)
    first = srv.stdout.readline()
    mport = int(first.split()[1]) if first.startswith('port ') else 0
    log = paths.RIG / 'amigapi_fixture10.log'
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01'], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    before = c.run('Status')[1].count('mcp-echo')     # servers left by something else are not this run's
    try:
        for f in ('pi10.out', 'pi10.rc', 'pi10.status'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        (VTC / 'pi10-mcp.json').write_text(
            '{"mcpServers":{"fake":{"url":"http://%s:%d/mcp"},"amiga":{"command":"VTC:mcp-echo"}}}\n' % (ip, mport))
        ok = run_window('pi10', 'amiga-pi step 10', [
            'Echo "amiga-pi step 10: MCP over LAN http (the Mac) and over PTY: (the Amiga)"',
            'If NOT EXISTS ENVARC:amiga-pi',
            'MakeDir ENVARC:amiga-pi',
            'EndIf',
            'Copy VTC:pi10-mcp.json ENVARC:amiga-pi/mcp.json QUIET',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session '
            '-p "use the mcp tools" >VTC:pi10.out' % port,
            'Echo "rc=$RC" >VTC:pi10.rc',
            'Wait 3',
            'Status >VTC:pi10.status',
            'Delete ENVARC:amiga-pi/mcp.json QUIET',
            'Type VTC:pi10.out'], 'pi10-done')
        check('10 the window ended', ok)
        time.sleep(4)
        read = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        text, rc, status = read('pi10.out'), read('pi10.rc').strip(), read('pi10.status')
        check('10a exit code 0', rc == 'rc=0', repr((rc, text[:400])))
        check('10a the final text', 'SetPatch' in text, repr(text[:400]))
        msgs = [json.loads(l) for l in open(mlog)] if mlog.exists() else []
        inits = [m for m in msgs if m['msg'].get('method') == 'initialize']
        calls = [m for m in msgs if m['msg'].get('method') == 'tools/call']
        check('10b one initialize, no session id yet', len(inits) == 1 and 'mcp-session-id' not in inits[0]['headers'],
              repr(inits))
        check('10b one tools/call with the arguments and the session',
              len(calls) == 1 and calls[0]['msg']['params'] == {'name': 'echo', 'arguments': {'text': 'from the Amiga'}}
              and calls[0]['headers'].get('mcp-session-id') == 'sess-1', repr(calls))
        check('10b from the LAN address', bool(calls) and calls[0]['headers'].get('peer') == ip, repr(calls))
        lines = open(log).read().splitlines()
        mac = [l for l in lines if 'tool_result call_McpMac:' in l]
        ami_ = [l for l in lines if 'tool_result call_McpAmiga:' in l]
        check("10b the Mac's result reached the model", len(mac) == 1 and 'echo: from the Amiga' in mac[0], repr(mac))
        check("10c the Amiga's server answered over PTY:",
              len(ami_) == 1 and 'echo: over PTY: (3 requests)' in ami_[0], repr(ami_))
        for l in mac + ami_:
            print('[INFO] ' + l.strip(), flush=True)
        check('10d the server ended with amiga-pi', status and status.count('mcp-echo') == before,
              repr((before, status[-400:])))
    finally:
        fx.terminate()
        srv.stdin.close()
        srv.wait(timeout=10)
        c.run('Delete RAM:pi10-done VTC:pi10.dos VTC:pi10-mcp.json ENVARC:amiga-pi/mcp.json QUIET')


def system_text(body):
    """the system and developer messages of a chat/completions body, joined"""
    return '\n'.join(str(m.get('content')) for m in body.get('messages', []) if m.get('role') in ('system', 'developer'))


def step7(port):
    import re
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture7.log'
    dump = paths.RIG / 'amigapi_dump7'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    (VTC / 'g7-outer.md').write_text('RIG-OUTER-RULES-7\n')
    (VTC / 'g7-inner.md').write_text('RIG-INNER-RULES-7\n')
    (VTC / 'g7-sys.md').write_text('RIG-PROJECT-SYSTEM-7')
    # a trust.json that was there already would make 7d pass whatever the run wrote: put it aside, restore it after
    had_trust = there('ENVARC:amiga-pi/trust.json')
    if had_trust:
        c.run('Rename ENVARC:amiga-pi/trust.json ENVARC:amiga-pi/trust.json.g7bak')
    try:
        for f in ('pi7.out', 'pi7.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ended, req = run_watched('pi7', 'amiga-pi step 7', [
            'Echo "amiga-pi step 7: AGENTS.md at RAM: and RAM:proj, .pi/SYSTEM.md untrusted then -a"',
            'Delete RAM:AGENTS.md RAM:proj ALL QUIET',
            'MakeDir RAM:proj RAM:proj/.pi',
            'Copy VTC:g7-outer.md RAM:AGENTS.md QUIET',
            'Copy VTC:g7-inner.md RAM:proj/AGENTS.md QUIET',
            'Copy VTC:g7-sys.md RAM:proj/.pi/SYSTEM.md QUIET',
            'CD RAM:proj',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session -p hello '
            '>VTC:pi7.out' % port,
            'Echo "rc=$RC" >VTC:pi7.rc',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session -a -p hello '
            '>VTC:pi7a.out' % port,
            'Echo "rc=$RC" >VTC:pi7a.rc',
            'CD RAM:',
            'Delete RAM:AGENTS.md RAM:proj ALL QUIET',
            'Type VTC:pi7.out'], 'pi7-done')
        check('7 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        check('7a exit codes 0', rd('pi7.rc').strip() == 'rc=0' and rd('pi7a.rc').strip() == 'rc=0',
              repr((rd('pi7.rc'), rd('pi7a.rc'), rd('pi7.out')[:300])))
        plain, trusted = system_text(body_of(dump, 1)), system_text(body_of(dump, 2))
        o, i = plain.find('RIG-OUTER-RULES-7'), plain.find('RIG-INNER-RULES-7')
        check('7a both AGENTS.md texts, RAM:AGENTS.md first', 0 <= o < i, repr(plain[-400:]))
        check('7a each in a project_instructions tag naming its file',
              re.search(r'<project_instructions path="[^"]*:AGENTS\.md">\nRIG-OUTER', plain) is not None and
              re.search(r'<project_instructions path="[^"]*:proj/AGENTS\.md">\nRIG-INNER', plain) is not None,
              repr(plain[-500:]))
        check('7b untrusted: the default preamble stays, SYSTEM.md is not sent',
              'expert coding assistant' in plain and 'RIG-PROJECT-SYSTEM-7' not in plain, repr(plain[:200]))
        check('7b no requester', not req, repr(req))
        check('7c -a: SYSTEM.md replaces the preamble', 'RIG-PROJECT-SYSTEM-7' in trusted and
              'expert coding assistant' not in trusted, repr(trusted[:300]))
        check('7c -a: the AGENTS.md texts are still there', 'RIG-OUTER-RULES-7' in trusted and
              'RIG-INNER-RULES-7' in trusted)
        check('7d no trust.json written', not there('ENVARC:amiga-pi/trust.json'))
    finally:
        fx.terminate()
        if had_trust:
            c.run('Delete ENVARC:amiga-pi/trust.json QUIET')
            c.run('Rename ENVARC:amiga-pi/trust.json.g7bak ENVARC:amiga-pi/trust.json')
        c.run('Delete RAM:pi7-done VTC:pi7.dos VTC:g7-outer.md VTC:g7-inner.md VTC:g7-sys.md RAM:AGENTS.md QUIET')
        c.run('Delete RAM:proj ALL QUIET')


def pathlib_home():
    return pathlib.Path.home()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8092, help="the fixture's port (8080 may be another rig's)")
    ap.add_argument('--step', type=int, choices=(1, 2, 3, 4, 5, 7, 10), help='one step only (default: all)')
    a = ap.parse_args()
    if a.step in (None, 1):
        step1(a.port)
    if a.step in (None, 2):
        step2(a.port + 1)
    if a.step in (None, 3):
        step3(a.port + 2)
    if a.step in (None, 4):
        step4(a.port + 3)
    if a.step in (None, 5):
        step5(a.port + 4)
    if a.step in (None, 7):
        step7(a.port + 6)
    if a.step in (None, 10):
        step10(a.port + 9)
    print('amigapi_rig: %s' % ('PASS' if not fails else 'FAIL: ' + ', '.join(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
