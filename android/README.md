# Kopi IDE (Android)

IDE Android untuk bahasa **Kopi** — APK standalone yang membundel compiler
(`kopicc`) dan VM (`kopivm`) di dalamnya. Tidak butuh Termux, tidak butuh
server, tidak butuh internet.

```
┌──────────────────────────────────────────────┐
│  WebView  (assets/: HTML + CSS + JS)         │
│  ├─ sidebar file ala VS Code                 │
│  ├─ editor + line numbers + pinch zoom       │
│  ├─ highlight sintaks + autocomplete         │
│  └─ panel log bawah (slide up/down)          │
│              ↕  JavascriptInterface "Kopi"   │
│  IdeBridge.java                              │
│  ├─ baca/tulis berkas proyek                 │
│  ├─ jalankan kopicc -t vm → out/*.kbc        │
│  └─ jalankan kopivm out/*.kbc (stream output)│
│              ↕ exec                          │
│  files/lib/libkopicc.so, libkopivm.so        │
└──────────────────────────────────────────────┘
```

## Tata letak folder

| Path | Isi |
|---|---|
| `/storage/emulated/0/KopiProject/` | proyek pengguna |
| `.../KopiProject/<nama>/src/` | sumber `.kop` |
| `.../KopiProject/<nama>/build/` | berkas antara |
| `.../KopiProject/<nama>/out/` | bytecode `.kbc` + hasil |
| `/data/data/com.rinnejay.kopiide/files/lib/` | **biner compiler & VM** (dipisah dari proyek) |
| `.../files/lib/lib/` | modul bahasa (`core/Str.kop`, `net/*.kop`, …) diekstrak dari `assets/kopilib.zip` |
| `/data/data/com.rinnejay.kopiide/files/tmp/` | scratch eksekusi (`TMPDIR`) |

Tombol **＋** membuat proyek baru lengkap dengan `src/Main.kop` contoh.

## Kenapa `targetSdkVersion="28"`

Android 10+ melarang aplikasi dengan `targetSdk >= 29` mengeksekusi biner
dari data dir-nya (`Permission denied`). Termux memakai trik yang sama:
tetap `targetSdk 28` supaya `files/lib/libkopicc.so` bisa dijalankan.

Konsekuensi: aplikasi ini untuk **sideload**, bukan Google Play (Play
mewajibkan targetSdk tinggi). Semua fitur lain tetap berjalan di Android
modern; akses folder memakai `MANAGE_EXTERNAL_STORAGE` (All files access).

Bila PackageManager tidak mengekstrak `jniLibs`, bridge mengambil biner
langsung dari dalam APK (`lib/arm64-v8a/lib*.so`) lalu menaruhnya di
`files/lib`. Jadi tidak bergantung pada `extractNativeLibs`.

Modul bahasa (`lib/core/Str.kop`, `lib/net/Net.kop`, …) ikut dibundel
sebagai `assets/kopilib.zip` dan diekstrak ke `files/lib/lib`. `kopicc`
dijalankan dengan `KOPPATH=files/lib/lib`, sehingga `import Str;` dan
kawan-kawan menemukan berkas modulnya di perangkat.

## Build

Prasyarat (Termux): `openjdk-17`, `aapt` (bukan `aapt2`), `dx`,
`apksigner`, `zipalign`, `python3`, `node` (opsional untuk cek JS).

```sh
cd ~/kopi
rm -f kopicc kopivm && make          # biner compiler + VM (aarch64)
sh android/build_apk.sh              # → android/dist/KopiIDE-1.0.4.apk
```

`build_apk.sh` otomatis:
1. mengunduh `android/sdk/android.jar` (API 30, sekali, ±21 MB, tidak di-commit);
2. membuat ikon mipmap (`tools/gen_icon.py`);
2b. membungkus seluruh `lib/*.kop` menjadi `assets/kopilib.zip` (`tools/mklibzip.py`) — **wajib**, karena `import Str;`/`Net;`/… mencari berkas modul, bukan builtin;
3. `aapt package` (manifest + res + assets);
4. `javac` + `dx` → `classes.dex`;
5. menambahkan dex + `lib/arm64-v8a/*.so` (`tools/addzip.py`, karena `zip` tidak ada);
6. `zipalign` + `apksigner` (debug keystore dibuat sekali);
7. menyalin APK ke `/storage/emulated/0/Download/`.

> Catatan Termux: `aapt2` rusak (`cannot locate symbol ...absl...`), karena
> itu build memakai `aapt` v0.2.

