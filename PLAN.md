# PLAN: Bahasa Kopi — Roadmap Menuju Bahasa Server Backend 100% Pertama di Duni

File ini adalah **perencanaan hidup** untuk membuat **Bahasa Kopi** menjadi **bahasa server backend pertama di dunia** yang unik dalam hal:
- **Deterministik real-time** (tanpa GC pause)
- **Jaringan zero-copy** & kebijakan capability-based
- **Pemrograman metaprogram & compile-time kode generation**
- **Prototipe berorientasi plugin & peningkatan bahasa di runtime**

Lokasi: `~/kopi/PLAN.md`

---

## 1. Visi & Tujuan Utama

### A. Tujuan Utama

1. **Satu binary, semua target** — `kopivm`, `kopicc-a64`, `kopicc-x64` yang menghasilkan bytecode KOP1, assembly ARM64, assembly x86-64.
2. **Deterministik & real-time** — arena memory manual + region manajemen → tidak ada GC pause, waktu eksekusi yang dapat diprediksi.
3. **Jaringan built-in** — TCP/UDP/HTTP/WebSocket/DNS/TLS *zero-copy*, *async-safe* dengan *capability-based* security (no ambient authority).
4. **Tipe-level & metaprogramming** — generik penuh, dependent types via *compile-time evaluation*, reflection & code generation.
5. **Plugin & peningkatan bahasa di runtime** — *runtime class loading*, *hot-patching*, *incremental compilation*.

### B. Unik Selling Proposition

| Fitur | Bahasa Lain | Kopi |
|---------|--------------|------|
| **Tanpa GC pause** | Java/Go ( paus occasional ) | Manual arena deterministik |
| **Bytecode + native** | JVM (hanya bytecode) | KOP1 VM + native assembly |
| **Capability networking** | Node.js (ambient socket) | Capability-based Net (grant per resource) |
| **Compile-time code generation** | Zig (macros) | *Macro VM*: generate bytecode langsung |
| **Runtime plugin hot-patching** | .NET (AppDomain) | Kelas dinamis & native method injection |
| **Deterministic debugging** | Rust (immutable) | Region lifetimes + stack trace lengkap |
| **Zero-copy serialization** | Serde (Rust) | Built-in `Net.bytes` / `Net.mkstr` |
| **Multi-pass compilation** | C++ (single-pass) | Lexer → Parser → AST → IR → Bytecode / ASM |

---

## 2. Fitur Bahasa Unik (Mengapa Kopi Unik)

### A. Memory Model
- **Arena + Region** – deterministik alokasi, deterministik pembebasan.
- **Manual `malloc{}`** – blok memori yang hidup sampai region berakhir.
- **Region bertingkat** – untuk nested scopes (lambda/closure).
- **Tidak ada GC** → waktu eksekusi yang dapat diprediksi.

### B. Concurrency & I/O
- **Green threads + OS threads** – `spawn()` dapat memilih implementation.
- **Channels (CSP)** – `chan T`, select, untuk komunikasi tanpa race.
- **Zero-copy async I/O** – `Net.recv` langsung ke arena.
- **Non-blocking epoll** – `pollfd`, `settimeout`, `setnonblock`.

### C. Type System & Metaprogramming
- **Generik penuh** – `List<T>`, `Map<K,V>`, `Result<T,E>`, `Option<T>`.
- **Traits / Interface** – untuk polymorphism & dependency injection.
- **Macro system** – * hygienic*, dapat menghasilkan bytecode KOP1 (compile-time).
- **Reflection** – tipe runtime, tipe anotasi, field introspection.
- **Dependent types lite** – tipe-level natural numbers untuk `List<3>`.

### D. Jaringan & Keamanan
- **Capability-based socket** – `Net.open` hanya jika memiliki capability.
- **Built-in TLS/HTTPS** – `TlsServer.kop`, `TlsClient.kop`.
- **HTTP/HTTPS server + router** – routing middleware, CORS, auth.
- **Auth framework** – JWT, OAuth2/OIDC, session state.
- **Zero-copy JSON/Bytes** – `Net.bytes("…")` ↔ `int[]`.

### E. Tooling & Ecosystem
- **Package manager** – `kopi add` dari registry lokal/online.
- **Plugin architecture** – library `.kop` dapat *hot-patch* server.
- **Language server** – LSP support untuk completion, hover, goto-def, refactoring.
- **Formatter & linter** – `kopi fmt`, `kopi lint`.
- **Test framework** – property testing (`QuickCheck`), mocking, coverage.
- **Observability** – structured log (`JSON`), metrics (`Prometheus`), tracing (`OpenTelemetry`).
- **Deploy** – Dockerfile generator, CI/CD pipeline, zero-downtime deploy.

---

## 3. Tahapan Implementasi (12–24 bulan)

### Pra-Fondasi (0–1 bulan)

| Bulan | Tujuan | Deliverable |
|--------|---------|-------------|
| 0 | Desain bahasa akhir, sintaks, ABI, document language spec | Bahasa Spesifikasi, paket `lexer + parser` |
| 1 | Implementasi makro VM & core reflection | Macro VM (generate bytecode), `reflect.kop` |

### Fase 1 — Fondasi Bahasa (1–4 bulan)

| Bulan | Fokus | Item Utama |
|--------|-------|------------|
| 1–2 | **Konkurensi & I/O** | Green threads, OS threads, channels, epoll, zero-copy |
| 2–3 | **Error Handling** | `Result<T,E>`, `try` expr, `catch`, pattern matching (`match`) |
| 3–4 | **Type System & Metaprogramming** | Generik, Traits, Macro VM, Dependent types, Reflection |
| 4 | **Plugin Runtime** | `Plugin.load()`, hot-patch method, class dynamic |

