# Showing an image in a vtcon console (kitty graphics), s48

Verified on rig 31 (060, 2026-11-01): a 200x150 PNG stream shown in an UP-Term (XCON:) window, screenshot
`~/Desktop/upterm-shots/kitty-demo-s48.png`. No icat command exists yet (another agent is writing it).

## Today, in an Amiga Shell inside vtcon

1. Get a kitty stream file onto the Amiga, e.g. `RAM:pic.kitty` (generator below).
2. In the vtcon window type:

       cat RAM:pic.kitty

   Do NOT use `type RAM:pic.kitty`: in vsh `type` is the shell builtin (prints "RAM:pic.kitty is RAM:pic.kitty").
   `C:Type` (full path) is the AmigaDOS command if you want it.

The handler parses the APC G sequence and draws the picture at the cursor (1:1 pixels, no scaling).

## Making the stream (any host)

A kitty stream is `ESC _ G <control> ; <base64 payload> ESC \`, payload chunked in 4096 bytes, `m=1` on all but the
last chunk, the control keys only on the first chunk. PNG: `a=T,f=100,t=d`. Raw RGB: `a=T,f=24,t=d,s=<w>,v=<h>`.

    python3 - <<'PY'
    import base64, sys
    png = open("pic.png", "rb").read()           # 8-bit PNG, non-interlaced or Adam7
    b = base64.b64encode(png)
    parts = [b[i:i+4096] for i in range(0, len(b), 4096)]
    out = b""
    for i, p in enumerate(parts):
        m = 1 if i < len(parts) - 1 else 0
        ctl = b"a=T,f=100,t=d,m=%d" % m if i == 0 else b"m=%d" % m
        out += b"\x1b_G" + ctl + b";" + p + b"\x1b\\"
    open("pic.kitty", "wb").write(out + b"\r\n")
    PY

The repo's `tools/rig/kitty_stream.py OUTDIR` writes the small 32x16 test streams the same way.

## Putting it on the Amiga

- Rig: `UPTERM_RIG=<n> python3 tools/rig/ami.py put pic.kitty RAM:pic.kitty`
- Real machine: any file transfer (FTP, serial, the wasabi put).

## A Mac-side program sending one

A program only has to write the same bytes to whatever stream ends up in the vtcon window (the console's input
side: ssh/telnet session or serial line into the Amiga shell, which echoes to the window). `cat pic.kitty` on the
Amiga side of that connection, or `printf` of the sequence above from the remote program, is identical to what the
handler sees. I did not test a live ssh/serial path in this run (unverified); only the file route above.

## Measured result

Source shapes (200x150): yellow square x100-159,y40-99; red disc centre (50,60) r30; blue diamond centre (150,120).
In the 800x600 screenshot the picture sits at offset (+4,+50), 1:1: yellow pixels bbox x104-163,y90-149 (3584 px);
red disc bbox x25-83,y81-139 (centre 54,110); blue bbox x130-178,y146-194 (centre 154,170). Flat colours are exact
(255,255,0 / 255,0,0 / 0,0,255). The gradient background is quantised to a 6-level palette cube on the rig screen
(e.g. source (10,10) shows 51,51,204), so only flat colours were used as exact checks.
