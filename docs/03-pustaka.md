# 03 — Pustaka standar

Semua pustaka **ditulis dalam bahasa Kopi** (`lib/*.kop`) di atas builtin
`Net` dan tipe dasar. Import selalu eksplisit:

```java
import Str;
import Math;
import HttpServer;
```

| Folder | Modul |
|---|---|
| `lib/core/` | `Str` `Bytes` `Math` `Digits` `Stats` `Matrix` `Arrays` `Json` `Map` `MapStr` `Set` `Deque` `File` `Path` `Time` `Log` |
| `lib/net/` | `Tcp` `Udp` `Dns` `Resolv` `Http` `HttpServer` `Static` `Mime` `Url` `WebSocket` `Jwt` `Session` `Auth` `Rfc` `Tls` `TlsServer` `Http2` `Http2Tabel` |
| `lib/crypto/` | `Crypto` `Cipher` `Hash` `Aes` `Gcm` `Pem` `X509` `Pbkdf2` `Kdf` `Big` `X25519` `ChaCha` `Poly` `Aead` `Rand` `Der` |
| `lib/io/` | `Reader` `Writer` |
| `lib/db/` | `Sqlite` (pembaca file `.db` read-only) |
| `lib/core/` | `Regex` (mesin regex backtracking) `BigInt` (bilangan presisi sembarang) |

> **Referensi API lengkap** (signature semua fungsi diekstrak langsung dari
> source): [Referensi pustaka](api.md) → [Core](api-core.md) ·
> [Net](api-net.md) · [Crypto](api-crypto.md). Halaman di bawah ini adalah
> turunan berisi modul yang paling sering dipakai dengan pola pemakaian.

---

## 1. `Str` — teks native `String`

Sejak **v1.23** `Str` hanya memakai `String`: parameter dan hasil teks
tidak lagi berupa `int[]`. Buffer byte ada di modul terpisah `Bytes`.

| Fungsi | Guna |
|---|---|
| `len isEmpty isBlank eq/equals cmp compare equalsIgnoreCase` | dasar & perbandingan |
| `cat(a,b) join(parts,sep) copy` | gabung / salinan nilai |
| `sub/slice(from,n) left(n) right(n) before/after beforeLast/afterLast between` | potong teks |
| `indexOf(s,ndl,from) lastIndexOf contains startsWith endsWith count` | pencarian |
| `trim trimLeft trimRight strip upper lower capitalize uncapitalize swapCase` | trim & huruf |
| `replace(old,new) replaceSeq replaceByte repeat reverse` | ubah teks |
| `split(s,sep) splitChar(s,sepByte) splitAt splitCount lines words` | pisah / baris / kata |
| `padLeft(s,w,pad) padRight center truncate ellipsis` | padding & batas |
| `fromInt/Long/Short/Byte/UInt8..64/Float/Double/Bool/Char` | skalar → teks |
| `toInt/Long/Short/Byte/UInt8..64/Float/Double/Bool` | teks → skalar |
| `parseInt/parseLong(s,radix) isDigits isNumber isHex` | parsing basis & validasi |
| `toHex fromHex` | hex ↔ teks byte |
| `utf8Count utf8Valid utf8Offset utf8Slice utf8CodeAt utf8Reverse` | utilitas UTF-8 |
| `print(s) line(s)` | tulis teks ke stdout |

`Str.padLeft(Str.fromInt(7), 3, 48)` → `"007"`.
`Str.join(Str.split("a,b,c", ","), "|")` → `"a|b|c"`.

## 1b. `Bytes` — buffer `int[]`

Untuk I/O biner, jaringan, dan kriptografi. Konversi eksplisit:
`Net.strbytes(teks)` → `int[]`, `Net.mkstr(buf)` → `String`
(atau `Bytes.of` / `Bytes.text`).

| Fungsi | Guna |
|---|---|
| `len isEmpty eq cmp copy cat cat3` | dasar |
| `slice/sub(from,n) at first last` | potong & indeks |
| `find(byte,from) indexOf lastIndexOf contains startsWith endsWith count` | cari |
| `trim trimLeft trimRight upper lower equalsIgnoreCase` | trim & case |
| `repeat reverse replaceByte replaceSeq padLeft padRight fill zero` | ubah buffer |
| `splitCount(sepByte) splitAt(sepByte,idx)` | pisah |
| `fromInt fromLong toInt isDigits` | angka ↔ byte |
| `of(text) text(buf) hex(data)` | konversi representasi & hex |
| `print(buf) line(buf)` | tulis buffer ke stdout |

