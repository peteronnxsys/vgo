# vgo — bộ gõ tiếng Việt (tự build)

Bộ gõ tiếng Việt cho Linux/fcitx5, viết từ đầu. Tách 2 lớp:

- **Engine** (`src/engine.{hpp,cpp}`) — lõi thuần C++, không phụ thuộc OS. Nhận
  chuỗi phím Telex → nhả Unicode có dấu (NFC). Test standalone được.
- **Addon fcitx5** (Phase 2) — bọc engine, móc vào input của hệ điều hành.

## Build & test

```sh
make test     # build + chạy unit test (chỉ cần g++, không cần gì thêm)
make          # build cả vgo-cli
./vgo-cli "Tooi yeeu tieengs Vieejt"   # -> Tôi yêu tiếng Việt
echo "chaof banj" | ./vgo-cli          # đọc stdin từng dòng
```

## Trạng thái

| Phase | Nội dung | Trạng thái |
|-------|----------|-----------|
| 1 | Engine Telex + unit test + CLI | ✅ xong |
| 2 | Addon fcitx5 in-process, KHÔNG preedit (ghi-đè surrounding text) | ✅ xong, đang dùng cạnh Lotus |
| 3 | Auto Anh-Việt (kiểm âm tiết, khỏi switch) | ✅ xong (76/76 test) |
| — | Gõ tắt, VNI, Vietnamese auto-correct | ❌ ngoài scope |

**Nguyên tắc:** đơn giản nhất có thể, chỉ phục vụ một người, **không đụng Lotus**
(file riêng `libvgo.so` + `vgo.conf`; gỡ = xoá 3 file). Tối ưu remote: in-process,
không server phụ, ghi-đè theo diff (chỉ phần đuôi đổi), không cửa sổ gợi ý.

### Auto Anh-Việt (Phase 3)
Mỗi từ, nếu chuỗi không tạo thành **âm tiết tiếng Việt hợp lệ** (sai phụ âm
đầu/cuối, hoặc >1 cụm nguyên âm) → nhả phím thô. Bắt được hầu hết từ Anh
(`browser`, `file`, `video`, `password`, `javascript`…).
**Giới hạn:** từ Anh ngắn trùng cấu trúc Việt (`test`→`tét`, `this`→`thí`) vẫn bị
đổi — không tránh được nếu không có từ điển. Gặp thì Ctrl+Space về tiếng Anh.

### Bảo trì
- Sửa engine → `make test` (kiểm) → `cmake --build build` → `sudo cmake --install build`
  → restart fcitx5 (`fcitx5 -r -d`). Đổi icon mà không hiện: kill hẳn fcitx5 rồi
  chạy lại (`pkill -x fcitx5; fcitx5 -d`) để xả icon cache — `-r` không đủ.
- Đặt dấu kiểu mới (`hoà`/`thuý`): đổi `oldStyleTone=false` trong `src/engine.hpp`.

### Lưu ý: app sandbox (snap/flatpak) chặn bộ gõ
App cài kiểu **snap/flatpak** chạy sandbox riêng, thường KHÔNG nối được fcitx5 →
gõ tiếng Việt chết trong app đó (vgo, Lotus, *mọi* bộ gõ — KHÔNG phải lỗi vgo).
Triệu chứng: gõ được ở browser/editor nhưng app sandbox thì không.
- Ví dụ đã gặp: **Telegram snap** → đổi sang bản **tĩnh** telegram.org
  (`~/.local/opt/Telegram`, không sandbox, tự cập nhật) là gõ được.

### Phase 2 cần (một lần, có sudo)

```sh
sudo apt install cmake extra-cmake-modules gettext \
  libfcitx5core-dev libfcitx5config-dev libfcitx5utils-dev
```

## Thuật toán đặt dấu thanh (phần khó nhất)

Cho một âm tiết, chọn nguyên âm mang dấu theo thứ tự:

1. **Âm đệm bỏ qua**: `qu` (u), `gi` + nguyên âm (i) → không mang dấu.
2. **Nguyên âm có dấu phụ** (â ê ô ă ơ ư) được ưu tiên. Nếu hai (`ươ`) → cái sau (`ơ`).
3. Không có dấu phụ:
   - 1 nguyên âm → nó.
   - Có phụ âm cuối → nguyên âm cuối của cụm.
   - Âm tiết mở, cụm `oa`/`oe`/`uy`:
     - **kiểu cũ** (mặc định): nguyên âm đầu — `hòa`, `khỏe`, `thủy`.
     - kiểu mới: nguyên âm sau — `hoà`, `khoẻ`, `thuỷ`.
   - Cụm mở khác → nguyên âm áp chót (`ái`, `mía`, `múa`, `ngoáy`).

Dấu được **chuẩn-hoá lại sau mỗi phím** (`normalizeTone`) nên gõ dấu trước hay
sau phụ âm cuối đều ra đúng (`toasn` và `toans` đều → `toán`).

## Quy ước Telex

| Gõ | Ra | | Gõ | Ra |
|----|----|-|----|----|
| s f r x j | sắc huyền hỏi ngã nặng | | aa ee oo | â ê ô |
| z | bỏ dấu thanh | | aw ow uw | ă ơ ư |
| dd | đ | | uow | ươ |

`w` đứng một mình = chữ `w` (gõ `ư` bằng `uw`, đỡ phá tiếng Anh).
Gõ lại phím điều khiển = undo + nhả chữ thật (`ass`→`as`, `aaa`→`aa`, `ddd`→`dd`).

## Tham khảo

- [Bamboo](https://github.com/BambooEngine/bamboo) — engine Go + `fcitx5-bamboo` (ref sát nhất cho fcitx5).
- [Bogo](https://github.com/BoGoEngine/bogo-python) — thuật toán Telex rõ ràng.
- [OpenKey](https://github.com/tuyenvm/OpenKey) — IME full (mac/Windows), đọc để hiểu cả stack.
