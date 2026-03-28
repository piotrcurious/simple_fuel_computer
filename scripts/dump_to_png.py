
import os
import sys
import zlib
import struct

def pbm_to_png(pbm_path, png_path):
    """
    Minimal PBM (P1) to PNG converter using only standard libraries (zlib).
    """
    with open(pbm_path, 'r') as f:
        lines = f.readlines()

    if not lines or lines[0].strip() != 'P1':
        return

    # Filter out comments and empty lines to find dimensions
    data_lines = [l for l in lines[1:] if l.strip() and not l.strip().startswith('#')]
    dims = data_lines[0].strip().split()
    width, height = int(dims[0]), int(dims[1])

    pixels = []
    for line in data_lines[1:]:
        pixels.extend([int(x) for x in line.split()])

    # PNG signature
    png = bytearray([137, 80, 78, 71, 13, 10, 26, 10])

    # IHDR chunk
    # Width (4), Height (4), Bit depth (1), Color type (0=grayscale),
    # Compression (0), Filter (0), Interlace (0)
    ihdr_data = struct.pack(">IIBBBBB", width, height, 1, 0, 0, 0, 0)
    ihdr_chunk = b'IHDR' + ihdr_data
    png += struct.pack(">I", len(ihdr_data)) + ihdr_chunk + struct.pack(">I", zlib.crc32(ihdr_chunk) & 0xffffffff)

    # IDAT chunk
    # Each row starts with a filter byte (0)
    scanlines = bytearray()
    row_bytes = (width + 7) // 8
    for y in range(height):
        scanlines.append(0) # Filter type 0
        row = bytearray(row_bytes)
        for x in range(width):
            if pixels[y * width + x]:
                row[x // 8] |= (1 << (7 - (x % 8)))
        scanlines += row

    idat_data = zlib.compress(scanlines)
    idat_chunk = b'IDAT' + idat_data
    png += struct.pack(">I", len(idat_data)) + idat_chunk + struct.pack(">I", zlib.crc32(idat_chunk) & 0xffffffff)

    # IEND chunk
    iend_chunk = b'IEND'
    png += struct.pack(">I", 0) + iend_chunk + struct.pack(">I", zlib.crc32(iend_chunk) & 0xffffffff)

    with open(png_path, 'wb') as f:
        f.write(png)

if __name__ == "__main__":
    if len(sys.argv) < 3:
        print("Usage: python dump_to_png.py <input.pbm> <output.png>")
    else:
        pbm_to_png(sys.argv[1], sys.argv[2])
