#!/usr/bin/env python3
"""
TermuOS BMP Generator
Creates 24-bit uncompressed BMP images for use as desktop backgrounds and icons.
Usage:
    python3 mkbmp.py                  # generate all presets
    python3 mkbmp.py bg gradient      # specific background style
    python3 mkbmp.py icon terminal    # specific icon
"""

import struct
import sys
import os
import math

# ─── BMP writer ───────────────────────────────────────────────────────────────

def write_bmp(path, pixels, width, height):
    """Write a 24-bit uncompressed bottom-up BMP file.
    pixels: list of (r, g, b) tuples, row-major top-to-bottom.
    """
    row_size = ((width * 3 + 3) // 4) * 4  # padded to 4-byte boundary
    pixel_data_size = row_size * height
    file_size = 54 + pixel_data_size

    with open(path, 'wb') as f:
        # BMP file header (14 bytes)
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))   # file size
        f.write(struct.pack('<HH', 0, 0))       # reserved
        f.write(struct.pack('<I', 54))           # pixel data offset

        # DIB header — BITMAPINFOHEADER (40 bytes)
        f.write(struct.pack('<I', 40))           # header size
        f.write(struct.pack('<i', width))        # width
        f.write(struct.pack('<i', -height))      # negative = top-down
        f.write(struct.pack('<HH', 1, 24))       # planes, bpp
        f.write(struct.pack('<I', 0))            # compression (none)
        f.write(struct.pack('<I', pixel_data_size))
        f.write(struct.pack('<ii', 2835, 2835))  # pixels per metre (~72 dpi)
        f.write(struct.pack('<II', 0, 0))        # colours used/important

        # Pixel data — BMP stores BGR, top-down (we used negative height)
        row_pad = row_size - width * 3
        for row in pixels:
            for (r, g, b) in row:
                f.write(bytes([b, g, r]))
            f.write(b'\x00' * row_pad)

    print(f"  wrote {path} ({width}x{height}, {os.path.getsize(path)} bytes)")


def make_pixels(width, height, fn):
    """Build a pixel grid by calling fn(x, y) -> (r, g, b) for each pixel."""
    return [[fn(x, y) for x in range(width)] for y in range(height)]


# ─── Colour helpers ───────────────────────────────────────────────────────────

def lerp(a, b, t):
    return int(a + (b - a) * t)

def lerp_colour(c1, c2, t):
    return (lerp(c1[0], c2[0], t),
            lerp(c1[1], c2[1], t),
            lerp(c1[2], c2[2], t))

def clamp(v, lo=0, hi=255):
    return max(lo, min(hi, int(v)))

# ─── Background generators ────────────────────────────────────────────────────

def bg_gradient(width, height):
    """Top-to-bottom gradient, dark navy to dark teal."""
    top    = (10, 15, 40)
    bottom = (10, 40, 50)
    def pixel(x, y):
        t = y / (height - 1)
        return lerp_colour(top, bottom, t)
    return make_pixels(width, height, pixel)


def bg_gradient_diagonal(width, height):
    """Diagonal gradient, dark purple to dark blue."""
    c1 = (20, 10, 40)
    c2 = (10, 20, 60)
    def pixel(x, y):
        t = (x / (width - 1) + y / (height - 1)) / 2
        return lerp_colour(c1, c2, t)
    return make_pixels(width, height, pixel)


def bg_solid(width, height, colour=(15, 15, 25)):
    """Solid colour background."""
    def pixel(x, y):
        return colour
    return make_pixels(width, height, pixel)


def bg_grid(width, height):
    """Dark background with a subtle dot grid."""
    bg   = (12, 12, 22)
    dot  = (25, 25, 45)
    spacing = 32
    def pixel(x, y):
        if x % spacing == 0 and y % spacing == 0:
            return dot
        return bg
    return make_pixels(width, height, pixel)


def bg_scanlines(width, height):
    """Dark gradient with horizontal scanlines for a retro CRT feel."""
    top    = (8, 12, 30)
    bottom = (5, 8, 20)
    line   = (0, 0, 0)
    def pixel(x, y):
        if y % 4 == 0:
            return line
        t = y / (height - 1)
        return lerp_colour(top, bottom, t)
    return make_pixels(width, height, pixel)


