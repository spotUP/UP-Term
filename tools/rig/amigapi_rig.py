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

Step 6 (F6): the model switch. models.json (in VTC:pi6agent, PI_CODING_AGENT_DIR) names two providers on the
fixture: fixa (anthropic-messages, a reasoning model) and fixo (openai-completions, apiKey "!Echo rig-test-key").
amiga-pi reads its lines from a file through "<": a prompt, "/model fixo/m2", a prompt.
  6a the fixture saw POST .../messages first, then .../chat/completions (both backends, in order)
  6b the first request asks for budget thinking, the second names model m2 and carries the first answer
  6c the apiKey command ran on the Amiga (the plain-http notice for fixo says a key was resolved)
  6d the session holds model_change, thinking_level_change (medium), system, user, answer, model_change,
     thinking_level_change (off), user, answer
  6e the screen says "Model: fixo/m2, thinking off"
Visible windows titled "amiga-pi step 6".

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

Step 8 (I6): skills. ENVARC:amiga-pi/skills holds rig-skill-8 (with a description) and rig-hidden-8 (disable-model-invocation:
true); "amiga-pi --no-session -p ..." against the fixture, three runs, RAM: as the current directory:
  8a exit code 0, the request's system prompt holds the <skills> section: <name>rig-skill-8</name>, its description and
     <location>ENVARC:amiga-pi/skills/rig-skill-8/SKILL.md</location>, after the preamble and before <cwd>; rig-hidden-8 is not in it
  8b "/skill:rig-skill-8 do the rig thing": the user message is the <skill name="rig-skill-8" location=...> block with
     "References are relative to ..." and the skill's text, then "do the rig thing"; the hidden skill loads the same way
  8c --no-skills: no <skills> section is sent
The skills are removed again (a skills directory that was there is put aside first and restored after). Visible windows titled
"amiga-pi step 8".

