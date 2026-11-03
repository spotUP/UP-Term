"""a1200_drift: the install.dos Copy lines map to Amiga paths; verdicts name same / DIFF / missing."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import a1200_drift as d

DOS = """Copy vtcon-handler L: CLONE
Copy upprefs C:UP-Term-Prefs CLONE
  Copy Amarchy amarchy-menu UP-Term:bin CLONE
Copy vsh UP-Term:bin/vsh CLONE
Copy vsh C:vsh CLONE
Copy userland/bin UP-Term:bin ALL CLONE QUIET
Copy keymap ENVARC:up-term/keymap CLONE QUIET
Copy up-console.device DEVS:up-console.device CLONE
"""
NAMES = ["vtcon-handler", "upprefs", "Amarchy", "amarchy-menu", "vsh", "up-console.device"]


def test_map():
    assert d.install_map(DOS, NAMES) == [
        ("vtcon-handler", "L:vtcon-handler"), ("upprefs", "C:UP-Term-Prefs"),
        ("Amarchy", "UP-Term:bin/Amarchy"), ("amarchy-menu", "UP-Term:bin/amarchy-menu"),
        ("vsh", "UP-Term:bin/vsh"), ("vsh", "C:vsh"), ("up-console.device", "DEVS:up-console.device")]


def test_real_install_dos_covers_every_amarchy_program():
    text = (pathlib.Path(__file__).resolve().parent.parent / "dist/install.dos").read_text(errors="replace")
    names = ["Amarchy", "amarchy-menu", "amarchy-gui", "amarchy-index"]
    got = d.install_map(text, names)
    for n in names:
        assert (n, "UP-Term:bin/" + n) in got


def test_verdicts():
    e = [("a", "C:a"), ("b", "C:b"), ("c", "C:c")]
    r = d.verdicts(e, {"a": "1", "b": "2", "c": "3"}, {"C:A": "1", "C:b": "9"})
    assert [x[0] for x in r] == ["same", "DIFF", "missing"]


def test_parse_md5sum():
    assert d.parse_md5sum("rc 0\n" + "a" * 32 + "  UP-Term:bin/x y\nmd5sum: nope\n") == {"UP-Term:bin/x y": "a" * 32}


def test_real_install_dos_checks_keymap_in_envarc_and_env():
    text = (pathlib.Path(__file__).resolve().parent.parent / "dist/install.dos").read_text(errors="replace")
    got = d.install_map(text, ["Amarchy"] + list(d.DIST_SOURCES))
    assert ("keymap", "ENVARC:up-term/keymap") in got
    assert ("keymap", "ENV:up-term/keymap") in got
    assert (pathlib.Path(__file__).resolve().parent.parent / "dist/keymap").is_file()


def test_program_names_from_the_amiga_and_dist_base_rules():
    mk = "amiga: $(BUILD)/amiga/vtengine-$(CPU).o $(BUILD)/amiga/vsh $(BUILD)/amiga/Amarchy\n" \
         "dist-base: amiga $(BUILD)/amiga/amarchy-menu $(BUILD)/amiga/vsh\nother: $(BUILD)/amiga/x\n"
    assert d.program_names(mk) == ["vsh", "Amarchy", "amarchy-menu"]


def test_unbuilt_installed_programs_are_named_not_skipped():
    progs = ["vtcon-handler", "upprefs", "Amarchy", "amarchy-menu", "vsh", "up-console.device", "notinstalled"]
    assert d.unchecked(DOS, progs, ["Amarchy", "vsh"]) == [
        "amarchy-menu", "up-console.device", "upprefs", "vtcon-handler"]


def test_real_makefile_lists_every_amarchy_program():
    root = pathlib.Path(__file__).resolve().parent.parent
    names = d.program_names((root / "Makefile").read_text(errors="replace"))
    for n in ("Amarchy", "amarchy-menu", "amarchy-gui", "amarchy-index", "vtcon-handler", "upprefs"):
        assert n in names


def test_libs_are_checked_from_the_paths_module_sources():
    got = {n: (p, src) for n, p, src in d.lib_entries()}
    assert got["ixemul.library"] == ("LIBS:ixemul.library", d.paths.IXEMUL_LIB)
    assert got["ixnet.library"] == ("LIBS:ixnet.library", d.paths.IXNET_LIB)


def test_nofpu_checks_the_soft_float_ixemul():
    got = {n: src for n, _, src in d.lib_entries(nofpu=True)}
    assert got["ixemul.library"] == d.paths.IXEMUL_LIB_SOFT != d.paths.IXEMUL_LIB
    assert got["ixnet.library"] == d.paths.IXNET_LIB


def test_loads_under_python_isolated_mode():
    # The documented run is `python3 -I tools/rig/a1200_drift.py`; -I leaves the script's drawer off sys.path,
    # so the script must put it there itself before `import paths`.
    import subprocess
    script = pathlib.Path(d.__file__).resolve()
    r = subprocess.run([sys.executable, "-I", "-c", "import runpy, sys; runpy.run_path(sys.argv[1])", str(script)],
                       cwd=str(script.anchor), capture_output=True, text=True)
    assert r.returncode == 0, r.stderr


def test_data_the_installer_lays_down_is_checked_and_missing_data_is_drift():
    # A1200-DRIFT-DATA: programs current but no Amarchy themes, lucide.dat or bar.conf must read as drift.
    root = pathlib.Path(__file__).resolve().parent.parent
    text = (root / "dist/install.dos").read_text(errors="replace")
    entries = [(n, p) for n, p, _ in d.data_entries(text)]
    got = {p: n for n, p in entries}
    assert got["UP-Term:Amarchy/Icons/lucide.dat"] == "lucide/lucide.dat"
    assert got["ENVARC:Amarchy/bar.conf"] == "bar.conf"
    assert "UP-Term:Amarchy/Themes/nord.theme" in got
    build = {n: None if n in d.USER_OWNED else "1" for n, _ in entries}
    assert {s for s, _, _ in d.verdicts(entries, build, {})} == {"missing"}
    there = {p: "1" for _, p in entries}
    there["ENVARC:Amarchy/bar.conf"] = "user edited"
    assert {s for s, _, _ in d.verdicts(entries, build, there)} == {"same"}
    there["UP-Term:Amarchy/Icons/lucide.dat"] = "old"
    assert [n for s, n, _ in d.verdicts(entries, build, there) if s == "DIFF"] == ["lucide/lucide.dat"]


def test_installed_kit_media_is_md5_checked_on_the_amiga(monkeypatch, capsys, tmp_path):
    """RIG-DRIFT-MEDIA-INSTALLED: main() asks the Amiga for UP-Term:share/updemo/earth.anim and reports missing / DIFF."""
    asked = []
    amiga = {}
    monkeypatch.setattr(d, "amiga_md5s", lambda ps: (asked.extend(ps), amiga)[1])
    monkeypatch.setattr(d, "lib_entries", lambda nofpu=False: [])
    path = "UP-Term:share/updemo/earth.anim"

    assert d.main(["--build", str(tmp_path)]) == 1
    assert path in asked
    assert "missing  share/updemo/earth.anim" in capsys.readouterr().out

    amiga[path] = "0" * 32
    assert d.main(["--build", str(tmp_path)]) == 1
    assert "DIFF     share/updemo/earth.anim" in capsys.readouterr().out

    amiga[path] = d.md5_file(d.ROOT / "dist/updemo/earth.anim")
    d.main(["--build", str(tmp_path)])
    assert "same     share/updemo/earth.anim" in capsys.readouterr().out


def test_every_kit_media_file_exists_and_install_dos_copies_it():
    """KIT-DEMO-MEDIA: each KIT_MEDIA entry (earth.anim, kanim-stream.bin, photo.png) is a shipped file in dist/ and has
    its own Copy line in install.dos to UP-Term:<its drawer>."""
    assert {"share/updemo/kanim-stream.bin", "share/updemo/photo.png"} <= set(d.KIT_MEDIA)
    install = (d.ROOT / "dist/install.dos").read_text()
    for rel, src in d.KIT_MEDIA.items():
        assert (d.ROOT / "dist" / src).stat().st_size > 0, src
        assert "Copy share/%s UP-Term:%s " % (src, rel.rsplit("/", 1)[0]) in install, src
