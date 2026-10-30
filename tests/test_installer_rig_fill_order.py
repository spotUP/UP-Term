"""installer_rig's space cases fill SYS: after the case's cleanup, not before.

run_base_case runs install_rig.prepare and the kit's Uninstall first; on a rig where a stopped run left SYS: parts
behind, that cleanup freed about 8.8 MB again. space-sys filled SYS: to 2285 KB free, then started the Installer with
11076 KB free, enough for its SYS: parts: the install ran instead of refusing (session 29, 2026-10-30).
"""
import pathlib, sys, unittest
from unittest import mock

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / "tools/rig"))

import installer_rig as rig


class Reached(Exception):
    """Raised at the free-space read the room checks use: the setup has run by then."""


def setup_order(case):
    calls = []
    def run_long(name, *a, **k):
        calls.append(name)
        return 0, ""
    def fill(left):
        calls.append(('fill', left))
    def free():
        raise Reached
    with mock.patch.object(rig.ir, "prepare", lambda dest: calls.append('prepare')), \
            mock.patch.object(rig.ir, "run_long", run_long), \
            mock.patch.object(rig.ir, "run", return_value=(5, "")), \
            mock.patch.object(rig.ir, "run_slow", return_value=(0, "")), \
            mock.patch.object(rig, "make_dest_parent", return_value=True), \
            mock.patch.object(rig, "big_drawer", return_value="SYS:UP-Term"), \
            mock.patch.object(rig, "fill_sys", fill), \
            mock.patch.object(rig, "sys_free_bytes", free):
        with reaches_free_read():
            rig.run_case(case)
    return calls


class reaches_free_read:
    def __enter__(self):
        return self

    def __exit__(self, kind, exc, tb):
        if kind is not Reached:
            raise AssertionError('the case did not reach its free-space read (%r)' % (exc,))
        return True


class FillOrder(unittest.TestCase):
    def check(self, case, left):
        calls = setup_order(case)
        self.assertIn(('fill', left), calls)
        fill = calls.index(('fill', left))
        self.assertLess(calls.index('prepare'), fill, calls)
        self.assertLess(calls.index('rununinstall'), fill, calls)

    def test_space_sys_fills_after_the_cleanup(self):
        self.check('space-sys', rig.SPACE_LEFT_SYS)

    def test_space_fills_after_the_cleanup(self):
        self.check('space', rig.SPACE_LEFT)


if __name__ == "__main__":
    unittest.main()