Step 9 (J4): prompt templates. RAM:proj/.pi/prompts/review.md ("Please review $1 with ${2:-nothing else} for
RIG-TEMPLATE-9.") and RAM:proj as the current directory; "amiga-pi --no-session -p '/review src'":
  9a exit codes 0, the project is not trusted in print mode (nothing decided): its template is not read, the request
     carries the line as typed ("/review src") and not the template's text
  9b the same with -a: the request carries "Please review src with nothing else for RIG-TEMPLATE-9." (the argument
     in place, the default used) and no longer the typed "/review src"
Everything made is removed again. Visible windows titled "amiga-pi step 9".

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

Step 11 (L10): Lua extensions on the Amiga. ENVARC:amiga-pi/extensions holds a-block.lua (blocks the bash tool with "no shell today",
adding the 13-digit turn_start timestamp and the literal 1760000000123 as json.encode writes them) and b-bad.lua (a handler that raises
an error); "amiga-pi --no-session -p 'try the blocked bash'" against the fixture, whose openai_bash.sse calls bash with a command
that would write RAM:pi-blocked.txt, then answers openai_final.sse; run through vsh so stderr goes to a file:
  11a exit code 0, the final text printed, two requests (the session went on after the other extension's error)
  11b the bash call was blocked: RAM:pi-blocked.txt was not written, and the second request's tool message holds
      the reason "no shell today" (pi's text for a blocked call)
  11c b-bad.lua's error is reported as `Extension "<path>" error: <message>` on stderr and the run continued (11a)
  11d (L13) the reason carries the turn_start timestamp as 13 digits and 1760000000123 exactly (64-bit Lua numbers, JSON written
      as JS writes it)
The extensions are removed again (an extensions directory that was there is put aside first and restored after). A visible
window titled "amiga-pi step 11". Needs rig 2 or another free rig, never 3.

Step 12 (M9): every adapter of amiga-pi against the fixture's routes. models.json (in VTC:pi12agent, PI_CODING_AGENT_DIR)
names one provider per api: anthropic-messages, openai-completions, openai-responses, google-generative-ai,
azure-openai-responses (a non-Azure base URL), bedrock-converse-stream (AWS keys in ENV:, signed; its provider carries no apiKey: a
stored key is the bearer token and would keep the request unsigned over plain http), openai-codex-responses (/codex/responses,
the fixture logs pi's originator header). `amiga-pi -p hello <NIL:`
each; per adapter: exit code 0, exactly one request on the adapter's route (key header none over plain http, sigv4 for
bedrock), the answer's text printed.

Step 13 (N8): the TUI in an UP-Term window. amiga-pi runs in a visible XCON: window titled "amiga-pi step 13a/b/c" and
the keys are typed into it the way claude_rig.py types them (tools/rig/typewin.py); the screen is read back as TEXT
(Right Amiga A, Right Amiga C, VTC:snipprobe CLIP <file>), the rest from the fixture's request bodies and the
session file. models.json names fixa/m1 (anthropic-messages, reasoning) and fixo/m2 (openai-completions) on the
fixture, settings.json enables both. The fixture answers slowly (--delay 0.4: about 11 s a reply).
  13a "hello", then while it works "steer me" + Enter and "and then this" + Alt+Enter (Left Amiga + Return, UP-Term's Meta key):
      the screen holds "Steering: steer me" and "Follow-up: and then this"; the second request carries the
      steering text (not the first), the third the follow-up; the session holds the three user messages in order
  13a (also) Ctrl+D twice ends amiga-pi
  13b Shift+Tab: "Thinking level: ..." on the screen and a second thinking_level_change in the session;
      Ctrl+P: "Switched to ..." and model_change m2 (then thinking off) in the session
  13c /tree: the selector shows ("Session Tree", "Type to search:")
Step 14 (H8): `amiga-pi --mode rpc` fed from a file through `<`, its stdout in a file. The fixture answers the model
fake-model with tests/claude/pirpc_text.sse (the golden's answer); the output file is compared line by line with
~/Code/amiga-pi tests/pi/rpc/prompt.out, masked as that suite's tests/test_pi_modes.c masks (mask_rpc below), then
get_messages and get_session_stats must answer. 14e (HF1): `--mode json` with a broken models.json in its own agent dir, run through vsh with 2> to a file:
the "[WARN] ..." diagnostic is ONE line in the error file and not in the stdout JSONL. Verdicts in build/<rig>/amigapi14.verdicts.json, output kept as
amigapi14-out.jsonl.
Verdicts are recorded in build/rig3/amigapi13.verdicts.json (build/<rig>/ of the rig used): a check that passed is
not run again; a phase with every check recorded is skipped. --force runs everything again.

  make build/amiga/pi-llmprobe build/amiga/Claude build/amiga/snipprobe; make -C ~/Code/amiga-pi amiga
  UPTERM_RIG=2 python3 -I tools/rig/rig.py start --max
  UPTERM_RIG=2 python3 -I tools/rig/amigapi_rig.py [--port 8092] [--step 1|2|3|4|5|6|7|8|9|10|11|13|14] [--force]
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
import typewin as tw  # noqa: E402
import piscript  # noqa: E402

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
            f.write(piscript.script_text([
                'Echo "amiga-pi step 1: one prompt through llm_request to the fixture"',
                'VTC:pi-llmprobe http://127.0.0.1:%d/v1 qwen2.5-coder:7b sk-must-not-be-sent hello >VTC:pi1.out' % port,
                'Type VTC:pi1.out'], 'pi1-done'))
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
        f.write(piscript.script_text(lines, done))
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


RIG_ASSIGN = False   # drop_rig_assign took the rig's own UP-Term: (VTC:) away; restore_rig_assign puts it back


def drop_rig_assign(had):
    """Steps 4-6 start from a rig with no UP-Term: and no Dir file. rig.py's boot script assigns UP-Term:
    to VTC:upterm for the glyph pages (UT1) when no kit is installed; take that one away for the step."""
    global RIG_ASSIGN
    if 'UP-TERM' in had and ami.read_file('ENVARC:up-term/Dir') is None:
        c.run('Assign UP-Term: REMOVE')
        RIG_ASSIGN = True
        return had - {'UP-TERM'}
    return had


def restore_rig_assign():
    global RIG_ASSIGN
    if RIG_ASSIGN:
        c.run('Assign UP-Term: REMOVE')
        c.run('Assign UP-Term: VTC:upterm')
        RIG_ASSIGN = False


def run_watched(name, title, lines, done):
    """run_window, polling amiagent's UI tree for a requester the while: (ended, requester titles seen)"""
    c.run('Delete RAM:%s QUIET' % done)
    with open(VTC / (name + '.dos'), 'w') as f:
        f.write(piscript.script_text(lines, done))
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
    had = drop_rig_assign(had)
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
            restore_rig_assign()
            c.run('Delete VTC:pi-kit ALL QUIET')
        c.run('Delete RAM:pi4a-done RAM:pi4b-done RAM:pi4d-done RAM:pi4e-done RAM:pi-wide.bmp QUIET')


def step5(port):
    import json
    import re
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    had = assigns()
    had = drop_rig_assign(had)
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
            restore_rig_assign()
            c.run('Delete VTC:pi-kit ALL QUIET')
        c.run('Delete RAM:pi5a-done RAM:pi5b-done RAM:pi5d-done QUIET')


def step6(port):
    import json
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    had = assigns()
    had = drop_rig_assign(had)
    saved_dir = ami.read_file('ENVARC:up-term/Dir')
    check('6 no UP-Term: on this rig to begin with', 'UP-TERM' not in had and saved_dir is None,
          repr(('UP-TERM' in had, saved_dir)))
    log = paths.RIG / 'amigapi_fixture6.log'
    dump = paths.RIG / 'amigapi_dump6'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    kit = VTC / 'pi-kit'
    sess = kit / 'var' / 'amiga-pi' / 'sessions'
    agent = VTC / 'pi6agent'
    base = 'http://127.0.0.1:%d' % port   # anthropic-messages: pi's baseUrl has no /v1, the backend adds /v1/messages
    made_assign = False
    try:
        agent.mkdir(exist_ok=True)
        (agent / 'models.json').write_text(json.dumps({'providers': {
            'fixa': {'baseUrl': base, 'api': 'anthropic-messages',
                     'models': [{'id': 'm1', 'reasoning': True, 'contextWindow': 200000, 'maxTokens': 20000}]},
            'fixo': {'baseUrl': base + '/v1', 'api': 'openai-completions', 'apiKey': '!Echo rig-test-key',
                     'models': [{'id': 'm2', 'contextWindow': 32000}]}}}))
        (VTC / 'pi6.in').write_text('hello\n/model fixo/m2\nagain\n')
        for f in ('pi6.out', 'pi6.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        c.run('MakeDir VTC:pi-kit VTC:pi-kit/bin ENVARC:up-term')
        with open(VTC / 'pi-kit-dir', 'w') as f:
            f.write('VTC:pi-kit\n')
        c.run('Copy VTC:pi-kit-dir ENVARC:up-term/Dir')
        c.run('Copy VTC:pi-kit-dir ENV:up-term/Dir')
        made_assign = True
        ended = run_window('pi6', 'amiga-pi step 6', [
            'Echo "amiga-pi step 6: hello, /model fixo/m2, again"',
            'SetEnv PI_CODING_AGENT_DIR VTC:pi6agent',
            'CD RAM:',
            'VTC:amiga-pi --model fixa/m1 --no-tools <VTC:pi6.in >VTC:pi6.out',
            'Echo "rc=$RC" >VTC:pi6.rc',
            'Type VTC:pi6.out'], 'pi6-done')
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        text, rc = rd('pi6.out'), rd('pi6.rc').strip()
        check('6 the run ended, exit code 0', ended and rc == 'rc=0', repr((ended, rc)))
        reqs = [l.split()[1] for l in open(log) if l.startswith('[INFO] /v1/')]
        check('6a the fixture saw .../messages, then .../chat/completions',
              len(reqs) == 2 and reqs[0].endswith('/messages') and reqs[1].endswith('/chat/completions'), repr(reqs))
        first, second = body_of(dump, 1), body_of(dump, 2)
        check('6b the first request: model m1 with budget thinking',
              first.get('model') == 'm1' and (first.get('thinking') or {}).get('type') == 'enabled'
              and (first.get('thinking') or {}).get('budget_tokens') == 8192, repr(first.get('thinking')))
        said = json.dumps(second)
        check('6b the second request: model m2, the first exchange and the new prompt',
              second.get('model') == 'm2' and 'hello' in said and 'Hello from the Amiga!' in said and 'again' in said
              and 'reasoning_effort' not in second, repr(said[-300:]))
        check('6c the apiKey command ran on the Amiga', 'key for fixo not sent: plain http' in text, repr(text[:300]))
        check('6e the screen says what the switch did', 'Model: fixo/m2, thinking off' in text, repr(text[-300:]))
        files = sorted(sess.glob('*/*.jsonl')) if sess.exists() else []
        entries = [json.loads(l) for l in files[0].read_text(encoding='utf-8').splitlines() if l.strip()] if files else []
        shape = []
        for e in entries[1:]:
            t = e.get('type')
            shape.append(t if t != 'message' else 'message/' + e['message']['role'])
            if t == 'thinking_level_change':
                shape[-1] += ':' + e.get('thinkingLevel', '')
            if t == 'model_change':
                shape[-1] += ':' + e.get('modelId', '')
        check('6d the session: pi\'s entries for the switch',
              shape == ['model_change:m1', 'thinking_level_change:medium', 'message/system', 'message/user',
                        'message/assistant', 'model_change:m2', 'thinking_level_change:off', 'message/user',
                        'message/assistant'], repr(shape))
    finally:
        fx.terminate()
        c.run('Delete ENVARC:up-term/Dir ENV:up-term/Dir QUIET')
        if made_assign:
            if 'UP-TERM' not in had:
                c.run('Assign UP-Term: REMOVE')
            restore_rig_assign()
            c.run('Delete VTC:pi-kit ALL QUIET')
        c.run('Delete RAM:pi6-done VTC:pi6agent ALL QUIET')
        shutil.rmtree(agent, ignore_errors=True)


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


def user_text(body):
    """the user messages of a chat/completions body, joined"""
    return '\n'.join(str(m.get('content')) for m in body.get('messages', []) if m.get('role') == 'user')


def step9(port):
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture9.log'
    dump = paths.RIG / 'amigapi_dump9'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    (VTC / 'j9-review.md').write_text('Please review $1 with ${2:-nothing else} for RIG-TEMPLATE-9.\n')
    # a trust.json that was there already would trust the project in the first run: put it aside, restore it after
    had_trust = there('ENVARC:amiga-pi/trust.json')
    if had_trust:
        c.run('Rename ENVARC:amiga-pi/trust.json ENVARC:amiga-pi/trust.json.j9bak')
    try:
        for f in ('pi9.out', 'pi9.rc', 'pi9a.out', 'pi9a.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ended, req = run_watched('pi9', 'amiga-pi step 9', [
            'Echo "amiga-pi step 9: a prompt template from RAM:proj/.pi/prompts, untrusted then -a"',
            'Delete RAM:proj ALL QUIET',
            'MakeDir RAM:proj RAM:proj/.pi RAM:proj/.pi/prompts',
            'Copy VTC:j9-review.md RAM:proj/.pi/prompts/review.md QUIET',
            'CD RAM:proj',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session '
            '-p "/review src" >VTC:pi9.out' % port,
            'Echo "rc=$RC" >VTC:pi9.rc',
            'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session -a '
            '-p "/review src" >VTC:pi9a.out' % port,
            'Echo "rc=$RC" >VTC:pi9a.rc',
            'CD RAM:',
            'Delete RAM:proj ALL QUIET',
            'Type VTC:pi9a.out'], 'pi9-done')
        check('9 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        check('9a exit codes 0', rd('pi9.rc').strip() == 'rc=0' and rd('pi9a.rc').strip() == 'rc=0',
              repr((rd('pi9.rc'), rd('pi9a.rc'), rd('pi9.out')[:300])))
        plain, trusted = user_text(body_of(dump, 1)), user_text(body_of(dump, 2))
        check('9a untrusted: the line goes as typed', '/review src' in plain and 'RIG-TEMPLATE-9' not in plain, repr(plain[-300:]))
        check('9a no requester', not req, repr(req))
        check('9b -a: the expansion is sent, the argument in place and the default used',
              'Please review src with nothing else for RIG-TEMPLATE-9.' in trusted, repr(trusted[-300:]))
        check('9b -a: the typed line is not', '/review src' not in trusted, repr(trusted[-300:]))
    finally:
        fx.terminate()
        if had_trust:
            c.run('Delete ENVARC:amiga-pi/trust.json QUIET')
            c.run('Rename ENVARC:amiga-pi/trust.json.j9bak ENVARC:amiga-pi/trust.json')
        c.run('Delete RAM:pi9-done VTC:pi9.dos VTC:j9-review.md QUIET')
        c.run('Delete RAM:proj ALL QUIET')


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




def user_text(body):
    """the user messages of a chat/completions body, joined"""
    out = []
    for m in body.get('messages', []):
        if m.get('role') != 'user':
            continue
        cont = m.get('content')
        out.append(cont if isinstance(cont, str) else '\n'.join(str(b.get('text', '')) for b in cont))
    return '\n'.join(out)


def step8(port):
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture8.log'
    dump = paths.RIG / 'amigapi_dump8'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    (VTC / 'g8-skill.md').write_text('---\nname: rig-skill-8\ndescription: Rig skill for step 8 & the XML escapes\n---\n'
                                     'RIG-SKILL-BODY-8: say the rig word.\n')
    (VTC / 'g8-hidden.md').write_text('---\nname: rig-hidden-8\ndescription: Hidden from the model\n'
                                      'disable-model-invocation: true\n---\nRIG-HIDDEN-BODY-8\n')
    # a skills directory that was there already would make 8a pass or fail on its contents: put it aside, restore it after
    had_skills = there('ENVARC:amiga-pi/skills')
    if had_skills:
        c.run('Rename ENVARC:amiga-pi/skills ENVARC:amiga-pi/skills.g8bak')
    try:
        for f in ('pi8.out', 'pi8.rc', 'pi8b.out', 'pi8b.rc', 'pi8c.out', 'pi8c.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        base = 'VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session' % port
        ended, req = run_watched('pi8', 'amiga-pi step 8', [
            'Echo "amiga-pi step 8: skills in ENVARC:amiga-pi/skills, /skill:name, --no-skills"',
            'Failat 21',                  # ENVARC:amiga-pi is there already: MakeDir says so with RC 10
            'MakeDir ENVARC:amiga-pi ENVARC:amiga-pi/skills ENVARC:amiga-pi/skills/rig-skill-8 ENVARC:amiga-pi/skills/rig-hidden-8',
            'Copy VTC:g8-skill.md ENVARC:amiga-pi/skills/rig-skill-8/SKILL.md QUIET',
            'Copy VTC:g8-hidden.md ENVARC:amiga-pi/skills/rig-hidden-8/SKILL.md QUIET',
            'CD RAM:',
            base + ' -p hello >VTC:pi8.out',
            'Echo "rc=$RC" >VTC:pi8.rc',
            base + ' -p "/skill:rig-skill-8 do the rig thing" >VTC:pi8b.out',
            'Echo "rc=$RC" >VTC:pi8b.rc',
            base + ' --no-skills -p hello >VTC:pi8c.out',
            'Echo "rc=$RC" >VTC:pi8c.rc',
            'Delete ENVARC:amiga-pi/skills ALL QUIET',
            'Type VTC:pi8.out'], 'pi8-done')
        check('8 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        check('8a exit codes 0', all(rd(f).strip() == 'rc=0' for f in ('pi8.rc', 'pi8b.rc', 'pi8c.rc')),
              repr((rd('pi8.rc'), rd('pi8b.rc'), rd('pi8c.rc'), rd('pi8.out')[:300])))
        plain, skilled, off = system_text(body_of(dump, 1)), body_of(dump, 2), system_text(body_of(dump, 3))
        check('8a the <skills> section names rig-skill-8 with its description and location',
              '<name>rig-skill-8</name>' in plain and '<description>Rig skill for step 8 &amp; the XML escapes</description>' in plain and
              '<location>ENVARC:amiga-pi/skills/rig-skill-8/SKILL.md</location>' in plain, repr(plain[-700:]))
        check('8a after the preamble, before <cwd>', 0 <= plain.find('expert coding assistant') < plain.find('<skills>') < plain.find('<cwd>'))
        check('8a the hidden skill is not in it', 'rig-hidden-8' not in plain)
        ut = user_text(skilled)
        check('8b /skill:rig-skill-8 sends the skill block and the arguments',
              '<skill name="rig-skill-8" location="ENVARC:amiga-pi/skills/rig-skill-8/SKILL.md">\nReferences are relative to ' in ut and
              'RIG-SKILL-BODY-8: say the rig word.\n</skill>\n\ndo the rig thing' in ut and '/skill:' not in ut, repr(ut[:500]))
        check('8c --no-skills: no <skills> section', '<skills>' not in off and 'rig-skill-8' not in off, repr(off[-300:]))
    finally:
        fx.terminate()
        c.run('Delete ENVARC:amiga-pi/skills ALL QUIET')
        if had_skills:
            c.run('Rename ENVARC:amiga-pi/skills.g8bak ENVARC:amiga-pi/skills')
        c.run('Delete RAM:pi8-done VTC:pi8.dos VTC:g8-skill.md VTC:g8-hidden.md QUIET')


LUA_BLOCK_11 = """local ts = nil
pi.on("turn_start", function(event, ctx) ts = event.timestamp end)
pi.on("tool_call", function(event, ctx)
  if event.toolName == "bash" then
    return { block = true, reason = "no shell today ts=" .. json.encode(ts) .. " big=" .. json.encode(json.decode("1760000000123")) }
  end
end)
"""
LUA_BAD_11 = """pi.on("agent_start", function(event, ctx) error("rig boom", 0) end)
"""


def step11(port):
    import re
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture11.log'
    dump = paths.RIG / 'amigapi_dump11'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    (VTC / 'x11-block.lua').write_text(LUA_BLOCK_11)
    (VTC / 'x11-bad.lua').write_text(LUA_BAD_11)
    (VTC / 'pi11.sh').write_text('VTC:amiga-pi --model fixture/qwen2.5-coder:7b --base-url http://127.0.0.1:%d/v1 --no-session '
                                 '-p "try the blocked bash" >VTC:pi11.out 2>VTC:pi11.err\necho "rc=$?" >VTC:pi11.rc\n' % port)
    # an extensions directory that was there already would add its own handlers: put it aside, restore it after
    had_ext = there('ENVARC:amiga-pi/extensions')
    if had_ext:
        c.run('Rename ENVARC:amiga-pi/extensions ENVARC:amiga-pi/extensions.l10bak')
    # MakeDir on a directory that is there complains (and an owner watching the window sees it): make only what is missing
    for d in ('ENVARC:amiga-pi', 'ENVARC:amiga-pi/extensions'):
        if not there(d):
            c.run('MakeDir ' + d)
    try:
        for f in ('pi11.out', 'pi11.err', 'pi11.rc'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        ended, req = run_watched('pi11', 'amiga-pi step 11', [
            'Echo "amiga-pi step 11: Lua extensions in ENVARC:amiga-pi/extensions, one blocks bash, one raises an error"',
            'Copy VTC:x11-block.lua ENVARC:amiga-pi/extensions/a-block.lua QUIET',
            'Copy VTC:x11-bad.lua ENVARC:amiga-pi/extensions/b-bad.lua QUIET',
            'Delete RAM:pi-blocked.txt QUIET',
            'CD RAM:',
            'VTC:vsh VTC:pi11.sh <NIL:',   # the Shell has no 2>; vsh does
            'Delete ENVARC:amiga-pi/extensions ALL QUIET',
            'Type VTC:pi11.out'], 'pi11-done')
        check('11 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        out, err, rc = rd('pi11.out'), rd('pi11.err'), rd('pi11.rc').strip()
        b1, b2 = body_of(dump, 1), body_of(dump, 2)
        tool_msgs = [m for m in b2.get('messages', []) if m.get('role') == 'tool']
        reason = str(tool_msgs[-1].get('content')) if tool_msgs else ''
        check('11a exit code 0, the final text printed, two requests', rc == 'rc=0' and 'Startup-Sequence' in out and b1 and b2
              and not body_of(dump, 3), repr((rc, out[:200], err[:200])))
        check('11b bash was blocked: the command did not run, the model got pi\'s text',
              not there('RAM:pi-blocked.txt') and reason.startswith('no shell today'), repr(reason[:200]))
        check('11c the other extension\'s error is reported as pi words it, and the run went on',
              re.search(r'Extension "[^"]*b-bad\.lua" error: rig boom', out + err) is not None and b2 != {}, repr((out + err)[:300]))
        m = re.search(r'ts=(\d+) big=(\d+)', reason)
        check('11d a 13-digit number round-trips (the timestamp, and 1760000000123 exactly)',
              m is not None and len(m.group(1)) == 13 and m.group(2) == '1760000000123', repr(reason[:200]))
    finally:
        fx.terminate()
        c.run('Delete ENVARC:amiga-pi/extensions ALL QUIET')
        if had_ext:
            c.run('Rename ENVARC:amiga-pi/extensions.l10bak ENVARC:amiga-pi/extensions')
        c.run('Delete RAM:pi11-done VTC:pi11.dos VTC:x11-block.lua VTC:x11-bad.lua VTC:pi11.sh RAM:pi-blocked.txt QUIET')


# M9: step 12's adapters: (tag, models.json api, baseUrl suffix, model id, the request line the fixture logs, the answer)
ADAPTERS_12 = (
    ('anthropic', 'anthropic-messages', '', 'm1', '/v1/messages', 'Hello from the Amiga!'),
    ('completions', 'openai-completions', '/v1', 'm2', '/v1/chat/completions', 'Hello from the Amiga!'),
    ('responses', 'openai-responses', '/v1', 'm3', '/v1/responses', 'Hello from the Amiga.'),
    ('google', 'google-generative-ai', '/v1beta', 'g1', '/v1beta/models/g1:streamGenerateContent?alt=sse', 'Hello from the Amiga.'),
    # azure: a base URL that is not an Azure host is kept as given; the route is /responses?api-version=v1 below it
    ('azure', 'azure-openai-responses', '/az', 'a1', '/az/responses?api-version=v1', 'Hello from the Amiga.'),
    # bedrock: the base URL is the endpoint; the request is signed with the AWS keys of the environment
    ('bedrock', 'bedrock-converse-stream', '', 'b1', '/model/b1/converse-stream', 'Hello from the Amiga.'),
    # codex: POST /codex/responses below the base URL; the fixture answers with the Responses recording. The token and the
    # account id stay home on plain http, so the check is the route, the answer and pi's originator header
    ('codex', 'openai-codex-responses', '', 'c1', '/codex/responses', 'Hello from the Amiga.'),
)
AWS_ENV_12 = ('AWS_ACCESS_KEY_ID', 'AWS_SECRET_ACCESS_KEY', 'AWS_REGION')


def step12(port):
    """M9: every adapter of amiga-pi (anthropic, completions, responses, google, azure, bedrock, openai-codex), one `-p`
    run each, against tools/claude_fixture.py's routes."""
    import json
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture12.log'
    dump = paths.RIG / 'amigapi_dump12'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    # models.json in the step's own agent dir (PI_CODING_AGENT_DIR): ENVARC:amiga-pi stays as it is
    agent = VTC / 'pi12agent'
    base = 'http://127.0.0.1:%d' % port
    providers = {}
    for tag, api, suffix, mid, _, _ in ADAPTERS_12:
        providers['fx' + tag] = {'baseUrl': base + suffix, 'api': api, 'apiKey': 'sk-rig-12-must-not-be-sent',
                                 'models': [{'id': mid, 'contextWindow': 32000}]}
        if tag == 'bedrock':
            # a models.json apiKey IS the bedrock bearer token (pi: options.apiKey -> useBearerToken); over plain http it
            # stays home and nothing is signed. The SigV4 check needs the AWS_* keys of the environment alone.
            del providers['fx' + tag]['apiKey']
    try:
        agent.mkdir(exist_ok=True)
        (agent / 'models.json').write_text(json.dumps({'providers': providers}))
        lines = ['Echo "amiga-pi step 12: one run per adapter against the fixture"',
                 'SetEnv PI_CODING_AGENT_DIR VTC:pi12agent',
                 'SetEnv AWS_ACCESS_KEY_ID AKIDRIG12', 'SetEnv AWS_SECRET_ACCESS_KEY rig12secret', 'SetEnv AWS_REGION us-east-1',
                 'CD RAM:']
        for tag, _, _, mid, _, _ in ADAPTERS_12:
            for f in ('pi12-%s.out' % tag, 'pi12-%s.rc' % tag):
                if (VTC / f).exists():
                    (VTC / f).unlink()
            # <NIL: so the run never reads the script as its standard input (R7)
            lines += ['VTC:amiga-pi --model fx%s/%s --no-session --no-tools -p "hello" <NIL: >VTC:pi12-%s.out' % (tag, mid, tag),
                      'Echo "rc=$RC" >VTC:pi12-%s.rc' % tag]
        lines += ['Delete ENV:AWS_ACCESS_KEY_ID ENV:AWS_SECRET_ACCESS_KEY ENV:AWS_REGION QUIET']
        ended, req = run_watched('pi12', 'amiga-pi step 12', lines, 'pi12-done')
        check('12 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        seen = [l for l in open(log) if l.startswith('[INFO] /')]
        for tag, api, _, mid, route, answer in ADAPTERS_12:
            out, rc = rd('pi12-%s.out' % tag), rd('pi12-%s.rc' % tag).strip()
            check('12 %s: exit code 0' % api, rc == 'rc=0', repr((rc, out[:200])))
            mine = [l for l in seen if l.split()[1] == route]
            check('12 %s: the fixture saw one request on %s' % (api, route), len(mine) == 1, repr(seen))
            if tag == 'bedrock':
                # a signature goes over plain http (it is no secret); the bearer form must not
                check('12 %s: signed (sigv4), no bearer' % api, len(mine) == 1 and 'auth=sigv4' in mine[0], repr(mine))
            else:
                check('12 %s: the key stayed home (no key header over plain http)' % api,
                      len(mine) == 1 and 'key_header=none' in mine[0], repr(mine))
            check('12 %s: the answer text printed' % api, answer in out, repr(out[:200]))
        check('12 openai-codex-responses: the request carried pi\'s originator header',
              '[INFO]   codex originator=pi' in open(log).read())
        check('12 no other request reached the fixture', len(seen) == len(ADAPTERS_12), repr(seen))
        check('12 no requester', not req, repr(req))
    finally:
        fx.terminate()
        c.run('Delete ENV:AWS_ACCESS_KEY_ID ENV:AWS_SECRET_ACCESS_KEY ENV:AWS_REGION QUIET')
        c.run('Delete RAM:pi12-done VTC:pi12.dos VTC:pi12agent ALL QUIET')
        shutil.rmtree(agent, ignore_errors=True)


def mask_rpc(o, spec):
    """A record of pi's RPC/JSON stream after the masks of amiga-pi's tests/pi/rpc/masks.json (spec: that file, parsed),
    the one list of this rig and of amiga-pi's tests/test_pi_modes.c, with each mask's reason. The rules are those of
    the file's "rules" section, applied as test_pi_modes.c applies them; the rig adds one of its own: the bash tool's
    description names the shell this machine runs (vsh), a difference on purpose."""
    import math, re
    fresh = re.compile(r'^([0-9a-f]{8}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{4}-[0-9a-f]{12}|(?!d000)[0-9a-f]{8})$')
    http_err = re.compile(r'^(HTTP )?[1-5][0-9][0-9][ :]')
    star, ids = set(spec['star']), set(spec['ids']['keys'])
    prov = spec['fixtureProvider']

    def m(v, start=False, cost=False, ovf=False):
        if isinstance(v, list):
            return [m(x) for x in v]
        if not isinstance(v, dict):
            return v
        r = {}
        asst = v.get('role') == 'assistant'
        start_msg = v.get('type') == 'message_start' and isinstance(v.get('message'), dict) and v['message'].get('role') == 'assistant'
        for k, x in v.items():
            if k == 'description' and v.get('name') == 'bash':
                r[k] = '*'
                continue
            if k == 'responseId' and start:
                continue
            if k in star or (ovf and k == 'tokensBefore'):
                r[k] = '*'
            elif cost and isinstance(x, (int, float)) and not isinstance(x, bool):
                r[k] = 'u$%d' % math.floor(x * 1e6 + 0.5)    # as test_pi_modes.c rounds
            elif k == 'errorMessage' and (asst or v.get('type') == 'auto_retry_start') and isinstance(x, str) and http_err.match(x):
                r[k] = '<http error>'
            elif k in ids and isinstance(x, str) and fresh.match(x):
                r[k] = '*'
            elif k == 'cost' and isinstance(x, dict) and 'total' in x:
                r[k] = m(x, cost=True)
            elif k == 'baseUrl' and isinstance(x, str):
                r[k] = '<base>'
            elif k == 'error' and v.get('command') == 'parse':
                r[k] = '*'
            elif k == 'usage' and start:
                r[k] = '*'
            elif k == 'message' and start_msg:
                r[k] = m(x, True)
            elif k == 'result' and v.get('type') == 'compaction_end' and v.get('reason') == 'overflow' and v.get('willRetry') is True:
                r[k] = m(x, ovf=True)
            elif k == 'models' and isinstance(x, list):
                r[k] = [m(e) for e in x if isinstance(e, dict) and e.get('provider') == prov]
            else:
                r[k] = m(x)
        return r
    return m(o)


def step14(port):
    """H8: amiga-pi --mode rpc fed from a file through `<`; its output file equals pi's golden (tests/pi/rpc/prompt.out of
    the amiga-pi tree), line by line, masked as tests/test_pi_modes.c masks. The commands are the golden's prompt.in and a
    get_state, a get_messages and a get_session_stats read after the run (the Amiga reads the lines one after the other)."""
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    gold = AMIGA_PI.parents[2] / 'tests' / 'pi' / 'rpc'
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    log = paths.RIG / 'amigapi_fixture14.log'
    dump = paths.RIG / 'amigapi_dump14'
    shutil.rmtree(dump, ignore_errors=True)
    dump.mkdir()
    fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                           '--delay', '0.01', '--dump', str(dump)], stdout=open(log, 'w'), stderr=subprocess.STDOUT)
    time.sleep(1)
    agent = VTC / 'pi14agent'
    agent.mkdir(exist_ok=True)
    models = json.loads((gold / 'models.json').read_text())
    models['providers']['fake']['baseUrl'] = 'http://127.0.0.1:%d' % port
    (agent / 'models.json').write_text(json.dumps(models))
    (agent / 'settings.json').write_text(json.dumps({'defaultProvider': 'fake', 'defaultModel': 'fake-model',
                                                     'defaultThinkingLevel': 'off', 'quietStartup': True}))
    cmds = [l for l in (gold / 'prompt.in').read_text().splitlines() if l and not l.startswith('#')]
    cmds += ['{"id":"2","type":"get_messages"}', '{"id":"3","type":"get_session_stats"}']
    (VTC / 'pi14.in').write_text(''.join(l + '\n' for l in cmds))
    for f in ('pi14.out', 'pi14.err'):
        if (VTC / f).exists():
            (VTC / f).unlink()
    try:
        ended, req = run_watched('pi14', 'amiga-pi step 14', [
            'SetEnv PI_CODING_AGENT_DIR VTC:pi14agent',
            'CD RAM:',
            'VTC:amiga-pi --mode rpc --model fake/fake-model --no-session <VTC:pi14.in >VTC:pi14.out',
            'Echo "rc=$RC" >VTC:pi14.rc'], 'pi14-done')
        check('14 the window ended', ended)
        time.sleep(3)
        rd = lambda f: (VTC / f).read_bytes().decode('utf-8', 'replace') if (VTC / f).exists() else ''
        got = [json.loads(l) for l in rd('pi14.out').splitlines() if l.strip().startswith('{')]
        want = [json.loads(l) for l in (gold / 'prompt.out').read_text().splitlines() if l.strip()]
        n = len(want)
        spec = json.loads((gold / 'masks.json').read_text())
        a = [json.dumps(mask_rpc(o, spec)) for o in want]
        b = [json.dumps(mask_rpc(o, spec)) for o in got[:n]]
        bad = [i for i in range(n) if i >= len(b) or a[i] != b[i]]
        verd = {'14a prompt: the stream equals the golden line by line': not bad and len(got) > n,
                '14b get_messages answered': any(o.get('command') == 'get_messages' and o.get('success') for o in got),
                '14c get_session_stats answered': any(o.get('command') == 'get_session_stats' and o.get('success') for o in got),
                '14d exit code 0': rd('pi14.rc').strip() == 'rc=0'}
        detail = 'first differing line %s: want %s got %s' % (bad[0], a[bad[0]][:300], (b[bad[0]] if bad[0] < len(b) else '')[:300]) \
            if bad else repr(rd('pi14.out')[:300])
        for name, ok in verd.items():
            record14(name, ok, detail)
        (paths.RIG / 'amigapi14-out.jsonl').write_text(rd('pi14.out'))
        # 14e (HF1, Q18): json mode's own diagnostic goes to the error stream (pr_CES) as ONE line, not to the JSONL on
        # stdout. Trigger: a models.json that is not JSON in the step's own agent dir, which makes amiga-pi write
        # "[WARN] <why> (models.json is ignored)" (amiga-pi/app.c, tests/test_pi_modes.c test_diag_one_line). An unknown
        # --tools entry no longer writes anything: pi 1.1.0 filters it silently (core/tools/sdk.ts:289-294).
        for f in ('pi14e.out', 'pi14e.err'):
            if (VTC / f).exists():
                (VTC / f).unlink()
        agente = VTC / 'pi14eagent'
        agente.mkdir(exist_ok=True)
        (agente / 'models.json').write_text('{not json')
        (VTC / 'pi14e.sh').write_text('VTC:amiga-pi --mode json --no-session -p hi >VTC:pi14e.out 2>VTC:pi14e.err\n')
        ended, req = run_watched('pi14e', 'amiga-pi step 14e', [
            'SetEnv PI_CODING_AGENT_DIR VTC:pi14eagent',
            'CD RAM:',
            'FailAt 21',   # amiga-pi may exit 2; the Shell would stop the script at 10
            'VTC:vsh VTC:pi14e.sh <NIL:',   # the Shell has no 2>; vsh does
            'Echo "rc=$RC" >VTC:pi14e.rc'], 'pi14e-done')
        time.sleep(2)
        eo, ee = rd('pi14e.out'), rd('pi14e.err')
        warn = [l for l in ee.splitlines() if l.startswith('[WARN] ')]
        record14('14e diag line in the error file as one line, not in the stdout JSONL',
                 ended and len(warn) == 1 and '(models.json is ignored)' in warn[0] and '[WARN]' not in eo
                 and 'models.json' not in eo,
                 'ended=%s req=%s out=%r err=%r' % (ended, req, eo[:200], ee[:200]))
    finally:
        fx.terminate()
        c.run('Delete RAM:pi14-done VTC:pi14.dos QUIET')


def record14(name, ok, detail=''):
    v = verdicts14()
    v[name] = 'PASS' if ok else 'FAIL'
    VERDICTS14.write_text(json.dumps(v, indent=1, sort_keys=True) + '\n')
    check(name, ok, detail)


def verdicts14():
    try:
        return json.loads(VERDICTS14.read_text())
    except (OSError, ValueError):
        return {}


VERDICTS14 = paths.RIG / 'amigapi14.verdicts.json'
VERDICTS = paths.RIG / 'amigapi13.verdicts.json'
STEP13 = {'a': ['13a Steering row', '13a Follow-up row', '13a steering text in request 2 only',
                '13a follow-up text in request 3', '13a session: three user messages', '13a Ctrl+D twice ends amiga-pi'],
          'b': ['13b Shift+Tab: Thinking level on screen', '13b Shift+Tab: thinking_level_change in session',
                '13b Ctrl+P: Switched to on screen', '13b Ctrl+P: model_change m2 in session'],
          'c': ['13c /tree: the selector shows'],
          'd': ['13d start header: logo, version and key hints']}
force13 = False


def verdicts():
    try:
        return json.loads(VERDICTS.read_text())
    except (OSError, ValueError):
        return {}


def record(name, ok, detail=''):
    v = verdicts()
    v[name] = 'PASS' if ok else 'FAIL'
    VERDICTS.write_text(json.dumps(v, indent=1, sort_keys=True) + '\n')
    check(name, ok, detail)


def phase_due(ph):
    v = verdicts()
    return force13 or any(v.get(n) != 'PASS' for n in STEP13[ph])


def session_entries(sess):
    """(file, entries) of the newest session file the Amiga wrote, or the one `newer_than` names"""
    files = sorted(sess.glob('*/*.jsonl'), key=lambda f: f.stat().st_mtime) if sess.exists() else []
    if not files:
        return None, []
    return files[-1], [json.loads(l) for l in files[-1].read_text(encoding='utf-8').splitlines() if l.strip()]


def user_texts(entries):
    out = []
    for e in entries:
        m = e.get('message') or {}
        if e.get('type') == 'message' and m.get('role') == 'user':
            c_ = m.get('content')
            out.append(c_ if isinstance(c_, str) else ' '.join(b.get('text', '') for b in c_ if isinstance(b, dict)))
    return out


def step13(port):
    global force13
    if not AMIGA_PI.exists():
        sys.exit('[ERROR] %s missing: make -C ~/Code/amiga-pi amiga' % AMIGA_PI)
    if not (ROOT / 'build' / 'amiga' / 'snipprobe').exists():
        sys.exit('[ERROR] build/amiga/snipprobe missing: make build/amiga/snipprobe')
    if not any(phase_due(p) for p in STEP13):
        print('13: every check already recorded as PASS in %s (--force to run again)' % VERDICTS)
        return
    shutil.copy(AMIGA_PI, VTC / 'amiga-pi')
    shutil.copy(ROOT / 'build' / 'amiga' / 'snipprobe', VTC / 'snipprobe')
    had = assigns()
    saved_dir = ami.read_file('ENVARC:up-term/Dir')
    log = paths.RIG / 'amigapi_fixture13.log'
    dump = paths.RIG / 'amigapi_dump13'
    kit = VTC / 'upterm'    # UP-Term: is the rig's own (rig.py, UT1): VTC:upterm holds unifont/ and emoji/, the sessions go to VTC:upterm/var/amiga-pi (UT2)
    sess = kit / 'var' / 'amiga-pi' / 'sessions'
    agent = VTC / 'pi13agent'
    base = 'http://127.0.0.1:%d' % port
    fx = None

    def fixture():
        nonlocal fx
        if fx:
            fx.terminate()
        shutil.rmtree(dump, ignore_errors=True)
        dump.mkdir()
        fx = subprocess.Popen([sys.executable, '-I', str(ROOT / 'tools' / 'claude_fixture.py'), '--port', str(port),
                               '--delay', '0.4', '--dump', str(dump)], stdout=open(log, 'w'),
                              stderr=subprocess.STDOUT)
        time.sleep(1)

    def requests():
        return len(list(dump.glob('*.json')))

    def wait_requests(n, secs=120):
        for _ in range(secs * 2):
            if requests() >= n:
                return True
            time.sleep(0.5)
        return False

    def launch(title, args):
        tw.open_xcon(title)
        tw.typ('Stack 32768')
        tw.key(tw.RET, wait=1)
        tw.typ('SetEnv PI_CODING_AGENT_DIR VTC:pi13agent')
        tw.key(tw.RET, wait=1)
        tw.typ('CD RAM:')
        tw.key(tw.RET, wait=1)
        tw.typ('VTC:amiga-pi --model fixa/m1 --no-tools ' + args)
        tw.key(tw.RET, wait=10)

    def leave():
        tw.key(0x22, tw.CTRL, wait=0.2)   # Ctrl+D twice within 0.8 s on the empty editor ends amiga-pi
        tw.key(0x22, tw.CTRL, wait=3)
        gone = 'VTC:amiga-pi' not in c.run('Status')[1]
        tw.typ('endcli')
        tw.key(tw.RET, wait=2)
        return gone

    def text(name):
        t = tw.screen_text('RAM:pi13-%s.txt' % name)
        (paths.RIG / ('amigapi13-%s.txt' % name)).write_text(t)
        return t

    try:
        agent.mkdir(exist_ok=True)
        (agent / 'models.json').write_text(json.dumps({'providers': {
            'fixa': {'baseUrl': base, 'api': 'anthropic-messages',
                     'models': [{'id': 'm1', 'reasoning': True, 'contextWindow': 200000, 'maxTokens': 20000}]},
            'fixo': {'baseUrl': base + '/v1', 'api': 'openai-completions', 'apiKey': '!Echo rig-test-key',
                     'models': [{'id': 'm2', 'contextWindow': 32000}]}}}))
        (agent / 'settings.json').write_text(json.dumps({'enabledModels': ['fixa/m1', 'fixo/m2']}))
        # UP-Term: stays the rig's (VTC:upterm, UT1): the pages the windows read are there. A drawer of its own
        # for the kit lacked unifont/ and emoji/; an assign with the kit first and VTC: added moved the
        # sessions out of amiga-pi's reach (measured 2026-10-12), so the run uses the rig's drawer itself.
        assert 'UP-TERM' in had, 'rig.py setup (UT1) assigns UP-Term: to VTC:upterm; reboot the rig'
        c.run('Delete VTC:upterm/var/amiga-pi ALL QUIET')    # from the Amiga's side: the emulator caches a drawer removed behind its back
        if phase_due('a'):
            fixture()
            launch('amiga-pi step 13a', '')
            tw.typ('hello')
            tw.key(tw.RET, wait=4)
            tw.typ('steer me')
            tw.key(tw.RET, wait=2)               # Enter while it works: steering
            tw.typ('and then this')
            tw.key(tw.RET, tw.LAMIGA, wait=2)    # Alt+Enter: UP-Term's Meta key is Left Amiga, it sends ESC CR
            t = text('13a')
            record('13a Steering row', 'Steering: steer me' in t, repr(t[-600:]))
            record('13a Follow-up row', 'Follow-up: and then this' in t, repr(t[-600:]))
            done = wait_requests(3)
            time.sleep(30)
            b = [json.dumps(body_of(dump, k)) for k in (1, 2, 3)]
            record('13a steering text in request 2 only', done and 'steer me' not in b[0] and 'steer me' in b[1],
                   repr([('steer me' in x) for x in b]))
            record('13a follow-up text in request 3', done and 'and then this' in b[2] and 'and then this' not in b[1],
                   repr([('and then this' in x) for x in b]))
            gone = leave()
            record('13a Ctrl+D twice ends amiga-pi', gone, 'amiga-pi still in Status')
            f, ents = session_entries(sess)
            record('13a session: three user messages', user_texts(ents) == ['hello', 'steer me', 'and then this'],
                   repr(user_texts(ents)))
        if phase_due('b'):
            fixture()
            launch('amiga-pi step 13b', '')
            tw.typ('hello')
            tw.key(tw.RET, wait=2)
            wait_requests(1)
            time.sleep(25)
            f, ents = session_entries(sess)
            name = f.name if f else ''
            tw.key(tw.TAB, tw.SHIFT, wait=2)
            t = text('13b-thinking')
            record('13b Shift+Tab: Thinking level on screen', 'Thinking level: ' in t, repr(t[-400:]))
            tw.key(0x19, tw.CTRL, wait=3)        # Ctrl+P
            t = text('13b-model')
            record('13b Ctrl+P: Switched to on screen', 'Switched to' in t, repr(t[-400:]))
            leave()
            f, ents = session_entries(sess)
            kinds = [(e.get('type'), e.get('modelId') or e.get('thinkingLevel')) for e in ents
                     if e.get('type') in ('model_change', 'thinking_level_change')]
            record('13b Shift+Tab: thinking_level_change in session',
                   [k for k in kinds if k[0] == 'thinking_level_change'][:2] != [] and
                   len([k for k in kinds if k[0] == 'thinking_level_change']) >= 2, repr(kinds))
            record('13b Ctrl+P: model_change m2 in session', ('model_change', 'm2') in kinds, repr(kinds))
        if phase_due('c'):
            fixture()
            launch('amiga-pi step 13c', '-c')
            tw.typ('/tree')
            tw.key(tw.RET, wait=3)
            t = text('13c')
            record('13c /tree: the selector shows', 'Session Tree' in t and 'Type to search:' in t, repr(t[-600:]))
            tw.key(tw.ESC, wait=1)
            leave()
        if phase_due('d'):
            # the start header (no quietStartup): pi's logo of block glyphs, the version line, the key hints
            fixture()
            launch('amiga-pi step 13d', '')
            t = text('13d')
            u = t.encode('latin-1', 'replace').decode('utf-8', 'replace')   # snipprobe's file is UTF-8, Type shows its bytes
            record('13d start header: logo, version and key hints', all(w in u for w in
                   ('\u2580\u2580\u2588', '\u2588\u2580 \u2588', 'pi 1.1.0', 'escape interrupt', 'ctrl+o more')), repr(u[:500]))
            leave()
    finally:
        if fx:
            fx.terminate()
        c.run('Delete VTC:upterm/var/amiga-pi ALL QUIET')
        c.run('Delete VTC:pi13agent ALL QUIET')
        shutil.rmtree(agent, ignore_errors=True)
        if saved_dir is not None:
            ami.restore_file('ENVARC:up-term/Dir', saved_dir)


def pathlib_home():
    return pathlib.Path.home()


def main():
    ap = argparse.ArgumentParser()
    ap.add_argument('--port', type=int, default=8092, help="the fixture's port (8080 may be another rig's)")
    ap.add_argument('--step', type=int, choices=(1, 2, 3, 4, 5, 6, 7, 8, 9, 10, 11, 12, 13, 14), help='one step only (default: all but 13 and 14)')
    ap.add_argument('--force', action='store_true', help='step 13: run every check again')
    a = ap.parse_args()
    global force13
    force13 = a.force
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
    if a.step in (None, 6):
        step6(a.port + 5)
    if a.step in (None, 7):
        step7(a.port + 6)
    if a.step in (None, 8):
        step8(a.port + 7)
    if a.step in (None, 9):
        step9(a.port + 8)
    if a.step in (None, 10):
        step10(a.port + 9)
    if a.step in (None, 11):
        step11(a.port + 10)
    if a.step in (None, 12):
        step12(a.port + 11)
    if a.step == 13:
        step13(a.port + 12)
    if a.step == 14:
        step14(a.port + 13)
    print('amigapi_rig: %s' % ('PASS' if not fails else 'FAIL: ' + ', '.join(fails)))
    return 1 if fails else 0


if __name__ == '__main__':
    sys.exit(main())
