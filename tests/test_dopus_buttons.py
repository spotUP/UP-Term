#!/usr/bin/env python3
"""The Directory Opus button bank tools/gen_dopus_buttons.py writes (plan 2026-10-25-amarchy-dopus-integration.md, D12):
the bytes are parsed back by an independent reader following L_OpenButtonBank (config_open.c:280-440), and the
stock DOpus5/Buttons/Hotkeys file (from vendor/dopus5-5.104, when present) is parsed by the same reader."""
import pathlib, struct, subprocess, sys, unittest
ROOT = pathlib.Path(__file__).resolve().parents[1]
sys.path.insert(0, str(ROOT / "tools"))
import gen_dopus_buttons as g


def chunks(data):
    assert data[:4] == b"FORM" and struct.unpack(">I", data[4:8])[0] == len(data) - 8
    form, pos, out = data[8:12], 12, []
    while pos < len(data):
        cid, n = data[pos:pos + 4], struct.unpack(">I", data[pos + 4:pos + 8])[0]
        out.append((cid, data[pos + 8:pos + 8 + n]))
        pos += 8 + n + (n & 1)
    assert pos == len(data)
    return form, out


def parse_bank(data):
    form, cs = chunks(data)
    assert cs[0][0] == b"BTNW" and len(cs[0][1]) == 132
    columns, rows, flags = struct.unpack(">HHI", cs[0][1][124:132])
    buttons, cur = [], None
    for cid, body in cs[1:]:
        if cid == b"BUTN":
            fpen, bpen, bflags, count, _, _ = struct.unpack(">BBIhhI", body[:14])
            assert bflags & g.BUTNF_NEW_FORMAT
            strs = body[14:].split(b"\0")[:2 * count]
            cur = {"label": strs[0], "marker": strs[1], "count": count, "funcs": []}
            buttons.append(cur)
        elif cid == b"FUNC":
            fl, fl2, _, code, qual, ftype, mask, same = struct.unpack(">IIIHHHHH", body[:22])
            cmd, kind = body[22:-2], struct.unpack(">h", body[-2:])[0]
            assert cmd.endswith(b"\0")
            cur["funcs"].append({"flags": fl, "type": ftype, "cmd": cmd[:-1], "kind": kind})
    return form, name_of(cs[0][1]), columns, rows, buttons


def name_of(btnw):
    return btnw[:32].rstrip(b"\0")


class Buttons(unittest.TestCase):
    def test_form_epus_and_header(self):
        form, name, columns, rows, buttons = parse_bank(g.generate())
        self.assertEqual((form, name, columns, rows, len(buttons)), (b"EPUS", b"UP-Term", 4, 1, 4))

    def test_each_button_label_and_launcher(self):
        _, _, _, _, buttons = parse_bank(g.generate())
        got = [(b["label"], b["marker"], b["count"], len(b["funcs"]), b["funcs"][0]["kind"],
                b["funcs"][0]["cmd"]) for b in buttons]
        shell = b"Run >NIL: Execute UP-Term:bin/uterm-shell"
        self.assertEqual(got, [
            (b"UP-Term here", b"\1", 1, 1, g.INST_AMIGADOS, shell),
            (b"Edit", b"\1", 1, 1, g.INST_AMIGADOS, b"Run >NIL: Execute UP-Term:bin/uterm-edit {f}"),
            (b"Page", b"\1", 1, 1, g.INST_AMIGADOS, b"Run >NIL: Execute UP-Term:bin/uterm-view {f}"),
            (b"Shell", b"\1", 1, 1, g.INST_AMIGADOS, shell)])

    def test_flags_here_edit_page_change_into_the_lister_path_shell_does_not(self):
        _, _, _, _, buttons = parse_bank(g.generate())
        cd = [bool(b["funcs"][0]["flags"] & g.FUNCF_CD_SOURCE) for b in buttons]
        self.assertEqual(cd, [True, True, True, False])
        self.assertTrue(all(b["funcs"][0]["flags"] & g.FUNCF_RUN_ASYNC for b in buttons))

    def test_launchers_named_exist_in_dist(self):
        for name in ("uterm-edit", "uterm-view", "uterm-shell"):
            self.assertTrue((ROOT / "dist" / name).is_file(), name)

    def test_the_reader_parses_the_stock_hotkeys_bank(self):
        stock = ROOT / "vendor/dopus5-5.104/DOpus5_104_os3.lha"
        if not stock.is_file():
            self.skipTest("vendor/dopus5-5.104 not present")
        import tempfile
        with tempfile.TemporaryDirectory() as d:
            r = subprocess.run(["lha", "xq", str(stock), "DOpus5/Buttons/Hotkeys"], cwd=d, capture_output=True)
            f = pathlib.Path(d) / "DOpus5/Buttons/Hotkeys"
            if r.returncode or not f.is_file():
                self.skipTest("lha not available")
            form, name, _, _, buttons = parse_bank(f.read_bytes())
            self.assertEqual((form, name, len(buttons)), (b"EPUS", b"Hotkeys", 3))


if __name__ == "__main__":
    unittest.main()
