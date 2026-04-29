#!/usr/bin/env python3

import struct
import sys
import zlib
from pathlib import Path


def paeth(a: int, b: int, c: int) -> int:
    p = a + b - c
    pa = abs(p - a)
    pb = abs(p - b)
    pc = abs(p - c)
    if pa <= pb and pa <= pc:
        return a
    if pb <= pc:
        return b
    return c


def decode_png_rgb8(path: Path):
    data = path.read_bytes()
    if data[:8] != b"\x89PNG\r\n\x1a\n":
        raise ValueError("Not a PNG file")

    pos = 8
    width = height = None
    bit_depth = color_type = compression = filter_method = interlace = None
    idat = bytearray()

    while pos < len(data):
        length = struct.unpack(">I", data[pos:pos + 4])[0]
        ctype = data[pos + 4:pos + 8]
        chunk = data[pos + 8:pos + 8 + length]
        pos += 12 + length

        if ctype == b"IHDR":
            width, height, bit_depth, color_type, compression, filter_method, interlace = struct.unpack(">IIBBBBB", chunk)
        elif ctype == b"IDAT":
            idat.extend(chunk)
        elif ctype == b"IEND":
            break

    if bit_depth != 8 or color_type != 2 or compression != 0 or filter_method != 0 or interlace != 0:
        raise ValueError("This converter supports only non-interlaced 8-bit RGB PNG files")

    raw = zlib.decompress(bytes(idat))
    stride = width * 3
    rows = []
    prev = bytearray(stride)
    i = 0

    for _ in range(height):
      filter_type = raw[i]
      i += 1
      cur = bytearray(raw[i:i + stride])
      i += stride

      if filter_type == 1:
          for x in range(stride):
              cur[x] = (cur[x] + (cur[x - 3] if x >= 3 else 0)) & 0xFF
      elif filter_type == 2:
          for x in range(stride):
              cur[x] = (cur[x] + prev[x]) & 0xFF
      elif filter_type == 3:
          for x in range(stride):
              left = cur[x - 3] if x >= 3 else 0
              cur[x] = (cur[x] + ((left + prev[x]) >> 1)) & 0xFF
      elif filter_type == 4:
          for x in range(stride):
              left = cur[x - 3] if x >= 3 else 0
              up = prev[x]
              up_left = prev[x - 3] if x >= 3 else 0
              cur[x] = (cur[x] + paeth(left, up, up_left)) & 0xFF
      elif filter_type != 0:
          raise ValueError(f"Unsupported PNG filter type {filter_type}")

      rows.append(cur)
      prev = cur

    return width, height, rows


def main():
    if len(sys.argv) != 3:
        print("Usage: png_to_rgb565.py input.png output.h")
        sys.exit(1)

    src = Path(sys.argv[1])
    dst = Path(sys.argv[2])
    width, height, rows = decode_png_rgb8(src)

    pixels = []
    for row in rows:
        for x in range(0, len(row), 3):
            r, g, b = row[x], row[x + 1], row[x + 2]
            rgb565 = ((r & 0xF8) << 8) | ((g & 0xFC) << 3) | (b >> 3)
            pixels.append(rgb565)

    with dst.open("w") as f:
        f.write("#pragma once\n#include <stdint.h>\n\n")
        f.write(f"constexpr uint16_t VAN_BACKGROUND_WIDTH = {width};\n")
        f.write(f"constexpr uint16_t VAN_BACKGROUND_HEIGHT = {height};\n")
        f.write("const uint16_t vanBackground565[] = {\n")
        for idx, px in enumerate(pixels):
            if idx % 12 == 0:
                f.write("  ")
            f.write(f"0x{px:04X}")
            if idx != len(pixels) - 1:
                f.write(", ")
            if idx % 12 == 11:
                f.write("\n")
        if len(pixels) % 12 != 0:
            f.write("\n")
        f.write("};\n")


if __name__ == "__main__":
    main()
