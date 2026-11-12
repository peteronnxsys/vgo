# vgo — bộ gõ tiếng Việt kiểu Telex cho Linux (fcitx5)

Gõ Telex, tự nhận tiếng Anh xen giữa câu (`cần test cái file này` không bị biến thành
`cần tét cái fi lê này`), tự sửa gõ nhầm 1 phím. Viết từ đầu bằng C++, không phụ thuộc
bộ gõ nào khác. Thiết kế chi tiết + lý do từng luật: xem `README.md`.

## 1. Dùng ngay (bản build sẵn, Ubuntu 24.04)

Làm theo **`CAI-DAT.md`**: bật fcitx5 (một lần) -> `./install.sh` -> thêm "vgo (Telex)"
trong fcitx5-configtool. Gặp trục trặc: bảng sự cố cuối `CAI-DAT.md`.

Gõ thử không cần cài (chạy từ thư mục gói, để nạp được `data/`):
`bin/vgo-cli "Tooi yeeu tieengs Vieejt"` -> `Tôi yêu tiếng Việt`.

Cách gõ: `s f r x j` = sắc huyền hỏi ngã nặng, `aa ee oo` = â ê ô, `aw ow uw` = ă ơ ư,
`dd` = đ, `z` = bỏ dấu. **Esc** khi đang gõ dở = lật Anh <-> Việt cho từ đó, và bộ gõ
tự nhớ lần sau, ghi vào `~/.config/vgo/`:
`words.txt` (từ Anh), `vn-words.txt` (ép Việt), `bigram.txt` (cặp từ). File chữ thường,
mở ra sửa / xoá dòng được; xoá cả thư mục = quên hết. Sửa xong `fcitx5 -r -d`.

Đặt dấu kiểu mới (`hoà`/`thuý` thay vì `hòa`/`thúy`): đổi `oldStyleTone = false` trong
`src/engine.hpp` rồi build lại.

## 2. Build từ source

```sh
sudo apt install g++ make                       # đủ cho engine + test
make test                                        # build + chạy mọi bộ test
./vgo-cli "Tooi yeeu tieengs Vieejt"            # -> Tôi yêu tiếng Việt

# addon fcitx5 (cần thêm thư viện dev, cài 1 lần):
sudo apt install cmake extra-cmake-modules gettext \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev
cmake -S . -B build && cmake --build build
sudo cmake --install build && fcitx5 -r -d
```

Sửa code xong: `make test` -> `cmake --build build` -> `sudo cmake --install build`
-> `fcitx5 -r -d`. Build từ source thì không cần `install.sh` (nó chỉ cài bản trong `bin/`).
`make share` chỉ chạy được trong repo gốc, không cần trong gói này.

## 3. Test

- `test/tests.cpp` — unit test từng từ: `{"phím", "kết quả"}`. Thêm dòng rồi `make test`.
- `test/vikey-cases.txt` — test cả câu (có nạp từ điển): `ID|phím|kết quả`.
- Thấy gõ sai: thêm ca vào một trong hai file **trước**, thấy nó báo sai, rồi mới sửa
  code cho tới khi qua — để lỗi không quay lại.

## 4. Đọc code — bắt đầu từ đâu

| File | Làm gì |
|---|---|
| `src/viet.cpp` | Lõi chữ Việt dùng chung (cả gói VNI): bảng dấu, đặt thanh đúng chỗ, kiểm âm tiết hợp lệ |
| `src/engine.cpp` | Telex: `Engine::key()` xử lý từng phím; `isShowingRaw()` quyết định Anh hay Việt; `correction()` tự sửa gõ nhầm |
| `fcitx5/vgoime.cpp` | Gắn vào fcitx5: nhận phím, ghi đè chữ trong ô nhập, Esc tự học |
| `data/` | Từ điển Anh, danh sách ép-Việt, bigram, âm tiết Việt + tần suất |

Gợi ý: đọc `Engine::key()` trước, chạy `./vgo-cli` với vài chuỗi phím, rồi đọc
`findToneTarget()` trong `viet.cpp` (luật đặt dấu — phần khó nhất, README có giải thích).

## 5. Có cả gói vgo-vni?

Xem `KET-HOP.md`.
