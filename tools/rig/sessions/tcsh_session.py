import sys, time, ami, struct
S = "../../build/rig/shots/"
def t(s): ami.req(0x08, bytes([4]) + s.encode('latin-1')); time.sleep(0.4)
def k(code, q=0):
    ami.key(code, q); time.sleep(0.25)
def ret(): k(0x44); time.sleep(1.5)
def shot(n): 
    b = ami.req(0x07); fmt, w, h, nc = b[0], *struct.unpack('>HHH', b[2:8])
    import zlib
    pal = b[8:8+nc*3]; px = b[8+nc*3:]
    rows = [b''.join(pal[i*3:i*3+3] for i in px[y*w:(y+1)*w]) for y in range(h)]
    raw = b''.join(b'\0'+r for r in rows)
    def ch(tg, d): return struct.pack('>I', len(d)) + tg + d + struct.pack('>I', zlib.crc32(tg + d))
    open(S+n+".png",'wb').write(b'\x89PNG\r\n\x1a\n' + ch(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0)) + ch(b'IDAT', zlib.compress(raw)) + ch(b'IEND', b''))
ami.req(0x02, struct.pack('>H',10)+b'run >NIL: newshell "XCON:0/12/640/228/tcsh test/CLOSE"'); time.sleep(4)
t('VTC:pkgs/shells/tcsh'); ret(); time.sleep(5)
t('echo hello'); ret()
k(0x4c); t(''); time.sleep(0.5)           # Up: history
k(0x4f); k(0x4f); t('XX'); ret()           # edit inside: helXXlo
t('echo ' + 'abcdefghij'*9 + 'END'); ret() # a long line that wraps
k(0x4c); k(0x20, 8); t('echo START '); ret()  # Up, Ctrl-A, insert at start
shot("ts1")
t('ls-F /VTC/pk'); k(0x42); time.sleep(1); ret()   # Tab completion
t('set color'); ret(); t('ls-F /VTC'); ret()
shot("ts2")
t('echo cancelled line'); k(0x33, 8); time.sleep(1)   # Ctrl-C
t('clear'); ret(); t('echo after clear'); ret()
shot("ts3")
