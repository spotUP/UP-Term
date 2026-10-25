"""The ;BEGIN <name> ... ;END <name> blocks that Install writes to S:User-Startup.

One reader for every rig script: install_rig strips the kit's blocks from a saved
S:User-Startup, rig.py strips the master image's wasabi block from each fresh
sys.hdf clone.
"""


def strip_upterm_blocks(data, name='UP-Term'):
    """data without the ;BEGIN <name> ... ;END <name> blocks (UP-Term: every flavour:
    assign, python, tmp, console, device ...) that Install writes."""
    out, skip = [], False
    for line in data.decode('latin-1').splitlines(True):
        if line.startswith(';BEGIN ' + name): skip = True
        if not skip: out.append(line)
        if skip and line.startswith(';END ' + name): skip = False
    return ''.join(out).encode('latin-1')
