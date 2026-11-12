# Gộp vgo (Telex) + vgo-vni

Hai gói được làm để gộp được: dùng chung **y hệt** các file sau —

- `src/viet.hpp`, `src/viet.cpp` — lõi chữ Việt
- `Makefile`, `CMakeLists.txt` — tự build bộ gõ nào có file nguồn (`src/engine.cpp` ->
  Telex, `src/vni.cpp` -> VNI)
- `install.sh`, `uninstall.sh` — cài / gỡ bộ gõ nào có trong `bin/`
- `test/run-cases.sh`, `.gitignore`, `KET-HOP.md`, `CAI-DAT.md`

## Cách 1 (khuyên dùng): một thư mục, hai bộ gõ

```sh
# 0. File chung đã bị sửa khác nhau ở 2 gói chưa? Có dòng nào in ra thì gộp tay
#    file đó trước (bước 1 sẽ ghi đè bản của vgo/ bằng bản của vgo-vni/).
for f in src/viet.cpp src/viet.hpp Makefile CMakeLists.txt install.sh uninstall.sh \
         test/run-cases.sh; do cmp -s vgo/$f vgo-vni/$f || echo "KHÁC: $f"; done

# 1. Chép gói VNI vào gói Telex
mv vgo/HUONG-DAN.md vgo/HUONG-DAN-telex.md
cp -r vgo-vni/. vgo/
mv vgo/HUONG-DAN.md vgo/HUONG-DAN-vni.md

# 2. Build + test cả hai
cd vgo && make test          # chạy test Telex + test VNI
cmake -S . -B build && cmake --build build    # ra libvgo.so + libvgo-vni.so
```

Kết quả: fcitx5 có 2 bộ gõ "vgo (Telex)" và "vgo (VNI)", chuyển bằng phím tắt
"Enumerate Input Method" của fcitx5 hoặc chọn trên khay (xem `CAI-DAT.md`). Lõi sửa một
chỗ (`viet.cpp`) là cả hai cùng hưởng.

**Đừng bỏ `-fvisibility=hidden`** trong `CMakeLists.txt`: hai `.so` cùng chứa lõi
`viet.cpp` và được nạp chung một tiến trình fcitx5. Không ẩn symbol thì `.so` này có
thể gọi nhầm hàm của `.so` kia — khi hai bên lệch phiên bản sẽ ra lỗi rất khó tìm.

## Cách 2: một bộ gõ, có công tắc Telex/VNI

Làm được nhưng tốn hơn nhiều — các lớp thông minh của Telex đều dựa vào phím chữ:

- `fcitx5/vgoime.cpp` coi số là ranh giới từ (chỉ nạp a–z vào engine).
- Từ điển Anh, danh sách ép-Việt, bigram, file tự học `~/.config/vgo/*` lưu theo
  **phím Telex** (`been`, `ddc`, `cos`…) — vô nghĩa với VNI, lại có thể ép sai
  (`been` không gõ số vẫn bị ép thành "bên").
- Tự sửa gõ nhầm (`correction()`) dựa trên cặp phím Telex (aa/ee/oo/dd) và tự tắt
  khi có số.

Nếu vẫn muốn: giữ `VniEngine` và `Engine` là hai class riêng, chỉ để lớp fcitx chọn
class theo công tắc — đừng trộn luật VNI vào `Engine::key()`.

## Sau khi gộp

`git init && git add -A && git commit -m "gộp vgo + vgo-vni"` để từ đây về sau thấy
được mình đã sửa gì.
