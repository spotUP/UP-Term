"""tools/rig/install_rig.py: UHC stays after its Uninstall (SYS:UHC is UHC's own, owner decision 2026-10-27), so the
rig must expect aget and the ;BEGIN UHC block to stay; a failed S:User-Startup compare names the first differing line.

Regression (s25 2026-10-29, rig 9 --only all FAIL 76 and 94): the check demanded SYS:UHC/C/aget gone and the
User-Startup compare did not take the UHC block out; the message showed only identical tails."""
import pathlib
import sys

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent / "rig"))
import install_rig as ir  # noqa: E402

fails = 0


def check(cond, what):
    global fails
    if not cond:
        fails += 1
        print("FAIL:", what)


check(ir.startup_leaves(['uhc', 'python']) == ('UHC',), 'uhc leaves its UHC block')
check(ir.startup_leaves(['dopus', 'uhc']) == ('DOpus5', 'UHC'), 'dopus and uhc leave their blocks')
check(ir.startup_leaves(['python']) == (), 'python leaves none')
check('uhc' in ir.LEFT_BY_UNINSTALL, 'uhc files are expected to stay')
msg = ir.first_difference('a\nb\nX\nc', 'a\nb\nY\nc')
check('line 3' in msg and "'X'" in msg and "'Y'" in msg, 'first_difference names the line: ' + msg)
res = []
ir.check_startup_after_uninstall(lambda c, w, d='': res.append(c),
                                 'a\n;BEGIN UHC\nx\n;END UHC\n', 'a\n;BEGIN UHC\nx\n;END UHC\nb\n', ('UHC',))
check(res and res[0] is False, 'a real difference outside the left block still fails')
# uhcinstall prepends the block AND one empty line before the file's own first line (rig 9 --only all FAIL 94,
# s25 2026-10-29): the left block goes with that separator, and the block is still demanded in the file.
orig = ';run >nil: c:copper-demon patchopenwb bordblnk\n;run >nil: c:minimigwheeldriver\n\n'
after = ';BEGIN UHC\n  If EXISTS "${UHCBIN}UHC-Startup"\n;END UHC\n\n' + orig
res = []
ir.check_startup_after_uninstall(lambda c, w, d='': res.append((c, w)), orig, after, ('UHC',))
check(res and all(c for c, _ in res), 'uhcinstall block plus its blank line is not a difference: %r' % res)
check(ir.strip_block(after, 'UHC') == orig, 'strip_block takes the separator blank too')
sys.exit(1 if fails else 0)
