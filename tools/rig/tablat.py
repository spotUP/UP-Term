"""Tab latency: type a word, press Tab, poll the input line until it changes."""
import sys, time, struct
sys.path.insert(0, "/Users/spot/Code/vtcon/tools/rig")
import ami
def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def region():
    return ami.req(0x07, struct.pack('>4H', 0, 12, 780, 120))  # the top lines only
ami.req(0x02, struct.pack('>H', 10) + b'run >NIL: newshell "XCON:0/12/780/560/tab latency/CLOSE"')
time.sleep(4)
t('VTC:vsh'); ami.key(0x44); time.sleep(3)
for word in sys.argv[1:]:
    t(word); time.sleep(1.5)
    before = region()
    t0 = time.time(); ami.key(0x42 if not word.startswith('BASE') else 0x20)  # BASE presses 'a' (baseline)
    while time.time() - t0 < 20:
        if region() != before:
            break
        time.sleep(0.05)
    print("%-16s %.2f s" % (word, time.time() - t0))
    ami.key(0x16, 0x08); time.sleep(0.5)       # Ctrl-U
