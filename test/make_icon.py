"""Writes the 10x10 1-bit app icon (icon.png) using only the stdlib."""
import os
import struct
import zlib

# arcade joystick + two buttons
ART = [
    "....#.....",
    "...###....",
    "....#.....",
    "....#..##.",
    "..###..##.",
    ".######...",
    ".#....#.##",
    ".#....#.##",
    ".######...",
    "..........",
]


def chunk(tag, data):
    return struct.pack(">I", len(data)) + tag + data + struct.pack(">I", zlib.crc32(tag + data) & 0xFFFFFFFF)


raw = b""
for row in ART:
    bits = 0
    for i, ch in enumerate(row):
        if ch != "#":  # 1-bit grayscale: 1 = white, 0 = black
            bits |= 1 << (15 - i)
    raw += b"\x00" + struct.pack(">H", bits)

png = b"\x89PNG\r\n\x1a\n"
png += chunk(b"IHDR", struct.pack(">IIBBBBB", 10, 10, 1, 0, 0, 0, 0))
png += chunk(b"IDAT", zlib.compress(raw))
png += chunk(b"IEND", b"")

out = os.path.join(os.path.dirname(__file__), "..", "icon.png")
with open(out, "wb") as f:
    f.write(png)
print("wrote", os.path.abspath(out))
