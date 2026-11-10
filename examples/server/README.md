# ServerDemo — HTTP server minimal di Kopi

Server HTTP/1.1 **single-threaded, blocking** yang ditulis sepenuhnya di
`.kop` (library `HttpServer`, `Static`, `Log`), di atas builtin socket baru.

## Jalankan

```sh
cd examples/server
../../kopicc ServerDemo.kop -t a64 -o demo.s
cc demo.s -o demo.bin
./demo.bin 8080          # argumen = port (via Net.argv)
# buka http://127.0.0.1:8080/
```

Atau lewat VM:

```sh
../../kopicc ServerDemo.kop -t vm -o demo.kbc
../../kopivm demo.kbc 8080
```

## Endpoint contoh

| Path | Keterangan |
|---|---|
| `/` | HTML kecil |
| `/health` | `ok` |
| `/json` | JSON |
| `/time` | waktu UTC |
| `/echo` | POST; mengembalikan body |
| `/sum?a=2&b=3` | query string |
| `/header?name=X-Test` | membaca header request |
| `/static/<file>` | file statis (menolak `..`) |

## Pola loop (penting: reset arena per request)

```java
int fd = Net.socket(2, 1);
Net.setsockopt(fd, 1, 2, 1);        // SO_REUSEADDR
Net.bind(fd, 0, port);
Net.listen(fd, 16);
while (true) {
    int c = Net.accept(fd);          // accept4
    int mark = Net.memMark();
    int[] req = HttpServer.readRequest(c, 16384);
    int[] resp = HttpServer.text(200, "ok\n");
    HttpServer.send(c, resp);
    Net.close(c);
    Net.memReset(mark);              // wajib: arena tidak punya GC
}
```

Tanpa `memMark/memReset`, server akan OOM setelah beberapa ratus request
(arena 32 MB). Reset hanya membebaskan alokasi **setelah** mark; data yang
disimpan sebelum mark (konfigurasi, route) tetap hidup.

## Batas jujur

- **Single-threaded**: satu koneksi dilayani sampai selesai; belum ada
  thread/process/fork atau event loop.
- HTTP/1.1 dengan **keep-alive** per koneksi (v1.9.2), HEAD, cookie, dan
  body request chunked; belum multipart, WebSocket di server HTTP,
  atau HTTP/2.
- Tidak ada TLS server; jalankan di belakang reverse proxy bila perlu HTTPS.
- Routing dilakukan manual (`if/else`) di `main`; belum framework router.
- Buffer request dibatasi `readRequest(fd, max)`; body lebih besar dari max
  akan terpotong.
- `Net.recv1` (satu `read`) dipakai untuk request; `Net.recvb` tetap untuk
  file/stream yang ingin dibaca sampai penuh.
