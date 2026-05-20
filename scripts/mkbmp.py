#!/usr/bin/env python3
"""
TermuOS BMP Generator
Creates 24-bit uncompressed BMP images for desktop backgrounds and icons.

Usage:
    python3 mkbmp.py                    # generate all presets into assets/
    python3 mkbmp.py bg gradient        # one background style
    python3 mkbmp.py bg gradient 1920 1080  # custom resolution
    python3 mkbmp.py icon terminal      # one icon

Output:
    assets/bg_*.bmp       -- background images (load from VFS at runtime)
    assets/icon_*.bmp     -- icon images
    assets/icon_*.h       -- C arrays for embedding icons in kernel
    assets/icons.h        -- master header that includes all icon arrays
"""

import struct
import sys
import os
import math

# ─── BMP writer ───────────────────────────────────────────────────────────────

def write_bmp(path, pixels, width, height):
    """Write a 24-bit uncompressed top-down BMP."""
    row_size = ((width * 3 + 3) // 4) * 4
    pixel_data_size = row_size * height
    file_size = 54 + pixel_data_size

    with open(path, 'wb') as f:
        f.write(b'BM')
        f.write(struct.pack('<I', file_size))
        f.write(struct.pack('<HH', 0, 0))
        f.write(struct.pack('<I', 54))
        f.write(struct.pack('<I', 40))
        f.write(struct.pack('<i', width))
        f.write(struct.pack('<i', -height))   # negative = top-down
        f.write(struct.pack('<HH', 1, 24))
        f.write(struct.pack('<I', 0))
        f.write(struct.pack('<I', pixel_data_size))
        f.write(struct.pack('<ii', 2835, 2835))
        f.write(struct.pack('<II', 0, 0))

        row_pad = row_size - width * 3
        for row in pixels:
            for (r, g, b) in row:
                f.write(bytes([b, g, r]))
            f.write(b'\x00' * row_pad)

    print(f"  wrote {path} ({width}x{height}, {os.path.getsize(path):,} bytes)")


def make_pixels(width, height, fn):
    return [[fn(x, y) for x in range(width)] for y in range(height)]


# ─── Colour helpers ───────────────────────────────────────────────────────────

def lerp(a, b, t):
    return int(a + (b - a) * t)

def lerp_colour(c1, c2, t):
    return (lerp(c1[0], c2[0], t),
            lerp(c1[1], c2[1], t),
            lerp(c1[2], c2[2], t))

# ─── Background generators ────────────────────────────────────────────────────

def bg_gradient(width, height):
    top, bottom = (10, 15, 40), (10, 40, 50)
    return make_pixels(width, height, lambda x, y: lerp_colour(top, bottom, y / (height - 1)))

def bg_gradient_diagonal(width, height):
    c1, c2 = (20, 10, 40), (10, 20, 60)
    return make_pixels(width, height,
        lambda x, y: lerp_colour(c1, c2, (x / (width-1) + y / (height-1)) / 2))

def bg_solid(width, height, colour=(15, 15, 25)):
    return make_pixels(width, height, lambda x, y: colour)

def bg_grid(width, height):
    bg, dot = (12, 12, 22), (25, 25, 45)
    return make_pixels(width, height,
        lambda x, y: dot if x % 32 == 0 and y % 32 == 0 else bg)

def bg_scanlines(width, height):
    top, bottom, line = (8, 12, 30), (5, 8, 20), (0, 0, 0)
    def pixel(x, y):
        if y % 4 == 0: return line
        return lerp_colour(top, bottom, y / (height - 1))
    return make_pixels(width, height, pixel)

def bg_waves(width, height):
    base, hi = (10, 12, 30), (15, 20, 50)
    return make_pixels(width, height,
        lambda x, y: lerp_colour(base, hi, (math.sin((x + y * 0.5) * 0.05) * 0.5 + 0.5) * 0.4))

def bg_stars(width, height):
    import random
    rng = random.Random(42)
    star_set = {(rng.randint(0, width-1), rng.randint(0, height-1))
                for _ in range((width * height) // 500)}
    rng2 = random.Random(99)
    star_bright = {pos: rng2.randint(150, 255) for pos in star_set}
    bg_col = (5, 5, 15)
    pixels = []
    for y in range(height):
        row = []
        for x in range(width):
            b = star_bright.get((x, y))
            row.append((b, b, b) if b else bg_col)
        pixels.append(row)
    return pixels

# ─── Icon generators ──────────────────────────────────────────────────────────

def icon_logo(size=32):
    """TermuOS logo -- T shape in a dark square with blue glow."""
    import math
    bg     = (10,  12,  25)
    glow   = (20,  30,  70)
    letter = (80,  140, 255)
    bright = (140, 190, 255)
    pixels = [[bg] * size for _ in range(size)]
    cx, cy = size // 2, size // 2
    for y in range(size):
        for x in range(size):
            dist = math.sqrt((x-cx)**2 + (y-cy)**2)
            t = max(0.0, 1.0 - dist / (size * 0.7))
            pixels[y][x] = (
                int(bg[0] + (glow[0]-bg[0])*t),
                int(bg[1] + (glow[1]-bg[1])*t),
                int(bg[2] + (glow[2]-bg[2])*t),
            )
    bar_y, bar_h = 7, 4
    bar_x, bar_w = 6, size - 12
    stem_x, stem_w = cx - 2, 4
    stem_y = bar_y + bar_h
    stem_h = size - bar_y - bar_h - 7
    for y in range(bar_y, bar_y + bar_h):
        for x in range(bar_x, bar_x + bar_w):
            t = 1.0 - abs(x - cx) / max(1, bar_w / 2)
            pixels[y][x] = (
                int(letter[0] + (bright[0]-letter[0])*t),
                int(letter[1] + (bright[1]-letter[1])*t),
                int(letter[2] + (bright[2]-letter[2])*t),
            )
    for y in range(stem_y, stem_y + stem_h):
        for x in range(stem_x, stem_x + stem_w):
            t = 1.0 - abs(y - (stem_y + stem_h//2)) / max(1, stem_h/2)
            pixels[y][x] = (
                int(letter[0] + (bright[0]-letter[0])*t*0.6),
                int(letter[1] + (bright[1]-letter[1])*t*0.6),
                int(letter[2] + (bright[2]-letter[2])*t*0.6),
            )
    return pixels


def icon_terminal(size=32):
    bg, border, fg = (20, 20, 30), (80, 120, 200), (0, 255, 136)
    pixels = [[bg] * size for _ in range(size)]
    for x in range(size):
        pixels[0][x] = border
        pixels[size-1][x] = border
    for y in range(size):
        pixels[y][0] = border
        pixels[y][size-1] = border
    for (cx, cy) in [(10,12),(11,13),(12,14),(13,15),(12,16),(11,17),(10,18)]:
        pixels[cy][cx] = fg
    for cx in range(15, 22):
        pixels[15][cx] = fg
    return pixels

def icon_folder(size=32):
    bg, body, tab, border = (20,20,30), (60,140,200), (80,160,220), (40,100,160)
    pixels = [[bg]*size for _ in range(size)]
    for y in range(8, 13):
        for x in range(4, 14): pixels[y][x] = tab
    for y in range(12, 26):
        for x in range(4, 28): pixels[y][x] = body
    for x in range(4, 28):
        pixels[12][x] = border; pixels[25][x] = border
    for y in range(12, 26):
        pixels[y][4] = border; pixels[y][27] = border
    return pixels

def icon_settings(size=32):
    bg, gear = (20,20,30), (180,180,200)
    cx_, cy_, r, ri = size//2, size//2, 10, 6
    pixels = [[bg]*size for _ in range(size)]
    for y in range(size):
        for x in range(size):
            dx, dy = x - cx_, y - cy_
            dist = math.sqrt(dx*dx + dy*dy)
            angle = math.atan2(dy, dx)
            teeth = abs(math.sin(angle * 4)) * 2
            if ri <= dist <= r + teeth:
                pixels[y][x] = gear
    return pixels

def icon_file_manager(size=32):
    bg, paper, line, border = (20,20,30), (220,220,235), (100,100,160), (80,80,140)
    pixels = [[bg]*size for _ in range(size)]
    for y in range(5, 27):
        for x in range(7, 25): pixels[y][x] = paper
    for i in range(5):
        for j in range(i+1): pixels[5+i][25-j] = bg
        pixels[5+i][24-i] = border
    for row_y in [12, 16, 20]:
        for x in range(10, 22): pixels[row_y][x] = line
    for x in range(7, 25):
        pixels[5][x] = border; pixels[26][x] = border
    for y in range(5, 27):
        pixels[y][7] = border; pixels[y][24] = border
    return pixels

# ─── C array exporter ─────────────────────────────────────────────────────────

def export_c_array(bmp_path, var_name, out_path):
    with open(bmp_path, 'rb') as f:
        data = f.read()
    with open(out_path, 'w') as f:
        f.write(f'// Auto-generated from {os.path.basename(bmp_path)}\n')
        f.write(f'// Pass to: bmp_draw(x, y, {var_name}, 32, 32)\n\n')
        f.write(f'#pragma once\n#include <stdint.h>\n\n')
        f.write(f'static const uint8_t {var_name}[] = {{\n')
        for i, byte in enumerate(data):
            if i % 16 == 0: f.write('    ')
            f.write(f'0x{byte:02x},')
            f.write('\n' if i % 16 == 15 else ' ')
        if len(data) % 16 != 0: f.write('\n')
        f.write(f'}};\n')
    print(f"  exported C header -> {out_path}")

def export_master_icons_header(icon_names, out_dir):
    """Generate icons.h that includes all icon headers."""
    path = os.path.join(out_dir, 'icons.h')
    with open(path, 'w') as f:
        f.write('// Master icon header — include this in your kernel\n')
        f.write('// All 32x32 icon BMP arrays\n\n')
        f.write('#pragma once\n\n')
        for name in icon_names:
            f.write(f'#include "icon_{name}.h"\n')
    print(f"  exported master header -> {path}")

# ─── Generators ───────────────────────────────────────────────────────────────

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
    'logo':         icon_logo,
    'terminal':     icon_terminal,
    'folder':       icon_folder,
    'settings':     icon_settings,
    'file_manager': icon_file_manager,
}

def generate_backgrounds(style=None, width=1280, height=720, out_dir='assets'):
    os.makedirs(out_dir, exist_ok=True)
    targets = {style: BACKGROUNDS[style]} if style else BACKGROUNDS
    print(f"Generating backgrounds ({width}x{height})...")
    for name, fn in targets.items():
        pixels = fn(width, height)
        bmp_path = os.path.join(out_dir, f'bg_{name}.bmp')
        write_bmp(bmp_path, pixels, width, height)
        # No .h for backgrounds — too large, load from VFS instead

def generate_icons(name=None, out_dir='assets'):
    os.makedirs(out_dir, exist_ok=True)
    targets = {name: ICONS[name]} if name else ICONS
    print("Generating icons (32x32)...")
    generated = []
    for iname, fn in targets.items():
        pixels = fn(32)
        bmp_path = os.path.join(out_dir, f'icon_{iname}.bmp')
        write_bmp(bmp_path, pixels, 32, 32)
        export_c_array(bmp_path, f'icon_{iname}_bmp',
                       os.path.join(out_dir, f'icon_{iname}.h'))
        generated.append(iname)
    if not name:  # only write master header when generating all
        export_master_icons_header(generated, out_dir)

def main():
    args = sys.argv[1:]

    if not args:
        generate_backgrounds()
        generate_icons()
        print("\nDone. Files in assets/")
        print("  bg_*.bmp     — copy into your OS disk image, load via VFS")
        print("  icon_*.h     — include in kernel, pass array to bmp_draw()")
        print("  icons.h      — include this one file to get all icons")
        return

    if args[0] == 'bg':
        style = args[1] if len(args) > 1 else None
        w = int(args[2]) if len(args) > 2 else 1280
        h = int(args[3]) if len(args) > 3 else 720
        if style and style not in BACKGROUNDS:
            print(f"Unknown: {style}. Available: {', '.join(BACKGROUNDS)}")
            sys.exit(1)
        generate_backgrounds(style, w, h)

    elif args[0] == 'icon':
        name = args[1] if len(args) > 1 else None
        if name and name not in ICONS:
            print(f"Unknown: {name}. Available: {', '.join(ICONS)}")
            sys.exit(1)
        generate_icons(name)

    else:
        print(__doc__)
        sys.exit(1)

if __name__ == '__main__':
    main()