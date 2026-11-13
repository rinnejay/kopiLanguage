#!/usr/bin/env python3
"""tls_srv_probe.py — klien TLS python (OpenSSL) untuk uji TlsServer.kop.

Menyambung ke 127.0.0.1:<port>, mengirim "ping", mengharap "pong".
Keluar 0 bila round-trip sah; mencetak versi TLS + hasil.
"""
import socket
import ssl
import sys

port = int(sys.argv[1])
ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_CLIENT)
ctx.check_hostname = False
ctx.verify_mode = ssl.CERT_NONE
with socket.create_connection(("127.0.0.1", port), timeout=30) as sock:
    with ctx.wrap_socket(sock, server_hostname="kopi") as s:
        ver = s.version()
        s.sendall(b"ping")
        data = b""
        while len(data) < 4:
            chunk = s.recv(4 - len(data))
            if not chunk:
                break
            data += chunk
        ok = data == b"pong"
        print("py", ver, ok)
        sys.exit(0 if ok else 1)
