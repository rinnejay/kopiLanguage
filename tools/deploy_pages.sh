#!/bin/sh
# deploy_pages.sh — salin situs dokumentasi ke checkout repo GitHub Pages.
# Pemakaian: tools/deploy_pages.sh /path/ke/rinnejay.github.io
# Skrip ini TIDAK melakukan git commit/push; periksa dulu dengan git diff.
set -e
cd "$(dirname "$0")/.."
target="${1:?pakai: tools/deploy_pages.sh /path/ke/rinnejay.github.io}"

[ -d "$target" ] || { echo "tujuan bukan folder: $target" >&2; exit 1; }
[ -d "$target/.git" ] || echo "peringatan: $target bukan repo git" >&2

python3 tools/md2html.py >/dev/null
python3 tools/check_docs.py

cp docs/index.html docs/01-mulai.html docs/02-bahasa.html docs/03-pustaka.html \
   docs/04-server.html docs/05-developer.html docs/06-batas-roadmap.html \
   docs/api.html docs/api-core.html docs/api-net.html docs/api-crypto.html \
   docs/404.html docs/style.css "$target/"
cp docs/.nojekyll "$target/"

echo "disalin ke $target"
echo "lanjutkan: cd $target && git add -A && git commit -m 'docs: perbarui' && git push"