## 2. `Arrays` — array `int[]`

| Fungsi | Guna |
|---|---|
| `sum max min find count arrEq` | agregat |
| `take(n) drop(n) merge(a,b) sort(a) binSearch(v)` | potong/gabung/urut/cari |
| `copy(a) reverse(a)` | salinan / balik |
| `forEach(f) forEachIndexed(f)` | iterasi |
| `map(f) filter(pred)` | transformasi |
| `reduce(init,f) any(pred) all(pred)` | lipat / uji |
| `countWhere(pred) findIndex(pred) sumBy(f)` | agregat lambda |

Bentuk gula: `a.map(x => x * 2)`, `a.reduce(0, (s, x) => s + x)`.

## 3. `Math`, `Digits`, `Stats`, `Matrix`

- **`Math`** (80+ fungsi): `abs sign min max clamp diff sumTo avgFloor`,
  bit (`isEven popcount highestBit bitLength isPowerOfTwo nextPow2
  reverseBits`), pangkat/akar/log (`pow mod addMod mulMod powMod sqrt cbrt
  log2 log10`), FPB/modular (`gcd lcm isCoprime extendedGcd modInverse`),
  prima (`isPrime nextPrime primesUpTo primeCount nthPrime factorize
  eulerPhi`), kombinatorika (`factorial binomial permutation catalan fib
  lucas`), geometri (`distanceSq manhattan rectArea isRightTriangle`).
- **`Digits`**: `reverse digitSum digitCount pow10 digitProduct
  isPalindrome isArmstrong digitalRoot printBase`.
- **`Stats`**: `sum product min max range mean median mode variance
  stddev countEven countOdd`.
- **`Matrix`** (`int[]` datar, persegi): `zero identity add sub scale mul
  transpose trace det det1 det2 det3 isIdentity`.

Semua fungsi integer membungkus 32-bit; untuk matematika besar pakai
`long`/`Big`.

## 4. `Json`

Representasi: tape `int[]`; literal `json {...}` / `json [...]`.

| Fungsi | Guna |
|---|---|
| `parse(text) write(v)` | teks ⇄ tape |
| `print/line(v)` | ke stdout |
| `typeOf isNull isBool isNumber isString isArray isObject` | inspeksi |
| `count(v) at(arr,i) get(obj,key) has(obj,key)` | akses |
| `put del add` | ubah (immutable, hasil tape baru) |
| `asInt asDouble asBool asText` | konversi nilai |
| `nullV boolV num str arrN objN arr1..arr8 obj1..obj4` | konstruktor |

```java
int[] u = json {"nama": "budi", "umur": 20};
int umur = Json.asInt(Json.get(u, "umur"));
```

## 5. Koleksi

- **`Map`** (int→int, open addressing): `make(cap) put get getOr has remove
  clear size grow forEach(f)`.
- **`MapStr`** (String→int, kapasitas+pool tetap): `make(cap,poolCap) put
  get getOr has remove clear size used keyAtSlot valAtSlot`.
- **`Set`** (di atas Map): `make add has remove size clear grow forEach`.
- **`Deque`**: `make(cap) pushBack pushFront popFront popBack get size
  capacity isEmpty isFull clear`.

`Map`/`Set` bisa tumbuh via `grow`; `MapStr` tidak (cocok untuk header).

## 6. File, Path, Time

```java
import File; import Path; import Time;

int fd = File.openRead("data.txt");
int n = File.size(fd);
int[] buf = new int[n];
File.readInto(fd, buf);
File.close(fd);

int out = File.create("hasil.txt");
File.writeAll(out, Net.bytes("halo\n"));
File.close(out);

System.out.println(Time.format(Time.epoch()));
System.out.println(Time.sleepMs(10));
```

| Modul | Fungsi |
|---|---|
| `File` | `openRead create append close writeAll readInto seekSet seekCur seekEnd size remove renameTo mkdir` + flag `O_*` |
| `Path` | `join base dir ext isAbsolute sep SEP` |
| `Time` | `epoch monoMs sleepMs civil weekday format` (UTC) |

`File.readInto` membaca sampai buffer penuh/EOF. Untuk file besar, baca
per blok.

## 7. `Log`

```java
import Log;
Log.info("boot", "siap");
Log.warn(...); Log.error(...);
```

