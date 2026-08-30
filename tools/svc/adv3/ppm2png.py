import sys, zlib, struct

def readppm(p):
    f = open(p, 'rb')
    assert f.readline().strip() == b'P6'
    line = f.readline()
    while line.startswith(b'#'):
        line = f.readline()
    w, h = map(int, line.split())
    f.readline()
    return w, h, f.read(w * h * 3)

def png(path, w, h, data, scale=1):
    if scale > 1:
        rows = []
        for y in range(h):
            row = data[y * w * 3:(y + 1) * w * 3]
            big = b''.join(row[x * 3:x * 3 + 3] * scale for x in range(w))
            for _ in range(scale):
                rows.append(big)
        w, h = w * scale, h * scale
        raw = b''.join(b'\x00' + r for r in rows)
    else:
        raw = b''.join(b'\x00' + data[y * w * 3:(y + 1) * w * 3] for y in range(h))
    def chunk(t, d):
        c = t + d
        return struct.pack('>I', len(d)) + c + struct.pack('>I', zlib.crc32(c) & 0xffffffff)
    out = b'\x89PNG\r\n\x1a\n'
    out += chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 2, 0, 0, 0))
    out += chunk(b'IDAT', zlib.compress(raw, 9))
    out += chunk(b'IEND', b'')
    open(path, 'wb').write(out)

for src in sys.argv[1:]:
    w, h, d = readppm(src)
    png(src[:-4] + '.png', w, h, d, 2)
    print(src, w, h)
