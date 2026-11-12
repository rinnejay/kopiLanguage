#!/bin/sh
# build_apk.sh — bangun Kopi IDE APK (aapt2 + javac + dx + apksigner).
#
# Hasil: android/dist/KopiIDE-<versi>.apk
# Butuh: aapt2, dx, javac, keytool, zipalign, apksigner (Termux: paket
#        aapt/apksigner/openjdk-17), dan android/sdk/android.jar (diunduh
#        otomatis sekali, ±21 MB, tidak di-commit).
set -e
cd "$(dirname "$0")/.."
APP="$PWD/android"
SDK="$APP/sdk"
BUILD="$APP/build"
DIST="$APP/dist"
JAR="$SDK/android.jar"
VER="1.0.4"

need() { command -v "$1" >/dev/null 2>&1 || { echo "butuh $1 di PATH" >&2; exit 1; }; }
need aapt; need javac; need dx; need zipalign; need apksigner; need keytool

if [ ! -f "$JAR" ]; then
    echo "→ unduh android.jar (API 30)…"
    mkdir -p "$SDK"
    curl -sL --max-time 300 -o "$JAR" \
        "https://raw.githubusercontent.com/Sable/android-platforms/master/android-30/android.jar"
fi

echo "→ siapkan biner compiler (libkopicc.so / libkopivm.so)"
mkdir -p "$APP/jniLibs/arm64-v8a"
if [ -x ./kopicc ]; then cp -f ./kopicc "$APP/jniLibs/arm64-v8a/libkopicc.so"; fi
if [ -x ./kopivm ]; then cp -f ./kopivm "$APP/jniLibs/arm64-v8a/libkopivm.so"; fi
[ -f "$APP/jniLibs/arm64-v8a/libkopicc.so" ] || { echo "kopicc belum dibuild (jalankan make)" >&2; exit 1; }
[ -f "$APP/jniLibs/arm64-v8a/libkopivm.so" ] || { echo "kopivm belum dibuild (jalankan make)" >&2; exit 1; }

echo "→ ikon"
python3 "$APP/tools/gen_icon.py" "$APP/app/res"

echo "→ pustaka Kopi (lib/*.kop → assets/kopilib.zip)"
python3 "$APP/tools/mklibzip.py" "$PWD/lib" "$APP/app/assets/kopilib.zip"

echo "→ bersihkan build lama"
rm -rf "$BUILD" "$DIST"
mkdir -p "$BUILD/classes" "$BUILD/res" "$BUILD/dex" "$BUILD/gen" "$DIST"

echo "→ aapt package (manifest + res + assets)"
aapt package -f -M "$APP/app/AndroidManifest.xml" \
    -S "$APP/app/res" -A "$APP/app/assets" \
    -I "$JAR" -F "$BUILD/base.apk" -J "$BUILD/gen" 

echo "→ javac"
javac -nowarn -source 8 -target 8 -bootclasspath "$JAR" \
    -d "$BUILD/classes" $(find "$APP/app/src" -name '*.java')

echo "→ dx → classes.dex"
dx --dex --output="$BUILD/dex/classes.dex" "$BUILD/classes"
python3 "$APP/tools/addzip.py" "$BUILD/base.apk" "$BUILD/dex/classes.dex" classes.dex

echo "→ jniLibs ke APK"
python3 "$APP/tools/addzip.py" "$BUILD/base.apk" \
    "$APP/jniLibs/arm64-v8a/libkopicc.so" lib/arm64-v8a/libkopicc.so \
    "$APP/jniLibs/arm64-v8a/libkopivm.so" lib/arm64-v8a/libkopivm.so

echo "→ zipalign"
zipalign -f 4 "$BUILD/base.apk" "$BUILD/aligned.apk"

KS="$APP/debug.keystore"
if [ ! -f "$KS" ]; then
    echo "→ buat debug keystore"
    keytool -genkeypair -keystore "$KS" -alias kopi -storepass android -keypass android \
        -keyalg RSA -keysize 2048 -validity 10000 \
        -dname "CN=Kopi IDE, O=Rinnejay, C=ID" >/dev/null 2>&1
fi

echo "→ tanda tangan"
apksigner sign --ks "$KS" --ks-pass pass:android --key-pass pass:android \
    --out "$DIST/KopiIDE-$VER.apk" "$BUILD/aligned.apk"
apksigner verify --verbose "$DIST/KopiIDE-$VER.apk" | sed -n '1,6p'

ls -lh "$DIST/KopiIDE-$VER.apk"
echo "✓ APK: $DIST/KopiIDE-$VER.apk"

# salin ke folder Download bila bisa (memudahkan pemasangan)
DL="/storage/emulated/0/Download"
if [ -d "$DL" ] && [ -w "$DL" ]; then
    cp -f "$DIST/KopiIDE-$VER.apk" "$DL/KopiIDE-$VER.apk" && echo "✓ disalin ke $DL/KopiIDE-$VER.apk"
fi
