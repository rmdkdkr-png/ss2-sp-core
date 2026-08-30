import struct, zlib, sys
src, dst, x0, x1, y0, y1, sc = sys.argv[1], sys.argv[2], int(sys.argv[3]), int(sys.argv[4]), int(sys.argv[5]), int(sys.argv[6]), int(sys.argv[7])
f = open(src, "rb"); f.readline()
w, h = map(int, f.readline().split()); f.readline(); d = f.read()
W = (x1 - x0) * sc; H = (y1 - y0) * sc
rows = []
for y in range(y0, y1):
    row = b""
    for x in range(x0, x1):
        i = (y * w + x) * 3
        row += d[i:i+3] * sc
    for _ in range(sc):
        rows.append(b"\x00" + row)
raw = b"".join(rows)
def ch(t, x):
    c = t + x
    return struct.pack(">I", len(x)) + c + struct.pack(">I", zlib.crc32(c))
open(dst, "wb").write(b"\x89PNG\r\n\x1a\n" + ch(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0)) + ch(b"IDAT", zlib.compress(raw)) + ch(b"IEND", b""))
print(dst, W, H)
