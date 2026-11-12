# 04 — Jaringan & server

Dokumen ini menghubungkan builtin `Net.*` dengan pola server yang sudah
diuji di repo. Semua contoh ada versi lengkapnya di `examples/`.

---

## 1. Builtin `Net` yang tersedia

| Builtin | Guna |
|---|---|
| `Net.socket(domain,type)` | `2`=AF_INET, `1`=SOCK_STREAM, `2`=SOCK_DGRAM |
| `Net.bind(fd, ip, port)` | ikat lokal; `ip=0` = 0.0.0.0 |
| `Net.listen(fd, backlog)` / `Net.accept(fd)` | server TCP (accept memakai `accept4`) |
| `Net.connect(fd, ip, port)` | klien TCP |
| `Net.sendto(fd, ip, port, buf)` / `Net.recvfrom(fd, buf, from)` | UDP; `from[0]`=ip, `from[1]`=port |
| `Net.sendb` / `Net.recvb` | kirim / baca sampai buffer penuh |
| `Net.recv1(fd, buf)` | **satu** `read` (HTTP/WebSocket) |
| `Net.rdb` / `Net.wrb` | satu byte |
| `Net.shutdown(fd, how)` | 0=RD 1=WR 2=RDWR |
| `Net.setsockopt(fd, lvl, opt, val)` | SO_REUSEADDR=1/2, TCP_NODELAY=6/1 |
| `Net.settimeout(fd, ms)` | SO_RCVTIMEO + SO_SNDTIMEO |
| `Net.setnonblock(fd)` | O_NONBLOCK |
| `Net.pollfd(fds,revents,ms)` | poll(2); revents 1=siap baca, 2=error/hup |
| `Net.localport(fd)` | port lokal |
| `Net.memMark()` / `Net.memReset(m)` | arena per request |
| `Net.argc/argv/getenv` | konfigurasi program |
| `Net.time/mono/sleep/exit` | waktu & proses |

Alamat IPv4 disusun dengan `Tcp.ip(a,b,c,d)`; `Tcp.loopback()` =
`127.0.0.1`; `0` pada `bind` berarti semua antarmuka.

## 2. Server TCP minimal (blocking)

```java
import Tcp;
import Log;

class Main {
    public static void main(String[] args) {
        int lfd = Net.socket(2, 1);
        Net.setsockopt(lfd, 1, 2, 1);          // SO_REUSEADDR
        Net.bind(lfd, 0, 9000);
        Net.listen(lfd, 16);

        while (true) {
            int c = Net.accept(lfd);
            if (c < 0) { Net.sleep(5); continue; }
            int mark = Net.memMark();
            int[] buf = new int[4096];
            int n = Net.recv1(c, buf);
            if (n > 0) Net.sendb(c, Net.bytes("HALO\n"));
            Net.close(c);
            Net.memReset(mark);
        }
    }
}
```

Pola penting: `mark` diambil **sebelum** alokasi per koneksi, lalu
`memReset(mark)` setelah selesai. Tanpa itu server akan OOM.

## 3. HTTP server blocking + keep-alive

`examples/server/ServerDemo.kop` memakai `Routes.kop`:

```java
while (true) {
    int c = Net.accept(fd);
    if (c < 0) { Net.sleep(5); continue; }
    while (true) {
        int mark = Net.memMark();
        int[] req = HttpServer.readRequest(c, 16384);
        if (req.length == 0) { Net.memReset(mark); break; }

        int keep = 0;
        if (HttpServer.keepAlive(req)) keep = 1;
        int[] resp = Routes.handle(req, keep);
        HttpServer.send(c, resp);
        Net.memReset(mark);
        if (keep == 0) break;
    }
    Net.close(c);
}
```

`Routes.handle` menangani `/`, `/health`, `/json`, `/time`, `/echo`,
`/sum`, `/header`, `/setcookie`, `/cookie`, `/static/*`.

### Helper respons