## Install & izin

1. Buka APK → izinkan "Install unknown apps".
2. Saat pertama dibuka, tekan **Izinkan** pada bar kuning:
   - Android 11+ → **All files access** untuk `Kopi IDE`;
   - Android ≤ 10 → izin penyimpanan biasa.
3. Kalau folder proyek belum bisa ditulis, IDE menampilkan panel masalah.

Diagnostik: tombol **Tes biner** di bawah sidebar menjalankan
`kopicc --version`, `kopivm`, dan **uji end-to-end** (compile + run
program kecil ber-`import Str;`), lalu menulis hasil + `targetSdk` +
`nativeLibraryDir` + jumlah modul ke panel Output. Kalau build gagal,
log compiler lengkap kini ditampilkan di tab **Masalah** (bukan lagi
hanya “gagal”). Ini cara tercepat memastikan biner
bisa dieksekusi di perangkat.

## Fitur IDE

- **Sidebar file** (geser dari kiri): pohon folder, buka/rename/hapus
  (tekan lama), tombol proyek baru, status biner.
- **Tab berkas** (ala VS Code): beberapa berkas terbuka sekaligus,
  ketuk tab untuk pindah, `×` untuk menutup. Buffer tiap tab
  (teks, posisi kursor, scroll, status belum tersimpan) disimpan di
  memori, jadi editan tidak hilang saat berpindah tab. Tab bertanda
  `●` bila belum disimpan; menutup tab otomatis menyimpannya. Daftar
  tab dipulihkan saat aplikasi dibuka lagi, dan berkas aktif ditandai
  di sidebar.
- **Editor**: line numbers, highlight Kopi, pinch zoom 10–30 px,
  auto-indent, Tab = 4 spasi, status Ln/Col + zoom. Setiap berkas
  punya hasil build (`.kbc`) sendiri, jadi ▶ selalu menjalankan
  berkas yang sedang aktif.
- **Autocomplete**: saran keyword/tipe/modul/anggota (`System.out.`,
  `Str.`, `Net.`, `String`), kata dari berkas aktif, snippet
  `System.out.println`. Muncul saat mengetik; pilih dengan ketuk atau
  Enter/Tab, tutup dengan Esc. Tombol **auto** di status bar memaksa
  muncul. Di dalam komentar/string saran dimatikan.
- **Build**: `kopicc <src> -t vm -o out/<nama>.kbc` (selalu VM).
- **Run**: `kopivm out/<nama>.kbc`, output di-stream ke panel.
  Tombol ▶ berubah ■ untuk menghentikan proses.
- **Panel Masalah**: mem-parse `baris N:` dari log compiler; ketuk untuk
  lompat ke baris.
- **Panel bawah** bisa di-drag naik/turun (Output / Masalah / bersihkan).
- **Back**: tutup sidebar → tutup panel → keluar aplikasi.

## Batasan

- **VM-only**: bytecode `.kbc`, bukan biner native a64/x64. Cocok untuk
  belajar dan logika; bukan untuk benchmark performa native.
- **arm64-v8a saja** (perangkat Android 64-bit modern). APK tidak berisi
  `armeabi-v7a`/`x86`.
- Compiler yang dibundel adalah **Kopi v1.20.0** (switch, `malloc { ... }`,
  field `long`/`String`/kelas, array bertipe, template string). Field
  `double`/`float` belum ada.
- Tidak ada terminal bebas / menjalankan biner eksternal; hanya
  `kopicc` dan `kopivm` yang dibundel.
- Proyek disimpan sebagai berkas biasa; menghapus aplikasi tidak
  menghapus `/storage/emulated/0/KopiProject`.

## Struktur sumber

```
android/
  app/AndroidManifest.xml
  app/src/com/rinnejay/kopiide/MainActivity.java   WebView + back
  app/src/com/rinnejay/kopiide/IdeBridge.java      bridge berkas + exec
  app/assets/index.html | style.css | app.js       UI
  app/assets/highlight.js                          tokenizer Kopi
  app/assets/complete.js                           mesin autocomplete
  tools/gen_icon.py  tools/addzip.py               generator ikon / zip
  build_apk.sh
```

`highlight.js` dan `complete.js` tidak menyentuh DOM, jadi bisa diuji:

```sh
node --check android/app/assets/app.js
node -e 'const C=require("./android/app/assets/complete.js"); console.log(C.suggest("System.out.pr",13,[]))'
```
