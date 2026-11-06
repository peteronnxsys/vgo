# vgo — bộ gõ tiếng Việt (tự build)

Bộ gõ tiếng Việt cho Linux/fcitx5, viết từ đầu. Tách 2 lớp:

- **Engine** (`src/engine.{hpp,cpp}`) — lõi thuần C++, không phụ thuộc OS. Nhận
  chuỗi phím Telex → nhả Unicode có dấu (NFC). Test standalone được.
- **Addon fcitx5** (Phase 2) — bọc engine, móc vào input của hệ điều hành.

## Build & test

```sh
make test     # build + chạy unit test (chỉ cần g++, không cần gì thêm)
make          # build cả vgo-cli
make test-vikey   # test cấp câu qua vgo-cli (có nạp data/*.txt): test/vikey-cases.txt
./vgo-cli "Tooi yeeu tieengs Vieejt"   # -> Tôi yêu tiếng Việt
echo "chaof banj" | ./vgo-cli          # đọc stdin từng dòng
```

## Trạng thái

| Phase | Nội dung | Trạng thái |
|-------|----------|-----------|
| 1 | Engine Telex + unit test + CLI | ✅ xong |
| 2 | Addon fcitx5 in-process, KHÔNG preedit (Chromium/Electron: preedit) | ✅ xong, đang dùng cạnh Lotus |
| 3 | Auto Anh-Việt: kiểm âm tiết + từ điển Anh + danh sách ép-Việt + Esc lật 2 chiều (tự học) | ✅ xong (141/141 test) |
| — | Gõ tắt, VNI, Vietnamese auto-correct | ❌ ngoài scope |

**Nguyên tắc:** đơn giản nhất có thể, chỉ phục vụ một người, **không đụng Lotus**
(file riêng `libvgo.so` + `vgo.conf`; gỡ = xoá 3 file). Tối ưu remote: in-process,
không server phụ, ghi-đè theo diff (chỉ phần đuôi đổi), không cửa sổ gợi ý.

### Auto Anh-Việt (Phase 3)
Mặc định nghiêng tiếng Việt; nhận diện tiếng Anh theo nhiều lớp, rẻ → mạnh:
1. **Kiểm cấu trúc âm tiết** — chuỗi không phải âm tiết Việt hợp lệ (sai phụ âm
   đầu/cuối, >1 cụm nguyên âm) → nhả thô. Bắt `browser`, `file`, `password`…
2. **Từ điển tiếng Anh 2 tầng** — chuỗi thô khớp từ Anh đã biết → nhả thô. Bắt
   cả từ trùng cấu trúc Việt (`test`/`this`/`code`/`list`/`cast`… giữ Anh, không mangle).
   - base (chỉ-đọc): `/usr/share/vgo/en-base.txt` (~8800 từ ≥4 ký tự + ~45 từ
     ngắn chọn tay ở cuối file: `see`/`is`/`of`/`are`/`yes`…).
   - cá nhân (tự học qua Esc): `~/.config/vgo/words.txt`.
3. **Danh sách ÉP-VIỆT** — ưu tiên **hơn** từ điển Anh, cho từ Việt trùng mặt chữ
   một từ Anh (`been`→bên, `teen`→tên, `queen`→quên, `barn`→bản, `dans`→dán…).
   - base (chỉ-đọc): `/usr/share/vgo/vn-base.txt`.
   - cá nhân (tự học qua Esc): `~/.config/vgo/vn-words.txt`.
   - thêm `VN_PRIORITY` hardcode trong engine: `cos`→có, `nos`→nó.
4. **Ngữ cảnh từ trước (bigram)** — `từ_trước phím_thô en|vn`, thắng cả lớp 2 và
   3 (Esc vẫn thắng tất): `it's been`→Anh nhưng `owr been`→ở bên; `xe bus`→Anh nhưng
   `cho con bus`→bú. Từ trước tra theo cả chữ đã hiện lẫn phím thô (`has`/"há").
   - base (chỉ-đọc): `/usr/share/vgo/bigram-base.txt`.
   - cá nhân: `~/.config/vgo/bigram.txt` (Esc tự ghi, sửa tay được).
   - Kèm luật **viết tắt IN HOA**: từ IN HOA 2–4 chữ, không ép-Việt, đứng sau một
     từ có chữ thường → nhả thô (`mua NAS`). Câu IN HOA toàn bộ / đầu dòng vẫn ra
     Việt (`VIỆT NAM VÀ`, `NAS` đầu dòng → NÁ).
5. **Esc = nút LẬT 2 CHIỀU** (khi đang gõ dở) — lật theo trạng thái hiện tại **và
   tự nhớ**:
   - đang ra Anh mà muốn Việt → Esc → về Việt (ghi `vn-words.txt`).
   - đang ra Việt mà muốn Anh → Esc → về Anh (ghi `words.txt`).
   - có từ đứng trước → chỉ nhớ **đúng cặp** đó (ghi `bigram.txt`, vd `xe bus en`),
     không nhớ cả từ — để lật `xe bus` không làm `cho con bus` thành Anh luôn.
     Đầu dòng / sau Enter, mũi tên, Ctrl… → nhớ cả từ như trên.
   - Dùng phím **không-modifier** vì app (VS Code/Telegram) nuốt tổ hợp Ctrl
     trước khi tới IME ở chế độ không-preedit.

