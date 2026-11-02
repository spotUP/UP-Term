# icat picture in UP-Term (rig 62, main 8a4f7483, 2 Nov)
Rig 62 binaries md5-equal to build/amiga: icat ade982e2, vtcon-handler 4e7bc5c3, UPTerm 1b931b71.
Window: ami.open_window() (SYS:System/UP-Term, 800x400). Typed `icat RAM:t.png` + Return.
PNG (160x100, 4 stripes 40x100 red/green/blue/yellow, made with PIL): PASS. Pixels exact-colour (tol 40): 4000/4000/4000/4000 = 16000 (160*100) in the 800x600 screenshot; empty terminal has 0 of these colours (palette: 000000, aaaaaa, 6688bb, ffffff).
JPEG (RAM:t.jpg): no picture, icat stderr empty (2> redirect), screen unchanged; rig has png.datatype only in SYS:Classes/DataTypes listing for jpeg (no jpeg class found by grep).
IFF (hand-made ILBM, 2 planes): icat printed "icat: no datatype reads RAM:t.iff (picture.datatype and the format's class must be installed...)". ILBM class grep count 1, so my hand-made ILBM may be malformed: unresolved.
Screenshot: ~/Desktop/upterm-shots/icat-upterm.png
