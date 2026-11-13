#!/usr/bin/env python3
"""h2_oracle.py — server HTTP/2 (pustaka h2) di atas TLS 1.3 + ALPN,
oracle untuk klien Kopi Http2.kop.

Pemakaian: tools/h2_oracle.py <port> <cert.pem> <key.pem>
Sertifikat dibuat (openssl, EC prime256v1) bila belum ada.

Protokol uji:
  GET  /        → 200, body "halo-http2\n"
  POST /echo    → 200, body = echo body request
Respons memakai header default hpack pustaka (Huffman + tabel dinamis)
sehingga dekoder Kopi diuji jalur nyata.
"""
import os
import socket
import ssl
import subprocess
import sys

from h2.config import H2Configuration
from h2.connection import H2Connection
from h2.events import (
    DataReceived,
    RequestReceived,
    StreamEnded,
)


def make_cert(cert, key):
    subprocess.run([
        "openssl", "req", "-x509", "-newkey", "ec",
        "-pkeyopt", "ec_paramgen_curve:prime256v1", "-nodes",
        "-keyout", key, "-out", cert, "-days", "3650", "-subj", "/CN=h2oracle",
    ], check=True, capture_output=True)


def main():
    port = int(sys.argv[1])
    cert, key = sys.argv[2], sys.argv[3]
    if not (os.path.exists(cert) and os.path.exists(key)):
        make_cert(cert, key)
    ctx = ssl.SSLContext(ssl.PROTOCOL_TLS_SERVER)
    ctx.load_cert_chain(cert, key)
    ctx.set_alpn_protocols(["h2"])
    s = socket.socket()
    s.setsockopt(socket.SOL_SOCKET, socket.SO_REUSEADDR, 1)
    s.bind(("127.0.0.1", port))
    s.listen(4)
    s.settimeout(60)
    while True:
        c, _ = s.accept()
        try:
            with ctx.wrap_socket(c, server_side=True) as w:
                if w.selected_alpn_protocol() != "h2":
                    print("ALPN bukan h2:", w.selected_alpn_protocol(), flush=True)
                    continue
                cfg = H2Configuration(client_side=False, header_encoding="utf-8")
                conn = H2Connection(config=cfg)
                conn.initiate_connection()
                w.sendall(bytes(conn.data_to_send()))
                meta = {}
                datas = {}
                while True:
                    data = w.recv(16384)
                    if not data:
                        break
                    for ev in conn.receive_data(data):
                        if isinstance(ev, RequestReceived):
                            meta[ev.stream_id] = list(ev.headers)
                        elif isinstance(ev, DataReceived):
                            datas.setdefault(ev.stream_id, bytearray()).extend(ev.data)
                        elif isinstance(ev, StreamEnded):
                            sid = ev.stream_id
                            path = None
                            for k, v in meta.get(sid, []):
                                if k in (b":path", ":path"):
                                    path = v.decode() if isinstance(v, bytes) else str(v)
                            body = bytes(datas.get(sid, bytearray()))
                            if path == "/echo":
                                resp_body = body
                            else:
                                resp_body = b"halo-http2\n"
                            conn.send_headers(sid, [
                                (b":status", b"200"),
                                (b"content-type", "text/plain".encode()),
                                (b"content-length", str(len(resp_body)).encode()),
                            ])
                            conn.send_data(sid, resp_body, end_stream=True)
                        elif isinstance(ev, DataReceived):
                            pass
                    w.sendall(bytes(conn.data_to_send()))
        except Exception as e:  # noqa: BLE001
            print("err:", type(e).__name__, e, flush=True)
        finally:
            try:
                c.close()
            except OSError:
                pass


if __name__ == "__main__":
    main()