Menulis ke stderr dengan timestamp UTC. `Log.emit(level,tag,msg)` untuk
level kustom.

## 8. Jaringan

### `Tcp`

```java
int fd = Tcp.open(Tcp.loopback(), 8080);   // -errno bila gagal
Tcp.sendBuf(fd, req);
int n = Tcp.recvAll(fd, buf);              // sampai EOF/buffer penuh
Tcp.shutdown(fd, 1);                       // 0=RD 1=WR 2=RDWR
Tcp.drop(fd);
```

`Tcp.ip(a,b,c,d)`, `Tcp.loopback()`, `Tcp.num(v)`, `Tcp.cat/indexOf/tail`.

### `Udp`

```java
int s = Udp.open();
Udp.bind(s, 0, 9000);
Udp.sendTo(s, ip, port, Net.bytes("hi"));   // maks 1200 byte/datagram
int n = Udp.recvFrom(s, buf, from);         // from[0]=ip, from[1]=port
```

### `Dns` / `Resolv`

- `Dns.lookup(addr, port, nama, hasil, cap)` — DNS **over TCP** ke server
  eksplisit.
- `Resolv.servers()` — baca `/etc/resolv.conf` (fallback `8.8.8.8`).
- `Resolv.lookupUdp(server, port, nama, timeoutMs, hasil, cap)` — DNS
  **UDP** dengan timeout.
- `Resolv.lookup(nama, timeoutMs, hasil, cap)` — coba semua server.

`hasil[0]` = RCODE, `hasil[1..4]` = oktet IPv4.

### `Url`

`encode decode formEncode formDecode pair appendQuery hasScheme scheme
authority host portOf path query fragment ip4 normalize resolve`.

```java
String u = Url.resolve("http://a/b/c/d;p?q", "../g");  // http://a/b/g
```

### `Http` (klien)

| Fungsi | Guna |
|---|---|
| `getInto(addr,port,host,path,buf)` | GET lama (HTTP/1.0) |
| `getFull(addr,port,host,path,cap)` | GET HTTP/1.1 → array respons |
| `exchange(addr,port,method,host,path,headers,body,cap)` | transaksi bebas |
| `follow(...)` | ikuti redirect (host sama) |
| `payload(resp)` | body sebenarnya (chunked/Content-Length) |
| `basic(user,pass)` | header Authorization Basic |
| `formPost(...) jsonPost(...)` | POST ter-encode |
| `statusOf bodyOf headerValue hasHeader` | parsing respons |

Timeout socket 5 detik dipasang otomatis.

### `HttpServer`, `Static`, `Mime`

`HttpServer`: `readRequest method path target version headerValue
contentLength body bodyAny queryValue urlDecode statusText response
responseKA respond responseFor headOnly send keepAlive cookie setCookie
methodIs`.

`Static.serve(path)` / `serveKA(path,keep)` menyajikan file + MIME dan
menolak `..`. `Mime.of(path)` untuk Content-Type.

### `WebSocket`

`accept(key) isUpgrade(req) handshake(req) frame(op,payload) text binary
ping pong close(code) decode(buf,n) payload(f) opcode(f)`.

### `Tls` (klien TLS 1.3)

`handshake(ip, port, pinSpki, randSes)` → context; `writeApp(ctx,data)`,
`readApp(ctx)`. `pinSpki` = SHA-256 DER SPKI leaf (TOFU). Belum verifikasi
ECDSA CertificateVerify (server memakai pin SPKI).

### `TlsServer` (server TLS 1.3)

`certDer(priv32)` → sertifikat self-signed ECDSA-SHA256 (X.509 v3);
`accept(fd, certDer, priv32, randSes)` → context (ctx[1]=1 bila sukses);
`writeApp(ctx,data)`, `readApp(ctx)`. Suite: ChaCha20-Poly1305 /
AES-128/256-GCM (prioritas SHA-256). Handshake tervalidasi lawan klien
OpenSSL/Python; belum verifikasi sertifikat klien (mTLS).

### `Http2` (klien HTTP/2)

`gate(ip, port, pinSpki, randSes)` → hctx (TLS + ALPN h2 + preface +
SETTINGS + WINDOW_UPDATE); `request(hctx, method, path, authority, body)`
→ kirim HEADERS+DATA (stream id bergulir 1, 3, 5, …);
`readResp(hctx)` → body; `respStatus(hctx)`, `respErr(hctx)`,
`close(hctx)`. Dekoder HPACK penuh (RFC 7541): indexed, literal
incremental/without/never, tabel dinamis 64 slot, Huffman kanonik.
Regresi: vektor oracle `tools/gen_http2vec.py` (pustaka python hpack);
uji live lawan oracle `tools/h2_oracle.py` (python h2).