```java
HttpServer.responseFor(req, 200, ctype, body, keep);  // HEAD otomatis tanpa body
HttpServer.respond(code, ctype, body, extraHeader, keep);
HttpServer.headOnly(resp);
HttpServer.setCookie(name, value, path, maxAge);
HttpServer.cookie(req, name);
HttpServer.bodyAny(req);       // mendukung Transfer-Encoding: chunked
HttpServer.keepAlive(req);     // HTTP/1.1 default keep-alive
```

## 4. Event loop: banyak klien, satu proses

Untuk melayani banyak koneksi tanpa thread, pakai `Net.pollfd`.
`examples/server/ServerEvent.kop` adalah implementasi lengkap (32 koneksi,
buffer 8 KiB/koneksi, keep-alive, timeout 20 dtk).

Kerangka:

```java
int lfd = Net.socket(2, 1);
Net.setsockopt(lfd, 1, 2, 1);
Net.bind(lfd, 0, port);
Net.listen(lfd, 64);
Net.setnonblock(lfd);

int MAXC = 32, BZ = 8192;
int[] cfd = new int[MAXC];      // -1 = kosong
int[] clen = new int[MAXC];
int[] cbuf = new int[MAXC * BZ];
int[] pfds = new int[MAXC + 1];
int[] rev  = new int[MAXC + 1];
int[] slot = new int[MAXC + 1];

while (true) {
    pfds[0] = lfd; slot[0] = -1;
    int np = 1;
    for (int i = 0; i < MAXC; i++)
        if (cfd[i] >= 0) { pfds[np] = cfd[i]; slot[np] = i; np++; }

    int nready = Net.pollfd(pfds, rev, 500);   // -1 = tunggu terus
    if (nready <= 0) { /* timeout: bersihkan koneksi idle */ continue; }

    if ((rev[0] & 1) != 0) {
        while (true) {
            int c = Net.accept(lfd);
            if (c < 0) break;
            Net.setnonblock(c);
            // taruh di slot kosong
        }
    }
    for (int k = 1; k < np; k++) {
        if ((rev[k] & 3) == 0) continue;
        int s = slot[k];
        int n = Net.recv1(cfd[s], tmp);        // satu read
        // append ke cbuf[s*BZ ..], deteksi request lengkap, route,
        // kirim, memReset mark per request
    }
}
```

Kontrak `revents`: **selalu ditulis**, `0` = tidak siap, `1` = ada data,
`2` = error/hup (tetap panggil `recv1` untuk menguras sisa data).

Kunci kebenaran:

- Socket **non-blocking**; `recv1` boleh mengembalikan `-EAGAIN`.
- Satu koneksi = satu buffer; jangan berbagi buffer antar koneksi.
- Deteksi request lengkap = `\r\n\r\n` + `Content-Length` (lihat
  `complete()` di `ServerEvent.kop`).
- Simpan semua state yang harus hidup (fd, buffer, config) **sebelum**
  `memMark`.
- CPU-bound handler tetap memblokir seluruh loop; batasi pekerjaan berat.

Uji `tools/event_probe.py` membuktikan 6 koneksi yang mengirim header
separuh tidak memblokir koneksi ke-7.

## 5. WebSocket

`examples/ws/WsEcho.kop`:

```java
int[] req = HttpServer.readRequest(c, 8192);
if (WebSocket.isUpgrade(req)) {
    Net.sendb(c, WebSocket.handshake(req));
    int[] buf = new int[65536];
    while (true) {
        int n = Net.recv1(c, buf);
        if (n <= 0) break;
        int[] f = WebSocket.decode(buf, n);       // [op, len, payload...]
        if (f[0] == 8) { Net.sendb(c, WebSocket.close(1000)); break; }
        if (f[0] == 9) Net.sendb(c, WebSocket.pong(WebSocket.payload(f)));
        else if (f[0] == 1) Net.sendb(c, WebSocket.text(WebSocket.payload(f)));
    }
}
```

