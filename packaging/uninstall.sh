#!/usr/bin/env bash
# Gỡ đúng bộ gõ của gói này (có bin/libvgo.so -> vgo Telex, có bin/libvgo-vni.so -> VNI),
# chỉ xoá các file install.sh đã cài, không đụng bộ gõ khác.
# Dữ liệu tự học của vgo Telex (~/.config/vgo) giữ nguyên; muốn xoá thì xoá tay.
set -u
cd "$(dirname "$0")"
LIBDIRS="/usr/lib/x86_64-linux-gnu/fcitx5 /usr/lib64/fcitx5 /usr/lib/fcitx5"
ICONS=/usr/share/icons/hicolor/scalable/apps
rmf() { [[ -e "$1" ]] && sudo rm -rv "$1"; return 0; }

if [[ -e bin/libvgo.so ]]; then
  for d in $LIBDIRS; do rmf "$d/libvgo.so"; done
  rmf /usr/share/fcitx5/addon/vgo.conf
  rmf /usr/share/fcitx5/inputmethod/vgo.conf
  rmf "$ICONS/fcitx-vgo.svg"
  rmf /usr/share/vgo
fi
if [[ -e bin/libvgo-vni.so ]]; then
  for d in $LIBDIRS; do rmf "$d/libvgo-vni.so"; done
  rmf /usr/share/fcitx5/addon/vgo-vni.conf
  rmf /usr/share/fcitx5/inputmethod/vgo-vni.conf
  rmf "$ICONS/fcitx-vgo-vni.svg"
fi
echo "Đã gỡ. Khởi động lại fcitx5: fcitx5 -r -d"