### `Http2Tabel` (tabel HPACK)

`hufCode()` `hufLen()` (Huffman 257 simbol), `stName()` `stNameOff()`
`stVal()` `stValOff()` (statik 61 entri). Hasil generator
`tools/gen_http2.py`; sumber data `tools/hpack_huffman.tsv`.

## 9. Database

### `Sqlite` (pembaca file `.db` read-only)

`open(path)` → ctx (validasi header, pageSize 512–65536, UTF-8);
`tables(ctx)` → nama tabel user; `columns(ctx, nm)` → nama kolom dari
CREATE TABLE; `query(ctx, nm)` → siapkan scan (hasil = jumlah kolom);
`next(ctx)` → record berikutnya; akses nilai `intOf` (long, termasuk
int64 & INTEGER PRIMARY KEY = rowid), `realOf` (double, konversi
IEEE-754 murni Kopi), `strOf` (String), `blobOf` (int[]),
`typeOf`/`isNull`/`rowid`/`count`; `close(ctx)`.
Mendukung: b-tree tabel leaf+interior, varint, serial type 0–12+,
rantai halaman overflow (payload ≤ 65536 byte). Read-only, tanpa
kunci — db yang sedang ditulis bisa terbaca setengah jadi.
Regresi: `tools/gen_sqlitevec.py` (fixture page 512, 3 tabel, 219 baris:
NULL/int64/REAL/blob/overflow + b-tree multipage) lawan oracle python
`sqlite3`.

## 10. Kriptografi

| Modul | Isi |
|---|---|
| `Hash` | `sha256 sha512 sha384 hmac hmacSha512 sha1 eq toHex` |
| `Pbkdf2` | `sha256(pass,salt,iter,dkLen)` |
| `Cipher` | `b64enc b64dec hexenc hexdec rc4 vigenere` |
| `Kdf` | HKDF + ExpandLabel TLS |
| `Rand` | `bytes(n)` dari `/dev/urandom` |
| `Aead`, `ChaCha`, `Poly` | ChaCha20-Poly1305 (TLS record) |
| `Aes`, `Gcm` | AES-128/192/256 (FIPS-197) + AES-GCM (SP 800-38D) |
| `Blake2b`, `Scrypt` | BLAKE2b (RFC 7693) + scrypt KDF memory-hard (RFC 7914) |
| `BigInt` | bilangan bulat presisi sembarang (limb 16-bit) |
| `Rsa` | RSA PKCS#1 v1.5 (tanda tangan & enkripsi) di atas `BigInt` |
| `Ecdsa` | ECDSA kurva P-256, k deterministik RFC 6979 (HMAC-SHA256) |
| `Pem`, `X509` | armor PEM + parsing sertifikat X.509 (RFC 5280) |
| `Big`, `X25519` | aritmetika medan 2²⁵⁵−19 & X25519 |
| `Der` | parser DER minimal + pin SPKI |

## 11. Auth & session

```java
import Pbkdf2; import Jwt; import Session; import Auth; import MapStr;

int[] hash  = Pbkdf2.sha256(pass, salt, 100000, 32);   // hash tetap biner
String token = Jwt.sign(payloadJson, secret);           // JWT = teks (HS256)
String t512  = Jwt.signHs512(payloadJson, secret);      // HS384/HS512 juga
String t256  = Jwt.signRs256(payloadJson, privDer);    // RS256 (RSA)
int valid = Jwt.verify(token, secret, out);             // 1 sah, 0 salah, -1 bentuk

int[] ids  = MapStr.make(256, 8192);
int[] exps = MapStr.make(256, 8192);
String sid = Session.create(ids, exps, uid, 3600);
int uid2   = Session.get(ids, exps, sid, Net.time());

String h = HttpServer.headerValue(req, "Authorization");
Auth.checkBasic(h, "budi", "rahasia");
Auth.bearer(h);
```

Penyimpanan sesi masih in-memory (dua `MapStr`); belum persisten.

## 12. `Log` + observability

Belum ada metrics/tracing. Pola yang disarankan: `Log.info` per request
dengan tag + path, dan kirim `X-Request-Id` sendiri. Lihat
`docs/04-server.md`.
