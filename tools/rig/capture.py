"""capture.py -- what is still reachable on the rig when a check fails or a call gets no answer.
Shared by splits_rig.py and repeat_rig.py: `dump(run, req)` is VTC:taskdump (every task's state) plus the
UITREE window list (a DOS requester a process waits in), each with an OSError fallback so a wasabid that no
longer answers still yields a file."""


def dump(run, req, why=''):
    """run(cmd, timeout) -> (rc, text); req(code, payload=b'', timeout=) -> bytes (ami.req)."""
    try:
        text = run('VTC:taskdump', 30)[1]
    except OSError:
        text = 'wasabid does not answer (%s)\n' % why
    try:
        text += '\n[UITREE]\n' + req(0x0D, timeout=30).decode('latin-1')
    except OSError:
        text += '\n[UITREE] no answer\n'
    return text
