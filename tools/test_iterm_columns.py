#!/usr/bin/env python3
"""The iterm column of tools/refdiff.py: parsing a vtdump-format dump (the
shape tools/iterm_compare.py writes from iTerm2) and counting text / attribute /
colour differences. No iTerm2 needed. Run: .venv/bin/python tools/test_iterm_columns.py"""
import pathlib, sys, unittest

sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent))
import refdiff

DEF = 256


def cell(ch, fg=DEF, bg=DEF, attr=0):
    return "%d,%d,%d,%d" % (fg, bg, attr, ord(ch))


def dump(rows, cursor="@1,0"):
    text = ["".join(c for c, *_ in r) for r in rows]
    cells = [" ".join(cell(*c) for c in r) for r in rows]
    return "\n".join(text + cells + [cursor, "unhandled 0", ""])


class IttermColumns(unittest.TestCase):
    def test_columns_count_separately(self):
        base = [[("a",), ("b",)], [("c",), ("d",)]]
        ours = refdiff.parse_dump(dump(base), 2)
        theirs = refdiff.parse_dump(dump([
            [("a",), ("X",)],                       # text differs at (0,1)
            [("c", DEF, DEF, 1), ("d", 2, DEF)],    # bold at (1,0), fg index 2 at (1,1)
        ], cursor="@0,1"), 2)
        counts, cells = refdiff.iterm_columns(ours, theirs, 2, 2)
        self.assertEqual(counts["cells"], 4)
        self.assertEqual(counts["text"], 1)
        self.assertEqual(counts["attr"], 1)
        self.assertEqual(counts["colour"], 1)
        self.assertEqual(counts["cursor"], 1)
        self.assertIn((0, 1, "text", "b", "X"), cells)
        self.assertEqual([c[:3] for c in cells if c[2] == "attr"], [(1, 0, "attr")])

    def test_identical_dumps_differ_nowhere(self):
        d = dump([[("a",), ("b",)]])
        g = refdiff.parse_dump(d, 1)
        counts, cells = refdiff.iterm_columns(g, g, 2, 1)
        self.assertEqual((counts["text"], counts["attr"], counts["colour"], counts["cursor"]), (0, 0, 0, 0))
        self.assertEqual(cells, [])


class ReplayCommand(unittest.TestCase):
    def test_bare_lf_reaches_the_terminal_unchanged(self):
        # The replay pty must not turn LF into CR LF (ONLCR): tmux pane repaints
        # rely on bare LF keeping the column.
        import os, pty, select, shlex, subprocess, tempfile
        import iterm_compare
        with tempfile.NamedTemporaryFile(suffix=".bin") as f:
            f.write(b"A\x1b[5GB\nC")
            f.flush()
            cmd = iterm_compare.replay_command(pathlib.Path(f.name)).replace("sleep 600", "true")
            master, slave = pty.openpty()
            proc = subprocess.Popen(shlex.split(cmd), stdin=slave, stdout=slave, stderr=slave, close_fds=True)
            os.close(slave)
            os.write(master, b"\n")
            out = b""
            while True:
                r, _, _ = select.select([master], [], [], 5)
                if not r:
                    break
                try:
                    chunk = os.read(master, 4096)
                except OSError:
                    break
                if not chunk:
                    break
                out += chunk
            proc.wait(timeout=5)
            os.close(master)
        self.assertIn(b"B\nC", out)
        self.assertNotIn(b"\r\nC", out)


if __name__ == "__main__":
    unittest.main()
