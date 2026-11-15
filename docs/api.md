# Referensi pustaka

Ringkasan semua modul di `lib/`. Setiap modul adalah class Kopi dengan method static dan/atau instance; impor memakai nama class.

| Kelompok | Folder | Modul |
|---|---|---|
| **Core** | `lib/core/` | `Arrays`, `BigInt`, `Bytes`, `Deque`, `Digits`, `List`, `ListStr`, `Map`, `MapSS`, `MapStr`, `Math`, `Matrix`, `Set`, `SetStr`, `Stats`, `Test` |
| **Text** | `lib/text/` | `Regex`, `Str`, `Text` |
| **Data** | `lib/data/` | `Cbor`, `Config`, `Cookie`, `Csv`, `Json`, `MessagePack` |
| **IO** | `lib/io/` | `File`, `Path`, `Reader`, `Writer` |
| **System** | `lib/system/` | `Args`, `Chan`, `Env`, `Log`, `Mutex`, `Time`, `Uuid` |
| **Compress** | `lib/compress/` | `Checksum`, `Compress`, `Filter`, `LzmaDec`, `Lzma2`, `LzmaAuto`, `LzmaStream`, `Xz` |
| **Crypto** | `lib/crypto/` | `Aead`, `Aes`, `Big`, `Blake2b`, `ChaCha`, `Cipher`, `Crypto`, `Der`, `Ecdsa`, `Gcm`, `Hash`, `Kdf`, `Pbkdf2`, `Pem`, `Poly`, `Rand`, `Rsa`, `Scrypt`, `X25519`, `X509` |
| **Net** | `lib/net/` | `Dns`, `Resolv`, `Rfc`, `Tcp`, `Tls`, `TlsServer`, `Udp` |
| **Web** | `lib/web/` | `Auth`, `Http`, `Http2`, `Http2Tabel`, `HttpServer`, `Jwt`, `Mime`, `Multipart`, `Router`, `Session`, `Static`, `Url`, `WebSocket`, `WsClient` |
| **Database** | `lib/db/` | `Sqlite` |

## Daftar modul

