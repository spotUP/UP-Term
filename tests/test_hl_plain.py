#!/usr/bin/env python3
"""hl -p is the cat of vsh (dist/vshrc, Prefs 'Highlight files shown with
cat'): a .md or .markdown file on a console is drawn as mdv draws it; to a
pipe or a file it is the bytes of the file, as cat. Any other file keeps its
colours on a console and stays plain through a pipe. Runs build/hl and
build/mdv on a pty."""
import os, pty, subprocess, sys, tempfile, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
HL = os.path.join(ROOT, 'build', 'hl')
MDV = os.path.join(ROOT, 'build', 'mdv')
DOC = b'# Title\n\nsome *text* and `code`\n\n- one\n- two\n'
ENV = dict(os.environ, TERM='xterm-256color', COLUMNS='60', HL_THEME='ansi')
ENV.pop('NO_COLOR', None)


def put(path, data):
    with open(path, 'wb') as f:
        f.write(data)


def on_tty(argv):
    """argv with standard output a pty: what it wrote."""
    master, slave = pty.openpty()
    p = subprocess.Popen(argv, stdout=slave, stdin=subprocess.DEVNULL, env=ENV)
    os.close(slave)
    out = b''
    while True:
        try:
            d = os.read(master, 4096)
        except OSError:
            break
        if not d:
            break
        out += d
    p.wait()
    os.close(master)
    return out.replace(b'\r\n', b'\n')


def piped(argv):
    return subprocess.run(argv, stdout=subprocess.PIPE, stdin=subprocess.DEVNULL, env=ENV).stdout


class HlPlain(unittest.TestCase):
    def setUp(self):
        self.tmp = tempfile.TemporaryDirectory()
        self.md = os.path.join(self.tmp.name, 'README.md')
        put(self.md, DOC)

    def tearDown(self):
        self.tmp.cleanup()

    def test_md_on_a_console_is_drawn_as_mdv_draws_it(self):
        got = on_tty([HL, '-p', self.md])
        self.assertEqual(got, on_tty([MDV, self.md]))
        self.assertIn(b'\x1b[', got)
        self.assertNotIn(b'# Title', got)    # the heading marker is gone: formatted

    def test_md_to_a_pipe_is_the_file(self):
        self.assertEqual(piped([HL, '-p', self.md]), DOC)

    def test_markdown_extension_and_capitals(self):
        for name in ('notes.markdown', 'NOTES.MD'):
            p = os.path.join(self.tmp.name, name)
            put(p, DOC)
            self.assertEqual(on_tty([HL, '-p', p]), on_tty([MDV, p]), name)
            self.assertEqual(piped([HL, '-p', p]), DOC, name)

    def test_without_p_a_md_file_stays_a_source(self):
        # numbers and colours: hl as it always was
        self.assertNotEqual(on_tty([HL, self.md]), on_tty([MDV, self.md]))

    def test_other_files_keep_hl_p_behaviour(self):
        c = os.path.join(self.tmp.name, 'a.c')
        put(c, b'int main(void) { return 0; }\n')
        coloured = on_tty([HL, '-p', c])
        self.assertIn(b'\x1b[', coloured)
        self.assertEqual(piped([HL, '-p', c]), b'int main(void) { return 0; }\n')
        # a name that merely ends in md is not Markdown
        m = os.path.join(self.tmp.name, 'cmd')
        put(m, DOC)
        self.assertEqual(piped([HL, '-p', m]), DOC)


if __name__ == '__main__':
    for b in (HL, MDV):
        if not os.path.exists(b):
            sys.exit('build %s first (make test does)' % b)
    unittest.main()
