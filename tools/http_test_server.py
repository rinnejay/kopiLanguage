#!/usr/bin/env python3
"""http_test_server.py — server HTTP/1.1 untuk uji klien Kopi.

Menyalakan di 127.0.0.1:<port>; endpoint:
  /chunked  → Transfer-Encoding: chunked
  /redirect → 302 ke /final
  /final    → "sampai"
  /whoami   → mengembalikan header Authorization
  /form     → "POST:" + body
  /json     → "JSON:" + body
"""
import http.server
import socketserver
import sys


class H(http.server.BaseHTTPRequestHandler):
    protocol_version = "HTTP/1.1"

    def log_message(self, *a):
        pass

    def _send(self, code, body, ctype="text/plain", extra=None):
        self.send_response(code)
        self.send_header("Content-Type", ctype)
        self.send_header("Content-Length", str(len(body)))
        for k, v in (extra or {}).items():
            self.send_header(k, v)
        self.end_headers()
        self.wfile.write(body)

    def do_GET(self):
        if self.path == "/chunked":
            self.send_response(200)
            self.send_header("Transfer-Encoding", "chunked")
            self.end_headers()
            for part in (b"halo ", b"dunia", b"!"):
                self.wfile.write(b"%x\r\n" % len(part) + part + b"\r\n")
            self.wfile.write(b"0\r\n\r\n")
        elif self.path == "/redirect":
            self._send(302, b"", extra={"Location": "/final"})
        elif self.path == "/final":
            self._send(200, b"sampai")
        elif self.path == "/whoami":
            self._send(200, self.headers.get("Authorization", "").encode())
        else:
            self._send(404, b"no")

    def do_POST(self):
        n = int(self.headers.get("Content-Length", "0"))
        body = self.rfile.read(n)
        if self.path == "/json":
            self._send(200, b"JSON:" + body)
        else:
            self._send(200, b"POST:" + body)


class S(socketserver.ThreadingTCPServer):
    allow_reuse_address = True
    daemon_threads = True


if __name__ == "__main__":
    port = int(sys.argv[1]) if len(sys.argv) > 1 else 10020
    S(("127.0.0.1", port), H).serve_forever()