### Core — [buka referensi](api-core.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Arrays`](api-core.md#arrays) | `lib/core/Arrays.kop` | 23 | Operasi array `int[]`: agregat, potong/gabung, sort, pencarian, dan gaya fungsional (lambda). |
| [`BigInt`](api-core.md#bigint) | `lib/core/BigInt.kop` | 22 | Bilangan bulat presisi sembarang: tambah, kali, bagi, pangkat modulo, invers modulo, FPB, konversi… |
| [`Bytes`](api-core.md#bytes) | `lib/core/Bytes.kop` | 62 | Utilitas buffer byte `int[]`: I/O biner, jaringan, kriptografi. Untuk teks gunakan `Str`. |
| [`Deque`](api-core.md#deque) | `lib/core/Deque.kop` | 11 | Antrean dua ujung (ring buffer) berkapasitas tetap: push/pop depan-belakang. |
| [`Digits`](api-core.md#digits) | `lib/core/Digits.kop` | 11 | Manipulasi digit bilangan: jumlah, jumlah digit, palindrom, Armstrong, digital root, dan cetak basis lain. |
| [`List`](api-core.md#list) | `lib/core/List.kop` | 25 | — |
| [`ListStr`](api-core.md#liststr) | `lib/core/ListStr.kop` | 24 | — |
| [`Map`](api-core.md#map) | `lib/core/Map.kop` | 13 | Hash map int→int dengan open addressing dan `grow` eksplisit. |
| [`MapSS`](api-core.md#mapss) | `lib/core/MapSS.kop` | 19 | — |
| [`MapStr`](api-core.md#mapstr) | `lib/core/MapStr.kop` | 14 | Peta String→int dengan open addressing dan kolam kunci tetap. Dipakai untuk header HTTP, session, dan… |
| [`Math`](api-core.md#math) | `lib/core/Math.kop` | 72 | Matematika integer 32-bit: bit, pangkat/akar/log, FPB/modular, prima, kombinatorika, dan geometri. |
| [`Matrix`](api-core.md#matrix) | `lib/core/Matrix.kop` | 15 | Matriks persegi yang disimpan sebagai `int[]` datar (baris demi baris): aritmetika, transpose, trace,… |
| [`Set`](api-core.md#set) | `lib/core/Set.kop` | 8 | Himpunan int di atas Map: tambah/hapus/uji keanggotaan. |
| [`SetStr`](api-core.md#setstr) | `lib/core/SetStr.kop` | 11 | — |
| [`Stats`](api-core.md#stats) | `lib/core/Stats.kop` | 14 | Statistik deskriptif sederhana untuk array int: mean, median, modus, variansi, standar deviasi. |
| [`Test`](api-core.md#test) | `lib/core/Test.kop` | 16 | — |

### Text — [buka referensi](api-text.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Regex`](api-text.md#regex) | `lib/text/Regex.kop` | 6 | Mesin regex backtracking (mirip PCRE/Python): kelas, kuantifier, grup, alternasi, anchor. |
| [`Str`](api-text.md#str) | `lib/text/Str.kop` | 109 | Utilitas teks native `String`: perbandingan, potong/gabung, cari/ganti, trim/case, split/join, padding,… |
| [`Text`](api-text.md#text) | `lib/text/Text.kop` | 19 | — |

### Data — [buka referensi](api-data.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Cbor`](api-data.md#cbor) | `lib/data/Cbor.kop` | 11 | — |
| [`Config`](api-data.md#config) | `lib/data/Config.kop` | 9 | — |
| [`Cookie`](api-data.md#cookie) | `lib/data/Cookie.kop` | 16 | — |
| [`Csv`](api-data.md#csv) | `lib/data/Csv.kop` | 5 | Baca & tulis CSV (RFC 4180): field berkutip, pemisah koma, baris CRLF/LF. |
| [`Json`](api-data.md#json) | `lib/data/Json.kop` | 72 | Parser/penulis JSON murni. Nilai direpresentasikan sebagai tape `int[]`; literal bahasa memakai… |
| [`MessagePack`](api-data.md#messagepack) | `lib/data/MessagePack.kop` | 11 | — |

### IO — [buka referensi](api-io.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`File`](api-io.md#file) | `lib/io/File.kop` | 18 | I/O file berbasis deskriptor: buka, baca/tulis, seek, ukuran, hapus, rename, mkdir. |
| [`Path`](api-io.md#path) | `lib/io/Path.kop` | 6 | Utilitas path POSIX: gabung, ambil direktori/nama/ekstensi, uji absolut. |
| [`Reader`](api-io.md#reader) | `lib/io/Reader.kop` | 10 | Pembaca berkas berbuffer: baris demi baris, byte, atau seluruh isi tanpa memuat file ke memori. |
| [`Writer`](api-io.md#writer) | `lib/io/Writer.kop` | 9 | Penulis berkas berbuffer: teks, baris, atau byte; flush otomatis saat buffer penuh. |

### System — [buka referensi](api-system.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Args`](api-system.md#args) | `lib/system/Args.kop` | 10 | — |
| [`Chan`](api-system.md#chan) | `lib/system/Chan.kop` | 9 | Channel CSP antar-thread: payload `long` atau closure `fn`, dengan buffer melingkar dan penunggu condvar. |
| [`Env`](api-system.md#env) | `lib/system/Env.kop` | 11 | — |
| [`Log`](api-system.md#log) | `lib/system/Log.kop` | 18 | Logging ke stderr dengan timestamp UTC dan level. |
| [`Mutex`](api-system.md#mutex) | `lib/system/Mutex.kop` | 3 | Kunci mutual-exclusion antar-thread untuk melindungi data bersama. |
| [`Time`](api-system.md#time) | `lib/system/Time.kop` | 32 | Waktu UTC: epoch detik, monotonic milidetik, sleep, format tanggal, dan nama hari. |
| [`Uuid`](api-system.md#uuid) | `lib/system/Uuid.kop` | 6 | UUID versi 4 (RFC 4122) acak + format/parse/validasi UUID. |

### Compress — [buka referensi](api-compress.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Checksum`](api-compress.md#checksum) | `lib/compress/Checksum.kop` | 11 | — |
| [`Compress`](api-compress.md#compress) | `lib/compress/Compress.kop` | 29 | — |
| [`Filter`](api-compress.md#filter) | `lib/compress/Filter.kop` | 27 | — |
| [`LzmaDec`](api-compress.md#lzmadec) | `lib/compress/Lzma.kop` | 53 | — |
| [`Lzma2`](api-compress.md#lzma2) | `lib/compress/Lzma2.kop` | 29 | — |
| [`LzmaAuto`](api-compress.md#lzmaauto) | `lib/compress/LzmaAuto.kop` | 3 | — |
| [`LzmaStream`](api-compress.md#lzmastream) | `lib/compress/LzmaStream.kop` | 27 | — |
| [`Xz`](api-compress.md#xz) | `lib/compress/Xz.kop` | 21 | — |

### Crypto — [buka referensi](api-crypto.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Aead`](api-crypto.md#aead) | `lib/crypto/Aead.kop` | 7 | AEAD ChaCha20-Poly1305 (RFC 7539 §2.8): enkripsi + autentikasi untuk TLS. |
| [`Aes`](api-crypto.md#aes) | `lib/crypto/Aes.kop` | 7 | AES-128/192/256 (FIPS-197) plus mode CTR, murni Kopi, untuk enkripsi blok dan keystream. |
| [`Big`](api-crypto.md#big) | `lib/crypto/Big.kop` | 20 | Aritmetika medan 2²⁵⁵−19 dengan 17 limb untuk X25519. |
| [`Blake2b`](api-crypto.md#blake2b) | `lib/crypto/Blake2b.kop` | 2 | BLAKE2b (RFC 7693): digest 1..64 byte, opsional dengan kunci — fondasi Argon2/scrypt. |
| [`ChaCha`](api-crypto.md#chacha) | `lib/crypto/ChaCha.kop` | 5 | ChaCha20 stream cipher (RFC 7539) untuk TLS record dan eksperimen. |
| [`Cipher`](api-crypto.md#cipher) | `lib/crypto/Cipher.kop` | 11 | Base64, hex, dan cipher klasik (RC4, Vigenère, XOR) untuk eksperimen/kompatibilitas. |
| [`Crypto`](api-crypto.md#crypto) | `lib/crypto/Crypto.kop` | 18 | Kumpulan utilitas bit/hash/cipher lama yang dipakai modul kripto lain (kompatibilitas v1). |
| [`Der`](api-crypto.md#der) | `lib/crypto/Der.kop` | 4 | Parser ASN.1 DER minimal untuk mengekstrak SubjectPublicKeyInfo (pin TLS). |
| [`Ecdsa`](api-crypto.md#ecdsa) | `lib/crypto/Ecdsa.kop` | 3 | ECDSA kurva P-256 (NIST) dengan k deterministik RFC 6979 (HMAC-SHA256): kunci publik, tanda tangan,… |
| [`Gcm`](api-crypto.md#gcm) | `lib/crypto/Gcm.kop` | 10 | AES-GCM (NIST SP 800-38D): enkripsi autentik dengan tag 16 byte. |
| [`Hash`](api-crypto.md#hash) | `lib/crypto/Hash.kop` | 12 | SHA-1/256/384/512 dan HMAC-SHA256/SHA512 murni Kopi untuk integritas, JWT, PBKDF2, dan TLS. |
| [`Kdf`](api-crypto.md#kdf) | `lib/crypto/Kdf.kop` | 8 | HKDF-SHA256 (RFC 5869) dan ExpandLabel TLS 1.3 (RFC 8446 §7.1). |
| [`Pbkdf2`](api-crypto.md#pbkdf2) | `lib/crypto/Pbkdf2.kop` | 2 | PBKDF2-HMAC-SHA256 untuk menyimpan kata sandi dengan iterasi tinggi. |
| [`Pem`](api-crypto.md#pem) | `lib/crypto/Pem.kop` | 3 | Armor PEM (RFC 7468): encode/decode sertifikat dan kunci ke DER biner. |
| [`Poly`](api-crypto.md#poly) | `lib/crypto/Poly.kop` | 10 | Poly1305 one-time MAC (RFC 7539) dengan aritmetika limb 15-bit. |
| [`Rand`](api-crypto.md#rand) | `lib/crypto/Rand.kop` | 1 | Byte acak kriptografis dari `/dev/urandom`. |
| [`Rsa`](api-crypto.md#rsa) | `lib/crypto/Rsa.kop` | 8 | RSA PKCS#1 v1.5: tanda tangan & verifikasi SHA-1/256/384/512, enkripsi/dekripsi blok. |
| [`Scrypt`](api-crypto.md#scrypt) | `lib/crypto/Scrypt.kop` | 1 | scrypt (RFC 7914): KDF memory-hard untuk menyimpan kata sandi (Salsa20/8 + BlockMix + ROMix). |
| [`X25519`](api-crypto.md#x25519) | `lib/crypto/X25519.kop` | 2 | Pertukaran kunci X25519 (RFC 7748) untuk TLS 1.3. |
| [`X509`](api-crypto.md#x509) | `lib/crypto/X509.kop` | 11 | Parsing sertifikat X.509 (RFC 5280): subject, issuer, masa berlaku, SPKI, pin, kunci publik. |

### Net — [buka referensi](api-net.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Dns`](api-net.md#dns) | `lib/net/Dns.kop` | 17 | Klien DNS-over-TCP (RFC 1035/7766) ke server eksplisit; query A + parser jawaban. |
| [`Resolv`](api-net.md#resolv) | `lib/net/Resolv.kop` | 5 | Resolver DNS UDP + pembaca `/etc/resolv.conf`, dengan timeout. |
| [`Rfc`](api-net.md#rfc) | `lib/net/Rfc.kop` | 5 | Klien protokol TCP klasik (daytime, echo, quote, whois, finger, discard) untuk latihan. |
| [`Tcp`](api-net.md#tcp) | `lib/net/Tcp.kop` | 18 | Transport TCP di atas builtin `Net`: buka koneksi, kirim/terima buffer, shutdown, helper alamat. |
| [`Tls`](api-net.md#tls) | `lib/net/Tls.kop` | 27 | Klien TLS 1.3: X25519, ChaCha20-Poly1305, HKDF, pin SPKI TOFU, verifikasi Finished, data aplikasi dua arah. |
| [`TlsServer`](api-net.md#tlsserver) | `lib/net/TlsServer.kop` | 29 | Server TLS 1.3: X25519 + ECDSA P-256 self-signed, ChaCha20-Poly1305/AES-GCM, verifikasi client Finished,… |
| [`Udp`](api-net.md#udp) | `lib/net/Udp.kop` | 9 | Datagram UDP: bind, kirim/terima satu datagram, shutdown. |

### Web — [buka referensi](api-web.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Auth`](api-web.md#auth) | `lib/web/Auth.kop` | 8 | Parser header Authorization: skema, Basic (user:pass), Bearer, tantangan 401. |
| [`Http`](api-web.md#http) | `lib/web/Http.kop` | 44 | Klien HTTP/1.1: GET/POST, header kustom, chunked, redirect, basic auth, timeout 5 detik. |
| [`Http2`](api-web.md#http2) | `lib/web/Http2.kop` | 23 | Klien HTTP/2 (RFC 9113/7540) di atas Tls: gate TLS+ALPN h2+preface, request HEADERS+DATA (stream id… |
| [`Http2Tabel`](api-web.md#http2tabel) | `lib/web/Http2Tabel.kop` | 6 | Tabel HPACK (RFC 7541) untuk Http2: kode Huffman kanonik 257 simbol (Appendix B) dan tabel statik 61 entri… |
| [`HttpServer`](api-web.md#httpserver) | `lib/web/HttpServer.kop` | 47 | Parser request HTTP/1.1 dan pembangun respons: method/path/query/header/body, keep-alive, HEAD, cookie,… |
| [`Jwt`](api-web.md#jwt) | `lib/web/Jwt.kop` | 18 | JWT HS256: tanda tangan, verifikasi, ambil payload. |
| [`Mime`](api-web.md#mime) | `lib/web/Mime.kop` | 1 | Pemetaan ekstensi file → header Content-Type. |
| [`Multipart`](api-web.md#multipart) | `lib/web/Multipart.kop` | 17 | — |
| [`Router`](api-web.md#router) | `lib/web/Router.kop` | 14 | — |
| [`Session`](api-web.md#session) | `lib/web/Session.kop` | 6 | Sesi in-memory ber-expiry di atas dua MapStr (id→uid, id→expiry). |
| [`Static`](api-web.md#static) | `lib/web/Static.kop` | 7 | Penyajian file statis + MIME; menolak path traversal `..`. |
| [`Url`](api-web.md#url) | `lib/web/Url.kop` | 26 | URL/URI RFC 3986: percent-encoding, form encoding, parsing komponen, resolusi relatif, normalisasi path, IPv4. |
| [`WebSocket`](api-web.md#websocket) | `lib/web/WebSocket.kop` | 12 | Server WebSocket RFC 6455: handshake, frame teks/biner/ping/pong/close, decode frame klien bermask. |
| [`WsClient`](api-web.md#wsclient) | `lib/web/WsClient.kop` | 15 | — |

### Database — [buka referensi](api-db.md)

| Modul | File | Fungsi | Ringkasan |
|---|---|---|---|
| [`Sqlite`](api-db.md#sqlite) | `lib/db/Sqlite.kop` | 18 | Pembaca file database SQLite read-only (RFC sqlite file format): b-tree tabel, varint, record serial-type,… |

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
