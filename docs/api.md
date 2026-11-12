# Referensi pustaka

Ringkasan semua modul di `lib/`. Setiap modul adalah class Kopi dengan method static; impor memakai nama class.

| Kelompok | Folder | Modul |
|---|---|---|
| **Core** | `lib/core/` | `Args`, `Arrays`, `Bytes`, `Compress`, `Config`, `Cookie`, `Deque`, `Digits`, `Env`, `File`, `Json`, `List`, `ListStr`, `Log`, `Map`, `MapSS`, `MapStr`, `Math`, `Matrix`, `Path`, `Router`, `Set`, `SetStr`, `Stats`, `Str`, `Test`, `Text`, `Time` |
| **Net** | `lib/net/` | `Auth`, `Dns`, `Http`, `HttpServer`, `Jwt`, `Mime`, `Multipart`, `Resolv`, `Rfc`, `Session`, `Static`, `Tcp`, `Tls`, `Udp`, `Url`, `WebSocket`, `WsClient` |
| **Crypto** | `lib/crypto/` | `Aead`, `Big`, `ChaCha`, `Cipher`, `Crypto`, `Der`, `Hash`, `Kdf`, `Pbkdf2`, `Poly`, `Rand`, `X25519` |

## Daftar modul

### Core — [buka referensi](api-core.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Args`](api-core.md#args) | `lib/core/Args.kop` | 10 | — |
| [`Arrays`](api-core.md#arrays) | `lib/core/Arrays.kop` | 23 | Operasi array `int[]`: agregat, potong/gabung, sort, pencarian, dan gaya fungsional (lambda). |
| [`Bytes`](api-core.md#bytes) | `lib/core/Bytes.kop` | 48 | Utilitas buffer byte `int[]`: I/O biner, jaringan, kriptografi. Untuk teks gunakan `Str`. |
| [`Compress`](api-core.md#compress) | `lib/core/Compress.kop` | 29 | — |
| [`Config`](api-core.md#config) | `lib/core/Config.kop` | 9 | — |
| [`Cookie`](api-core.md#cookie) | `lib/core/Cookie.kop` | 16 | — |
| [`Deque`](api-core.md#deque) | `lib/core/Deque.kop` | 11 | Antrean dua ujung (ring buffer) berkapasitas tetap: push/pop depan-belakang. |
| [`Digits`](api-core.md#digits) | `lib/core/Digits.kop` | 11 | Manipulasi digit bilangan: jumlah, jumlah digit, palindrom, Armstrong, digital root, dan cetak basis lain. |
| [`Env`](api-core.md#env) | `lib/core/Env.kop` | 11 | — |
| [`File`](api-core.md#file) | `lib/core/File.kop` | 15 | I/O file berbasis deskriptor: buka, baca/tulis, seek, ukuran, hapus, rename, mkdir. |
| [`Json`](api-core.md#json) | `lib/core/Json.kop` | 72 | Parser/penulis JSON murni. Nilai direpresentasikan sebagai tape `int[]`; literal bahasa memakai… |
| [`List`](api-core.md#list) | `lib/core/List.kop` | 25 | — |
| [`ListStr`](api-core.md#liststr) | `lib/core/ListStr.kop` | 24 | — |
| [`Log`](api-core.md#log) | `lib/core/Log.kop` | 18 | Logging ke stderr dengan timestamp UTC dan level. |
| [`Map`](api-core.md#map) | `lib/core/Map.kop` | 13 | Hash map int→int dengan open addressing dan `grow` eksplisit. |
| [`MapSS`](api-core.md#mapss) | `lib/core/MapSS.kop` | 19 | — |
| [`MapStr`](api-core.md#mapstr) | `lib/core/MapStr.kop` | 14 | Peta String→int dengan open addressing dan kolam kunci tetap. Dipakai untuk header HTTP, session, dan… |
| [`Math`](api-core.md#math) | `lib/core/Math.kop` | 72 | Matematika integer 32-bit: bit, pangkat/akar/log, FPB/modular, prima, kombinatorika, dan geometri. |
| [`Matrix`](api-core.md#matrix) | `lib/core/Matrix.kop` | 15 | Matriks persegi yang disimpan sebagai `int[]` datar (baris demi baris): aritmetika, transpose, trace,… |
| [`Path`](api-core.md#path) | `lib/core/Path.kop` | 6 | Utilitas path POSIX: gabung, ambil direktori/nama/ekstensi, uji absolut. |
| [`Router`](api-core.md#router) | `lib/core/Router.kop` | 14 | — |
| [`Set`](api-core.md#set) | `lib/core/Set.kop` | 8 | Himpunan int di atas Map: tambah/hapus/uji keanggotaan. |
| [`SetStr`](api-core.md#setstr) | `lib/core/SetStr.kop` | 11 | — |
| [`Stats`](api-core.md#stats) | `lib/core/Stats.kop` | 14 | Statistik deskriptif sederhana untuk array int: mean, median, modus, variansi, standar deviasi. |
| [`Str`](api-core.md#str) | `lib/core/Str.kop` | 109 | Utilitas teks native `String`: perbandingan, potong/gabung, cari/ganti, trim/case, split/join, padding,… |
| [`Test`](api-core.md#test) | `lib/core/Test.kop` | 16 | — |
| [`Text`](api-core.md#text) | `lib/core/Text.kop` | 19 | — |
| [`Time`](api-core.md#time) | `lib/core/Time.kop` | 32 | Waktu UTC: epoch detik, monotonic milidetik, sleep, format tanggal, dan nama hari. |

### Net — [buka referensi](api-net.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Auth`](api-net.md#auth) | `lib/net/Auth.kop` | 8 | Parser header Authorization: skema, Basic (user:pass), Bearer, tantangan 401. |
| [`Dns`](api-net.md#dns) | `lib/net/Dns.kop` | 17 | Klien DNS-over-TCP (RFC 1035/7766) ke server eksplisit; query A + parser jawaban. |
| [`Http`](api-net.md#http) | `lib/net/Http.kop` | 44 | Klien HTTP/1.1: GET/POST, header kustom, chunked, redirect, basic auth, timeout 5 detik. |
| [`HttpServer`](api-net.md#httpserver) | `lib/net/HttpServer.kop` | 47 | Parser request HTTP/1.1 dan pembangun respons: method/path/query/header/body, keep-alive, HEAD, cookie,… |
| [`Jwt`](api-net.md#jwt) | `lib/net/Jwt.kop` | 6 | JWT HS256: tanda tangan, verifikasi, ambil payload. |
| [`Mime`](api-net.md#mime) | `lib/net/Mime.kop` | 1 | Pemetaan ekstensi file → header Content-Type. |
| [`Multipart`](api-net.md#multipart) | `lib/net/Multipart.kop` | 17 | — |
| [`Resolv`](api-net.md#resolv) | `lib/net/Resolv.kop` | 5 | Resolver DNS UDP + pembaca `/etc/resolv.conf`, dengan timeout. |
| [`Rfc`](api-net.md#rfc) | `lib/net/Rfc.kop` | 5 | Klien protokol TCP klasik (daytime, echo, quote, whois, finger, discard) untuk latihan. |
| [`Session`](api-net.md#session) | `lib/net/Session.kop` | 6 | Sesi in-memory ber-expiry di atas dua MapStr (id→uid, id→expiry). |
| [`Static`](api-net.md#static) | `lib/net/Static.kop` | 7 | Penyajian file statis + MIME; menolak path traversal `..`. |
| [`Tcp`](api-net.md#tcp) | `lib/net/Tcp.kop` | 13 | Transport TCP di atas builtin `Net`: buka koneksi, kirim/terima buffer, shutdown, helper alamat. |
| [`Tls`](api-net.md#tls) | `lib/net/Tls.kop` | 25 | Klien TLS 1.3: X25519, ChaCha20-Poly1305, HKDF, pin SPKI TOFU, verifikasi Finished, data aplikasi dua arah. |
| [`Udp`](api-net.md#udp) | `lib/net/Udp.kop` | 7 | Datagram UDP: bind, kirim/terima satu datagram, shutdown. |
| [`Url`](api-net.md#url) | `lib/net/Url.kop` | 26 | URL/URI RFC 3986: percent-encoding, form encoding, parsing komponen, resolusi relatif, normalisasi path, IPv4. |
| [`WebSocket`](api-net.md#websocket) | `lib/net/WebSocket.kop` | 12 | Server WebSocket RFC 6455: handshake, frame teks/biner/ping/pong/close, decode frame klien bermask. |
| [`WsClient`](api-net.md#wsclient) | `lib/net/WsClient.kop` | 15 | — |

### Crypto — [buka referensi](api-crypto.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Aead`](api-crypto.md#aead) | `lib/crypto/Aead.kop` | 7 | AEAD ChaCha20-Poly1305 (RFC 7539 §2.8): enkripsi + autentikasi untuk TLS. |
| [`Big`](api-crypto.md#big) | `lib/crypto/Big.kop` | 20 | Aritmetika medan 2²⁵⁵−19 dengan 17 limb untuk X25519. |
| [`ChaCha`](api-crypto.md#chacha) | `lib/crypto/ChaCha.kop` | 5 | ChaCha20 stream cipher (RFC 7539) untuk TLS record dan eksperimen. |
| [`Cipher`](api-crypto.md#cipher) | `lib/crypto/Cipher.kop` | 11 | Base64, hex, dan cipher klasik (RC4, Vigenère, XOR) untuk eksperimen/kompatibilitas. |
| [`Crypto`](api-crypto.md#crypto) | `lib/crypto/Crypto.kop` | 18 | Kumpulan utilitas bit/hash/cipher lama yang dipakai modul kripto lain (kompatibilitas v1). |
| [`Der`](api-crypto.md#der) | `lib/crypto/Der.kop` | 4 | Parser ASN.1 DER minimal untuk mengekstrak SubjectPublicKeyInfo (pin TLS). |
| [`Hash`](api-crypto.md#hash) | `lib/crypto/Hash.kop` | 8 | SHA-256, SHA-1, dan HMAC-SHA256 murni Kopi untuk integritas, JWT, PBKDF2, dan TLS. |
| [`Kdf`](api-crypto.md#kdf) | `lib/crypto/Kdf.kop` | 8 | HKDF-SHA256 (RFC 5869) dan ExpandLabel TLS 1.3 (RFC 8446 §7.1). |
| [`Pbkdf2`](api-crypto.md#pbkdf2) | `lib/crypto/Pbkdf2.kop` | 1 | PBKDF2-HMAC-SHA256 untuk menyimpan kata sandi dengan iterasi tinggi. |
| [`Poly`](api-crypto.md#poly) | `lib/crypto/Poly.kop` | 10 | Poly1305 one-time MAC (RFC 7539) dengan aritmetika limb 15-bit. |
| [`Rand`](api-crypto.md#rand) | `lib/crypto/Rand.kop` | 1 | Byte acak kriptografis dari `/dev/urandom`. |
| [`X25519`](api-crypto.md#x25519) | `lib/crypto/X25519.kop` | 2 | Pertukaran kunci X25519 (RFC 7748) untuk TLS 1.3. |

## Cara menerapkan

| Kebutuhan | Modul yang dipakai | Pola singkat |
|---|---|---|
| CLI & pengolah teks | `Str`, `File`, `Path`, `Arrays` | baca file → `Str.trim`/`splitAt` → cetak |
| Matematika & statistik | `Math`, `Digits`, `Stats`, `Matrix` | fungsi murni, hasil int/long |
| Data terstruktur | `Json`, `Map`, `MapStr`, `Set`, `Deque` | `Json.parse` / map untuk lookup |
| Server HTTP | `Tcp`, `HttpServer`, `Static`, `Mime`, `Url` | accept → `readRequest` → route → response |
| Banyak klien | `Net.pollfd` + `HttpServer` | event loop non-blocking (`ServerEvent`) |
| WebSocket | `WebSocket`, `Hash`, `Cipher` | handshake → decode frame → balas |
| Klien HTTP | `Http`, `Url`, `Resolv` | `getFull`/`follow` → `payload` |
| Keamanan | `Pbkdf2`, `Jwt`, `Auth`, `Session`, `Rand` | hash password, token, cookie sesi |
| TLS | `Tls`, `X25519`, `Aead`, `Der` | handshake pin SPKI → `writeApp`/`readApp` |
| Game realtime | `Udp`, `Math`, `Map` | loop tick + snapshot (lihat `examples/game`) |

**Penting:** modul jaringan memakai socket blocking kecuali Anda memasang `Net.setnonblock`/`Net.settimeout`; lihat [04 — Server](04-server.html) untuk pola event loop dan batas.

## Builtin `Net.*`

Fungsi bawaan bahasa (tanpa impor) — pembungkus syscall native.

| Fungsi | Keterangan |
|---|---|
| `Net.socket(fd_type, proto)` | buat socket (TCP/UDP); deskriptor |
| `Net.connect(fd, ip, port)` | hubungkan ke alamat (oktet di int) |
| `Net.rdb(fd, buf)` | baca byte mentah ke buffer; jumlah terbaca |
| `Net.wrb(fd, buf)` | tulis byte mentah; jumlah tertulis |
| `Net.close(fd)` | tutup deskriptor |
| `Net.bytes("teks")` | literal string → array byte (saat kompilasi) |
| `Net.openr(path)` | buka file hanya-baca |
| `Net.recvb(fd, buf)` | baca dari koneksi (blocking/timeout) |
| `Net.sendb(fd, buf)` | kirim seluruh buffer ke koneksi |
| `Net.bind(fd, ip, port)` | ikat alamat/port |
| `Net.sendto(fd, ip, port, buf)` | kirim datagram UDP |
| `Net.recvfrom(fd, buf, from)` | terima datagram; from = int[2] (ip, port) |
| `Net.shutdown(fd, how)` | matikan satu/dua arah koneksi |
| `Net.listen(fd, backlog)` | jadikan socket server |
| `Net.accept4(fd)` | terima koneksi baru (syscall accept4, Android aman) |
| `Net.setsockopt(fd, level, opt, val)` | set opsi socket (SO_REUSEADDR, dll.) |
| `Net.memMark()` | tandai batas arena memori saat ini |
| `Net.memReset(mark)` | kembali ke tanda; byte > mark bebas dipakai lagi |
| `Net.argc()` | jumlah argumen program |
| `Net.argv(i)` | argumen ke-i sebagai byte |
| `Net.getenv(name)` | nilai variabel lingkungan (byte kosong bila tidak ada) |
| `Net.recv1(fd, buf)` | satu panggilan read saja (tanpa loop) |
| `Net.setnonblock(fd)` | socket non-blocking |
| `Net.key()` | byte tombol stdin (mode raw) |
| `Net.ttyRaw(on)` | aktif/matikan mode raw terminal |
| `Net.localport(fd)` | port lokal socket terikat |
| `Net.settimeout(fd, ms)` | timeout operasi socket dalam ms |
| `Net.pollfd(fds, revents, ms)` | poll(2); revents[i] 0/1 siap baca/2 error |
| `Net.time()` | epoch detik (32-bit) |
| `Net.mono()` | waktu monotonic |
| `Net.sleep(ms)` | tidur dalam milidetik |
| `Net.exit(code)` | keluar program (buffer stdio tidak di-flush) |
| `Net.cat(a, b)` | gabung dua array byte |
| `Net.sequal(a, b)` | true bila isi dua array sama |
| `Net.sne(a, b)` | true bila isi berbeda |

> `System.out.println/print` juga builtin; `Net.bytes("teks")`
> mengubah string literal menjadi `int[]` byte saat kompilasi.
