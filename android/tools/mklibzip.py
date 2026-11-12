#!/usr/bin/env python3
"""Bungkus pohon lib/*.kop (modul Kopi) menjadi satu zip aset APK."""
import os
import sys
import zipfile


def main():
    src = sys.argv[1]
    out = sys.argv[2]
    files = []
    for root, dirs, names in os.walk(src):
        dirs.sort()
        for n in sorted(names):
            if not n.endswith('.kop'):
                continue
            p = os.path.join(root, n)
            rel = os.path.relpath(p, src).replace(os.sep, '/')
            files.append((p, rel))
    with zipfile.ZipFile(out, 'w', zipfile.ZIP_DEFLATED) as z:
        for p, rel in files:
            z.write(p, rel)
    print('kopilib.zip: %d modul .kop' % len(files))


if __name__ == '__main__':
    main()
