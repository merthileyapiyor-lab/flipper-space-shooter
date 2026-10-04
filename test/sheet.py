"""Combines test/out/*.pgm frames into one upscaled PNG contact sheet (stdlib only)."""
import glob
import os
import struct
import zlib

HERE = os.path.dirname(os.path.abspath(__file__))
SCALE, PAD, COLS = 2, 5, 4
FG, BG, FRAME_BG = (20, 20, 20), (60, 60, 60), (255, 140, 30)  # Flipper orange LCD


def read_pgm(path):
    data = open(path, "rb").read()
    header_end = data.index(b"255\n") + 4
    return data[header_end:]


frames = sorted(glob.glob(os.path.join(HERE, "out", "*.pgm")))
fw, fh = 128 * SCALE, 64 * SCALE
rows = (len(frames) + COLS - 1) // COLS
W, H = COLS * fw + (COLS + 1) * PAD, rows * fh + (rows + 1) * PAD
img = [[BG] * W for _ in range(H)]
for n, path in enumerate(frames):
    px = read_pgm(path)
    ox = PAD + (n % COLS) * (fw + PAD)
    oy = PAD + (n // COLS) * (fh + PAD)
    for y in range(fh):
        for x in range(fw):
            img[oy + y][ox + x] = FG if px[(y // SCALE) * 128 + x // SCALE] == 0 else FRAME_BG

raw = b"".join(b"\x00" + b"".join(struct.pack("BBB", *p) for p in row) for row in img)


def chunk(tag, d):
    return struct.pack(">I", len(d)) + tag + d + struct.pack(">I", zlib.crc32(tag + d) & 0xFFFFFFFF)


out = os.path.join(HERE, "out", "sheet.png")
with open(out, "wb") as f:
    f.write(b"\x89PNG\r\n\x1a\n" + chunk(b"IHDR", struct.pack(">IIBBBBB", W, H, 8, 2, 0, 0, 0))
            + chunk(b"IDAT", zlib.compress(raw, 9)) + chunk(b"IEND", b""))
print("frames:", [os.path.basename(p) for p in frames])
print("wrote", out)
