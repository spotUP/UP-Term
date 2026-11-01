"""tools/ans2utf8.py: the vsh banner's bright letters are not drawn bold.

Regression (2026-10-22): the owner saw the Up Rough banner's text drawn
thicker than it should be. The art (art/up_rough_banner.ans) picks its bright
colours with SGR 1 (ESC[1;31m), the ANSI art convention, and ans2utf8.py
copied that SGR 1 into Files/banner, which the xterm personality draws bold
(double-struck) as well as bright.
"""
import pathlib
import re
import sys
import tempfile
import unittest

ROOT = pathlib.Path(__file__).resolve().parent.parent
sys.path.insert(0, str(ROOT / 'tools'))
import ans2utf8  # noqa: E402

SGR = re.compile(r'\x1b\[([0-9;]*)m')

def params(text):
    return [m.group(1) for m in SGR.finditer(text)]

class Unbold(unittest.TestCase):
    def test_bold_colour_becomes_bright_colour(self):
        self.assertEqual(ans2utf8.unbold('\x1b[1;31mA'), '\x1b[91mA')

    def test_bold_stays_on_across_a_colour_change(self):
        self.assertEqual(params(ans2utf8.unbold('\x1b[1m\x1b[33mA\x1b[34mB')),
                         ['97', '93', '94'])

    def test_reset_ends_the_bright_colour(self):
        self.assertEqual(params(ans2utf8.unbold('\x1b[1;35mA\x1b[0;35mB')),
                         ['95', '0;35'])
        self.assertEqual(params(ans2utf8.unbold('\x1b[1;31mA\x1b[mB\x1b[32mC')),
                         ['91', '0', '32'])

    def test_sgr_22_ends_the_bright_colour(self):
        self.assertEqual(params(ans2utf8.unbold('\x1b[1;36mA\x1b[22mB')),
                         ['96', '36'])

    def test_other_attributes_and_colours_pass_through(self):
        self.assertEqual(params(ans2utf8.unbold('\x1b[44;5mA\x1b[38;5;1mB\x1b[37mC')),
                         ['44;5', '38;5;1', '37'])

class Banner(unittest.TestCase):
    def test_banner_has_no_bold(self):
        art = ROOT / 'art' / 'up_rough_banner.ans'
        with tempfile.TemporaryDirectory() as d:
            out = pathlib.Path(d) / 'banner'
            ans2utf8.main(str(art), str(out))
            text = out.read_text('utf-8')
        sgrs = params(text)
        self.assertTrue(any(s.split(';')[-1].startswith('9') for s in sgrs),
                        'the banner lost its bright colours')
        bold = [s for s in sgrs if '1' in s.split(';')]
        self.assertEqual(bold, [], 'SGR 1 left in the banner')

if __name__ == '__main__':
    unittest.main()
