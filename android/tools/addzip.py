#!/usr/bin/env python3
"""Tambah berkas ke APK (zip) tanpa alat `zip`. Pemakaian:
   addzip.py out.apk src1 arcname1 [src2 arcname2 ...]"""
import sys, zipfile

def main():
    apk = sys.argv[1]
    rest = sys.argv[2:]
    with zipfile.ZipFile(apk, 'a', zipfile.ZIP_DEFLATED) as z:
        for i in range(0, len(rest), 2):
            z.write(rest[i], rest[i + 1])
            print('  +', rest[i + 1])

if __name__ == '__main__':
    main()
