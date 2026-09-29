#!/usr/bin/env python3
"""ttyprobe_rig.py -- runs tests/amiga/ttyprobe in an XCON: window on the
rig and types what it asks for; the screenshot is the result
(build/rig/shots/ttyprobe.png). The rig must be up, VTC:ttyprobe and the
handler installed."""
import os, struct, sys, time
sys.path.insert(0, os.path.dirname(os.path.abspath(__file__)))
import ami

OUT = os.path.join(os.path.dirname(os.path.abspath(__file__)), "../../build/rig/shots/ttyprobe.png")

def t(s):
    ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)

def main():
    ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/640/220/ttyprobe/CLOSE"')
    time.sleep(4)
    t('VTC:ttyprobe'); ami.key(0x44); time.sleep(3)
    t('x'); time.sleep(3)                  # 3: one raw byte; 4: a one-second VTIME read
    t('secret'); ami.key(0x44); time.sleep(2)   # 5: canonical, no echo
    ami.key(0x33, 0x08); time.sleep(2)     # 6: Ctrl-C through ISIG
    time.sleep(2)
    ami.main(['shot', OUT])

if __name__ == '__main__':
    main()
