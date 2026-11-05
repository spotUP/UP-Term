"""install_rig: a check that drops the UP-Term: assign leaves it as it was (S57-INSTALL-CHECK-ASSIGN / RIG96-ASSIGN-LOST).

`INSTALLER_KEEP=1 ... ONLY fresh-all` ended with the assign removed on the rig: the checks run a bare `Assign UP-Term:`
(which REMOVES it) and nothing put it back. A fake Amiga shell below models the assign table the way AmigaOS does
(bare `Assign X:` removes, `EXISTS` only reads).
"""
import pathlib, re, sys, unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import install_rig as ir

WAS = ['Work:Tools/UP-Term', 'VTC:Apps']


class FakeShell:
    def __init__(self, targets):
        self.t = list(targets)
        self.log = []

    def __call__(self, cmd, timeout=60):
        self.log.append(cmd)
        m = re.match(r'Assign (?:>NIL: )?UP-Term:(?: (.*))?$', cmd)
        if not m:
            return 0, ''
        arg = (m.group(1) or '').strip()
        if arg == 'EXISTS':
            return (0, 'UP-Term  ' + '\n+ '.join(self.t) + '\n') if self.t else (5, '')
        if arg == '':
            self.t = []                      # bare Assign X: removes it
        elif arg.endswith(' ADD'):
            self.t.append(arg[:-4].strip('"'))
        else:
            self.t = [arg.strip('"')]
        return 0, ''


def own_fixtures(fn):
    fn.own_fixtures = True
    return fn


class UptermAssign(unittest.TestCase):
    def setUp(self):
        ir._UPTERM_WAS = None
        self.sh = FakeShell(WAS)
        self._run, ir.run = ir.run, self.sh

    def tearDown(self):
        ir.run = self._run
        ir._UPTERM_WAS = None

    def test_drop_then_restore(self):
        ir.drop_upterm_assign()
        self.assertEqual(self.sh.t, [])
        ir.restore_upterm_assign()
        self.assertEqual(self.sh.t, WAS)

    def test_restored_when_the_check_raises(self):
        with self.assertRaises(RuntimeError):
            with ir.keep_upterm_assign():
                ir.drop_upterm_assign()
                raise RuntimeError('check blew up')
        self.assertEqual(self.sh.t, WAS)

    def test_run_case_restores_after_a_dropping_case(self):
        def case(ck):
            ir.drop_upterm_assign()
            raise RuntimeError('case failed after the drop')
        with self.assertRaises(RuntimeError):
            ir.run_case('x', own_fixtures(case))
        self.assertEqual(self.sh.t, WAS)

    def test_reading_never_removes(self):
        self.assertEqual(ir.upterm_assign_state(), WAS)
        self.assertFalse(any(c.rstrip().endswith('UP-Term:') for c in self.sh.log))

    def test_still_gone_fails_loudly(self):
        self.sh.t = []
        with self.assertRaises(AssertionError):
            ir.require_upterm_assign()


if __name__ == '__main__':
    unittest.main()
