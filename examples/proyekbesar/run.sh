#!/bin/sh
# Jalankan uji ketangguhan proyek besar (butuh python3 untuk bench.py).
cd "$(dirname "$0")"
exec python3 bench.py "$@"
