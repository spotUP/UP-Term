"""wbprobe: wincensus text gives diag_ok and the Workbench window census."""
import pathlib, sys
sys.path.insert(0, str(pathlib.Path(__file__).resolve().parent.parent / "tools/rig"))
import wbprobe

TXT = """WBR phase=0 closing=0 closes=2 opens=2 failed=0 timeouts=0 diag_vis=0 diag_wins=2 diag_ok=1
SCREEN 0040a000 WB flags=0001 title=Workbench
WIN 00401000 flags=02000000 WB=yes task=Workbench title=Work
WIN 00402000 flags=00000000 WB=NO task=DOpus title=Lister
SCREEN 0040b000 other flags=000f title=x
WIN 00403000 flags=00000000 WB=NO task=t title=y
"""


def test_census():
    assert wbprobe.read_wincensus(TXT) == {"diag_ok": 1, "diag_wins": 2, "windows": 2, "foreign": 1}


def test_no_wbreset():
    assert wbprobe.read_wincensus("WBR none\n")["diag_ok"] is None
