#!/usr/bin/env python3
"""wget() and tldr() of dist/vshrc (unix-tool-ports 5.3 and 5.9): functions
over curl. The functions are cut out of the real dist/vshrc and run by a POSIX
sh against a local HTTP server with the host's curl; tldr draws through
build/mdv. The rig half is tools/rig/wget_rig.py."""
import http.server, os, re, shutil, subprocess, sys, tempfile, threading, unittest

ROOT = os.path.abspath(os.path.join(os.path.dirname(__file__), '..'))
MDV = os.path.join(ROOT, 'build', 'mdv')


def functions():
    src = open(os.path.join(ROOT, 'dist', 'vshrc')).read()
    out = ''
    for name in ('wget', 'tldr'):
        m = re.search(r'^%s\(\) \{\n.*?^\}\n' % name, src, re.S | re.M)
        assert m, name + '() is not in dist/vshrc'
        out += m.group(0)
    return out


class Quiet(http.server.SimpleHTTPRequestHandler):
    def log_message(self, *a):
        pass


class Net(unittest.TestCase):
    @classmethod
    def setUpClass(cls):
        cls.tmp = tempfile.mkdtemp(prefix='vshrcnet')
        cls.www = os.path.join(cls.tmp, 'www')
        os.makedirs(os.path.join(cls.www, 'pages', 'common'))
        open(os.path.join(cls.www, 'hello.txt'), 'w').write('hello net\n')
        open(os.path.join(cls.www, 'pages', 'common', 'tar.md'), 'w').write('# tar\n\n> Archiving utility.\n\n- Create an archive:\n\n`tar cf {{target.tar}} {{file1}}`\n')
        cls.work = os.path.join(cls.tmp, 'work')
        os.makedirs(cls.work)
        handler = lambda *a, **k: Quiet(*a, directory=cls.www, **k)
        cls.srv = http.server.ThreadingHTTPServer(('127.0.0.1', 0), handler)
        cls.url = 'http://127.0.0.1:%d' % cls.srv.server_address[1]
        threading.Thread(target=cls.srv.serve_forever, daemon=True).start()

    @classmethod
    def tearDownClass(cls):
        cls.srv.shutdown()
        shutil.rmtree(cls.tmp, ignore_errors=True)

    def sh(self, line, **env):
        e = dict(os.environ, TMPDIR=self.tmp, **env)
        e['PATH'] = os.path.join(ROOT, 'build') + ':' + e['PATH']
        p = subprocess.run(['sh', '-c', functions() + line], cwd=self.work, env=e, capture_output=True, text=True)
        return p.returncode, p.stdout, p.stderr

    def test_wget_saves_the_file_under_the_last_name_of_the_url(self):
        rc, _, _ = self.sh('wget -q %s/hello.txt' % self.url)
        self.assertEqual(rc, 0)
        self.assertEqual(open(os.path.join(self.work, 'hello.txt')).read(), 'hello net\n')

    def test_wget_O_names_the_file_and_O_dash_writes_to_stdout(self):
        rc, _, _ = self.sh('wget -q -O named.txt %s/hello.txt' % self.url)
        self.assertEqual(rc, 0)
        self.assertEqual(open(os.path.join(self.work, 'named.txt')).read(), 'hello net\n')
        rc, out, _ = self.sh('wget -O - %s/hello.txt' % self.url)
        self.assertEqual((rc, out), (0, 'hello net\n'))

    def test_wget_fails_on_a_missing_page_and_leaves_no_file(self):
        rc, _, _ = self.sh('wget -q %s/nothing.txt' % self.url)
        self.assertNotEqual(rc, 0)
        self.assertFalse(os.path.exists(os.path.join(self.work, 'nothing.txt')))

    def test_wget_refuses_an_option_it_does_not_know_and_a_call_without_url(self):
        rc, _, err = self.sh('wget --mirror %s/hello.txt' % self.url)
        self.assertEqual(rc, 2)
        self.assertIn('not supported', err)
        rc, _, err = self.sh('wget')
        self.assertEqual(rc, 2)
        self.assertIn('usage', err)

    @unittest.skipUnless(os.path.exists(MDV), 'build/mdv missing (make build/mdv)')
    def test_tldr_draws_the_page_of_the_command(self):
        rc, out, _ = self.sh('tldr tar', TLDR_URL=self.url + '/pages', TERM='xterm-256color')
        self.assertEqual(rc, 0)
        self.assertIn('Create an archive', out)
        self.assertIn('target.tar', out)

    def test_tldr_says_so_when_there_is_no_page(self):
        rc, _, err = self.sh('tldr nosuchtool', TLDR_URL=self.url + '/pages')
        self.assertEqual(rc, 1)
        self.assertIn('no page for nosuchtool', err)


if __name__ == '__main__':
    unittest.main()
