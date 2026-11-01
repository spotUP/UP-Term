"""THEME-PICK-MENU-LEVEL: after a theme pick the Amarchy menu may return to Style level ('Amarchy theme|Terminal', SEL 0);
that is the product's behaviour (owner, 2026-11-01). The rig's one_select must accept it and check "(current)" on the
reopened theme list. Pure decision functions plus one flow through one_select with a mocked menu."""
import os, sys
sys.path.insert(0, os.path.join(os.path.dirname(os.path.abspath(__file__)), '..', 'tools', 'rig'))
import amarchy_theme_save_rig as t

fails = []
themes = ['mono', 'vantablack', 'nord']
style = ['Amarchy theme', 'Terminal']
if t.menu_level(style) != 'style':
    fails.append('menu_level did not recognise the Style level')
if t.menu_level(themes) != 'themes' or t.menu_level(['mono (current)', 'nord']) != 'themes':
    fails.append('menu_level took the theme list for Style level')
after = ['mono', 'vantablack (current)', 'nord']
if t.current_verdict('vantablack', themes, after, '1', '1'):
    fails.append('current_verdict refused a correctly marked theme list')
if not t.current_verdict('vantablack', themes, style, '1', '0'):
    fails.append('current_verdict accepted Style rows as the theme list')
if not t.current_verdict('vantablack', themes, themes, '1', '1'):
    fails.append('current_verdict accepted an unmarked theme')
if not t.current_verdict('vantablack', themes, after, '1', '2'):
    fails.append('current_verdict accepted a moved cursor')

# flow: one_select with the menu back on Style level after the pick reopens the list and passes
rows = {'v': style}
calls = {'reopen': 0}
env = {'AMARCHY_THEME': 'vantablack', 'AMARCHY_ROWS': '|'.join(themes), 'AMARCHY_SEL': '1'}
phase = {'n': 0}
t.theme_line = lambda p: 'mono' if phase['n'] == 0 else 'vantablack'
t.dopus_pens_reset = lambda: None
t.open_menu = lambda: None
t.wait_stable = lambda *a, **k: (('b', 'd'), 0.0)
t.menu_state = lambda: ('b', 'd')
t.style_downs = lambda: 0
t.walk_key = lambda *a: ('k', 'Down')
t.wait_apply_done = lambda: True
t.header_accent_pixels = lambda rgb: 99
t.theme_accent = lambda name: (0, 0, 0)
t.strangers = lambda allowed: []
t.wb_pens_check = lambda name: None
t.env = lambda k: env[k]
t.c.run = lambda cmd: (0, '')
t.g.where = lambda title: (None, True)
t.g.close_menu = lambda: None
n_return = {'n': 0}


def key(k, *a):
    if k == t.g.RETURN:
        n_return['n'] += 1
        if n_return['n'] == 3:          # the pick: apply, menu falls back to Style level
            phase['n'] = 1
            env['AMARCHY_ROWS'] = '|'.join(style)
            env['AMARCHY_SEL'] = '0'
        elif n_return['n'] == 4:        # Return on "Amarchy theme": the list again, cursor on the current theme
            calls['reopen'] += 1
            env['AMARCHY_ROWS'] = '|'.join(after)
            env['AMARCHY_SEL'] = '1'


t.ami.key = key
ok, detail, _ = t.one_select(1, set())
if not ok:
    fails.append('one_select refused the Style level after the pick: %s' % detail)
if calls['reopen'] != 1:
    fails.append('one_select did not reopen the theme list (%d reopens)' % calls['reopen'])

if fails:
    sys.exit('FAIL: ' + '; '.join(fails))
print('OK one_select accepts the Style level after a pick and checks "(current)" on the reopened theme list')
