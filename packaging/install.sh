#!/usr/bin/env bash
# Cài bản build sẵn trong bin/ cho fcitx5 (có libvgo.so -> vgo Telex, có
# libvgo-vni.so -> vgo VNI; gộp 2 gói thì cài cả hai). Chạy: ./install.sh
# Build từ source thì dùng: cmake -S . -B build && cmake --build build && sudo cmake --install build
set -euo pipefail
cd "$(dirname "$0")"

command -v fcitx5 >/dev/null || {
  echo "Chưa có fcitx5. Cài trước: sudo apt install fcitx5 fcitx5-config-qt" >&2; exit 1; }
LIBDIR=""
for d in /usr/lib/x86_64-linux-gnu/fcitx5 /usr/lib64/fcitx5 /usr/lib/fcitx5; do
  [[ -d "$d" ]] && { LIBDIR="$d"; break; }
done
[[ -n "$LIBDIR" ]] || { echo "Không tìm thấy thư mục addon của fcitx5" >&2; exit 1; }

# .so build trên Ubuntu 24.04 / fcitx5 5.1.x: thiếu thư viện = khác bản -> build từ source
for so in bin/libvgo.so bin/libvgo-vni.so; do
  [[ -e "$so" ]] || continue
  if ldd "$so" | grep -q "not found"; then
    echo "$so không chạy được trên máy này (khác bản fcitx5):" >&2
    ldd "$so" | grep "not found" >&2
    echo "-> build từ source, xem HUONG-DAN.md" >&2; exit 1
  fi
done

ICONS=/usr/share/icons/hicolor/scalable/apps
if [[ -e bin/libvgo.so ]]; then
  sudo install -m 644 bin/libvgo.so "$LIBDIR/libvgo.so"
  sudo install -m 644 fcitx5/vgo-addon.conf /usr/share/fcitx5/addon/vgo.conf
  sudo install -m 644 fcitx5/vgo.conf /usr/share/fcitx5/inputmethod/vgo.conf
  sudo install -m 644 fcitx5/vgo.svg "$ICONS/fcitx-vgo.svg"
  sudo install -d /usr/share/vgo
  sudo install -m 644 data/en-base.txt data/vn-base.txt data/bigram-base.txt \
       data/vn-syllables.txt /usr/share/vgo/
  echo "Đã cài vgo (Telex)."
fi
if [[ -e bin/libvgo-vni.so ]]; then
  sudo install -m 644 bin/libvgo-vni.so "$LIBDIR/libvgo-vni.so"
  sudo install -m 644 fcitx5/vgo-vni-addon.conf /usr/share/fcitx5/addon/vgo-vni.conf
  sudo install -m 644 fcitx5/vgo-vni.conf /usr/share/fcitx5/inputmethod/vgo-vni.conf
  sudo install -m 644 fcitx5/vgo.svg "$ICONS/fcitx-vgo-vni.svg"
  echo "Đã cài vgo (VNI)."
fi
sudo gtk-update-icon-cache -f -q /usr/share/icons/hicolor 2>/dev/null || true

echo "Tiếp theo:"
echo "  1. Khởi động lại fcitx5:  fcitx5 -r -d   (hoặc đăng xuất / đăng nhập lại)"
echo "  2. fcitx5-configtool -> thêm 'vgo (Telex)' / 'vgo (VNI)' vào danh sách bộ gõ"
