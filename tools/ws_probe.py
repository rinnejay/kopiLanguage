#!/usr/bin/env python3
"""ws_probe.py — uji server WebSocket Kopi tanpa pustaka websocket.

Memeriksa: handshake 101 + Sec-WebSocket-Accept (RFC 6455 contoh),
gema teks, ping→pong, frame 200 byte, dan close.
"""
import base64
import hashlib
import socket
import struct
import sys

port = int(sys.argv[1]) if len(sys.argv) > 1 else 8090

GUID = "258EAFA5-E914-47DA-95CA-C5AB0DC85B11"
KEY = "dGhlIHNhbXBsZSBub25jZQ=="
EXPECT = base64.b64encode(hashlib.sha1((KEY + GUID).encode()).digest()).decode()

MASK = b"\x01\x02\x03\x04"


def frame(op, data):
    n = len(data)
    h = bytes([0x80 | op])
    if n < 126:
        h += bytes([0x80 | n])
    else:
        h += bytes([0x80 | 126]) + struct.pack(">H", n)
    h += MASK
    return h + bytes(b ^ MASK[i % 4] for i, b in enumerate(data))


def read_frame(s):
    h = s.recv(2)
    assert len(h) == 2, "frame terpotong"
    op = h[0] & 15
    n = h[1] & 127
    if n == 126:
        n = struct.unpack(">H", s.recv(2))[0]
    elif n == 127:
        n = struct.unpack(">Q", s.recv(8))[0]
    data = b""
    while len(data) < n:
        chunk = s.recv(n - len(data))
        if not chunk:
            break
        data += chunk
    return op, data


s = socket.create_connection(("127.0.0.1", port), timeout=5)
req = (
    "GET /chat HTTP/1.1\r\nHost: x\r\nUpgrade: websocket\r\n"
    "Connection: Upgrade\r\nSec-WebSocket-Key: %s\r\n"
    "Sec-WebSocket-Version: 13\r\n\r\n" % KEY
).encode()
s.sendall(req)

buf = b""
while b"\r\n\r\n" not in buf:
    buf += s.recv(1024)
head = buf.split(b"\r\n\r\n")[0]
lines = head.split(b"\r\n")
assert lines[0] == b"HTTP/1.1 101 Switching Protocols", lines[0]
acc = [l for l in lines if l.lower().startswith(b"sec-websocket-accept")][0]
assert acc.split(b": ")[1].decode() == EXPECT, acc
print("OK handshake")

s.sendall(frame(1, b"halo kopi"))
op, data = read_frame(s)
assert op == 1 and data == b"halo kopi", (op, data)
print("OK echo")

s.sendall(frame(9, b"p"))
op, data = read_frame(s)
assert op == 10 and data == b"p", (op, data)
print("OK ping-pong")

s.sendall(frame(1, b"A" * 200))
op, data = read_frame(s)
assert op == 1 and data == b"A" * 200, (op, len(data))
print("OK frame-200")

s.sendall(frame(8, bytes([3, 232])))
op, data = read_frame(s)
assert op == 8, (op, data)
print("OK close")
s.close()
