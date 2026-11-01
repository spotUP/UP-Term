"""tools/rig/amarchy_menu_rig.py writes each key to the store that owns it.

Since 5270f4ba Amarchy reads ENV:Amarchy/prefs before ENV:up-term/up-term, so a case that Echoes one of the
product table's keys (am_keys[] in amarchy/amconf_keys.c) into the up-term file is ignored and the menu stays text.
Static guard: no line of the rig that names X9_CONF names an am_keys key. Behaviour: x9_conf() sends Amarchy keys to
ENV:Amarchy/prefs and the rest to ENV:up-term/up-term. Usage: test_rig_amarchy_conf.py [rig-source-to-scan]
"""
import pathlib
import re
import sys

ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / 'tools' / 'rig'))
import amarchy_menu_rig as r

src = pathlib.Path(sys.argv[1]) if len(sys.argv) > 1 else ROOT / 'tools' / 'rig' / 'amarchy_menu_rig.py'
keys = r.am_key_names()
assert len(keys) == 10 and {'menu-style', 'amarchy-gui-theme', 'menu-in-terminal'} <= keys, keys
bad = [n + 1 for n, l in enumerate(src.read_text().splitlines()) if 'X9_CONF' in l and any(re.search(r'["\s]%s\s*=' % re.escape(k), l) for k in keys)]
assert not bad, '%s writes an am_keys key to the up-term file at lines %s' % (src.name, bad)

# a case that expects the drawn menu must write menu-in-terminal = graphical (x9_drawn does); a bare x9_conf( call
# leaves the default text and Super+Space over the rig's terminal opens amarchy-menu
bare = [n + 1 for n, l in enumerate(src.read_text().splitlines()) if re.search(r'\bx9_conf\(', l) and not l.startswith('def ') and 'menu-in-terminal' not in l and 'DRAWN' not in l]
assert not bare, '%s: drawn-menu cases call x9_conf without menu-in-terminal = graphical at lines %s' % (src.name, bare)

calls = []
r.c.run = lambda cmd, *a, **k: calls.append(cmd) or (0, '')
r.x9_conf('menu-style = text', 'fg = E0E0A0', 'amarchy-gui-theme = nord')
am = [x for x in calls if r.X9_AMCONF in x]
up = [x for x in calls if r.X9_CONF in x]
assert any('menu-style = text' in x for x in am) and any('amarchy-gui-theme = nord' in x for x in am), calls
assert not any('fg = ' in x for x in am) and any('fg = E0E0A0' in x for x in up), calls
assert not any('menu-style' in x or 'gui-theme' in x for x in up), calls
print('rig amarchy conf: ok')
