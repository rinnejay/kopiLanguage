#!/usr/bin/env python3
"""Buat ikon aplikasi Kopi IDE (cangkir kopi sederhana) untuk semua kerapatan."""
import os, struct, zlib, math, sys

def png(w, h, pixels):
    raw = b''.join(b'\x00' + bytes(pixels[y * w * 4:(y + 1) * w * 4]) for y in range(h))
    def chunk(t, d):
        c = struct.pack('>I', len(d)) + t + d
        return c + struct.pack('>I', zlib.crc32(t + d) & 0xffffffff)
    return (b'\x89PNG\r\n\x1a\n'
            + chunk(b'IHDR', struct.pack('>IIBBBBB', w, h, 8, 6, 0, 0, 0))
            + chunk(b'IDAT', zlib.compress(raw, 9))
            + chunk(b'IEND', b''))

def icon(size):
    px = bytearray(size * size * 4)
    s = size
    def put(x, y, r, g, b, a=255):
        if 0 <= x < s and 0 <= y < s:
            i = (y * s + x) * 4
            px[i:i+4] = bytes((r, g, b, a))
    # latar bulat (kopi gelap)
    cx = cy = (s - 1) / 2.0
    rad = s * 0.48
    for y in range(s):
        for x in range(s):
            d = math.hypot(x - cx, y - cy)
            if d <= rad:
                # gradasi vertikal
                t = y / max(1, s - 1)
                put(x, y, int(38 + 26 * t), int(30 + 20 * t), int(24 + 14 * t))
            elif d <= rad + 1:
                put(x, y, 224, 164, 88, 90)
    # cangkir: badan trapesium
    top = s * 0.46
    bot = s * 0.74
    lx = s * 0.26
    rx = s * 0.66
    for y in range(int(top), int(bot)):
        t = (y - top) / (bot - top)
        x0 = lx + t * s * 0.04
        x1 = rx - t * s * 0.04
        for x in range(int(x0), int(x1)):
            put(x, y, 224, 164, 88)
    # kopi di dalam cangkir
    for y in range(int(top), int(top + s * 0.07)):
        for x in range(int(lx + s * 0.03), int(rx - s * 0.03)):
            put(x, y, 90, 58, 34)
    # gagang
    hx = s * 0.68
    for a in range(0, 360, 2):
        rr = s * 0.09
        x = hx + math.cos(math.radians(a)) * rr * 0.9
        y = s * 0.58 + math.sin(math.radians(a)) * rr
        if x > hx - 2:
            for dx in range(0, max(2, int(s * 0.028))):
                put(int(x) + dx, int(y), 224, 164, 88)
    # alas
    for y in range(int(bot), int(bot + s * 0.035)):
        for x in range(int(lx - s * 0.03), int(rx + s * 0.03)):
            put(x, y, 224, 164, 88)
    # uap
    for k in range(2):
        bx = s * (0.40 + k * 0.16)
        for y in range(int(s * 0.20), int(s * 0.42)):
            t = (y / s - 0.20) / 0.22
            x = bx + math.sin(t * math.pi * 2.2) * s * 0.024
            for dx in range(0, max(2, int(s * 0.022))):
                put(int(x) + dx, y, 240, 235, 228, 210)
    return png(s, s, px)

def main():
    out = sys.argv[1] if len(sys.argv) > 1 else 'res'
    for d, sz in [('mipmap-mdpi', 48), ('mipmap-hdpi', 72), ('mipmap-xhdpi', 96),
                  ('mipmap-xxhdpi', 144), ('mipmap-xxxhdpi', 192)]:
        p = os.path.join(out, d)
        os.makedirs(p, exist_ok=True)
        with open(os.path.join(p, 'ic_launcher.png'), 'wb') as f:
            f.write(icon(sz))
    print('ikon dibuat di', out)

if __name__ == '__main__':
    main()