#### Milestone 1 — Deterministic Runtime
- Arena + Region → bukan GC
- Concurrency → `spawn()` & channels
- Error → `Result<T,E>` & `try`

### Fase 2 — Ekosistem Server (4–11 bulan)

| Bulan | Fokus | Library |
|--------|-------|----------|
| 5–6 | **Jaringan & HTTP** | `Net.*` (TCP/UDP/HTTP/WebSocket/TLS), `HttpServer.kop` + router, middleware, auth, CORS |
| 7–8 | **Database** | `sqlite`, `pg`, `mysql`, `redis`, `mongodb` (native klien) |
| 9–10 | **Serialisasi & Config** | JSON / MessagePack / CBOR (built-in), `toml`, `yaml` parser, auto-validation |
| 11 | **Caching & Rate Limiting** | In-memory cache (`MapStr`), distributed (`redis`), rate limiter |

#### Milestone 2 — Full-stack Runtime
- Server siap produksi dengan `HttpServer`, DB drivers, auth, serialization.

### Fase 3 — Tooling & DX (6–14 bulan)

| Bulan | Fokus | Tool |
|--------|-------|------|
| 6–8 | **Package Manager** | `kopi` CLI, registry server, lockfile, binary caching |
| 9–11 | **Test & Lint** | Property testing (`kopi test`), mocking, coverage, fuzzing (`kopi fuzz`) |
| 12–13 | **Formatter & Linter** | `kopi fmt`, `kopi lint`, rules (`import`, `naming`) |
| 14 | **Language Server** | LSP server (`kopiserver`) — completion, hover, goto-def, refactoring |

#### Milestone 3 — Developer Experience
- Lingkungan pengembangan penuh (CLI + LSP + plugin).

### Fase 4 — Produksi & Ops (12–20 bulan)

| Bulan | Fokus | Ops |
|--------|-------|-----|
| 15–16 | **Observability** | Structured logging (`JSON`), metrics (`Prometheus`), tracing (`OTel`) |
| 17–18 | **Health & Graceful Shutdown** | `/health`, `/ready`, `/live`, graceful drain, zero-downtime deploy |
| 19–20 | **Deploy & CI/CD** | Dockerfile template, GitHub Actions, canary releases, rollback |

#### Milestone 4 — Produksi Siap
- Observability lengkap, health check, CI/CD pipeline.

### Fase 5 — Enterprise & 100% (18–24 bulan)

| Bulan | Fokus | Fitur |
|--------|-------|---------|
| 21–22 | **Security & Scanning** | Binary hardening, race detection, static analysis, policy as code |
| 23–24 | **Advanced Interop** | multi-language interop (`C`/`Rust`), WebAssembly (`.kop` → WASM), serverless (`kopicc` → Lambda) |

#### Milestone 5 — 100% Produksi
- Bahasa server benar-benar siap enterprise (observability, security, interop).

---

## 4. Daftar Checklist (Versi 2.0)

### A. Konfirmasi Bahasa (20 item)
- [ ] Deterministic arena memory (no GC)
- [ ] Green threads + OS threads + channels
- [ ] Capability-based Net security
- [ ] Macro VM (compile-time bytecode generation)
- [ ] Runtime hot-patch plugin system
- [ ] Full HTTP router + middleware
- [ ] Minimal 3 driver DB (PostgreSQL, SQLite, Redis)
- [ ] Zero-copy JSON/MsgPack/CBOR serialization
- [ ] Built-in TLS/HTTPS
- [ ] Auth & JWT (OAuth2/OIDC)
- [ ] Package manager (`kopi add`) + registry
- [ ] Property testing + mocking + coverage
- [ ] Linter & Formatter
- [ ] Language server (LSP) + completion + refactoring
- [ ] Structured logging + metrics + tracing
- [ ] Health check + graceful shutdown
- [ ] Dockerfile & CI template
- [ ] Canary release + rollback
- [ ] Binary hardening & race detection
- [ ] Multi-language interop (`C`/`Rust`)

---

## 5. Estimasi Timeline & Owner

| Fase | Durasi | Owner | Critical Path |
|------|---------|-------|---------------|
| Pra-Fondasi | 1 bulan | Tim bahasa inti | Desain bahasa final |
| Fase 1 | 3 bulan | Tim runtime | Konkurensi & error handling |
| Fase 2 | 7 bulan | Tim libs | DB + HTTP + serialisasi |
| Fase 3 | 8 bulan | Tim tooling | Package manager + LSP |
| Fase 4 | 5 bulan | Ops & CI | Observability & deploy |
| Fase 5 | 6 bulan | Security | Interop + scanning |
| **Total** | **30 bulan** | | **Kopi → Bahasa server production pertama** |

**Estimasi risiko:** timeline dapat dipersingkat jika parallelization (Fase 2 & 3 overlap).

---

## 6. Catatan Penting

- **Deterministic = Real-time** → tidak ada GC paus, cocok untuk latency-sensitive services.
- **Capability networking** → native Rust/Go, tetapi Kopi melakukannya *dengan safety bawaan* dari arena.
- **Macro VM** → unik: Anda dapat menghasilkan *bytecode* selama compile-time, tidak hanya template code.
- **Runtime hot-patch** → server *pindah* class dinamis tanpa restart.
- **Multi-pass compilation** → memungkinkan *optimasi* per tahap (`lexer` → `parser` → `IR` → `bytecode`/`asm`).

---

File ini adalah rencana hidup. Perbarui setiap bulan dalam rapat pra-koordinasi dan push ke repo.

**Berangkat menuju pembuatan bahasa server backend *pertama di dunia* yang deterministik, zero-copy, dan metaprogram.** 🚀