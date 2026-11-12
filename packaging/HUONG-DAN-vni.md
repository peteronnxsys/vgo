# vgo-vni — bộ gõ tiếng Việt kiểu VNI cho Linux (fcitx5)

## Cách gõ

| Số | Dấu | | Số | Dấu |
|---|---|-|---|---|
| 1 | sắc | | 6 | mũ (â ê ô) |
| 2 | huyền | | 7 | móc (ơ ư) |
| 3 | hỏi | | 8 | trăng (ă) |
| 4 | ngã | | 9 | đ |
| 5 | nặng | | 0 | xoá dấu thanh |

```
tie6ng1 Vie65t   -> tiếng Việt
nguo7i2          -> người
d9i / di9        -> đi          (số gõ ở đâu trong từ cũng được)
tieng16          -> tiếng
a11 / d99        -> a1 / d9     (gõ lại cùng số = bỏ dấu, ra số thật)
```

Số thật và mã kỹ thuật giữ nguyên: `2026 h264 mp3 win10 covid19 iphone15 user1 A4`.
Luật: số chỉ thành dấu khi kết quả là một tiếng Việt đúng chính tả (giống UniKey).
**Esc** khi đang gõ dở = lật qua lại giữa chữ thô và tiếng Việt cho từ đó.

## 1. Dùng ngay (bản build sẵn, Ubuntu 24.04)

Làm theo **`CAI-DAT.md`**: bật fcitx5 (một lần) -> `./install.sh` -> thêm "vgo (VNI)"
trong fcitx5-configtool. Gặp trục trặc: bảng sự cố cuối `CAI-DAT.md`.

Gõ thử không cần cài: `bin/vgo-vni-cli "Xin cha2o ca3 nha2"` -> `Xin chào cả nhà`.

## 2. Build từ source

```sh
sudo apt install g++ make
make test                                       # build + chạy test/vni-cases.txt
./vgo-vni-cli "tie6ng1 Vie65t"

# addon fcitx5 (cần thêm thư viện dev, cài 1 lần):
sudo apt install cmake extra-cmake-modules gettext \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev
cmake -S . -B build && cmake --build build
sudo cmake --install build && fcitx5 -r -d
```

Build từ source thì không cần `install.sh` (nó chỉ cài bản trong `bin/`).
Đặt dấu kiểu mới (`hoà`/`thuý`): đổi `oldStyleTone = false` trong `src/vni.hpp`.

Khác vgo Telex: VNI **không** có từ điển Anh, không tự học, không tự sửa gõ nhầm — gõ
tiếng Anh vốn không đụng số nên không cần.

## 3. Tự kiểm tra + thêm ca

`test/vni-cases.txt`, mỗi dòng `ID|phím đã gõ|kết quả mong muốn`:

```
Z1|ho5c sinh|học sinh
```

Chạy `make test` (hoặc chưa có g++: `bash test/run-cases.sh test/vni-cases.txt bin/vgo-vni-cli`).
Ca nào sai hiện `FAIL` kèm kết quả thật. Thấy gõ sai ngoài đời: thêm ca **trước**, thấy
nó FAIL, rồi mới sửa code cho tới khi qua.

Kỳ vọng trong file đã đối chiếu với UniKey thật. Chỗ cố ý khác UniKey: `a4`/`A4` giữ
nguyên (UniKey ra `ã`), `yeu1` -> `yếu` (UniKey để nguyên).

## 4. Đọc code — bắt đầu từ đâu

| File | Làm gì |
|---|---|
| `src/vni.cpp` | `VniEngine::key()`: số nào ra dấu gì; `applyDigit()`: khi nào số giữ nguyên |
| `src/viet.cpp` | Lõi chữ Việt dùng chung (cả gói vgo Telex): bảng dấu, đặt thanh, kiểm âm tiết |
| `fcitx5/vniime.cpp` | Gắn vào fcitx5: nhận phím, ghi đè chữ trong ô nhập, Esc |

Gợi ý: đọc `VniEngine::key()` -> `applyDigit()` -> `handleTone()`, vừa đọc vừa chạy
`./vgo-vni-cli` với các chuỗi phím trong `test/vni-cases.txt`.

## 5. Có cả gói vgo (Telex)?

Xem `KET-HOP.md`.
