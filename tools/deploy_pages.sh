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

cp docs/index.html "$target/"
cp docs/01-mulai.html "$target/"
cp docs/02-bahasa.html "$target/"
cp docs/03-pustaka.html "$target/"
cp docs/04-server.html "$target/"
cp docs/05-developer.html "$target/"
cp docs/06-batas-roadmap.html "$target/"
cp docs/404.html "$target/"
cp docs/style.css "$target/"
cp docs/.nojekyll "$target/"

echo "disalin ke $target"
echo "lanjutkan: cd $target && git add -A && git commit -m 'docs: perbarui' && git push"