6. **Tự sửa gõ nhầm 1 phím** (mô hình AOSP LatinIME, chỉ khi đường cũ sắp nhả thô vì
   chuỗi không phải âm tiết Việt và không phải từ Anh): xoá 1 phím thừa rồi gõ lại qua
   engine, chỉ nhận âm tiết **có trong** `/usr/share/vgo/vn-syllables.txt` (~6.2k âm
   tiết + tần suất, lọc từ FrequencyWords vi_50k bằng `make lex`). Chấm điểm theo chi
   phí lỗi + độ hiếm của LatinIME, ngưỡng 0.185 ("modest"), bản nhì sát nút thì thôi.
   - Nhận: phụ âm gõ đúp (`ccofn`→còn, `nhhir`→nhỉ), phím đầu trượt sang phím kề
     (`jkhas`→khá, `bnos`→nó, `cghuwa`→chưa), nguyên âm gõ sớm kẹp giữa phụ âm đầu
     (`nahawns`→nhắn).
   - Không làm (đo trên 74.7k từ `/usr/share/dict/words`): thay phím / chèn phím /
     đảo phím — song ngữ thì từ Anh nào cũng cách 1 âm tiết Việt đúng 1 phím thay
     (1.279/1.314 ca sửa nhầm); xoá ở đuôi từ (đuôi `-s`/`-rs` tiếng Anh, hoàn tác
     Telex `thijj`); dạng biến tố của từ Anh (`warns`). Còn lại: 2 tên riêng.
   - Hiện ngay lúc gõ; **Esc** lật về đúng phím thô. **Backspace ngay sau khi chốt**
     (space/dấu câu) bản tự sửa → trả lại phím thô (như LatinIME `revertCommit`;
     chỉ khi text trước con trỏ khớp đúng, không thì Backspace thường). Hoàn tác
     không ghi file học; Esc lật sang Việt cũng không lưu từ gõ nhầm.

**Bản chất:** va chạm Anh↔Việt là cùng phím khác ý → không tự đoán 100% được.
Esc + tự nhớ là van chỉnh tay: gặp từ lệch (chiều nào cũng vậy) → Esc một phát
là về đúng + nhớ cho lần sau.

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

### Lưu ý: app Chromium/Electron dùng preedit
Chrome, VS Code, Slack, Electron… cài đặt surrounding-text **xoá lỗi** (cả
`deleteSurroundingText` lẫn forward-backspace đều không ăn) → ghi-đè theo diff bị
dồn: `các`→`caác`, `được`→`dđuoươợc`. Giải: nhận diện qua `program()` (chứa
`chrom`/`electron`/`code`/`slack`/`discord`) → ép **preedit** (chạy đúng như
terminal); app khác giữ no-preedit (commit, autofill OK). Preedit không phá
autofill Chrome (đã kiểm). Gặp app Chromium lạ → thêm vào `isChromiumApp()` trong
`fcitx5/vgoime.cpp`.

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
`w` áp lên **cả cụm nguyên âm**, gõ sau phụ âm cuối vẫn đúng: `cuarw`→cửa, `thuongwr`→thưởng,
`nguoiwf`→người, `ruouwj`→rượu. `uo`+`w` ở âm tiết mở → `uơ` (`thuowr`→thuở, `huow`→huơ),
gõ tiếp chữ → tự thành `ươ` (`huowng`→hương). `qu`/`gi` đầu từ không nhận sừng (`quowr`→quở).
Mũ / đ **không cần liền nhau**: `toio`→tôi, `tiene`→tiên, `muonos`→muốn; `dd` gõ cuối từ bắt đầu
bằng `d` → đ đầu từ (`didd`→đi, `duongwddf`→đường). `did` (1 chữ d) giữ thô. Vần `oeo` thật
(`ngoeoj`→ngoẹo) không bị biến thành mũ. Đánh đổi: từ Anh khớp khuôn (`dense`, `photo`, `data`)
dựa vào từ điển Anh để giữ thô — từ lạ ngoài từ điển thì Esc.
Gõ đúp **phím dấu thanh** (s/f/r/x/j) = **bỏ dấu + nhả chữ thật** (chuẩn Telex như
UniKey/phone/Lotus) → "thoát" từ có dấu về thô. Lỡ tay thì **Backspace** lùi đúng
1 phím là về lại. Gõ lại **mũ/sừng/đ** = revert (`aaa`→`aa`, `ddd`→`dd`, `aww`→`aw`).

## Tham khảo

- [Bamboo](https://github.com/BambooEngine/bamboo) — engine Go + `fcitx5-bamboo` (ref sát nhất cho fcitx5).
- [Bogo](https://github.com/BoGoEngine/bogo-python) — thuật toán Telex rõ ràng.
- [OpenKey](https://github.com/tuyenvm/OpenKey) — IME full (mac/Windows), đọc để hiểu cả stack.
- [AOSP LatinIME](https://android.googlesource.com/platform/packages/inputmethods/LatinIME) —
  `native/jni/src/suggest/policyimpl/typing/scoring_params.cpp` (chi phí lỗi),
  `InputLogic.revertCommit` (Backspace hoàn tác). Nguồn cho tự sửa gõ nhầm.
- [FrequencyWords](https://github.com/hermitdave/FrequencyWords) — `vi_50k.txt`
  (OpenSubtitles, CC-BY-SA-4.0) → `data/vn-syllables.txt`.
