"""piscript.py -- the text of the CON: window scripts amigapi_rig.py runs (NewShell FROM VTC:<name>.dos).

A Shell reads a script file through its input stream; a program started from it that reads
stdin (amiga-pi --mode rpc, or -p when it takes the prompt's rest from stdin) reads the
REST OF THE SCRIPT: the trailing commands never ran (R7, from N1). So every line that starts
amiga-pi or the llm probe and gives no input of its own ends in <NIL:.
"""

STDIN_GUARDED = ('VTC:amiga-pi', 'VTC:pi-llmprobe')


def _redirects_stdin(line):
    """True when the line has a < outside AmigaDOS quotes (*" is a quote inside a string)"""
    quoted = False
    i = 0
    while i < len(line):
        ch = line[i]
        if ch == '*' and quoted:
            i += 2
            continue
        if ch == '"':
            quoted = not quoted
        elif ch == '<' and not quoted:
            return True
        i += 1
    return False


def no_stdin(line):
    """line with <NIL: appended when it starts a guarded program and has no < of its own"""
    words = line.split(None, 1)
    if not words or words[0] not in STDIN_GUARDED or _redirects_stdin(line):
        return line
    return line + ' <NIL:'


def script_text(lines, done=None):
    """the script: one line each, guarded; with done, the closing lines every window script ends in"""
    text = ''.join(no_stdin(l) + '\n' for l in lines)
    if done is not None:
        text += 'Echo >RAM:%s "done"\nWait 3\nEndCLI\n' % done
    return text