def bg_waves(width, height):
    """Dark background with subtle sine wave bands."""
    base = (10, 12, 30)
    highlight = (15, 20, 50)
    def pixel(x, y):
        wave = math.sin((x + y * 0.5) * 0.05) * 0.5 + 0.5
        return lerp_colour(base, highlight, wave * 0.4)
    return make_pixels(width, height, pixel)


def bg_stars(width, height):
    """Deep space — dark background with pseudo-random stars."""
    import random
    rng = random.Random(42)  # fixed seed for reproducibility

    # Pre-generate star positions
    star_set = set()
    num_stars = (width * height) // 500
    for _ in range(num_stars):
        sx = rng.randint(0, width - 1)
        sy = rng.randint(0, height - 1)
        star_set.add((sx, sy))

    bg_col = (5, 5, 15)
    def pixel(x, y):
        if (x, y) in star_set:
            brightness = rng.randint(150, 255)
            return (brightness, brightness, brightness)
        return bg_col

    # Rebuild with consistent rng by pre-building the grid
    pixels = []
    rng2 = random.Random(99)
    star_bright = {pos: rng2.randint(150, 255) for pos in star_set}
    for y in range(height):
        row = []
        for x in range(width):
            if (x, y) in star_bright:
                b = star_bright[(x, y)]
                row.append((b, b, b))
            else:
                row.append(bg_col)
        pixels.append(row)
    return pixels


# ─── Icon generators ──────────────────────────────────────────────────────────

def icon_terminal(size=32):
    """Terminal icon — dark square with a '>' prompt symbol."""
    bg     = (20, 20, 30)
    border = (80, 120, 200)
    fg     = (0, 255, 136)

    pixels = [[bg] * size for _ in range(size)]

    # Border
    for x in range(size):
        pixels[0][x] = border
        pixels[size-1][x] = border
    for y in range(size):
        pixels[y][0] = border
        pixels[y][size-1] = border

    # Draw '>' at roughly centre-left
    # Simple pixel art chevron
    chevron = [
        (10, 12), (11, 13), (12, 14), (13, 15),
        (12, 16), (11, 17), (10, 18),
    ]
    for (cx, cy) in chevron:
        if 0 <= cy < size and 0 <= cx < size:
            pixels[cy][cx] = fg

    # Draw '_' cursor after chevron
    for cx in range(15, 22):
        if cx < size:
            pixels[15][cx] = fg

    return pixels


def icon_folder(size=32):
    """Folder icon."""
    bg     = (20, 20, 30)
    body   = (60, 140, 200)
    tab    = (80, 160, 220)
    border = (40, 100, 160)

    pixels = [[bg] * size for _ in range(size)]

    # Folder tab (top-left)
    for y in range(8, 13):
        for x in range(4, 14):
            pixels[y][x] = tab

    # Folder body
    for y in range(12, 26):
        for x in range(4, 28):
            pixels[y][x] = body

    # Border
    for x in range(4, 28):
        pixels[12][x] = border
        pixels[25][x] = border
    for y in range(12, 26):
        pixels[y][4]  = border
        pixels[y][27] = border

    return pixels


def icon_settings(size=32):
    """Settings/gear icon — simplified circle with notches."""
    bg   = (20, 20, 30)
    gear = (180, 180, 200)
    cx_  = size // 2
    cy_  = size // 2
    r    = 10
    ri   = 6

    pixels = [[bg] * size for _ in range(size)]

    for y in range(size):
        for x in range(size):
            dx = x - cx_
            dy = y - cy_
            dist = math.sqrt(dx*dx + dy*dy)
            angle = math.atan2(dy, dx)

            # Gear teeth — 8 teeth
            teeth = abs(math.sin(angle * 4)) * 2
            if ri <= dist <= r + teeth:
                pixels[y][x] = gear

    return pixels


