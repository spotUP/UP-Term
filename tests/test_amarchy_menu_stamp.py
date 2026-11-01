#!/usr/bin/env python3
"""test_amarchy_menu_stamp.py -- the Amarchy hotkey commodity's press path reaches ms_binary_changed.

  python3 -I tests/test_amarchy_menu_stamp.py

The commodity (amarchy/amarchy.c, act.c, menu_open.c) is Amiga-only code that no host binary links, so
this walks the call chain a hotkey press takes and fails when a link is missing:
  act_menu_kind (amarchy.c) -> act_menu_open (act.c) -> ms_open (menusrv_core.c) -> ms_binary_changed,
  with ms_open reading ops->stamp (mo_srv_ops, menu_open.c, from mo_srv.stamp, set in act.c srv_get) and
  ops->quit (mo_srv_ops). Without either hook ms_open skips the check and an old server keeps running.
Exit 1 on a missing link."""
import pathlib, re, sys

ROOT = pathlib.Path(__file__).resolve().parents[1] / 'amarchy'


def body(path, name):
    """The text of function `name` in file `path`: from its definition line to the closing brace at column 0."""
    src = (ROOT / path).read_text()
    m = re.search(r'^[A-Za-z][^\n;]*\b' + name + r'\([^;{]*\)\s*\n?\{.*?^\}', src, re.S | re.M)
    if not m:
        sys.exit('FAIL: %s has no function %s' % (path, name))
    return m.group(0)


CHAIN = [
    ('amarchy.c', 'act_menu_kind', r'\bact_menu_open\('),
    ('act.c', 'act_menu_open', r'\bms_open\(&ops'),
    ('act.c', 'act_menu_open', r'\bmo_srv_ops\(&ops'),
    ('act.c', 'srv_get', r's->stamp\s*=\s*srv_stamp_file'),
    ('menu_open.c', 'mo_srv_ops', r'ops->stamp\s*=\s*srv_stamp'),
    ('menu_open.c', 'mo_srv_ops', r'ops->quit\s*=\s*srv_quit'),
    ('menu_open.c', 'srv_stamp', r's->stamp\(s->user\)'),
    ('menusrv_core.c', 'ms_open', r'ms_binary_changed\(ops->st->bin_stamp, ops->stamp\(ops->ctx\)\)'),
]

bad = 0
for path, fn, pat in CHAIN:
    if not re.search(pat, body(path, fn)):
        print('FAIL: %s %s() no longer matches %s' % (path, fn, pat))
        bad += 1
if bad:
    sys.exit(1)
print('ok: hotkey press -> ms_open -> ms_binary_changed (%d links)' % len(CHAIN))
