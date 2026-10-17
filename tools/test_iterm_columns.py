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


class Normalisations(unittest.TestCase):
    def test_empty_iterm_cell_equals_engine_blank(self):
        # iTerm2 reports a never-written cell as NUL with fg/bg default; the engine blank has no foreground
        ours = refdiff.parse_dump(dump([[(" ",)]]), 1)
        theirs = refdiff.parse_dump("\0\n%s\n@0,0\nunhandled 0\n" % cell(" ").replace("32", "0"), 1)
        counts, _ = refdiff.iterm_columns(ours, theirs, 1, 1)
        self.assertEqual((counts["colour"], counts["text"]), (0, 0))

    def test_unreadable_cell_is_its_own_column(self):
        ours = refdiff.parse_dump(dump([[(" ", 7, 4)]]), 1)
        theirs = refdiff.parse_dump(dump([[(" ", refdiff.UNREADABLE, refdiff.UNREADABLE)]]), 1)
        counts, cells = refdiff.iterm_columns(ours, theirs, 1, 1)
        self.assertEqual((counts["colour"], counts["attr"], counts["unreadable"]), (0, 0, 1))
        self.assertEqual(cells, [])
        plain = refdiff.parse_dump(dump([[(" ",)]]), 1)
        self.assertEqual(refdiff.iterm_columns(plain, theirs, 1, 1)[0]["unreadable"], 0)

    def test_inverse_colours_are_unswapped_to_the_engine_meaning(self):
        import iterm_compare
        self.assertEqual(iterm_compare.paint_colours(DEF, 27, True), (27, DEF))
        self.assertEqual(iterm_compare.paint_colours(DEF, 27, False), (DEF, 27))
        # the engine cell (fg 27 + inverse) and the API cell (swapped, inverse) then agree, blank or not
        for ch in ("i", " "):
            ours = refdiff.parse_dump(dump([[(ch, 27, DEF, 0x20)]]), 1)
            fg, bg = iterm_compare.paint_colours(DEF, 27, True)
            theirs = refdiff.parse_dump(dump([[(ch, fg, bg, 0x20)]]), 1)
            self.assertEqual(refdiff.iterm_columns(ours, theirs, 1, 1)[0]["colour"], 0)

    def test_cursor_row_is_screen_relative(self):
        import iterm_compare
        self.assertEqual(iterm_compare.screen_row(94, 72), 22)


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
