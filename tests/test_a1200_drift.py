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
