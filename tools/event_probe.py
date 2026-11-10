#!/usr/bin/env python3
"""event_probe.py PORT — buktikan ServerEvent melayani banyak koneksi.

6 klien mengirim header separuh (belum CRLFCRLF); klien ke-7 harus tetap
dapat respons < 1,5 dtk (bukti non-blocking). Lalu semua diselesaikan,
keep-alive dan POST diuji.
"""
import socket
import sys
import time

port = int(sys.argv[1]) if len(sys.argv) > 1 else 10080


def conn():
    return socket.create_connection(("127.0.0.1", port), timeout=5)


def readresp(s):
    buf = b""
    while b"\r\n\r\n" not in buf:
        d = s.recv(4096)
        if not d:
            raise AssertionError("koneksi ditutup")
        buf += d
    head, rest = buf.split(b"\r\n\r\n", 1)
    n = 0
    for line in head.split(b"\r\n")[1:]:
        if line.lower().startswith(b"content-length:"):
            n = int(line.split(b":")[1])
    while len(rest) < n:
        rest += s.recv(4096)
    return head.split(b"\r\n")[0], rest[:n]


partial = []
for _ in range(6):
    s = conn()
    s.sendall(b"GET /health HTTP/1.1\r\nHost: x\r\n")   # sengaja belum selesai
    partial.append(s)

t0 = time.time()
s = conn()
s.sendall(b"GET /health HTTP/1.1\r\nHost: x\r\n\r\n")
st, body = readresp(s)
dt = time.time() - t0
assert b" 200 " in st and body == b"ok\n", (st, body)
assert dt < 1.5, "request baru terblokir %.2fs" % dt
s.close()
print("OK nonblocking %.2fs" % dt)

for s in partial:
    s.sendall(b"\r\n")
    st, body = readresp(s)
    assert b" 200 " in st and body == b"ok\n"
    s.close()
print("OK partial", len(partial))

c = conn()
c.sendall(b"GET /health HTTP/1.1\r\nHost: x\r\n\r\n")
st, b1 = readresp(c)
c.sendall(b"GET /json HTTP/1.1\r\nHost: x\r\n\r\n")
st2, b2 = readresp(c)
assert b" 200 " in st and b1 == b"ok\n" and b" 200 " in st2 and b'"ok"' in b2
c.close()
print("OK keep-alive")

p = conn()
p.sendall(b"POST /echo HTTP/1.1\r\nHost: x\r\nContent-Length: 7\r\n"
          b"Connection: close\r\n\r\nABCDEFG")
st, b = readresp(p)
assert b == b"ABCDEFG", b
p.close()
print("OK post")
