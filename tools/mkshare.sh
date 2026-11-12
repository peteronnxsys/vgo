#!/usr/bin/env bash
# Đóng 2 gói chia sẻ (source build được + bản build sẵn trong bin/):
#   dist/share/vgo-YYYYMMDD.tar.gz      — Telex
#   dist/share/vgo-vni-YYYYMMDD.tar.gz  — VNI
# Hai gói dùng chung y hệt lõi + build + script cài, gộp lại được (packaging/KET-HOP.md).
# Chạy từ gốc repo: make share
set -euo pipefail
cd "$(dirname "$0")/.."

VER=$(date +%Y%m%d)
OUT=dist/share
rm -rf "$OUT" && mkdir -p "$OUT"

cmake -S . -B build -DCMAKE_BUILD_TYPE=Release >/dev/null
cmake --build build -j >/dev/null
make -s vgo-cli vgo-vni-cli

COMMON=(Makefile CMakeLists.txt .gitignore src/viet.hpp src/viet.cpp
        test/run-cases.sh fcitx5/vgo.svg)
TELEX=(README.md src/engine.hpp src/engine.cpp src/cli.cpp tools/mklex.cpp
       data/en-base.txt data/vn-base.txt data/bigram-base.txt data/vn-syllables.txt
       fcitx5/vgoime.h fcitx5/vgoime.cpp fcitx5/vgo.conf fcitx5/vgo-addon.conf
       test/tests.cpp test/vikey-cases.txt)
VNI=(src/vni.hpp src/vni.cpp src/vni-cli.cpp
     fcitx5/vniime.h fcitx5/vniime.cpp fcitx5/vgo-vni.conf fcitx5/vgo-vni-addon.conf
     test/vni-cases.txt)

# pack <tên> <file hướng dẫn> <bin...> -- <file nguồn...>
pack() {
  local name=$1 guide=$2; shift 2
  local bins=()
  while [[ $1 != -- ]]; do bins+=("$1"); shift; done; shift
  local dir="$OUT/$name"
  mkdir -p "$dir/bin"
  cp --parents "${COMMON[@]}" "$@" "$dir/"
  cp packaging/install.sh packaging/uninstall.sh packaging/KET-HOP.md packaging/CAI-DAT.md "$dir/"
  cp "$guide" "$dir/HUONG-DAN.md"
  cp "${bins[@]}" "$dir/bin/"
  strip "$dir"/bin/*
  tar -C "$OUT" -czf "$OUT/$name-$VER.tar.gz" "$name"
  echo "-> $OUT/$name-$VER.tar.gz"
}

pack vgo packaging/HUONG-DAN-telex.md build/libvgo.so vgo-cli -- "${TELEX[@]}"
pack vgo-vni packaging/HUONG-DAN-vni.md build/libvgo-vni.so vgo-vni-cli -- "${VNI[@]}"