Handshake: `Sec-WebSocket-Accept = base64(SHA1(key + GUID))` — sudah
disediakan `WebSocket.handshake(req)`.

Batas: satu frame per `read` (tanpa fragmentasi/continuation), payload
< 64 KiB, tanpa TLS/kompresi. Klien harus mengirim frame **bermask**.

## 6. TLS klien

```java
import Tls;
import Rand;

int[] pin = ...;                                  // SHA-256 DER SPKI leaf (TOFU)
int[] ctx = Tls.handshake(addr, 443, pin, Rand.bytes(32));
if (ctx.length > 0) {
    Tls.writeApp(ctx, Net.bytes("GET / HTTP/1.1\r\nHost: example.com\r\n\r\n"));
    int[] data = Tls.readApp(ctx);
}
```

Yang **belum**: TLS server, verifikasi ECDSA CertificateVerify, validasi
rantai/PKI, hostname, masa berlaku, session resumption.

## 7. Auth, session, JWT di server

```java
String h = HttpServer.headerValue(req, "Authorization");
if (!Auth.checkBasic(h, user, pass)) {
    int[] r = HttpServer.respond(401, ctype,
                  "unauthorized\n", Auth.challenge("admin"), keep);
    HttpServer.send(c, r);
    continue;
}
```

Session in-memory:

```java
int[] ids  = MapStr.make(256, 8192);
int[] exps = MapStr.make(256, 8192);
// login:
String sid = Session.create(ids, exps, uid, 3600);
String extra = HttpServer.setCookie("sid", sid, "/", 3600);
// request berikutnya:
int uid = Session.get(ids, exps, HttpServer.cookie(req, "sid"), Net.time());
```

JWT:

```java
int[] out = new int[256];
if (Jwt.verify(token, secret, out) != 1) { /* 401 */ }
```

Simpan password sebagai `Pbkdf2.sha256(pass, salt, 100000, 32)` + salt
acak (`Rand.bytes(16)`), jangan plaintext.

## 8. Game realtime (UDP)

Pola `examples/game/`:

1. Server UDP non-blocking, tick 20/dtk.
2. Pesan biner kecil (`Wire.kop`), 1 datagram = 1 pesan.
3. Snapshot server-authoritative dikirim ke tiap pemain tiap tick.
4. Klien mengirim input 10×/dtk, menggambar snapshot terbaru.
5. `Net.memMark/memReset` per iterasi; dunia dialokasikan sebelum loop.

Batas: UDP tidak reliable/berurut; tanpa interpolasi; satu proses.

## 9. Deployment & operasional

- Jalankan di port > 1024 (tidak perlu root).
- **Single-thread**: taruh di belakang reverse proxy (nginx/caddy) untuk
  TLS, buffering, dan banyak koneksi; atau jalankan beberapa proses
  dengan `SO_REUSEADDR` + port berbeda di belakang LB.
- Belum ada graceful shutdown / signal handler. Pola aman: kirim SIGTERM
  dari supervisor; request yang sedang berjalan bisa terpotong.
- Log ke stderr (`Log`), biarkan supervisor yang mengumpulkan.
- Pantau RSS: tanpa GC, kebocoran logis = OOM. Uji beban: `200+` request
  beruntun, 6 koneksi lambat, dan tes kebocoran dengan `memMark`.
- Untuk file statis, `Static` membaca seluruh file ke memori; file besar
  sebaiknya dilayani reverse proxy.

## 10. Menguji server

- Python `http.client` untuk keep-alive/HEAD/cookie/chunked
  (`tools/event_probe.py`, srvtest di `test.sh`).
- `socket` mentah untuk WebSocket (`tools/ws_probe.py`).
- DNS lokal (`tools/dns_test_server.py`).
- Selalu uji **dua backend** (a64 + VM) karena bug codegen bisa berbeda.

Lanjut ke [05 — Developer](05-developer.md).
