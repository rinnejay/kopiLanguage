# Kopi Arena — game realtime (server + klien, 100% `.kop`)

Modul game ini **sengaja tidak ditaruh di `lib/`** agar tidak bercampur
dengan pustaka inti Kopi. Import antar-berkas di folder ini memakai
pencarian "folder pengimpor" bawaan kompiler.

## Jalankan

```sh
cd examples/game
../../kopicc GameServer.kop -t a64 -o srv.s && cc srv.s -o srv.bin
../../kopicc GameClient.kop -t a64 -o cli.s && cc cli.s -o cli.bin
./srv.bin 9999            # UDP, 20 tick/detik
./cli.bin 9999 127.0.0.1 budi
```

Kontrol klien: **WASD / panah** = arah (terus bergerak sampai diganti),
**spasi** = berhenti, **q** = keluar. Kumpulkan `*` untuk menambah skor.

Lewat VM: `../../kopivm srv.kbc 9999` dan `../../kopivm cli.kbc 9999`.

## Berkas

| Berkas | Peran |
|---|---|
| `Wire.kop` | protokol biner 1 datagram/pesan (`J` join, `I` input, `P` ping, `Q` quit; `A` ack, `S` snapshot, `X` galat) |
| `Arena.kop` | aturan dunia (60×20 sel, mili-sel, 8 pemain, 4 koin, timeout 60 tick) |
| `ArenaServer.kop` | loop UDP non-blocking + siaran snapshot + `memReset` per iterasi |
| `Render.kop` | gambar ANSI (peta, koin, HUD, papan skor) |
| `GameServer.kop` / `GameClient.kop` | program contoh |
| `BotClient.kop` | klien tanpa layar untuk uji otomatis |
| `arenaunit.kop` | uji deterministik aturan (tanpa jaringan) |

## Batas jujur

- UDP tanpa retransmisi/urutan; paket hilang = satu tick terlewat.
- Tanpa interpolasi/prediksi gerak; posisi digambar apa adanya.
- Satu proses single-thread; belum ada rooms, auth, atau TLS.
- Nama pemain hanya untuk log server; papan skor memakai huruf id.
- Arena 32 MB tanpa GC — gunakan `Net.memMark/memReset` seperti contoh.