def icon_file_manager(size=32):
    """File manager icon — stack of document lines."""
    bg     = (20, 20, 30)
    paper  = (220, 220, 235)
    line   = (100, 100, 160)
    border = (80, 80, 140)

    pixels = [[bg] * size for _ in range(size)]

    # Paper body
    for y in range(5, 27):
        for x in range(7, 25):
            pixels[y][x] = paper

    # Folded corner
    for i in range(5):
        for j in range(i + 1):
            pixels[5 + i][25 - j] = bg
    for i in range(5):
        pixels[5 + i][24 - i] = border

    # Text lines
    for row_y in [12, 16, 20]:
        for x in range(10, 22):
            pixels[row_y][x] = line

    # Border
    for x in range(7, 25):
        pixels[5][x]  = border
        pixels[26][x] = border
    for y in range(5, 27):
        pixels[y][7]  = border
        pixels[y][24] = border

    return pixels


# ─── C array exporter ─────────────────────────────────────────────────────────

def export_c_array(bmp_path, var_name, out_path):
    """Convert a BMP file to a C uint8_t array for embedding in the kernel."""
    with open(bmp_path, 'rb') as f:
        data = f.read()

    with open(out_path, 'w') as f:
        f.write(f'// Auto-generated from {os.path.basename(bmp_path)}\n')
        f.write(f'// Include in your kernel and pass to bmp_draw()\n\n')
        f.write(f'#pragma once\n')
        f.write(f'#include <stdint.h>\n\n')
        f.write(f'static const uint8_t {var_name}[] = {{\n')
        for i, byte in enumerate(data):
            if i % 16 == 0:
                f.write('    ')
            f.write(f'0x{byte:02x},')
            if i % 16 == 15:
                f.write('\n')
            else:
                f.write(' ')
        if len(data) % 16 != 0:
            f.write('\n')
        f.write(f'}};\n')

    print(f"  exported C array -> {out_path} ({len(data)} bytes)")


# ─── Main ─────────────────────────────────────────────────────────────────────

BACKGROUNDS = {
    'gradient':          bg_gradient,
    'gradient_diagonal': bg_gradient_diagonal,
    'solid':             bg_solid,
    'grid':              bg_grid,
    'scanlines':         bg_scanlines,
    'waves':             bg_waves,
    'stars':             bg_stars,
}

ICONS = {
    'terminal':     icon_terminal,
    'folder':       icon_folder,
    'settings':     icon_settings,
    'file_manager': icon_file_manager,
}

def generate_backgrounds(style=None, width=1280, height=720, out_dir='assets'):
    os.makedirs(out_dir, exist_ok=True)
    targets = {style: BACKGROUNDS[style]} if style else BACKGROUNDS
    for name, fn in targets.items():
        pixels = fn(width, height)
        bmp_path = os.path.join(out_dir, f'bg_{name}.bmp')
        write_bmp(bmp_path, pixels, width, height)
        export_c_array(bmp_path, f'bg_{name}_bmp', os.path.join(out_dir, f'bg_{name}.h'))

def generate_icons(name=None, out_dir='assets'):
    os.makedirs(out_dir, exist_ok=True)
    targets = {name: ICONS[name]} if name else ICONS
    for iname, fn in targets.items():
        pixels = fn(32)
        bmp_path = os.path.join(out_dir, f'icon_{iname}.bmp')
        write_bmp(bmp_path, pixels, 32, 32)
        export_c_array(bmp_path, f'icon_{iname}_bmp', os.path.join(out_dir, f'icon_{iname}.h'))

def main():
    args = sys.argv[1:]

    if not args:
        print("Generating all backgrounds (1280x720) and icons (32x32)...")
        generate_backgrounds()
        generate_icons()
        print("\nDone. Files in assets/")
        print("  .bmp  — view in any image viewer")
        print("  .h    — include in kernel for bmp_draw()")
        return

    if args[0] == 'bg':
        style = args[1] if len(args) > 1 else None
        w = int(args[2]) if len(args) > 2 else 1280
        h = int(args[3]) if len(args) > 3 else 720
        if style and style not in BACKGROUNDS:
            print(f"Unknown background: {style}")
            print(f"Available: {', '.join(BACKGROUNDS)}")
            sys.exit(1)
        generate_backgrounds(style, w, h)

    elif args[0] == 'icon':
        name = args[1] if len(args) > 1 else None
        if name and name not in ICONS:
            print(f"Unknown icon: {name}")
            print(f"Available: {', '.join(ICONS)}")
            sys.exit(1)
        generate_icons(name)

    else:
        print(__doc__)
        sys.exit(1)

if __name__ == '__main__':
    main()