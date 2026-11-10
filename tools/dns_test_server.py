#!/usr/bin/env python3
"""dns_test_server.py PORT — DNS UDP minimal untuk uji Resolv.kop.
kopi.test → 10.1.2.3 (NOERROR); nama lain → NXDOMAIN."""
import socket
import struct
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 10053
s = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
s.bind(("127.0.0.1", port))

while True:
    data, addr = s.recvfrom(512)
    tid = data[0:2]
    # question = dari offset 12 sampai akhir
    q = data[12:]
    flags = b"\x81\x80"                     # QR=1 RD=1 RA=1
    qd = b"\x00\x01"
    # nama pertama (label) untuk menentukan jawaban
    l = q[0]
    name = q[1:1 + l].decode(errors="ignore").lower()
    if name == "kopi":
        ans = b"\xc0\x0c" + struct.pack(">HHIH", 1, 1, 60, 4) + bytes([10, 1, 2, 3])
        hdr = tid + flags + qd + b"\x00\x01\x00\x00\x00\x00"
        s.sendto(hdr + q + ans, addr)
    else:
        hdr = tid + b"\x81\x83" + qd + b"\x00\x00\x00\x00\x00\x00"
        s.sendto(hdr + q, addr)
