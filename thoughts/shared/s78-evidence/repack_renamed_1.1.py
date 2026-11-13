"""Repack live 1.0 ipks of the renamed packages as 1.1: only control's Version changes; data.tar.gz byte-equal."""
import hashlib, io, pathlib, re, sys, tarfile
sys.path.insert(0, '/Users/spot/Code/UP-Term/vtcon/tools')
import pkgs
S = pathlib.Path(sys.argv[1]); live, out = S / 'live', S / 'out'
out.mkdir(exist_ok=True)

def ar_members(b):
    assert b[:8] == b'!<arch>\n'; i, m = 8, []
    while i < len(b):
        h = b[i:i + 60]; name = h[:16].decode().strip().rstrip('/'); size = int(h[48:58]); i += 60
        m.append((name, b[i:i + size])); i += size + (size & 1)
    return m

text = (live / 'Packages').read_text()
for p in ('up-hexed', 'up-picom', 'up-sqlite'):
    src = live / ('%s_1.0_m68k-amigaos.ipk' % p)
    mem = dict(ar_members(src.read_bytes()))
    ents = []
    with tarfile.open(fileobj=io.BytesIO(mem['control.tar.gz'])) as tf:
        for ti in tf.getmembers():
            data = tf.extractfile(ti).read() if ti.isreg() else None
            if ti.name == './control':
                new = re.sub(rb'^Version: 1\.0$', b'Version: 1.1', data, flags=re.M)
                assert new != data, p
                data = new
            ents.append((pkgs._tinfo(ti.name, len(data) if data is not None else 0, ti.mode,
                                     ti.type, ti.linkname), data))
    b = pkgs._ar([('debian-binary', mem['debian-binary']), ('control.tar.gz', pkgs._targz(ents)),
                  ('data.tar.gz', mem['data.tar.gz'])])
    name = '%s_1.1_m68k-amigaos.ipk' % p
    (out / name).write_bytes(b)
    old = re.search(r'(?ms)^Package: %s\n.*?(?=\n\n|\Z)' % re.escape(p), text).group(0)
    new = old.replace('Version: 1.0', 'Version: 1.1').replace('_1.0_', '_1.1_')
    new = re.sub(r'(?m)^Size: \d+$', 'Size: %d' % len(b), new)
    new = re.sub(r'(?m)^SHA256sum: \w+$', 'SHA256sum: ' + hashlib.sha256(b).hexdigest(), new)
    text = text.replace(old, new)
    print('[OK]', name, len(b))
(out / 'Packages').write_text(text)
import gzip
with open(out / 'Packages.gz', 'wb') as f, gzip.GzipFile(filename='', mode='wb', fileobj=f, compresslevel=9, mtime=0) as gz:
    gz.write(text.encode())
