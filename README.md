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
| 3 | Auto Anh-Việt: kiểm âm tiết + từ điển 2 tầng + restore (Esc) tự học | ✅ xong (81/81 test) |
| — | Gõ tắt, VNI, Vietnamese auto-correct | ❌ ngoài scope |

**Nguyên tắc:** đơn giản nhất có thể, chỉ phục vụ một người, **không đụng Lotus**
(file riêng `libvgo.so` + `vgo.conf`; gỡ = xoá 3 file). Tối ưu remote: in-process,
không server phụ, ghi-đè theo diff (chỉ phần đuôi đổi), không cửa sổ gợi ý.

### Auto Anh-Việt (Phase 3)
Ba lớp, rẻ → mạnh:
1. **Kiểm cấu trúc âm tiết** — chuỗi không phải âm tiết Việt hợp lệ (sai phụ âm
   đầu/cuối, >1 cụm nguyên âm) → nhả thô. Bắt `browser`, `file`, `password`…
2. **Từ điển tiếng Anh 2 tầng** — chuỗi thô khớp từ Anh đã biết → nhả thô. Bắt
   cả từ trùng cấu trúc Việt (`test`→`test` thay vì `tét`, `this`, `code`…).
   - **base** (chỉ-đọc): `/usr/share/vgo/en-base.txt` (~8800 từ **≥4 ký tự** —
     lọc ≥4 để tránh va chạm `cas`→cá, `mas`→má, `cos`→có…).
   - **cá nhân** (tự nuôi): `~/.config/vgo/words.txt`, gộp với base khi tra.
   - `VN_PRIORITY` (hardcode trong engine) giữ vài âm Việt thường: `cos`→có, `nos`→nó.
3. **Restore = Esc** (khi đang gõ dở) — khoá từ về thô + **tự ghi vào
   `words.txt`** (lần sau auto). Dùng phím **không-modifier** vì app (VS Code/
   Telegram) nuốt tổ hợp Ctrl trước khi tới IME ở chế độ không-preedit.

**Giới hạn còn lại:** từ Anh **ngắn (≤3 ký tự)** trùng âm Việt và chưa có trong
dict vẫn bị đổi → bấm **Esc** để restore, nó tự nhớ cho lần sau.

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
